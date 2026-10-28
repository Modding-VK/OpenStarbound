#include "StarExplorationProfile.hpp"
#include "StarWorldGenerationTasks.hpp"
#include "StarConfiguration.hpp"
#include "StarJsonExtra.hpp"
#include "StarLogging.hpp"
#include "StarMonster.hpp"
#include "StarNpc.hpp"
#include "StarRoot.hpp"
#include "StarTime.hpp"
#include "StarWorldServer.hpp"

#include <atomic>
#include <cmath>
#include <thread>

namespace Star {

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
  profile->m_windowStart = Time::monotonicTime();
  Logger::info("[ExplorationProfile] enabled component={} instance={} context={}",
      profile->m_component, profile->m_instance, profile->m_context);
  return profile;
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
  m_context = std::move(context);
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

void ExplorationProfile::recordEntity(Entity const& entity, double totalSeconds, double luaSeconds, double loggingSeconds) {
  String type = EntityTypeNames.getRight(entity.entityType());
  auto& timing = m_entityTimings[type];
  timing.add(totalSeconds, m_slowSeconds);
  timing.luaSeconds += luaSeconds;
  timing.loggingSeconds += loggingSeconds;

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

  String name = entity.name();
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
  auto generationTasks = std::move(m_generationTaskEvents);
  m_generationTaskEvents = {};
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
  if (!stages.empty() || !counts.empty() || !entityTypes.empty() || !slowEntities.empty()
      || !npcStages.empty() || !oldestQueuedSector.isNull() || !slowSectors.empty() || !tilePackets.empty()
      || !staleBaseTiles.empty() || !evictedBaseTiles.empty() || !generationTasks.empty()) {
    Json report = JsonObject{
      {"component", m_component}, {"context", m_context}, {"instance", m_instance},
      {"windowMilliseconds", (now - m_windowStart) * 1000},
      {"slowOperationMilliseconds", m_slowSeconds * 1000}, {"final", final},
      {"stages", std::move(stages)}, {"counts", std::move(counts)},
      {"entityTypes", std::move(entityTypes)}, {"slowEntities", std::move(slowEntities)},
      {"oldestQueuedSector", std::move(oldestQueuedSector)}, {"slowSectors", std::move(slowSectors)},
      {"tilePackets", std::move(tilePackets)},
      {"staleBaseTiles", std::move(staleBaseTiles)}, {"evictedBaseTiles", std::move(evictedBaseTiles)},
      {"generationTasks", std::move(generationTasks)},
      {"regionalDiagnostics", std::move(regionalDiagnostics)},
      {"npcDetails", m_npcDetails}, {"npcStages", std::move(npcStages)},
      {"percentileMethod", "histogram-upper-bound-8-buckets-per-octave-1us"}
    };
    Logger::info("[ExplorationProfile] {}", report.printJson(0, true));
  }
  // Exclude this collector's serialization/logging from the next window.
  m_windowStart = Time::monotonicTime();
}

}
