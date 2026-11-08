#include "StarExplorationProfile.hpp"
#include "StarWorldGenerationTasks.hpp"
#include "StarWorldRegionCoordinator.hpp"
#include "StarConfiguration.hpp"
#include "StarFile.hpp"
#include "StarJsonExtra.hpp"
#include "StarLogging.hpp"
#include "StarMonster.hpp"
#include "StarNpc.hpp"
#include "StarRoot.hpp"
#include "StarTime.hpp"
#include "StarWorldServer.hpp"
#include "StarWorldClient.hpp"

#include <atomic>
#include <cmath>
#include <initializer_list>
#include <thread>

namespace Star {

namespace {

String diagnosticPathPart(String const& value, String const& fallback) {
  String result;
  for (auto c : value) {
    if (String::isAsciiLetter(c) || String::isAsciiNumber(c) || c == '_' || c == '-')
      result += c;
    else
      result += '_';
    if (result.utf8Size() >= 80)
      break;
  }
  return result.empty() ? fallback : result;
}

bool hasPrefix(String const& name, std::initializer_list<char const*> prefixes) {
  for (auto prefix : prefixes) {
    if (name.beginsWith(prefix))
      return true;
  }
  return false;
}

}

thread_local ExplorationProfile::EntityScope* ExplorationProfile::s_currentEntityScope = nullptr;
thread_local ExplorationProfile::NpcScope* ExplorationProfile::s_currentNpcScope = nullptr;

ExplorationProfilePtr ExplorationProfile::create(String component, String context) {
  auto config = Root::singleton().configuration()->get("explorationProfiling", JsonObject());
  if (!config.isType(Json::Type::Object) || !config.getBool("enabled", false))
    return {};

  static std::atomic<uint64_t> nextInstance{0};
  auto profile = make_shared<ExplorationProfile>();
  profile->m_component = std::move(component);
  profile->m_context = std::move(context);
  profile->m_instance = ++nextInstance;
  auto interval = config.getDouble("intervalSeconds", 5);
  auto slowMs = config.getDouble("slowOperationMilliseconds", 50);
  profile->m_interval = std::isfinite(interval) ? clamp(interval, 1.0, 60.0) : 5.0;
  profile->m_slowSeconds = (std::isfinite(slowMs) ? clamp(slowMs, 1.0, 60000.0) : 50.0) / 1000.0;
  profile->m_entityDetailLimit = (unsigned)clamp<uint64_t>(config.getUInt("entityDetailLimit", 12), 1, 50);
  profile->m_npcDetails = profile->m_component == "server" && config.getBool("npcDetails", false);
  profile->m_regionalDetails = profile->m_component == "server" && config.getBool("regionalDetails", false);
  profile->m_writeReportsToFiles = config.getBool("writeReportsToFiles", true);
  profile->m_reportFileMaxBytes = clamp<uint64_t>(config.getUInt("reportFileMaxBytes", 8 * 1024 * 1024),
      64 * 1024, 64 * 1024 * 1024);
  profile->m_reportFileBackups = (unsigned)clamp<uint64_t>(config.getUInt("reportFileBackups", 3), 0, 20);
  if (profile->m_writeReportsToFiles) {
    profile->m_reportDirectory = Root::singleton().toStoragePath("logs/exploration");
    File::makeDirectoryRecursive(profile->m_reportDirectory);
  }
  profile->m_sessionStart = Time::monotonicTime();
  profile->m_windowStart = profile->m_sessionStart;
  Logger::info("[ExplorationProfile] enabled component={} instance={} context={} reportFiles={}",
      profile->m_component, profile->m_instance, profile->m_context, profile->m_writeReportsToFiles);
  return profile;
}

ExplorationProfile* ExplorationProfile::forWorld(World* world) {
  if (auto server = dynamic_cast<WorldServer*>(world))
    return server->explorationProfile();
  if (auto client = dynamic_cast<WorldClient*>(world))
    return client->explorationProfile();
  return nullptr;
}

ExplorationProfile::Scope::Scope(ExplorationProfile* profile, char const* stage)
  : m_profile(profile), m_stage(stage), m_start(profile ? Time::monotonicTime() : 0) {}

ExplorationProfile::Scope::~Scope() {
  stop();
}

void ExplorationProfile::Scope::stop() {
  if (m_profile && m_stage) {
    m_profile->record(m_stage, Time::monotonicTime() - m_start);
    m_stage = nullptr;
  }
}

void ExplorationProfile::Scope::next(char const* stage) {
  stop();
  m_stage = stage;
  if (m_profile)
    m_start = Time::monotonicTime();
}

ExplorationProfile::EntityScope::EntityScope(ExplorationProfile* profile, Entity const& entity)
  : m_profile(profile), m_entity(profile ? &entity : nullptr), m_previousScope(nullptr),
    m_previousLogAccumulator(nullptr), m_start(0) {
  if (m_profile) {
    m_previousScope = s_currentEntityScope;
    s_currentEntityScope = this;
    m_previousLogAccumulator = Logger::setThreadLogTimeAccumulator(&m_loggingSeconds);
    m_start = Time::monotonicTime();
  }
}

ExplorationProfile::EntityScope::~EntityScope() {
  if (m_profile) {
    double elapsed = Time::monotonicTime() - m_start;
    Logger::setThreadLogTimeAccumulator(m_previousLogAccumulator);
    s_currentEntityScope = m_previousScope;
    // Diagnostics must not turn an entity update exception into termination
    // from this noexcept destructor.
    try {
      m_profile->recordEntity(*m_entity, elapsed, m_luaSeconds, m_loggingSeconds);
    } catch (...) {}
  }
}

void ExplorationProfile::EntityScope::recordLua(double seconds) {
  m_luaSeconds += seconds;
}

ExplorationProfile::ObjectWorldCallScope::ObjectWorldCallScope(char const* stage, Maybe<EntityId> target, char const* operation)
  : m_stage(stage), m_operation(operation), m_target(target) {
  auto scope = s_currentEntityScope;
  if (scope && scope->m_entity && scope->m_entity->entityType() == EntityType::Object) {
    m_profile = scope->m_profile;
    if (m_profile->m_regionalDetails) {
      m_rootId = scope->m_entity->entityId();
      auto position = scope->m_entity->position();
      if (std::isfinite(position[0]) && std::isfinite(position[1])
          && std::abs(position[0]) < 1e9 && std::abs(position[1]) < 1e9)
        m_rootRegion = Vec2I((int)std::floor(position[0] / 64.0), (int)std::floor(position[1] / 64.0));
    }
    m_start = Time::monotonicTime();
  }
}

void ExplorationProfile::ObjectWorldCallScope::nextPhase(char const* phase) {
  if (!m_profile || !m_profile->m_regionalDetails)
    return;
  double now = Time::monotonicTime();
  try {
    if (m_phase)
      m_profile->record(m_phase, now - m_phaseStart);
  } catch (...) {}
  m_phase = phase;
  m_phaseStart = now;
}

ExplorationProfile::ObjectWorldCallScope::~ObjectWorldCallScope() {
  if (m_profile) {
    double elapsed = Time::monotonicTime() - m_start;
    // Diagnostic allocation must never interrupt a gameplay callback unwinding.
    try {
      nextPhase(nullptr);
      m_profile->record(m_stage, elapsed);
      if (m_profile->m_regionalDetails && m_operation)
        m_profile->record(m_operation, elapsed);
      if (m_profile->m_regionalDetails && !m_target)
        m_profile->count("regional.objectCallsWithoutEntityTarget");
      if (m_profile->m_regionalDetails && m_target) {
        auto key = std::make_tuple(m_rootId, *m_target, String(m_stage));
        auto& edges = m_profile->m_objectDependencies;
        auto found = edges.find(key);
        if (found == edges.end() && edges.size() >= 1024) {
          ++m_profile->m_dependencyOverflow;
        } else {
          auto& edge = found == edges.end() ? edges[key] : found->second;
          ++edge.calls;
          edge.seconds += elapsed;
          edge.rootRegion = m_rootRegion;
        }
      }
    } catch (...) {}
  }
}

ExplorationProfile::EntityLuaScope::EntityLuaScope()
  : m_entityScope(s_currentEntityScope), m_start(m_entityScope ? Time::monotonicTime() : 0) {}

ExplorationProfile::EntityLuaScope::~EntityLuaScope() {
  if (m_entityScope)
    m_entityScope->recordLua(Time::monotonicTime() - m_start);
}

void ExplorationProfile::setContext(String context) {
  if (m_context == context)
    return;
  m_context = std::move(context);
  // A server is constructed as Nowhere and receives its stable world id a
  // moment later. Never keep report paths derived from the transient context;
  // the next report must create files under the real world identity.
  m_reportFiles.clear();
}

ExplorationProfile* ExplorationProfile::npcProfile(World* world) {
  auto server = dynamic_cast<WorldServer*>(world);
  auto profile = server ? server->explorationProfile() : nullptr;
  return profile && profile->m_npcDetails ? profile : nullptr;
}

ExplorationProfile::NpcScope::NpcScope(World* world, NpcStage stage)
  : NpcScope(npcProfile(world), stage) {}

ExplorationProfile::NpcScope::NpcScope(NpcStage stage)
  : NpcScope(s_currentNpcScope ? s_currentNpcScope->m_profile : nullptr, stage) {}

ExplorationProfile::NpcScope::NpcScope(ExplorationProfile* profile, NpcStage stage)
  : m_profile(profile), m_previous(s_currentNpcScope), m_stage(stage) {
  // Also mask a previous context when entering an unprofiled world/client.
  if (m_profile || m_previous)
    s_currentNpcScope = this;
  if (m_profile) {
    m_previousLogAccumulator = Logger::setThreadLogTimeAccumulator(&m_loggingSeconds);
    m_start = Time::monotonicTime();
  }
}

ExplorationProfile::NpcScope::~NpcScope() {
  if (m_profile) {
    double elapsed = Time::monotonicTime() - m_start;
    Logger::setThreadLogTimeAccumulator(m_previousLogAccumulator);
    // Preserve the existing entity logging subtotal and inclusive parent logs.
    if (m_previousLogAccumulator)
      *m_previousLogAccumulator += m_loggingSeconds;
    if (m_previous && m_previous->m_profile == m_profile)
      m_previous->m_childSeconds += elapsed;
    auto& timing = m_profile->m_npcTimings[static_cast<size_t>(m_stage)];
    timing.add(elapsed, m_profile->m_slowSeconds);
    timing.exclusiveSeconds += max(0.0, elapsed - m_childSeconds);
    timing.loggingSeconds += m_loggingSeconds;
  }
  if (m_profile || m_previous)
    s_currentNpcScope = m_previous;
}

void ExplorationProfile::recordNpcPathResult(Maybe<bool> result) {
  if (s_currentNpcScope && s_currentNpcScope->m_profile)
    ++s_currentNpcScope->m_profile->m_npcPathResults[result ? (*result ? 1 : 2) : 0];
}

ExplorationProfile* ExplorationProfile::activeNpcProfile() {
  return s_currentNpcScope ? s_currentNpcScope->m_profile : nullptr;
}

void ExplorationProfile::recordNpcPopulation(uint64_t updatedEntities) {
  if (m_npcDetails) {
    ++m_npcPopulationSamples;
    m_npcUpdatedEntities += updatedEntities;
    m_npcMaximumUpdatedEntities = max(m_npcMaximumUpdatedEntities, updatedEntities);
  }
}

void ExplorationProfile::Timing::add(double seconds, double slowSeconds) {
  ++samples;
  slowSamples += seconds >= slowSeconds;
  totalSeconds += seconds;
  maximumSeconds = max(maximumSeconds, seconds);
  size_t bucket = 0;
  double microseconds = seconds * 1000000;
  if (microseconds >= 1) {
    int exponent;
    double mantissa = std::frexp(microseconds, &exponent) * 2;
    bucket = min(HistogramSize - 1,
        static_cast<size_t>(1 + (exponent - 1) * 8 + static_cast<int>((mantissa - 1) * 8)));
  }
  ++histogram[bucket];
}

double ExplorationProfile::Timing::percentileUpperMilliseconds(unsigned percentile) const {
  if (!samples)
    return 0;
  uint64_t rank = (samples / 100) * percentile + ((samples % 100) * percentile + 99) / 100;
  uint64_t accumulated = 0;
  for (size_t i = 0; i < HistogramSize; ++i) {
    accumulated += histogram[i];
    if (accumulated >= rank) {
      if (i == HistogramSize - 1)
        return maximumSeconds * 1000;
      double upperUs = i == 0 ? 1.0 : std::ldexp(1.0 + ((i - 1) % 8 + 1) / 8.0, (i - 1) / 8);
      return min(maximumSeconds * 1000, upperUs / 1000);
    }
  }
  return maximumSeconds * 1000;
}

JsonObject ExplorationProfile::Timing::toJson() const {
  return JsonObject{
    {"samples", samples}, {"slowSamples", slowSamples},
    {"totalMilliseconds", totalSeconds * 1000},
    {"meanMilliseconds", samples ? totalSeconds * 1000 / samples : 0},
    {"maximumMilliseconds", maximumSeconds * 1000},
    {"p50UpperMilliseconds", percentileUpperMilliseconds(50)},
    {"p95UpperMilliseconds", percentileUpperMilliseconds(95)},
    {"p99UpperMilliseconds", percentileUpperMilliseconds(99)}
  };
}

void ExplorationProfile::record(char const* stage, double seconds) {
  auto& timing = m_timings[stage];
  timing.add(seconds, m_slowSeconds);
}

void ExplorationProfile::count(char const* counter, uint64_t amount) {
  m_counts[counter] += amount;
}

void ExplorationProfile::maximum(char const* counter, uint64_t value) {
  auto& current = m_maxima[counter];
  current = std::max(current, value);
}

void ExplorationProfile::recordStoryHop(uint64_t traceId, char const* hop, double queueSeconds) {
  if (!traceId)
    return;
  if (m_storyHops.size() >= 64) {
    count("story.hopsDropped");
    return;
  }
  m_storyHops.append(JsonObject{{"traceId", traceId}, {"hop", hop},
      {"queueMilliseconds", std::max(0.0, queueSeconds) * 1000.0}});
}

void ExplorationProfile::recordQueuedSector(Vec2I sector, double seconds) {
  if (!m_oldestQueuedSector || seconds > m_oldestQueuedSector->seconds)
    m_oldestQueuedSector = SectorWait{sector, seconds};
}

void ExplorationProfile::recordCompletedSector(Vec2I sector, double seconds) {
  if (seconds < m_slowSeconds)
    return;
  SectorWait wait{sector, seconds};
  if (m_slowSectors.size() < m_entityDetailLimit) {
    m_slowSectors.append(wait);
  } else {
    auto shortest = std::min_element(m_slowSectors.begin(), m_slowSectors.end(), [](auto const& a, auto const& b) {
      return a.seconds < b.seconds;
    });
    if (wait.seconds > shortest->seconds)
      *shortest = wait;
  }
}

void ExplorationProfile::recordTilePacket(Vec2I tileOrigin) {
  if (m_tilePackets.size() < 64)
    m_tilePackets.append(TilePacketSample{tileOrigin, Time::monotonicTime()});
  else
    count("tilePacketMarkersDropped");
}

void ExplorationProfile::recordBaseTileOutcome(Vec2I sector, bool stale, uint64_t revisionDelta,
    JsonObject reasons, double ageSeconds) {
  BaseTileOutcome outcome{sector, revisionDelta, std::move(reasons), ageSeconds};
  auto& list = stale ? m_staleBaseTiles : m_evictedBaseTiles;
  if (list.size() < m_entityDetailLimit) {
    list.append(std::move(outcome));
  } else {
    auto shortest = std::min_element(list.begin(), list.end(), [](auto const& a, auto const& b) {
      return a.ageSeconds < b.ageSeconds;
    });
    if (outcome.ageSeconds > shortest->ageSeconds)
      *shortest = std::move(outcome);
  }
}

void ExplorationProfile::recordGenerationTask(WorldGenerationTaskIdentity const& identity, char const* event, size_t reservedBytes) {
  if (!m_regionalDetails)
    return;
  if (m_generationTaskEvents.size() >= 64) {
    count("generation.tasks.eventsDropped");
    return;
  }
  m_generationTaskEvents.append(JsonObject{
    {"worldEpoch", identity.worldEpoch}, {"sequence", identity.sequence},
    {"sector", JsonArray{identity.sector[0], identity.sector[1]}},
    {"region", JsonArray{identity.region[0], identity.region[1]}},
    {"stage", "baseTiles"}, {"event", event}, {"reservedArrayBytes", reservedBytes},
    {"monotonicMilliseconds", Time::monotonicTime() * 1000}
  });
}

void ExplorationProfile::recordGenerationRequest(uint64_t epoch, uint64_t sequence, Vec2I sector,
    char const* event, double ageSeconds, int generationLevel) {
  count(strf("generation.requests.{}", event).c_str());
  if (!m_regionalDetails)
    return;
  bool selection = String(event) == "selectedHead" || String(event) == "shadowOldest";
  auto& events = selection ? m_generationSelectionEvents : m_generationRequestEvents;
  if (events.size() >= (selection ? 16 : 64)) {
    count(selection ? "generation.selection.eventsDropped" : "generation.requests.eventsDropped");
    return;
  }
  events.append(JsonObject{
    {"requestEpoch", epoch}, {"requestSequence", sequence},
    {"sector", jsonFromVec2I(sector)}, {"event", event},
    {"ageMilliseconds", ageSeconds * 1000}, {"generationLevel", generationLevel},
    {"monotonicMilliseconds", Time::monotonicTime() * 1000}
  });
}

void ExplorationProfile::recordSlowMicroDungeonFactory(uint64_t templateHash, Vec2I sector,
    size_t footprintTiles, bool generated, double seconds,
    double definitionSeconds, double anchorSeconds, double collisionSeconds,
    double canPlaceSeconds, double buildSeconds, uint32_t placementAttempts,
    uint32_t boundsRejected, uint32_t collisionRejected, uint32_t placementRejected,
    bool anchorFound) {
  if (!m_regionalDetails || seconds < m_slowSeconds)
    return;
  SlowMicroDungeonFactory sample{templateHash, sector, footprintTiles, generated, seconds,
      definitionSeconds, anchorSeconds, collisionSeconds, canPlaceSeconds, buildSeconds,
      placementAttempts, boundsRejected, collisionRejected, placementRejected, anchorFound};
  if (m_slowMicroDungeonFactories.size() < m_entityDetailLimit) {
    m_slowMicroDungeonFactories.append(std::move(sample));
  } else {
    auto shortest = std::min_element(m_slowMicroDungeonFactories.begin(), m_slowMicroDungeonFactories.end(),
        [](auto const& a, auto const& b) { return a.seconds < b.seconds; });
    if (sample.seconds > shortest->seconds)
      *shortest = std::move(sample);
  }
}

void ExplorationProfile::recordRegionalTurn(WorldRegionTurn const& turn, char const* event, uint64_t observationStep) {
  if (!m_regionalDetails)
    return;
  if (m_regionTurnEvents.size() >= 64) {
    count("regional.shadow.eventsDropped");
    return;
  }
  m_regionTurnEvents.append(JsonObject{
    {"worldEpoch", turn.worldEpoch}, {"sequence", turn.sequence}, {"lane", turn.lane},
    {"region", JsonArray{turn.snapshot.region[0], turn.snapshot.region[1]}},
    {"captureStep", turn.snapshot.captureStep}, {"observationStep", observationStep},
    {"event", event}
  });
}

void ExplorationProfile::recordEntity(Entity const& entity, double totalSeconds, double luaSeconds, double loggingSeconds) {
  String type = EntityTypeNames.getRight(entity.entityType());
  auto& timing = m_entityTimings[type];
  timing.add(totalSeconds, m_slowSeconds);
  timing.luaSeconds += luaSeconds;
  timing.loggingSeconds += loggingSeconds;

  // Keep the high-cost built-in coordinator label distinct without retaining
  // arbitrary mod script names. This is diagnostics only and is intentionally
  // a subset of the stagehand aggregate above.
  String name = entity.name();
  if (type == "stagehand" && name == "coordinator") {
    record("entity.stagehand.coordinator", totalSeconds);
    record("entity.stagehand.coordinator.lua", luaSeconds);
    count("entity.stagehand.coordinator.updates");
    if (totalSeconds >= m_slowSeconds)
      count("entity.stagehand.coordinator.slowUpdates");
  }

  if (m_regionalDetails) {
    auto position = entity.position();
    if (std::isfinite(position[0]) && std::isfinite(position[1])
        && std::abs(position[0]) < 1e9 && std::abs(position[1]) < 1e9) {
      // Logical 64-tile bins only; no change to physical sector geometry.
      auto key = std::make_tuple((int)std::floor(position[0] / 64.0),
          (int)std::floor(position[1] / 64.0), type);
      auto found = m_regionalEntities.find(key);
      if (found == m_regionalEntities.end() && m_regionalEntities.size() >= 256) {
        ++m_regionOverflow;
      } else {
        auto& region = found == m_regionalEntities.end() ? m_regionalEntities[key] : found->second;
        ++region.calls;
        region.seconds += totalSeconds;
        region.luaSeconds += luaSeconds;
      }
    }
  }

  if (totalSeconds < m_slowSeconds)
    return;

  String kind = name;
  if (auto npc = dynamic_cast<Npc const*>(&entity))
    kind = npc->npcType();
  else if (auto monster = dynamic_cast<Monster const*>(&entity))
    kind = monster->typeName();

  SlowEntity slow{
    type,
    entity.entityId(),
    std::move(name),
    std::move(kind),
    entity.position(),
    (uint64_t)std::hash<std::thread::id>{}(std::this_thread::get_id()),
    totalSeconds,
    luaSeconds,
    loggingSeconds
  };
  if (m_slowEntities.size() < m_entityDetailLimit) {
    m_slowEntities.append(std::move(slow));
  } else {
    auto least = std::min_element(m_slowEntities.begin(), m_slowEntities.end(), [](auto const& a, auto const& b) {
      return a.totalSeconds < b.totalSeconds;
    });
    if (slow.totalSeconds > least->totalSeconds)
      *least = std::move(slow);
  }
}

void ExplorationProfile::writeReportFile(String const& category, Json const& report) {
  if (!m_writeReportsToFiles)
    return;

  auto file = m_reportFiles.ptr(category);
  if (!file) {
    auto directory = File::relativeTo(m_reportDirectory, diagnosticPathPart(category, "world"));
    directory = File::relativeTo(directory, diagnosticPathPart(m_component, "component"));
    File::makeDirectoryRecursive(directory);
    auto context = diagnosticPathPart(m_context, "session");
    file = &m_reportFiles[category];
    // Instance numbering restarts for every process and was not sufficient to
    // isolate consecutive launches when a previous primary JSONL existed.
    // Session start is recorded in every line and makes the filename unique
    // across normal launches; the existing bounded rotation remains a safety
    // net for an exceptionally identical session identity.
    *file = File::relativeTo(directory, strf("{}-{}-{}.jsonl", context, m_instance,
        static_cast<uint64_t>(m_sessionStart * 1000)));
    // Instance numbering is process-local and restarts at one on the next
    // launch. Never append that next launch to a matching old JSONL: preserve
    // it through the configured rotation and make the primary file the current
    // diagnostic session. This keeps each uploaded primary report analyzable
    // without reconstructing boundaries from monotonic timestamps.
    if (File::isFile(*file))
      File::backupFileInSequence(*file, m_reportFileBackups);
  }

  String line = report.printJson(0, true) + "\n";
  if (File::isFile(*file) && File::fileSize(*file) + line.utf8Size() > m_reportFileMaxBytes) {
    File::backupFileInSequence(*file, m_reportFileBackups);
    File::writeFile(line, *file);
  } else {
    auto output = File::open(*file, IOMode::Write | IOMode::Append);
    output->writeFull(line.utf8Ptr(), line.utf8Size());
  }
}

void ExplorationProfile::report(bool final) {
  double now = Time::monotonicTime();
  if (!final && now - m_windowStart < m_interval)
    return;

  JsonArray dependencies;
  for (auto const& entry : m_objectDependencies)
    dependencies.append(JsonObject{{"rootObjectId", std::get<0>(entry.first)},
        {"targetId", std::get<1>(entry.first)}, {"lastRootRegion", jsonFromVec2I(entry.second.rootRegion)}, {"operation", std::get<2>(entry.first)},
        {"calls", entry.second.calls}, {"inclusiveMilliseconds", entry.second.seconds * 1000}});
  JsonArray regions;
  for (auto const& entry : m_regionalEntities)
    regions.append(JsonObject{{"region", jsonFromVec2I(Vec2I(std::get<0>(entry.first), std::get<1>(entry.first)))},
        {"entityType", std::get<2>(entry.first)}, {"updates", entry.second.calls},
        {"totalMilliseconds", entry.second.seconds * 1000}, {"luaMilliseconds", entry.second.luaSeconds * 1000}});
  JsonObject regionalDiagnostics{{"enabled", m_regionalDetails}, {"regionSizeTiles", 64},
      {"objectDependencies", std::move(dependencies)}, {"entityRegions", std::move(regions)},
      {"dependencyCallsDropped", m_dependencyOverflow}, {"entityUpdatesDropped", m_regionOverflow}};
  m_objectDependencies.clear();
  m_regionalEntities.clear();
  m_dependencyOverflow = m_regionOverflow = 0;

  JsonObject stages;
  for (auto& entry : m_timings) {
    auto& timing = entry.second;
    if (!timing.samples)
      continue;
    stages.set(entry.first, timing.toJson());
    timing = {};
  }
  JsonObject counts;
  for (auto& entry : m_counts) {
    if (entry.second)
      counts.set(entry.first, entry.second);
    entry.second = 0;
  }
  JsonObject maxima;
  for (auto& entry : m_maxima) {
    if (entry.second)
      maxima.set(entry.first, entry.second);
    entry.second = 0;
  }
  JsonObject entityTypes;
  for (auto& entry : m_entityTimings) {
    auto& timing = entry.second;
    if (!timing.samples)
      continue;
    auto json = timing.toJson();
    json.set("luaTotalMilliseconds", timing.luaSeconds * 1000);
    json.set("loggingTotalMilliseconds", timing.loggingSeconds * 1000);
    entityTypes.set(entry.first, std::move(json));
    timing = {};
  }
  std::sort(m_slowEntities.begin(), m_slowEntities.end(), [](auto const& a, auto const& b) {
    return a.totalSeconds > b.totalSeconds;
  });
  JsonArray slowEntities;
  for (auto const& entity : m_slowEntities) {
    slowEntities.append(JsonObject{
      {"type", entity.type}, {"id", entity.id}, {"name", entity.name}, {"kind", entity.kind},
      {"position", jsonFromVec2F(entity.position)}, {"threadId", entity.threadId},
      {"totalMilliseconds", entity.totalSeconds * 1000},
      {"luaMilliseconds", entity.luaSeconds * 1000},
      {"loggingMilliseconds", entity.loggingSeconds * 1000}
    });
  }
  m_slowEntities.clear();
  Json oldestQueuedSector;
  if (m_oldestQueuedSector)
    oldestQueuedSector = JsonObject{{"sector", jsonFromVec2I(m_oldestQueuedSector->sector)},
        {"ageMilliseconds", m_oldestQueuedSector->seconds * 1000}};
  m_oldestQueuedSector.reset();
  std::sort(m_slowSectors.begin(), m_slowSectors.end(), [](auto const& a, auto const& b) {
    return a.seconds > b.seconds;
  });
  JsonArray slowSectors;
  for (auto const& sector : m_slowSectors)
    slowSectors.append(JsonObject{{"sector", jsonFromVec2I(sector.sector)},
        {"waitMilliseconds", sector.seconds * 1000}});
  m_slowSectors.clear();
  JsonArray tilePackets;
  for (auto const& packet : m_tilePackets)
    tilePackets.append(JsonObject{{"tileOrigin", jsonFromVec2I(packet.tileOrigin)},
        {"monotonicMilliseconds", packet.monotonicSeconds * 1000}});
  m_tilePackets.clear();
  auto baseTileOutcomesJson = [](List<BaseTileOutcome>& outcomes) {
    std::sort(outcomes.begin(), outcomes.end(), [](auto const& a, auto const& b) {
      return a.ageSeconds > b.ageSeconds;
    });
    JsonArray json;
    for (auto const& outcome : outcomes)
      json.append(JsonObject{{"sector", jsonFromVec2I(outcome.sector)},
          {"ageMilliseconds", outcome.ageSeconds * 1000},
          {"revisionDelta", outcome.revisionDelta}, {"reasons", outcome.reasons}});
    outcomes.clear();
    return json;
  };
  auto staleBaseTiles = baseTileOutcomesJson(m_staleBaseTiles);
  auto evictedBaseTiles = baseTileOutcomesJson(m_evictedBaseTiles);
  bool hasSlowMicroDungeonFactories = !m_slowMicroDungeonFactories.empty();
  std::sort(m_slowMicroDungeonFactories.begin(), m_slowMicroDungeonFactories.end(),
      [](auto const& a, auto const& b) { return a.seconds > b.seconds; });
  JsonArray slowMicroDungeonFactories;
  for (auto const& sample : m_slowMicroDungeonFactories) {
    slowMicroDungeonFactories.append(JsonObject{
      {"templateHash", sample.templateHash}, {"sector", jsonFromVec2I(sample.sector)},
      {"footprintTiles", sample.footprintTiles}, {"generated", sample.generated},
      {"milliseconds", sample.seconds * 1000},
      {"definitionMilliseconds", sample.definitionSeconds * 1000},
      {"anchorMilliseconds", sample.anchorSeconds * 1000},
      {"collisionMilliseconds", sample.collisionSeconds * 1000},
      {"canPlaceMilliseconds", sample.canPlaceSeconds * 1000},
      {"buildMilliseconds", sample.buildSeconds * 1000},
      {"placementAttempts", sample.placementAttempts},
      {"boundsRejected", sample.boundsRejected},
      {"collisionRejected", sample.collisionRejected},
      {"placementRejected", sample.placementRejected},
      {"anchorFound", sample.anchorFound}
    });
  }
  m_slowMicroDungeonFactories.clear();
  auto generationTasks = std::move(m_generationTaskEvents);
  m_generationTaskEvents = {};
  auto generationRequests = std::move(m_generationRequestEvents);
  m_generationRequestEvents = {};
  auto generationSelections = std::move(m_generationSelectionEvents);
  m_generationSelectionEvents = {};
  auto regionTurns = std::move(m_regionTurnEvents);
  m_regionTurnEvents = {};
  auto storyHops = std::move(m_storyHops);
  m_storyHops = {};
  static char const* const npcStageNames[] = {
    "update", "scriptDispatch", "movement", "status", "shared", "init", "uninit", "damage",
    "damagedOther", "shouldDestroy", "destroy", "interact", "receiveMessage", "callScript",
    "lua.update", "lua.invoke", "lua.init", "lua.uninit", "lua.message", "lua.threadMessage", "lua.eval",
    "pathfinding.init", "pathfinding.explore", "outgoing.callScriptedEntity", "outgoing.sendEntityMessage", "outgoing.interact"
  };
  static_assert(sizeof(npcStageNames) / sizeof(*npcStageNames) == static_cast<size_t>(NpcStage::Count), "NPC stage labels");
  JsonObject npcStages;
  for (size_t i = 0; i < m_npcTimings.size(); ++i) {
    auto& timing = m_npcTimings[i];
    if (!timing.samples)
      continue;
    auto json = timing.toJson();
    json.set("exclusiveMilliseconds", timing.exclusiveSeconds * 1000);
    json.set("loggingTotalMilliseconds", timing.loggingSeconds * 1000);
    npcStages.set(npcStageNames[i], std::move(json));
    timing = {};
  }
  if (m_npcDetails) {
    counts.set("npc.pathfinding.pendingExploreCalls", m_npcPathResults[0]);
    counts.set("npc.pathfinding.successfulExploreCalls", m_npcPathResults[1]);
    counts.set("npc.pathfinding.failedExploreCalls", m_npcPathResults[2]);
    counts.set("npc.populationTickSamples", m_npcPopulationSamples);
    counts.set("npc.updatedEntitiesTotal", m_npcUpdatedEntities);
    counts.set("npc.maximumUpdatedEntitiesPerTick", m_npcMaximumUpdatedEntities);
    m_npcPathResults = {};
    m_npcPopulationSamples = m_npcUpdatedEntities = m_npcMaximumUpdatedEntities = 0;
  }
  if (!stages.empty() || !counts.empty() || !maxima.empty() || !entityTypes.empty() || !slowEntities.empty()
      || !npcStages.empty() || !oldestQueuedSector.isNull() || !slowSectors.empty() || !tilePackets.empty()
      || !staleBaseTiles.empty() || !evictedBaseTiles.empty() || !generationTasks.empty() || !regionTurns.empty()
      || !storyHops.empty() || !generationRequests.empty() || !generationSelections.empty()
      || hasSlowMicroDungeonFactories) {
    Json report = JsonObject{
      {"component", m_component}, {"context", m_context}, {"instance", m_instance},
      {"sessionStartedMonotonicMilliseconds", m_sessionStart * 1000},
      {"windowMilliseconds", (now - m_windowStart) * 1000},
      {"slowOperationMilliseconds", m_slowSeconds * 1000}, {"final", final},
      {"stages", std::move(stages)}, {"counts", std::move(counts)}, {"maxima", std::move(maxima)},
      {"entityTypes", std::move(entityTypes)}, {"slowEntities", std::move(slowEntities)},
      {"oldestQueuedSector", std::move(oldestQueuedSector)}, {"slowSectors", std::move(slowSectors)},
      {"tilePackets", std::move(tilePackets)},
      {"staleBaseTiles", std::move(staleBaseTiles)}, {"evictedBaseTiles", std::move(evictedBaseTiles)},
      {"slowMicroDungeonFactories", std::move(slowMicroDungeonFactories)},
      {"generationTasks", std::move(generationTasks)},
      {"generationRequests", std::move(generationRequests)},
      {"generationSelections", std::move(generationSelections)},
      {"regionTurns", std::move(regionTurns)},
      {"storyHops", std::move(storyHops)},
      {"regionalDiagnostics", std::move(regionalDiagnostics)},
      {"npcDetails", m_npcDetails}, {"npcStages", std::move(npcStages)},
      {"percentileMethod", "histogram-upper-bound-8-buckets-per-octave-1us"}
    };
    String reportCategory = "prepare";
    try {
      auto object = report.toObject();
      auto makeCategory = [&](String const& category, JsonObject fields, bool hasData) {
        if (!hasData)
          return;
        fields.set("component", m_component);
        fields.set("context", m_context);
        fields.set("instance", m_instance);
        fields.set("category", category);
        fields.set("windowMilliseconds", object.get("windowMilliseconds"));
        fields.set("slowOperationMilliseconds", object.get("slowOperationMilliseconds"));
        fields.set("final", object.get("final"));
        reportCategory = category;
        try {
          writeReportFile(category, Json(std::move(fields)));
        } catch (BadVariantType const&) {
          // A malformed optional field must not make an otherwise useful
          // profiling window disappear. Persist a minimal, schema-valid
          // record, mark it for offline diagnosis, and continue with the
          // other categories. This is specifically recoverable from the
          // historical Windows `BadVariantType` storage-report failure.
          JsonObject fallback{
            {"component", m_component}, {"context", m_context}, {"instance", m_instance},
            {"category", category}, {"windowMilliseconds", object.get("windowMilliseconds")},
            {"slowOperationMilliseconds", object.get("slowOperationMilliseconds")},
            {"final", object.get("final")}, {"reportFallback", "badVariantType"}
          };
          try {
            writeReportFile(category, Json(std::move(fallback)));
            if (!m_reportWriteFailureCategories.contains(category)) {
              Logger::warn("[ExplorationProfile] recovered report category={} with minimal fallback after BadVariantType", category);
              m_reportWriteFailureCategories[category] = true;
            }
          } catch (...) {
            if (!m_reportWriteFailureCategories.contains(category)) {
              Logger::warn("[ExplorationProfile] report fallback failed component={} instance={} category={}",
                  m_component, m_instance, category);
              m_reportWriteFailureCategories[category] = true;
            }
          }
        } catch (std::exception const& exception) {
          if (!m_reportWriteFailureCategories.contains(category)) {
            Logger::warn("[ExplorationProfile] report file write failed component={} instance={} category={} failure=standardException detail={}",
                m_component, m_instance, category, exception.what());
            m_reportWriteFailureCategories[category] = true;
          }
        } catch (...) {
          if (!m_reportWriteFailureCategories.contains(category)) {
            Logger::warn("[ExplorationProfile] report file write failed component={} instance={} category={} failure=unknownException",
                m_component, m_instance, category);
            m_reportWriteFailureCategories[category] = true;
          }
        }
      };
      // Category extraction must tolerate an omitted optional report section.
      // In particular, a short profiling window can have storage counters
      // without timings or maxima.  Keep the category schema object-shaped
      // instead of iterating a null Json value and losing the whole report.
      auto selectByPrefix = [](JsonObject const& source, std::initializer_list<char const*> prefixes) {
        JsonObject selected;
        for (auto const& entry : source) {
          if (hasPrefix(entry.first, prefixes))
            selected.set(entry.first, entry.second);
        }
        return selected;
      };
      auto objectField = [&](char const* name) {
        if (auto value = object.ptr(name))
          if (value->isType(Json::Type::Object))
            return value->toObject();
        return JsonObject();
      };
      auto objectOrEmpty = [](Json const& value) {
        return value.isType(Json::Type::Object) ? value.toObject() : JsonObject();
      };
      auto arrayOrEmpty = [](Json const& value) {
        return value.isType(Json::Type::Array) ? value.toArray() : JsonArray();
      };
      auto selectEntity = [&](Json const& source, String const& type) {
        JsonObject selected;
        if (auto value = objectOrEmpty(source).ptr(type))
          selected.set(type, *value);
        return selected;
      };
      auto selectSlowEntities = [&](Json const& source, String const& type) {
        JsonArray selected;
        for (auto const& entity : arrayOrEmpty(source)) {
          if (entity.getString("type", "") == type)
            selected.append(entity);
        }
        return selected;
      };
      auto entityCategory = [&](String const& category, String const& type, char const* stagePrefix, char const* countPrefix) {
        JsonObject fields;
        auto stages = selectByPrefix(objectField("stages"), {stagePrefix});
        auto counts = selectByPrefix(objectField("counts"), {countPrefix});
        auto maxima = selectByPrefix(objectField("maxima"), {countPrefix});
        auto entityTypes = selectEntity(object.get("entityTypes"), type);
        auto slowEntities = selectSlowEntities(object.get("slowEntities"), type);
        Json npcStages;
        if (category == "npcs")
          npcStages = object.get("npcStages");
        bool hasData = !stages.empty() || !counts.empty() || !maxima.empty() || !entityTypes.empty() || !slowEntities.empty()
            || !objectOrEmpty(npcStages).empty();
        fields.set("stages", std::move(stages));
        fields.set("counts", std::move(counts));
        fields.set("maxima", std::move(maxima));
        fields.set("entityTypes", std::move(entityTypes));
        fields.set("slowEntities", std::move(slowEntities));
        if (!npcStages.isNull())
          fields.set("npcStages", std::move(npcStages));
        makeCategory(category, std::move(fields), hasData);
      };

      makeCategory("world", object, true);

      // Determine storage-category presence from the typed local objects,
      // before moving them into a JsonObject.  Calling toObject() on a
      // missing/optional Json field was the actual `BadVariantType` path on
      // short Windows storage windows; it occurred before makeCategory() and
      // therefore bypassed that writer's fallback.
      auto storageStages = selectByPrefix(objectField("stages"), {"storage."});
      auto storageCounts = selectByPrefix(objectField("counts"), {"storage."});
      auto storageMaxima = selectByPrefix(objectField("maxima"), {"storage."});
      bool hasStorage = !storageStages.empty() || !storageCounts.empty() || !storageMaxima.empty();
      JsonObject storage;
      storage.set("stages", std::move(storageStages));
      storage.set("counts", std::move(storageCounts));
      storage.set("maxima", std::move(storageMaxima));
      makeCategory("storage", std::move(storage), hasStorage);

      entityCategory("npcs", "npc", "npc.", "npc.");
      entityCategory("monsters", "monster", "entity.monster.", "entity.monster.");
      entityCategory("vehicles", "vehicle", "entity.vehicle.", "entity.vehicle.");
      entityCategory("stagehands", "stagehand", "entity.stagehand.", "entity.stagehand.");

      JsonObject sectors;
      sectors.set("counts", selectByPrefix(objectField("counts"), {"generation.", "storage.", "entity.sector", "entity.monster.sector", "entity.vehicle.sector", "entity.stagehand.sector", "entity.multiSector"}));
      sectors.set("oldestQueuedSector", object.get("oldestQueuedSector"));
      sectors.set("slowSectors", object.get("slowSectors"));
      sectors.set("tilePackets", object.get("tilePackets"));
      sectors.set("regionalDiagnostics", object.get("regionalDiagnostics"));
      sectors.set("regionTurns", object.get("regionTurns"));
      bool hasSectors = !objectOrEmpty(sectors.get("counts")).empty()
          || !sectors.get("oldestQueuedSector").isNull()
          || !arrayOrEmpty(sectors.get("slowSectors")).empty()
          || !arrayOrEmpty(sectors.get("tilePackets")).empty()
          || !arrayOrEmpty(sectors.get("regionTurns")).empty();
      makeCategory("sectors", std::move(sectors), hasSectors);

      JsonObject generation;
      generation.set("stages", selectByPrefix(objectField("stages"), {"generation."}));
      generation.set("counts", selectByPrefix(objectField("counts"), {"generation."}));
      generation.set("staleBaseTiles", object.get("staleBaseTiles"));
      generation.set("evictedBaseTiles", object.get("evictedBaseTiles"));
      generation.set("generationTasks", object.get("generationTasks"));
      generation.set("generationRequests", object.get("generationRequests"));
      generation.set("generationSelections", object.get("generationSelections"));
      generation.set("slowMicroDungeonFactories", object.get("slowMicroDungeonFactories"));
      bool hasGeneration = !objectOrEmpty(generation.get("stages")).empty() || !objectOrEmpty(generation.get("counts")).empty()
          || !arrayOrEmpty(generation.get("staleBaseTiles")).empty() || !arrayOrEmpty(generation.get("evictedBaseTiles")).empty()
          || !arrayOrEmpty(generation.get("generationTasks")).empty()
          || !arrayOrEmpty(generation.get("generationRequests")).empty()
          || !arrayOrEmpty(generation.get("generationSelections")).empty()
          || !arrayOrEmpty(generation.get("slowMicroDungeonFactories")).empty();
      makeCategory("generation", std::move(generation), hasGeneration);

      // Dungeon generation is authoritative and serial. Keep its fixed-label
      // phases isolated so profiling can distinguish planning, validation,
      // painting and entity publication without retaining asset/mod details.
      JsonObject dungeons;
      dungeons.set("stages", selectByPrefix(objectField("stages"), {"dungeon."}));
      dungeons.set("counts", selectByPrefix(objectField("counts"), {"dungeon."}));
      dungeons.set("maxima", selectByPrefix(objectField("maxima"), {"dungeon."}));
      bool hasDungeons = !objectOrEmpty(dungeons.get("stages")).empty()
          || !objectOrEmpty(dungeons.get("counts")).empty() || !objectOrEmpty(dungeons.get("maxima")).empty();
      makeCategory("dungeons", std::move(dungeons), hasDungeons);

      JsonObject wiring;
      wiring.set("stages", selectByPrefix(objectField("stages"), {"wiring."}));
      wiring.set("counts", selectByPrefix(objectField("counts"), {"wiring."}));
      wiring.set("maxima", selectByPrefix(objectField("maxima"), {"wiring."}));
      bool hasWiring = !objectOrEmpty(wiring.get("stages")).empty() || !objectOrEmpty(wiring.get("counts")).empty() || !objectOrEmpty(wiring.get("maxima")).empty();
      makeCategory("wiring", std::move(wiring), hasWiring);

      JsonObject liquids;
      liquids.set("stages", selectByPrefix(objectField("stages"), {"liquid."}));
      liquids.set("counts", selectByPrefix(objectField("counts"), {"liquid."}));
      liquids.set("maxima", selectByPrefix(objectField("maxima"), {"liquid."}));
      bool hasLiquids = !objectOrEmpty(liquids.get("stages")).empty() || !objectOrEmpty(liquids.get("counts")).empty() || !objectOrEmpty(liquids.get("maxima")).empty();
      makeCategory("liquids", std::move(liquids), hasLiquids);

      JsonObject scripts;
      scripts.set("stages", selectByPrefix(objectField("stages"), {"quest.", "universe.", "script."}));
      scripts.set("storyHops", object.get("storyHops"));
      if (m_component == "playerQuests" || m_component == "universeScripts")
        scripts.set("allStages", object.get("stages"));
      bool hasScripts = !objectOrEmpty(scripts.get("stages")).empty() || !arrayOrEmpty(scripts.get("storyHops")).empty()
          || (scripts.contains("allStages") && !objectOrEmpty(scripts.get("allStages")).empty());
      makeCategory("scripts", std::move(scripts), hasScripts);
    } catch (BadVariantType const&) {
      // Some transitional server profiles (notably Nowhere before a world id
      // is assigned) can still fail while a category is assembled, before the
      // per-category writer fallback. Preserve a minimal record here too.
      try {
        JsonObject fallback{
          {"component", m_component}, {"context", m_context}, {"instance", m_instance},
          {"category", reportCategory}, {"windowMilliseconds", (now - m_windowStart) * 1000},
          {"slowOperationMilliseconds", m_slowSeconds * 1000},
          {"final", final}, {"reportFallback", "badVariantType"}
        };
        writeReportFile(reportCategory, Json(std::move(fallback)));
        if (!m_reportWriteFailureLogged) {
          Logger::warn("[ExplorationProfile] recovered report category={} with outer BadVariantType fallback", reportCategory);
          m_reportWriteFailureLogged = true;
        }
      } catch (...) {
        if (!m_reportWriteFailureLogged) {
          Logger::warn("[ExplorationProfile] report fallback failed component={} instance={} category={}",
              m_component, m_instance, reportCategory);
          m_reportWriteFailureLogged = true;
        }
      }
    } catch (std::exception const& exception) {
      if (!m_reportWriteFailureLogged) {
        Logger::warn("[ExplorationProfile] report file write failed component={} instance={} category={} failure=standardException detail={}",
            m_component, m_instance, reportCategory, exception.what());
        m_reportWriteFailureLogged = true;
      }
    } catch (...) {
      if (!m_reportWriteFailureLogged) {
        Logger::warn("[ExplorationProfile] report file write failed component={} instance={} category={} failure=unknownException",
            m_component, m_instance, reportCategory);
        m_reportWriteFailureLogged = true;
      }
    }
  }
  // Exclude this collector's serialization/logging from the next window.
  m_windowStart = Time::monotonicTime();
}

}
