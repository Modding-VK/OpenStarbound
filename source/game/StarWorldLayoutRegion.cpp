#include "StarWorldLayout.hpp"
#include "StarWorldGenerationSelectorSnapshot.hpp"
#include "StarRoot.hpp"
#include "StarTerrainDatabase.hpp"
#include "StarBiomeDatabase.hpp"

namespace Star {

BiomeIndex WorldLayout::registerBiome(BiomeConstPtr biome) {
  size_t foundIndex = m_biomes.indexOf(biome);
  if (foundIndex != NPos)
    return foundIndex + 1;

  m_biomes.append(biome);
  return m_biomes.size();
}

TerrainSelectorIndex WorldLayout::registerTerrainSelector(TerrainSelectorConstPtr terrainSelector) {
  size_t foundIndex = m_terrainSelectors.indexOf(terrainSelector);
  if (foundIndex != NPos)
    return foundIndex + 1;

  m_terrainSelectors.append(terrainSelector);
  return m_terrainSelectors.size();
}

WorldRegion WorldLayout::buildRegion(uint64_t seed, RegionParams const& regionParams) {
  auto terrainDatabase = Root::singleton().terrainDatabase();
  auto biomeDatabase = Root::singleton().biomeDatabase();

  WorldRegion region;

  auto selectorSnapshots = WorldGenerationRegionSelectorSnapshots::capture(m_worldSize[0], regionParams.baseHeight, seed);
  TerrainSelectorParameters terrainSelectorParameters = selectorSnapshots.terrain.materialize();
  TerrainSelectorParameters foregroundCaveSelectorParameters = selectorSnapshots.foregroundCave.materialize();
  TerrainSelectorParameters backgroundCaveSelectorParameters = selectorSnapshots.backgroundCave.materialize();

  if (regionParams.terrainSelector)
    region.terrainSelectorIndex = registerTerrainSelector(terrainDatabase->createNamedSelector(*regionParams.terrainSelector, terrainSelectorParameters));
  if (regionParams.fgCaveSelector)
    region.foregroundCaveSelectorIndex = registerTerrainSelector(terrainDatabase->createNamedSelector(*regionParams.fgCaveSelector, foregroundCaveSelectorParameters));
  if (regionParams.bgCaveSelector)
    region.backgroundCaveSelectorIndex = registerTerrainSelector(terrainDatabase->createNamedSelector(*regionParams.bgCaveSelector, backgroundCaveSelectorParameters));

  if (regionParams.biomeName) {
    auto biome = biomeDatabase->createBiome(*regionParams.biomeName, staticRandomU64(seed, "BiomeSeed"), regionParams.baseHeight, regionParams.threatLevel);

    if (regionParams.subBlockSelector) {
      for (size_t i = 0; i < biome->subBlocks.size(); ++i) {
        auto selector = terrainDatabase->createNamedSelector(*regionParams.subBlockSelector, terrainSelectorParameters.withSeed(staticRandomU64(seed, i, "subBlocks")));
        region.subBlockSelectorIndexes.append(registerTerrainSelector(selector));
      }
    }

    for (auto const& p : enumerateIterator(biome->ores)) {
      auto oreSelectorTerrainParameters = terrainSelectorParameters.withCommonality(p.first.second);

      if (regionParams.fgOreSelector) {
        auto fgSelector = terrainDatabase->createNamedSelector(*regionParams.fgOreSelector, oreSelectorTerrainParameters.withSeed(staticRandomU64(seed, p.second, "FGOreSelector")));
        region.foregroundOreSelectorIndexes.append(registerTerrainSelector(fgSelector));
      }

      if (regionParams.bgOreSelector) {
        auto bgSelector = terrainDatabase->createNamedSelector(*regionParams.bgOreSelector, oreSelectorTerrainParameters.withSeed(staticRandomU64(seed, p.second, "BGOreSelector")));
        region.backgroundOreSelectorIndexes.append(registerTerrainSelector(bgSelector));
      }
    }

    region.blockBiomeIndex = registerBiome(biome);
    region.environmentBiomeIndex = region.blockBiomeIndex;
  }

  region.regionLiquids = regionParams.regionLiquids;

  return region;
}

}
