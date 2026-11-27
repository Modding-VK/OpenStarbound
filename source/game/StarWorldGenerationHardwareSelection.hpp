#pragma once

#include "StarString.hpp"

#include <cmath>

namespace Star {

// First H5 / GPU-05 cut: choose Cpu vs Gpu by full delivery cost, with a
// mandatory CPU path when compute is absent. This is not a driver, queue,
// fence, publish path or world authority. GPU selection remains advisory until
// H4 kernels are equivalent and an adoption gate records measured gain.

enum class WorldGenerationComputeBackend { Cpu, Gpu };

enum class WorldGenerationPerformanceMode {
  Legacy,
  Adaptive,
  Aggressive,
  Dynamic,
  Unknown
};

enum class WorldGenerationHardwareSelectionReason {
  CpuOnlyCapability,
  LegacyForcesCpu,
  LowerDeliveryCost,
  DeadlinePrefersCpu,
  HysteresisHold,
  InvalidCost
};

struct WorldGenerationDeliveryCost {
  double executionMs = 0;
  double transferMs = 0;
  double fenceMs = 0;
  double materializeMs = 0;

  double totalMs() const noexcept {
    return executionMs + transferMs + fenceMs + materializeMs;
  }

  bool valid() const noexcept {
    return std::isfinite(executionMs) && executionMs >= 0
        && std::isfinite(transferMs) && transferMs >= 0
        && std::isfinite(fenceMs) && fenceMs >= 0
        && std::isfinite(materializeMs) && materializeMs >= 0;
  }
};

struct WorldGenerationHardwareCapabilities {
  bool gpuComputeAvailable = false;
};

struct WorldGenerationHardwareSelectionLimits {
  // Other backend must beat the sticky choice by at least this margin.
  double switchMarginMs = 0;
  // When > 0, prefer CPU if its total meets the deadline and GPU does not, or
  // if CPU is strictly sooner under the same deadline pressure.
  double deadlineMs = 0;
};

struct WorldGenerationHardwareSelection {
  WorldGenerationComputeBackend backend = WorldGenerationComputeBackend::Cpu;
  WorldGenerationHardwareSelectionReason reason =
      WorldGenerationHardwareSelectionReason::CpuOnlyCapability;
  double selectedCostMs = 0;
};

inline WorldGenerationPerformanceMode parseWorldGenerationPerformanceMode(String const& mode) {
  if (mode == "legacy")
    return WorldGenerationPerformanceMode::Legacy;
  if (mode == "adaptive")
    return WorldGenerationPerformanceMode::Adaptive;
  if (mode == "aggressive")
    return WorldGenerationPerformanceMode::Aggressive;
  if (mode == "dynamic")
    return WorldGenerationPerformanceMode::Dynamic;
  return WorldGenerationPerformanceMode::Unknown;
}

// Existing threading performanceMode strings are not renamed by H5 profiles.
// Legacy keeps the CPU-only path; other known modes may use cost routing when
// GPU compute is present. Unknown modes stay on CPU.
inline bool worldGenerationPerformanceModeAllowsGpu(
    WorldGenerationPerformanceMode mode) noexcept {
  return mode == WorldGenerationPerformanceMode::Adaptive
      || mode == WorldGenerationPerformanceMode::Aggressive
      || mode == WorldGenerationPerformanceMode::Dynamic;
}

// Caller-local sticky selector. Not a reservation, scheduler ticket or publish.
class WorldGenerationHardwareSelector {
public:
  WorldGenerationHardwareSelector() = default;

  explicit WorldGenerationHardwareSelector(WorldGenerationComputeBackend initial) noexcept
    : m_current(initial) {}

  WorldGenerationComputeBackend current() const noexcept { return m_current; }

  WorldGenerationHardwareSelection select(
      WorldGenerationHardwareCapabilities const& capabilities,
      WorldGenerationPerformanceMode mode,
      WorldGenerationDeliveryCost const& cpuCost,
      WorldGenerationDeliveryCost const& gpuCost,
      WorldGenerationHardwareSelectionLimits const& limits = {}) noexcept {
    if (!cpuCost.valid() || !gpuCost.valid()) {
      return holdOrCpu(WorldGenerationHardwareSelectionReason::InvalidCost, cpuCost.valid()
          ? cpuCost.totalMs()
          : 0);
    }

    double const cpuTotal = cpuCost.totalMs();
    if (!capabilities.gpuComputeAvailable) {
      return choose(WorldGenerationComputeBackend::Cpu,
          WorldGenerationHardwareSelectionReason::CpuOnlyCapability, cpuTotal);
    }

    if (mode == WorldGenerationPerformanceMode::Legacy
        || mode == WorldGenerationPerformanceMode::Unknown
        || !worldGenerationPerformanceModeAllowsGpu(mode)) {
      return choose(WorldGenerationComputeBackend::Cpu,
          WorldGenerationHardwareSelectionReason::LegacyForcesCpu, cpuTotal);
    }

    double const gpuTotal = gpuCost.totalMs();
    double const margin = limits.switchMarginMs > 0 ? limits.switchMarginMs : 0;

    if (limits.deadlineMs > 0) {
      bool const cpuMeets = cpuTotal <= limits.deadlineMs;
      bool const gpuMeets = gpuTotal <= limits.deadlineMs;
      if (cpuMeets && !gpuMeets) {
        return choose(WorldGenerationComputeBackend::Cpu,
            WorldGenerationHardwareSelectionReason::DeadlinePrefersCpu, cpuTotal);
      }
      if (cpuMeets && gpuMeets && cpuTotal < gpuTotal) {
        return choose(WorldGenerationComputeBackend::Cpu,
            WorldGenerationHardwareSelectionReason::DeadlinePrefersCpu, cpuTotal);
      }
    }

    WorldGenerationComputeBackend preferred = cpuTotal <= gpuTotal
        ? WorldGenerationComputeBackend::Cpu
        : WorldGenerationComputeBackend::Gpu;
    double preferredCost = preferred == WorldGenerationComputeBackend::Cpu ? cpuTotal : gpuTotal;
    double currentCost = m_current == WorldGenerationComputeBackend::Cpu ? cpuTotal : gpuTotal;
    double alternateCost = m_current == WorldGenerationComputeBackend::Cpu ? gpuTotal : cpuTotal;

    if (preferred == m_current) {
      return choose(m_current, WorldGenerationHardwareSelectionReason::LowerDeliveryCost, preferredCost);
    }

    if (alternateCost + margin < currentCost) {
      WorldGenerationComputeBackend next = m_current == WorldGenerationComputeBackend::Cpu
          ? WorldGenerationComputeBackend::Gpu
          : WorldGenerationComputeBackend::Cpu;
      return choose(next, WorldGenerationHardwareSelectionReason::LowerDeliveryCost, alternateCost);
    }

    return choose(m_current, WorldGenerationHardwareSelectionReason::HysteresisHold, currentCost);
  }

private:
  WorldGenerationHardwareSelection choose(WorldGenerationComputeBackend backend,
      WorldGenerationHardwareSelectionReason reason, double costMs) noexcept {
    m_current = backend;
    return {backend, reason, costMs};
  }

  WorldGenerationHardwareSelection holdOrCpu(WorldGenerationHardwareSelectionReason reason,
      double costMs) noexcept {
    return {m_current, reason, costMs};
  }

  WorldGenerationComputeBackend m_current = WorldGenerationComputeBackend::Cpu;
};

}
