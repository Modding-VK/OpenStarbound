#pragma once

#include "StarWorldGenerationDeliveryMetrics.hpp"
#include "StarWorldGenerationHardwareSelection.hpp"

#include <cstdint>

namespace Star {

// Second H5 / VAL-03+GPU-05 cut: aggregate H4-shaped shadow observations into
// routing samples for the hardware selector. Caller supplies timings and
// compare verdicts from a real probe (Desktop RTX) or synthetic fixtures.
// This header does not own GL context, kernels, publish authority or Tracy.

enum class WorldGenerationShadowCompareVerdict : uint8_t {
  Equal = 0,
  Diverged,
  Unavailable,
  Count
};

enum class WorldGenerationShadowKernelFamily : uint8_t {
  DenseUint32Xor = 0,
  DenseUint32Add,
  DenseUint32HalfSignMask,
  DenseUint32HalfIsZero,
  DenseUint32HalfIsNonzero,
  Count
};

struct WorldGenerationShadowBatchObservation {
  WorldGenerationShadowKernelFamily kernel = WorldGenerationShadowKernelFamily::DenseUint32Xor;
  WorldGenerationShadowCompareVerdict verdict = WorldGenerationShadowCompareVerdict::Unavailable;
  uint32_t compared = 0;
  uint32_t divergences = 0;
  double cpuExecutionMs = 0;
  double gpuExecutionMs = 0;
  double transferMs = 0;
  double fenceMs = 0;
  double materializeMs = 0;
  bool anyPendingOpenKernels = false;
};

struct WorldGenerationScaleProbeCapabilities {
  bool gpuComputeAvailable = false;
  bool glContextCreated = false;
  // True when the Desktop/H4 probe claimed OpenGL compute (opt-in build).
  bool worldgenGlComputeProbePass = false;
};

struct WorldGenerationScaleMetricsSnapshot {
  static constexpr unsigned ModelVersion = 1;
  uint32_t batchesRecorded = 0;
  uint32_t equalBatches = 0;
  uint32_t divergedBatches = 0;
  uint32_t unavailableBatches = 0;
  uint32_t pendingOpenKernelHits = 0;
  uint32_t kernelMask = 0;
  double cpuTotalMsSum = 0;
  double gpuTotalMsSum = 0;
  bool capabilitiesClaimed = false;
  WorldGenerationScaleProbeCapabilities capabilities;

  bool hasKernel(WorldGenerationShadowKernelFamily kernel) const noexcept {
    auto bit = static_cast<uint32_t>(kernel);
    return bit < static_cast<uint32_t>(WorldGenerationShadowKernelFamily::Count)
        && (kernelMask & (1u << bit)) != 0;
  }

  bool coversH4TipHalfPredicates() const noexcept {
    return hasKernel(WorldGenerationShadowKernelFamily::DenseUint32HalfSignMask)
        && hasKernel(WorldGenerationShadowKernelFamily::DenseUint32HalfIsZero)
        && hasKernel(WorldGenerationShadowKernelFamily::DenseUint32HalfIsNonzero);
  }

  bool measuredDeliveryGain() const noexcept {
    return equalBatches > 0 && cpuTotalMsSum > 0 && gpuTotalMsSum > 0
        && gpuTotalMsSum < cpuTotalMsSum;
  }
};

struct WorldGenerationScaleRoutingSample {
  WorldGenerationHardwareCapabilities capabilities;
  WorldGenerationDeliveryCost cpuCost;
  WorldGenerationDeliveryCost gpuCost;
  bool ready = false;
  bool measuredDeliveryGain = false;
};

inline bool worldGenerationShadowObservationValid(
    WorldGenerationShadowBatchObservation const& observation) noexcept {
  if (!(observation.cpuExecutionMs >= 0) || !(observation.gpuExecutionMs >= 0)
      || !(observation.transferMs >= 0) || !(observation.fenceMs >= 0)
      || !(observation.materializeMs >= 0))
    return false;
  if (observation.divergences > observation.compared)
    return false;
  if (observation.verdict == WorldGenerationShadowCompareVerdict::Equal
      && observation.divergences != 0)
    return false;
  if (observation.verdict == WorldGenerationShadowCompareVerdict::Diverged
      && observation.divergences == 0)
    return false;
  return true;
}

inline WorldGenerationDeliveryCost worldGenerationCostFromShadowObservation(
    WorldGenerationShadowBatchObservation const& observation,
    WorldGenerationHardwareRoutingBackend backend) noexcept {
  WorldGenerationDeliveryCost cost;
  if (backend == WorldGenerationHardwareRoutingBackend::Cpu) {
    cost.executionMs = observation.cpuExecutionMs;
    cost.transferMs = 0;
    cost.fenceMs = 0;
    cost.materializeMs = observation.materializeMs;
  } else {
    cost.executionMs = observation.gpuExecutionMs;
    cost.transferMs = observation.transferMs;
    cost.fenceMs = observation.fenceMs;
    cost.materializeMs = observation.materializeMs;
  }
  return cost;
}

// Caller-local aggregator. Not a telemetry bus or H4 service client.
class WorldGenerationScaleMetricsRecorder {
public:
  WorldGenerationScaleMetricsSnapshot const& snapshot() const noexcept { return m_snapshot; }

