#pragma once

#include "StarWorldGenerationExecutionProfile.hpp"
#include "StarWorldGenerationHardwareSelection.hpp"

#include <cstdint>

namespace Star {

// First CPU-05 cut (complete policy belongs to H5): local governor advice from
// profile + observed demand/cost. Not a live WorkerPool resizer, ticket owner
// or publish authority. Existing performanceMode strings remain the config key.

enum class WorldGenerationGovernorAction : uint8_t {
  Hold = 0,
  GrowConcurrency,
  DrainWorkers,
  PreferCpuBackend,
  PreferGpuBackend,
  Count
};

enum class WorldGenerationGovernorDemand : uint8_t {
  None = 0,
  Visible,
  Speculative,
  Count
};

struct WorldGenerationGovernorObservation {
  WorldGenerationGovernorDemand demand = WorldGenerationGovernorDemand::None;
  double observedDeliveryMs = 0;
  double targetDeliveryMs = 0;
  size_t retainedResults = 0;
  size_t maxRetainedResults = 0;
  bool renderPressure = false;
  bool memoryPressure = false;
};

struct WorldGenerationGovernorAdvice {
  WorldGenerationGovernorAction action = WorldGenerationGovernorAction::Hold;
  WorldGenerationComputeBackend preferredBackend = WorldGenerationComputeBackend::Cpu;
  WorldGenerationExecutionProfile profile = WorldGenerationExecutionProfile::Balanced;
  bool allowsGrowth = false;
};

inline WorldGenerationGovernorAdvice adviseWorldGenerationResources(
    WorldGenerationProfileBinding const& binding,
    WorldGenerationGovernorObservation const& observation,
    WorldGenerationHardwareSelection const& hardware) noexcept {
  WorldGenerationGovernorAdvice advice;
  advice.profile = binding.conceptual;
  advice.preferredBackend = hardware.backend;
  advice.allowsGrowth = binding.conceptual == WorldGenerationExecutionProfile::HighThroughput
      || binding.conceptual == WorldGenerationExecutionProfile::DynamicBias;

  if (observation.memoryPressure || observation.renderPressure) {
    advice.action = WorldGenerationGovernorAction::DrainWorkers;
    advice.allowsGrowth = false;
    return advice;
  }

  if (observation.maxRetainedResults != 0
      && observation.retainedResults >= observation.maxRetainedResults) {
    advice.action = WorldGenerationGovernorAction::DrainWorkers;
    advice.allowsGrowth = false;
    return advice;
  }

  if (hardware.backend == WorldGenerationComputeBackend::Cpu
      && binding.allowsGpuSelection
      && hardware.reason == WorldGenerationHardwareSelectionReason::DeadlinePrefersCpu) {
    advice.action = WorldGenerationGovernorAction::PreferCpuBackend;
    return advice;
  }

  if (hardware.backend == WorldGenerationComputeBackend::Gpu) {
    advice.action = WorldGenerationGovernorAction::PreferGpuBackend;
    return advice;
  }

  bool demand = observation.demand == WorldGenerationGovernorDemand::Visible
      || observation.demand == WorldGenerationGovernorDemand::Speculative;
  bool slow = observation.targetDeliveryMs > 0
      && observation.observedDeliveryMs > observation.targetDeliveryMs;
  if (demand && slow && advice.allowsGrowth) {
    advice.action = WorldGenerationGovernorAction::GrowConcurrency;
    return advice;
  }

  advice.action = WorldGenerationGovernorAction::Hold;
  return advice;
}

}
