#pragma once

#include "StarWorldGenerationHardwareSelection.hpp"
#include "StarWorldGenerationResourceGovernor.hpp"
#include "StarWorldGenerationScenarioMatrix.hpp"
#include "StarWorldGenerationScaleMetrics.hpp"

#include <cstdint>

namespace Star {

// Second H5 / VAL-04 cut: record scenario campaign outcomes without running
// streaming worlds. Callers supply results from Desktop/fixtures. Skips must
// name the missing capability — never count a skip as a pass.

enum class WorldGenerationScenarioOutcome : uint8_t {
  NotRun = 0,
  Passed,
  Failed,
  SkippedMissingGpu,
  SkippedMissingAssets,
  SkippedMissingMultiplayer,
  Count
};

struct WorldGenerationScenarioCampaignSnapshot {
  static constexpr unsigned ModelVersion = 1;
  uint32_t attemptedMask = 0;
  uint32_t passedMask = 0;
  uint32_t failedMask = 0;
  uint32_t skippedMask = 0;

  bool attempted(WorldGenerationValidationScenario scenario) const noexcept {
    auto bit = static_cast<uint32_t>(scenario);
    return bit < worldGenerationScenarioCount() && (attemptedMask & (1u << bit)) != 0;
  }

  bool passed(WorldGenerationValidationScenario scenario) const noexcept {
    auto bit = static_cast<uint32_t>(scenario);
    return bit < worldGenerationScenarioCount() && (passedMask & (1u << bit)) != 0;
  }

  uint32_t passedCount() const noexcept {
    uint32_t count = 0;
    for (uint32_t bit = 0; bit < worldGenerationScenarioCount(); ++bit)
      count += (passedMask & (1u << bit)) ? 1u : 0u;
    return count;
  }

  uint32_t failedCount() const noexcept {
    uint32_t count = 0;
    for (uint32_t bit = 0; bit < worldGenerationScenarioCount(); ++bit)
      count += (failedMask & (1u << bit)) ? 1u : 0u;
    return count;
  }

  // CPU-only matrix covered when every scenario that does not require GPU has
  // been attempted and none of those attempts failed.
  bool cpuOnlyMatrixCovered() const noexcept {
    for (uint32_t i = 0; i < worldGenerationScenarioCount(); ++i) {
      auto scenario = static_cast<WorldGenerationValidationScenario>(i);
      auto entry = worldGenerationScenarioEntry(scenario);
      if (entry.requiresGpu)
        continue;
      if ((attemptedMask & (1u << i)) == 0)
        return false;
      if ((failedMask & (1u << i)) != 0)
        return false;
    }
    return true;
  }
};

inline bool worldGenerationScenarioOutcomeValid(
    WorldGenerationValidationScenario scenario,
    WorldGenerationScenarioOutcome outcome,
    WorldGenerationHardwareCapabilities const& capabilities) noexcept {
  auto entry = worldGenerationScenarioEntry(scenario);
  switch (outcome) {
    case WorldGenerationScenarioOutcome::Passed:
    case WorldGenerationScenarioOutcome::Failed:
      if (entry.requiresGpu && !capabilities.gpuComputeAvailable)
        return false;
      return true;
    case WorldGenerationScenarioOutcome::SkippedMissingGpu:
      return entry.requiresGpu;
    case WorldGenerationScenarioOutcome::SkippedMissingAssets:
      return entry.requiresAssets;
    case WorldGenerationScenarioOutcome::SkippedMissingMultiplayer:
      return entry.requiresMultiplayer;
    case WorldGenerationScenarioOutcome::NotRun:
    case WorldGenerationScenarioOutcome::Count:
      return false;
  }
  return false;
}

class WorldGenerationScenarioCampaignRecorder {
public:
  WorldGenerationScenarioCampaignSnapshot const& snapshot() const noexcept {
    return m_snapshot;
  }

