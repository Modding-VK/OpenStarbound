#pragma once

#include "StarTerrainDatabase.hpp"

namespace Star {

// Capture only after every parameter field has been initialized on the owner.
// This value is not a layout, asset lease, job admission or commit authority.
struct WorldGenerationSelectorSnapshot {
  unsigned worldWidth;
  float baseHeight;
  uint64_t seed;
  float commonality;

  static WorldGenerationSelectorSnapshot capture(TerrainSelectorParameters const& value) {
    return {value.worldWidth, value.baseHeight, value.seed, value.commonality};
  }

  TerrainSelectorParameters materialize() const {
    return TerrainSelectorParameters(worldWidth, baseHeight, seed, commonality);
  }
};

}
