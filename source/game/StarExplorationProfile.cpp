#include "StarExplorationProfile.hpp"
#include "StarConfiguration.hpp"
#include "StarLogging.hpp"
#include "StarRoot.hpp"
#include "StarTime.hpp"

#include <atomic>
#include <cmath>

namespace Star {

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
  if (!stages.empty() || !counts.empty()) {
    Json report = JsonObject{
      {"component", m_component}, {"context", m_context}, {"instance", m_instance},
      {"windowMilliseconds", (now - m_windowStart) * 1000},
      {"slowOperationMilliseconds", m_slowSeconds * 1000}, {"final", final},
      {"stages", std::move(stages)}, {"counts", std::move(counts)}
    };
    Logger::info("[ExplorationProfile] {}", report.printJson(0, true));
  }
  // Exclude this collector's serialization/logging from the next window.
  m_windowStart = Time::monotonicTime();
}

}