  bool record(WorldGenerationValidationScenario scenario,
      WorldGenerationScenarioOutcome outcome,
      WorldGenerationHardwareCapabilities const& capabilities) noexcept {
    auto bit = static_cast<uint32_t>(scenario);
    if (bit >= worldGenerationScenarioCount())
      return false;
    if (!worldGenerationScenarioOutcomeValid(scenario, outcome, capabilities))
      return false;

    m_snapshot.attemptedMask |= (1u << bit);
    m_snapshot.passedMask &= ~(1u << bit);
    m_snapshot.failedMask &= ~(1u << bit);
    m_snapshot.skippedMask &= ~(1u << bit);

    switch (outcome) {
      case WorldGenerationScenarioOutcome::Passed:
        m_snapshot.passedMask |= (1u << bit);
        break;
      case WorldGenerationScenarioOutcome::Failed:
        m_snapshot.failedMask |= (1u << bit);
        break;
      case WorldGenerationScenarioOutcome::SkippedMissingGpu:
      case WorldGenerationScenarioOutcome::SkippedMissingAssets:
      case WorldGenerationScenarioOutcome::SkippedMissingMultiplayer:
        m_snapshot.skippedMask |= (1u << bit);
        break;
      case WorldGenerationScenarioOutcome::NotRun:
      case WorldGenerationScenarioOutcome::Count:
        return false;
    }
    return true;
  }

  // Apply ScaleMetrics health to GPU-focused scenarios when probe capabilities
  // are known. Does not invent streaming results for asset/multiplayer rows.
  bool recordGpuScenariosFromScale(
      WorldGenerationScaleMetricsSnapshot const& scale) noexcept {
    if (!scale.capabilitiesClaimed)
      return false;

    WorldGenerationHardwareCapabilities caps;
    caps.gpuComputeAvailable = scale.capabilities.gpuComputeAvailable
        && scale.capabilities.glContextCreated
        && scale.capabilities.worldgenGlComputeProbePass;

    if (!caps.gpuComputeAvailable) {
      bool ok = record(WorldGenerationValidationScenario::MemoryPressureGpuKinds,
          WorldGenerationScenarioOutcome::SkippedMissingGpu, caps);
      ok = record(WorldGenerationValidationScenario::GpuAbsentSlowLost,
              WorldGenerationScenarioOutcome::SkippedMissingGpu, caps)
          && ok;
      return ok;
    }

    bool healthy = scale.equalBatches > 0 && scale.divergedBatches == 0
        && scale.coversH4TipHalfPredicates();
    auto outcome = healthy ? WorldGenerationScenarioOutcome::Passed
                           : WorldGenerationScenarioOutcome::Failed;
    bool ok = record(WorldGenerationValidationScenario::MemoryPressureGpuKinds,
        outcome, caps);
    ok = record(WorldGenerationValidationScenario::GpuAbsentSlowLost, outcome, caps) && ok;
    return ok;
  }

private:
  WorldGenerationScenarioCampaignSnapshot m_snapshot;
};

// CPU-05 bridge: prefer GPU only when scale routing is ready with measured gain.
inline WorldGenerationGovernorAdvice adviseWorldGenerationResourcesFromScale(
    WorldGenerationProfileBinding const& binding,
    WorldGenerationGovernorObservation const& observation,
    WorldGenerationHardwareSelector& selector,
    WorldGenerationScaleMetricsRecorder const& scale,
    WorldGenerationPerformanceMode mode,
    WorldGenerationHardwareSelectionLimits const& limits = {}) noexcept {
  auto hardware = selectFromScaleMetrics(selector, scale, mode, limits);
  auto advice = adviseWorldGenerationResources(binding, observation, hardware);
  auto routing = scale.tryRoutingSample();
  if (routing.ready && routing.measuredDeliveryGain
      && hardware.backend == WorldGenerationComputeBackend::Gpu
      && advice.action == WorldGenerationGovernorAction::Hold
      && !observation.memoryPressure && !observation.renderPressure) {
    advice.action = WorldGenerationGovernorAction::PreferGpuBackend;
    advice.preferredBackend = WorldGenerationComputeBackend::Gpu;
  }
  return advice;
}

}
