#pragma once

#include "StarWorldGenerationDeliveryMetrics.hpp"
#include "StarWorldGenerationHardwareSelection.hpp"
#include "StarWorldGenerationScenarioMatrix.hpp"

#include <cstdint>

namespace Star {

// First H5 / VAL-05 cut: adoption gate checklist over known evidence. A green
// local test suite alone never flips adopt=true. Missing evidence stays false.

enum class WorldGenerationAdoptionGate : uint8_t {
  Invariants = 0,
  OutputComparison,
  EquivalentBenchmark,
  MemoryBounded,
  FailureRecovery,
  KnownLimitsDocumented,
  CpuFallbackComplete,
  Count
};

struct WorldGenerationAdoptionEvidence {
  bool invariantsPassed = false;
  bool outputComparisonPassed = false;
  bool equivalentBenchmarkPassed = false;
  bool memoryBounded = false;
  bool failureRecoveryCovered = false;
  bool knownLimitsDocumented = false;
  bool cpuFallbackComplete = false;
  bool gpuBackendAuthoritative = false;
  bool measuredDeliveryGain = false;
};

struct WorldGenerationAdoptionDecision {
  uint32_t satisfiedMask = 0;
  uint32_t requiredMask = 0;
  bool adopt = false;
  bool coverageComplete = false;

  bool has(WorldGenerationAdoptionGate gate) const noexcept {
    auto bit = static_cast<uint32_t>(gate);
    return (satisfiedMask & (1u << bit)) != 0;
  }
};

inline WorldGenerationAdoptionDecision evaluateWorldGenerationAdoption(
    WorldGenerationAdoptionEvidence const& evidence) noexcept {
  WorldGenerationAdoptionDecision decision;
  auto require = [&](WorldGenerationAdoptionGate gate, bool ok) {
    auto bit = 1u << static_cast<uint32_t>(gate);
    decision.requiredMask |= bit;
    if (ok)
      decision.satisfiedMask |= bit;
  };

  require(WorldGenerationAdoptionGate::Invariants, evidence.invariantsPassed);
  require(WorldGenerationAdoptionGate::OutputComparison, evidence.outputComparisonPassed);
  require(WorldGenerationAdoptionGate::EquivalentBenchmark, evidence.equivalentBenchmarkPassed);
  require(WorldGenerationAdoptionGate::MemoryBounded, evidence.memoryBounded);
  require(WorldGenerationAdoptionGate::FailureRecovery, evidence.failureRecoveryCovered);
  require(WorldGenerationAdoptionGate::KnownLimitsDocumented, evidence.knownLimitsDocumented);
  require(WorldGenerationAdoptionGate::CpuFallbackComplete, evidence.cpuFallbackComplete);

  decision.coverageComplete = decision.satisfiedMask == decision.requiredMask;
  // Authoritative GPU adoption additionally needs backend + measured gain, but
  // those are not substitutes for the required CPU-complete gates above.
  decision.adopt = decision.coverageComplete
      && (!evidence.gpuBackendAuthoritative || evidence.measuredDeliveryGain);
  return decision;
}

inline WorldGenerationAdoptionEvidence worldGenerationAdoptionFromPartialH5(
    WorldGenerationDeliveryMetricsSnapshot const& metrics,
    WorldGenerationHardwareCapabilities const& capabilities,
    bool reportDocumented) noexcept {
  WorldGenerationAdoptionEvidence evidence;
  evidence.knownLimitsDocumented = reportDocumented;
  evidence.cpuFallbackComplete = !capabilities.gpuComputeAvailable
      || metrics.has(WorldGenerationDeliveryMilestone::Resources);
  // Partial H5 instrumentation is never enough for the remaining gates.
  evidence.invariantsPassed = false;
  evidence.outputComparisonPassed = false;
  evidence.equivalentBenchmarkPassed = false;
  evidence.memoryBounded = metrics.has(WorldGenerationDeliveryMilestone::Resources)
      && metrics.resources.coverageComplete;
  evidence.failureRecoveryCovered = false;
  evidence.gpuBackendAuthoritative = false;
  evidence.measuredDeliveryGain = false;
  return evidence;
}

inline uint32_t worldGenerationRunnableCpuScenarioCount() noexcept {
  uint32_t count = 0;
  for (uint32_t i = 0; i < worldGenerationScenarioCount(); ++i) {
    auto scenario = static_cast<WorldGenerationValidationScenario>(i);
    if (worldGenerationScenarioRunnableWithoutGpu(scenario))
      ++count;
  }
  return count;
}

}
