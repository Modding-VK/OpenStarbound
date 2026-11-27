#pragma once

#include "StarWorldGenerationHardwareSelection.hpp"

#include <cstdint>

namespace Star {

// First H5 / MOD-03 cut: conceptual execution profiles and explicit migration
// from existing threading performanceMode strings. Profiles never silently
// replace legacy/adaptive/aggressive/dynamic. Regional activation requires an
// explicit checkpoint/reload boundary before leaving the compatible path.

enum class WorldGenerationExecutionProfile : uint8_t {
  Balanced = 0,
  HighThroughput,
  DynamicBias,
  Manual,
  Count
};

enum class WorldGenerationFallbackKind : uint8_t {
  CpuComplete = 0,
  LegacyPreparation,
  CompatiblePath,
  CheckpointReloadRequired,
  Count
};

enum class WorldGenerationProfileMigration : uint8_t {
  UnchangedExistingMode = 0,
  ExplicitMapToConceptual,
  RejectedUnknown,
  Count
};

struct WorldGenerationProfileBinding {
  WorldGenerationPerformanceMode existingMode = WorldGenerationPerformanceMode::Unknown;
  WorldGenerationExecutionProfile conceptual = WorldGenerationExecutionProfile::Balanced;
  WorldGenerationFallbackKind fallback = WorldGenerationFallbackKind::CpuComplete;
  WorldGenerationProfileMigration migration = WorldGenerationProfileMigration::RejectedUnknown;
  bool allowsGpuSelection = false;
  bool allowsRegionalOwners = false;
  bool requiresCheckpointBeforeRegional = true;
};

inline WorldGenerationProfileBinding bindWorldGenerationExecutionProfile(
    WorldGenerationPerformanceMode mode) noexcept {
  WorldGenerationProfileBinding binding;
  binding.existingMode = mode;
  binding.fallback = WorldGenerationFallbackKind::CpuComplete;
  binding.requiresCheckpointBeforeRegional = true;
  binding.allowsRegionalOwners = false;

  switch (mode) {
    case WorldGenerationPerformanceMode::Legacy:
      binding.conceptual = WorldGenerationExecutionProfile::Balanced;
      binding.migration = WorldGenerationProfileMigration::UnchangedExistingMode;
      binding.fallback = WorldGenerationFallbackKind::LegacyPreparation;
      binding.allowsGpuSelection = false;
      break;
    case WorldGenerationPerformanceMode::Adaptive:
      binding.conceptual = WorldGenerationExecutionProfile::Balanced;
      binding.migration = WorldGenerationProfileMigration::ExplicitMapToConceptual;
      binding.fallback = WorldGenerationFallbackKind::CompatiblePath;
      binding.allowsGpuSelection = true;
      break;
    case WorldGenerationPerformanceMode::Aggressive:
      binding.conceptual = WorldGenerationExecutionProfile::HighThroughput;
      binding.migration = WorldGenerationProfileMigration::ExplicitMapToConceptual;
      binding.fallback = WorldGenerationFallbackKind::CompatiblePath;
      binding.allowsGpuSelection = true;
      break;
    case WorldGenerationPerformanceMode::Dynamic:
      binding.conceptual = WorldGenerationExecutionProfile::DynamicBias;
      binding.migration = WorldGenerationProfileMigration::ExplicitMapToConceptual;
      binding.fallback = WorldGenerationFallbackKind::CompatiblePath;
      binding.allowsGpuSelection = true;
      break;
    case WorldGenerationPerformanceMode::Unknown:
      binding.conceptual = WorldGenerationExecutionProfile::Manual;
      binding.migration = WorldGenerationProfileMigration::RejectedUnknown;
      binding.fallback = WorldGenerationFallbackKind::CpuComplete;
      binding.allowsGpuSelection = false;
      break;
  }
  return binding;
}

inline WorldGenerationProfileBinding bindWorldGenerationExecutionProfile(String const& performanceMode) {
  return bindWorldGenerationExecutionProfile(parseWorldGenerationPerformanceMode(performanceMode));
}

// Regional experimental/amplified modes stay opt-in and never activate from a
// performanceMode string alone.
inline bool worldGenerationRegionalActivationAllowed(
    WorldGenerationProfileBinding const& binding, bool checkpointBoundaryReady) noexcept {
  if (!binding.allowsRegionalOwners)
    return false;
  if (binding.requiresCheckpointBeforeRegional && !checkpointBoundaryReady)
    return false;
  return checkpointBoundaryReady;
}

inline WorldGenerationFallbackKind worldGenerationSelectFallback(
    WorldGenerationProfileBinding const& binding, bool gpuFailed, bool regionalPartial) noexcept {
  if (regionalPartial)
    return WorldGenerationFallbackKind::CheckpointReloadRequired;
  if (gpuFailed)
    return WorldGenerationFallbackKind::CpuComplete;
  return binding.fallback;
}

}
