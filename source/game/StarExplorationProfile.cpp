#include "StarExplorationProfile.hpp"
#include "StarConfiguration.hpp"
#include "StarJsonExtra.hpp"
#include "StarLogging.hpp"
#include "StarMonster.hpp"
#include "StarNpc.hpp"
#include "StarRoot.hpp"
#include "StarTime.hpp"

#include <atomic>
#include <cmath>
#include <thread>

namespace Star {

thread_local ExplorationProfile::EntityScope* ExplorationProfile::s_currentEntityScope = nullptr;

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

ExplorationProfile::EntityLuaScope::EntityLuaScope()
  : m_entityScope(s_currentEntityScope), m_start(m_entityScope ? Time::monotonicTime() : 0) {}

ExplorationProfile::EntityLuaScope::~EntityLuaScope() {
  if (m_entityScope)
    m_entityScope->recordLua(Time::monotonicTime() - m_start);
}

void ExplorationProfile::setContext(String context) {
  m_context = std::move(context);
}

void ExplorationProfile::record(char const* stage, double seconds) {
  auto& timing = m_timings[stage];
  ++timing.samples;
  timing.slowSamples += seconds >= m_slowSeconds;
  timing.totalSeconds += seconds;
  timing.maximumSeconds = max(timing.maximumSeconds, seconds);
}

void ExplorationProfile::count(char const* counter, uint64_t amount) {
  m_counts[counter] += amount;
}

void ExplorationProfile::recordEntity(Entity const& entity, double totalSeconds, double luaSeconds, double loggingSeconds) {
  String type = EntityTypeNames.getRight(entity.entityType());
  auto& timing = m_entityTimings[type];
  ++timing.samples;
  timing.slowSamples += totalSeconds >= m_slowSeconds;
  timing.totalSeconds += totalSeconds;
  timing.maximumSeconds = max(timing.maximumSeconds, totalSeconds);
  timing.luaSeconds += luaSeconds;
  timing.loggingSeconds += loggingSeconds;

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

  JsonObject stages;
  for (auto& entry : m_timings) {
    auto& timing = entry.second;
    if (!timing.samples)
      continue;
    stages.set(entry.first, JsonObject{
      {"samples", timing.samples},
      {"slowSamples", timing.slowSamples},
      {"totalMilliseconds", timing.totalSeconds * 1000},
      {"meanMilliseconds", timing.totalSeconds * 1000 / timing.samples},
      {"maximumMilliseconds", timing.maximumSeconds * 1000}
    });
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
    entityTypes.set(entry.first, JsonObject{
      {"samples", timing.samples},
      {"slowSamples", timing.slowSamples},
      {"totalMilliseconds", timing.totalSeconds * 1000},
      {"meanMilliseconds", timing.totalSeconds * 1000 / timing.samples},
      {"maximumMilliseconds", timing.maximumSeconds * 1000},
      {"luaTotalMilliseconds", timing.luaSeconds * 1000},
      {"loggingTotalMilliseconds", timing.loggingSeconds * 1000}
    });
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
  if (!stages.empty() || !counts.empty() || !entityTypes.empty() || !slowEntities.empty()) {
    Json report = JsonObject{
      {"component", m_component}, {"context", m_context}, {"instance", m_instance},
      {"windowMilliseconds", (now - m_windowStart) * 1000},
      {"slowOperationMilliseconds", m_slowSeconds * 1000}, {"final", final},
      {"stages", std::move(stages)}, {"counts", std::move(counts)},
      {"entityTypes", std::move(entityTypes)}, {"slowEntities", std::move(slowEntities)}
    };
    Logger::info("[ExplorationProfile] {}", report.printJson(0, true));
  }
  // Exclude this collector's serialization/logging from the next window.
  m_windowStart = Time::monotonicTime();
}

}
