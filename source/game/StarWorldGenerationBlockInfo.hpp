#pragma once

#include "StarWorldTemplateTypes.hpp"
#include "StarBiomeTypes.hpp"
#include "StarInterpolation.hpp"

namespace Star {

// Inherited WorldTemplate block calculation, extracted without a new terrain
// algorithm. Original expression order and first-match rules are retained.
// Calculation only: no cache, Root, world mutation or ownership acquisition.
// Source supplies the existing lazy queries. It is NOT a frozen snapshot or a
// purity guarantee: the legacy adapter still reaches Root/custom terrain and
// worker-private mutable selector caches. Do not share that adapter across jobs.
// Query order, branching, arithmetic and first-match priority are compatibility
// contracts. In particular, do not eagerly read config or selector arrays.
// Kept templated so production has no virtual calls, std::function or per-tile
// input tape; a future owned context must prove the SAME query semantics.
template <typename Source>
WorldTemplate::BlockInfo calculateWorldGenerationBlockInfo(Source& source, uint32_t x, uint32_t y) {
  WorldTemplate::BlockInfo blockInfo;

  if (!source.hasLayout())
    return blockInfo;

  // The environment biome is calculated with weighting based on the flat coordinates.
  auto flatWeighting = source.getWeighting(x, y);

  // The block biome is calculated optionally with higher frequency noise
  // added to prevent straight lines appearing on the boundaries of
  // regions.

  int blendNoiseOffset = source.blendNoiseOffset(x, y);

  Vec2I blockPos;
  decltype(flatWeighting) blockWeighting;
  decltype(flatWeighting) transitionWeighting;
  if (auto blockPosition = source.blockNoisePosition(x, y)) {
    blockPos = *blockPosition;
    blockWeighting = source.getWeighting(blockPos[0] + blendNoiseOffset, blockPos[1]);
    transitionWeighting = source.getWeighting(blockPos[0], blockPos[1]);
  } else {
    blockPos = Vec2I(x, y);
    blockWeighting = flatWeighting;
    transitionWeighting = flatWeighting;
  }

  if (flatWeighting.empty() || blockWeighting.empty())
    return blockInfo;

  auto const& primaryFlatWeighting = flatWeighting.first();
  auto const& primaryBlockWeighting = blockWeighting.first();

  blockInfo.blockBiomeIndex = primaryBlockWeighting.region->blockBiomeIndex;
  blockInfo.environmentBiomeIndex = primaryFlatWeighting.region->environmentBiomeIndex;

  blockInfo.biomeTransition = transitionWeighting.first().weight < source.configFloat("biomeTransitionThreshold", 0);

  float terrainSelect = 0.0f;
  float foregroundCaveSelect = 0.0f;
  float backgroundCaveSelect = 0.0f;

  // Terrain weighting uses the flat weighting, and weights each selector
  // to blend among them.
  for (auto const& weighting : flatWeighting) {
    if (weighting.region->terrainSelectorIndex != NullTerrainSelectorIndex) {
      float select = source.terrainSelectorValue(weighting.region->terrainSelectorIndex, weighting.xValue, y) * weighting.weight;
      terrainSelect += select;
    }
  }

  // This is a bit of a cheat. Since customTerrainWeighting is always flat,
  // there are some odd effects that come from linearly interpolating from
  // the generally non-flat terrain sources to flat regions of space.  By
  // using an interpolator that has an exaggerated S curve between the
  // points, this hides some of these effects.
  auto ctweighting = source.customTerrainWeighting(x, y);
  terrainSelect = quintic2(ctweighting.second, terrainSelect, ctweighting.first);

  if (terrainSelect > 0.0f) {
    blockInfo.terrain = true;

    for (auto const& weighting : flatWeighting) {
      if (weighting.region->foregroundCaveSelectorIndex != NullTerrainSelectorIndex) {
        foregroundCaveSelect += source.terrainSelectorValue(weighting.region->foregroundCaveSelectorIndex, weighting.xValue, y) * weighting.weight;
      }

      if (weighting.region->backgroundCaveSelectorIndex != NullTerrainSelectorIndex) {
        backgroundCaveSelect += source.terrainSelectorValue(weighting.region->backgroundCaveSelectorIndex, weighting.xValue, y) * weighting.weight;
      }
    }

    auto surfaceCaveAttenuationDist = source.configFloat("surfaceCaveAttenuationDist", 0);
    if (terrainSelect < surfaceCaveAttenuationDist) {
      auto surfaceCaveAttenuationFactor = source.configFloat("surfaceCaveAttenuationFactor", 1);
      foregroundCaveSelect -= (surfaceCaveAttenuationDist - terrainSelect) * surfaceCaveAttenuationFactor;
      backgroundCaveSelect -= (surfaceCaveAttenuationDist - terrainSelect) * surfaceCaveAttenuationFactor;
    }
  }

  blockInfo.foregroundCave = foregroundCaveSelect > 0.0f;
  blockInfo.backgroundCave = backgroundCaveSelect > 0.0f;

  auto const& regionLiquids = primaryFlatWeighting.region->regionLiquids;
  blockInfo.caveLiquid = regionLiquids.caveLiquid;
  blockInfo.caveLiquidSeedDensity = regionLiquids.caveLiquidSeedDensity;
  blockInfo.oceanLiquid = regionLiquids.oceanLiquid;
  blockInfo.oceanLiquidLevel = regionLiquids.oceanLiquidLevel;
  blockInfo.encloseLiquids = regionLiquids.encloseLiquids;
  blockInfo.fillMicrodungeons = regionLiquids.fillMicrodungeons;

  if (!blockInfo.terrain && blockInfo.encloseLiquids && (int)y < blockInfo.oceanLiquidLevel) {
    blockInfo.terrain = true;
    blockInfo.foregroundCave = true;
  }

  if (blockInfo.terrain) {
    if (auto blockBiome = source.biome(blockInfo.blockBiomeIndex)) {
      if (!blockInfo.foregroundCave) {
        blockInfo.foreground = blockBiome->mainBlock;
        blockInfo.background = blockInfo.foreground;
      } else if (!blockInfo.backgroundCave) {
        blockInfo.background = blockBiome->mainBlock;
      }

      // subBlock, foregroundOre, and backgroundOre selectors can be empty
      // if they are not enabled, otherwise they will always have the
      // correct count

      if (!primaryBlockWeighting.region->subBlockSelectorIndexes.empty()) {
        for (size_t i = 0; i < blockBiome->subBlocks.size(); ++i) {
          if (source.terrainSelectorValue(primaryBlockWeighting.region->subBlockSelectorIndexes.at(i), primaryBlockWeighting.xValue - blendNoiseOffset, blockPos[1]) > 0.0f) {
            if (!blockInfo.foregroundCave) {
              blockInfo.foreground = blockBiome->subBlocks.at(i);
              blockInfo.background = blockInfo.foreground;
            } else if (!blockInfo.backgroundCave) {
              blockInfo.background = blockBiome->subBlocks.at(i);
            }

            break;
          }
        }
      }

      if (!blockInfo.foregroundCave && !primaryBlockWeighting.region->foregroundOreSelectorIndexes.empty()) {
        for (size_t i = 0; i < blockBiome->ores.size(); ++i) {
          if (source.terrainSelectorValue(primaryBlockWeighting.region->foregroundOreSelectorIndexes.at(i), x, y) > 0.0f) {
            blockInfo.foregroundMod = blockBiome->ores.at(i).first;
            break;
          }
        }
      }

      if (!blockInfo.backgroundCave && !primaryBlockWeighting.region->backgroundOreSelectorIndexes.empty()) {
        for (size_t i = 0; i < blockBiome->ores.size(); ++i) {
          if (source.terrainSelectorValue(primaryBlockWeighting.region->backgroundOreSelectorIndexes.at(i), x, y) > 0.0f) {
            blockInfo.backgroundMod = blockBiome->ores.at(i).first;
            break;
          }
        }
      }
    }
  }

  return blockInfo;
}

}
