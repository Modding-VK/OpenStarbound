#pragma once

#include "StarTerrainDatabase.hpp"
#include "StarStaticRandom.hpp"

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

// Owner-only: preserves the single legacy global RNG draw before deriving
// region seeds. Snapshots may be materialized privately after this returns.
struct WorldGenerationRegionSelectorSnapshots {
  WorldGenerationSelectorSnapshot terrain;
  WorldGenerationSelectorSnapshot foregroundCave;
  WorldGenerationSelectorSnapshot backgroundCave;

  static WorldGenerationRegionSelectorSnapshots capture(unsigned width, float height, uint64_t regionSeed) {
    TerrainSelectorParameters base;
    base.worldWidth = width;
    base.baseHeight = height;
    return {WorldGenerationSelectorSnapshot::capture(base.withSeed(staticRandomU64(regionSeed, "Terrain"))),
      WorldGenerationSelectorSnapshot::capture(base.withSeed(staticRandomU64(regionSeed, "ForegroundCaveSeed"))),
      WorldGenerationSelectorSnapshot::capture(base.withSeed(staticRandomU64(regionSeed, "BackgroundCave")))};
  }
};

}
