#pragma once

#include <cstdint>

namespace Star {

// First H5 / VAL-04 cut: typed scenario matrix for scale campaigns. Entries
// name the failure/gain sought; they do not run streaming or claim a benchmark.

enum class WorldGenerationValidationScenario : uint8_t {
  OpenSkyTerrain = 0,
  TmxColdAndWarm,
  SavedEditedWorld,
  DirectionChangeTeleport,
  HorizontalVerticalEdge,
  WorkersAndWorlds,
  MemoryPressureGpuKinds,
  GpuAbsentSlowLost,
  TwoPlayersNearFar,
  MantiziGlitchFuBase,
  UnloadSyncCrashRestart,
  Count
};

enum class WorldGenerationScenarioFocus : uint8_t {
  Throughput = 0,
  CacheVsMaterialize,
  NoSpuriousRegen,
  CancelPrefetchWaste,
  WrapPartialPriority,
  ScalabilityBudget,
  MemoryAccounting,
  CpuFallback,
  FairnessTransfer,
  StructureIntegrity,
  RecoveryCheckpoint,
  Count
};

struct WorldGenerationScenarioEntry {
  WorldGenerationValidationScenario scenario;
  WorldGenerationScenarioFocus focus;
  bool requiresGpu = false;
  bool requiresMultiplayer = false;
  bool requiresAssets = false;
};

inline WorldGenerationScenarioEntry worldGenerationScenarioEntry(
    WorldGenerationValidationScenario scenario) noexcept {
  switch (scenario) {
    case WorldGenerationValidationScenario::OpenSkyTerrain:
      return {scenario, WorldGenerationScenarioFocus::Throughput, false, false, false};
    case WorldGenerationValidationScenario::TmxColdAndWarm:
      return {scenario, WorldGenerationScenarioFocus::CacheVsMaterialize, false, false, true};
    case WorldGenerationValidationScenario::SavedEditedWorld:
      return {scenario, WorldGenerationScenarioFocus::NoSpuriousRegen, false, false, true};
    case WorldGenerationValidationScenario::DirectionChangeTeleport:
      return {scenario, WorldGenerationScenarioFocus::CancelPrefetchWaste, false, false, false};
    case WorldGenerationValidationScenario::HorizontalVerticalEdge:
      return {scenario, WorldGenerationScenarioFocus::WrapPartialPriority, false, false, false};
    case WorldGenerationValidationScenario::WorkersAndWorlds:
      return {scenario, WorldGenerationScenarioFocus::ScalabilityBudget, false, false, false};
    case WorldGenerationValidationScenario::MemoryPressureGpuKinds:
      return {scenario, WorldGenerationScenarioFocus::MemoryAccounting, true, false, false};
    case WorldGenerationValidationScenario::GpuAbsentSlowLost:
      return {scenario, WorldGenerationScenarioFocus::CpuFallback, true, false, false};
    case WorldGenerationValidationScenario::TwoPlayersNearFar:
      return {scenario, WorldGenerationScenarioFocus::FairnessTransfer, false, true, false};
    case WorldGenerationValidationScenario::MantiziGlitchFuBase:
      return {scenario, WorldGenerationScenarioFocus::StructureIntegrity, false, false, true};
    case WorldGenerationValidationScenario::UnloadSyncCrashRestart:
      return {scenario, WorldGenerationScenarioFocus::RecoveryCheckpoint, false, false, true};
    case WorldGenerationValidationScenario::Count:
      break;
  }
  return {WorldGenerationValidationScenario::OpenSkyTerrain,
      WorldGenerationScenarioFocus::Throughput, false, false, false};
}

inline uint32_t worldGenerationScenarioCount() noexcept {
  return static_cast<uint32_t>(WorldGenerationValidationScenario::Count);
}

inline bool worldGenerationScenarioRunnableWithoutGpu(
    WorldGenerationValidationScenario scenario) noexcept {
  return !worldGenerationScenarioEntry(scenario).requiresGpu;
}

}
