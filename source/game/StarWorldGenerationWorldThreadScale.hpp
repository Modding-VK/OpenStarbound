#pragma once

#include "StarWorldGenerationComputeRuntime.hpp"
#include "StarWorldGenerationDeliveryMetrics.hpp"
#include "StarWorldGenerationHardwareSelection.hpp"
#include "StarWorldGenerationScaleMetrics.hpp"

#include <cstdint>

namespace Star {

// H5 / GPU-05 cut after INT tip 191cb6e4 (H4 WorldGenerationComputeRuntime pump):
// Observe WorldThread pump signals without owning GL, DenseUint32 kernels, or
// live-world publish. Rehearsal completions never count as measured gain.
// Real workload costs must be supplied by Desktop/H4 (gain_pipeline), not invented.

struct WorldGenerationWorldThreadPumpObservation {
  bool runtimeActive = false;
  bool rehearsal = false;
  size_t lastPumpCompletions = 0;
  // Optional calibrated costs from a non-rehearsal Desktop gain_pipeline run.
  bool deliveryCostsCalibrated = false;
  WorldGenerationDeliveryCost cpuCost;
  WorldGenerationDeliveryCost gpuCost;
};

struct WorldGenerationWorldThreadScaleSnapshot {
  static constexpr unsigned ModelVersion = 1;
  uint32_t pumpTicks = 0;
  uint32_t completionSum = 0;
  uint32_t rehearsalTicks = 0;
  uint32_t livePumpTicks = 0; // active && !rehearsal
  uint32_t calibratedSamples = 0;
  bool sawActiveRuntime = false;
  bool worldThreadSignalsReady = false;
  bool measuredGainFromLivePump = false;
};

inline bool worldGenerationWorldThreadPumpObservationValid(
    WorldGenerationWorldThreadPumpObservation const& observation) noexcept {
  if (observation.deliveryCostsCalibrated) {
    if (!observation.cpuCost.valid() || !observation.gpuCost.valid())
      return false;
  }
  return true;
}

// Map live ComputeRuntime pump getters into the H5 observation contract.
// Does not invent delivery costs; calibrated samples must be attached separately.
inline WorldGenerationWorldThreadPumpObservation observeWorldGenerationComputeRuntime(
    WorldGenerationComputeRuntime const& runtime) noexcept {
  WorldGenerationWorldThreadPumpObservation observation;
  observation.runtimeActive = runtime.active();
  observation.rehearsal = runtime.rehearsal();
  observation.lastPumpCompletions = runtime.lastPumpCompletions();
  return observation;
}

inline WorldGenerationWorldThreadPumpObservation observeWorldGenerationComputeRuntime(
    WorldGenerationComputeRuntime const& runtime,
    WorldGenerationDeliveryCost cpuCost,
    WorldGenerationDeliveryCost gpuCost) noexcept {
  auto observation = observeWorldGenerationComputeRuntime(runtime);
  observation.deliveryCostsCalibrated = true;
  observation.cpuCost = cpuCost;
  observation.gpuCost = gpuCost;
  return observation;
}

class WorldGenerationWorldThreadScaleRecorder {
public:
  WorldGenerationWorldThreadScaleSnapshot const& snapshot() const noexcept {
    return m_snapshot;
  }

  bool recordPump(WorldGenerationWorldThreadPumpObservation observation) noexcept {
    if (!worldGenerationWorldThreadPumpObservationValid(observation))
      return false;

    m_snapshot.pumpTicks += 1;
    m_snapshot.completionSum += static_cast<uint32_t>(observation.lastPumpCompletions);
    if (observation.runtimeActive)
      m_snapshot.sawActiveRuntime = true;
    if (observation.runtimeActive && observation.rehearsal)
      m_snapshot.rehearsalTicks += 1;
    if (observation.runtimeActive && !observation.rehearsal)
      m_snapshot.livePumpTicks += 1;

    // Cloud-safe rehearsal proves the pump path, not delivery gain.
    if (observation.deliveryCostsCalibrated && observation.runtimeActive
        && !observation.rehearsal) {
      m_snapshot.calibratedSamples += 1;
      m_lastLiveCosts = observation;
      m_hasLiveCosts = true;
      if (observation.gpuCost.totalMs() > 0
          && observation.gpuCost.totalMs() < observation.cpuCost.totalMs())
        m_snapshot.measuredGainFromLivePump = true;
    }

    m_snapshot.worldThreadSignalsReady = m_snapshot.sawActiveRuntime
        && m_snapshot.completionSum > 0;
    return true;
  }

  // Build selector inputs only from non-rehearsal calibrated pump samples.
  WorldGenerationScaleRoutingSample tryRoutingSample() const noexcept {
    WorldGenerationScaleRoutingSample sample;
    if (!m_hasLiveCosts || !m_snapshot.measuredGainFromLivePump)
      return sample;
    sample.capabilities.gpuComputeAvailable = true;
    sample.cpuCost = m_lastLiveCosts.cpuCost;
    sample.gpuCost = m_lastLiveCosts.gpuCost;
    sample.ready = sample.cpuCost.valid() && sample.gpuCost.valid();
    sample.measuredDeliveryGain = sample.ready
        && sample.gpuCost.totalMs() < sample.cpuCost.totalMs();
    return sample;
  }

  // Rehearsal-only path: claim probe capabilities for ScaleMetrics bridge tests
  // without marking measured gain.
  bool fillRehearsalCapabilities(WorldGenerationScaleMetricsRecorder& scale) const noexcept {
    if (!m_snapshot.worldThreadSignalsReady)
      return false;
    scale.claimCapabilities({
        /*gpuComputeAvailable=*/false,
        /*glContextCreated=*/false,
        /*worldgenGlComputeProbePass=*/false});
    return true;
  }

private:
  WorldGenerationWorldThreadScaleSnapshot m_snapshot;
  WorldGenerationWorldThreadPumpObservation m_lastLiveCosts;
  bool m_hasLiveCosts = false;
};

inline WorldGenerationHardwareSelection selectFromWorldThreadScale(
    WorldGenerationHardwareSelector& selector,
    WorldGenerationWorldThreadScaleRecorder const& recorder,
    WorldGenerationPerformanceMode mode,
    WorldGenerationHardwareSelectionLimits const& limits = {}) noexcept {
  auto sample = recorder.tryRoutingSample();
  if (!sample.ready) {
    WorldGenerationHardwareCapabilities cpuOnly;
    WorldGenerationDeliveryCost cpu{1, 0, 0, 0};
    WorldGenerationDeliveryCost gpu{1, 0, 0, 0};
    return selector.select(cpuOnly, mode, cpu, gpu, limits);
  }
  return selector.select(sample.capabilities, mode, sample.cpuCost, sample.gpuCost, limits);
}

}
