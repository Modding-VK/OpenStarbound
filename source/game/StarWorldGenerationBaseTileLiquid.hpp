#pragma once

#include "StarLiquidTypes.hpp"
#include "StarMaterialTypes.hpp"

namespace Star {

// Scalar boundary extracted from prepareBaseTileArray. Already resolved tile
// materials and ocean parameters in; no template, Root, RNG, heap or world out.
// This versions this decision only, not the snapshot/identity of a whole sector.
struct WorldGenerationBaseTileLiquidInput {
  MaterialId foreground;
  MaterialId background;
  LiquidId oceanLiquid;
  int oceanLiquidLevel;
  bool encloseLiquids;
  int y;
};

enum class WorldGenerationBaseTileLiquidAction { Keep, Clear, Endless, Filled };

struct WorldGenerationBaseTileLiquidDecision {
  static constexpr unsigned ModelVersion = 1;
  WorldGenerationBaseTileLiquidAction action = WorldGenerationBaseTileLiquidAction::Keep;
  LiquidId liquid = EmptyLiquidId;
  float pressure = 0;
};

inline WorldGenerationBaseTileLiquidDecision worldGenerationBaseTileLiquid(WorldGenerationBaseTileLiquidInput const& input) {
  using Action = WorldGenerationBaseTileLiquidAction;
  if (input.foreground != EmptyMaterialId) return {Action::Clear, EmptyLiquidId, 0};
  if (input.oceanLiquid != EmptyLiquidId && input.y < input.oceanLiquidLevel) {
    // Preserve the original integer subtraction/conversion and branch order.
    // Inputs require the difference to fit int, as in the existing world path.
    float pressure = input.oceanLiquidLevel - input.y;
    if (input.background == EmptyMaterialId) return {Action::Endless, input.oceanLiquid, pressure};
    if (input.encloseLiquids) return {Action::Filled, input.oceanLiquid, pressure};
  }
  return {};
}

// Apply only to a caller-owned private LiquidStore; Keep performs NO assignment.
inline void applyWorldGenerationBaseTileLiquid(LiquidStore& target, WorldGenerationBaseTileLiquidInput const& input) {
  auto decision = worldGenerationBaseTileLiquid(input);
  switch (decision.action) {
    case WorldGenerationBaseTileLiquidAction::Keep: break;
    case WorldGenerationBaseTileLiquidAction::Clear: target = LiquidStore(); break;
    case WorldGenerationBaseTileLiquidAction::Endless: target = LiquidStore::endless(decision.liquid, decision.pressure); break;
    case WorldGenerationBaseTileLiquidAction::Filled: target = LiquidStore::filled(decision.liquid, 1.0f, decision.pressure); break;
  }
}

}