  void claimCapabilities(WorldGenerationScaleProbeCapabilities capabilities) noexcept {
    m_snapshot.capabilities = capabilities;
    m_snapshot.capabilitiesClaimed = true;
  }

  bool recordShadowBatch(WorldGenerationShadowBatchObservation observation) noexcept {
    if (!worldGenerationShadowObservationValid(observation))
      return false;

    auto bit = static_cast<uint32_t>(observation.kernel);
    if (bit >= static_cast<uint32_t>(WorldGenerationShadowKernelFamily::Count))
      return false;

    m_snapshot.batchesRecorded += 1;
    m_snapshot.kernelMask |= (1u << bit);
    if (observation.anyPendingOpenKernels)
      m_snapshot.pendingOpenKernelHits += 1;

    switch (observation.verdict) {
      case WorldGenerationShadowCompareVerdict::Equal:
        m_snapshot.equalBatches += 1;
        m_snapshot.cpuTotalMsSum += worldGenerationCostFromShadowObservation(
            observation, WorldGenerationHardwareRoutingBackend::Cpu).totalMs();
        m_snapshot.gpuTotalMsSum += worldGenerationCostFromShadowObservation(
            observation, WorldGenerationHardwareRoutingBackend::Gpu).totalMs();
        break;
      case WorldGenerationShadowCompareVerdict::Diverged:
        m_snapshot.divergedBatches += 1;
        break;
      case WorldGenerationShadowCompareVerdict::Unavailable:
        m_snapshot.unavailableBatches += 1;
        break;
      case WorldGenerationShadowCompareVerdict::Count:
        return false;
    }
    m_lastEqual = observation;
    m_hasLastEqual = observation.verdict == WorldGenerationShadowCompareVerdict::Equal;
    return true;
  }

  // Build selector inputs from claimed probe capabilities + last equal batch.
  // Never fabricates GPU availability or gain from diverged/unavailable rows.
  WorldGenerationScaleRoutingSample tryRoutingSample() const noexcept {
    WorldGenerationScaleRoutingSample sample;
    if (!m_snapshot.capabilitiesClaimed || !m_hasLastEqual)
      return sample;
    if (m_snapshot.divergedBatches != 0)
      return sample;

    sample.capabilities.gpuComputeAvailable =
        m_snapshot.capabilities.gpuComputeAvailable
        && m_snapshot.capabilities.worldgenGlComputeProbePass
        && m_snapshot.capabilities.glContextCreated;
    sample.cpuCost = worldGenerationCostFromShadowObservation(
        m_lastEqual, WorldGenerationHardwareRoutingBackend::Cpu);
    sample.gpuCost = worldGenerationCostFromShadowObservation(
        m_lastEqual, WorldGenerationHardwareRoutingBackend::Gpu);
    if (!sample.cpuCost.valid() || !sample.gpuCost.valid())
      return sample;

    sample.ready = true;
    sample.measuredDeliveryGain = m_snapshot.measuredDeliveryGain();
    return sample;
  }

  // Optional VAL-03 bridge: stamp preparation/resources from an equal sample
  // so tryObservedDeliveryCost can run without inventing transfer timing.
  bool fillDeliveryRecorder(WorldGenerationDeliveryMetricsRecorder& recorder) const noexcept {
    auto routing = tryRoutingSample();
    if (!routing.ready)
      return false;

    WorldGenerationPreparationMetric prep;
    prep.reservationId = 1;
    prep.startMs = 0;
    prep.endMs = routing.gpuCost.executionMs;
    prep.backend = WorldGenerationHardwareRoutingBackend::Gpu;
    prep.bytes = 0;
    prep.inputRevision = 1;
    recorder.recordPreparation(prep);

    WorldGenerationResourceMetric resources;
    resources.usefulCpuMs = routing.cpuCost.executionMs;
    resources.gpuWaitMs = routing.gpuCost.fenceMs;
    resources.transferBytes = 0;
    resources.transferMs = routing.gpuCost.transferMs;
    resources.transferTimingCalibrated = true;
    resources.materializeMs = routing.gpuCost.materializeMs;
    resources.materializeTimingCalibrated = true;
    resources.coverageComplete = true;
    recorder.recordResources(resources);
    return true;
  }

private:
  WorldGenerationScaleMetricsSnapshot m_snapshot;
  WorldGenerationShadowBatchObservation m_lastEqual;
  bool m_hasLastEqual = false;
};

inline WorldGenerationHardwareSelection selectFromScaleMetrics(
    WorldGenerationHardwareSelector& selector,
    WorldGenerationScaleMetricsRecorder const& recorder,
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
