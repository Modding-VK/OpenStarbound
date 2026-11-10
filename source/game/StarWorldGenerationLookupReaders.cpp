// Existing reader bodies relocated unchanged for linking real readers without
// Root/assets constructors. Production and NoAssets use these SAME definitions.
#include "StarWorldGenerationLookupResolver.hpp"
#include "StarBiomeTypes.hpp"

namespace Star {

uint64_t WorldTemplate::generationRevision() const {
  return m_generationRevision;
}

void WorldTemplate::setWorldLayout(WorldLayoutPtr newLayout) {
  m_layout = take(newLayout);
  m_blockCache.clear();
  ++m_generationRevision;
}

MaterialHue WorldTemplate::biomeMaterialHueShift(BiomeIndex biomeIndex, MaterialId material) const {
  if (m_layout && biomeIndex != NullBiomeIndex) {
    auto const& biome = m_layout->getBiome(biomeIndex);
    if (material == biome->mainBlock)
      return biome->materialHueShift;
  }

  return MaterialHue();
}

MaterialHue WorldTemplate::biomeModHueShift(BiomeIndex biomeIndex, ModId mod) const {
  if (m_layout && biomeIndex != NullBiomeIndex) {
    auto const& biome = m_layout->getBiome(biomeIndex);
    if (mod == biome->surfacePlaceables.grassMod || mod == biome->surfacePlaceables.ceilingGrassMod
        || mod == biome->undergroundPlaceables.grassMod
        || mod == biome->undergroundPlaceables.ceilingGrassMod)
      return biome->materialHueShift;
  }
  return MaterialHue();
}

bool MaterialDatabase::supportsMod(MaterialId materialId, ModId modId) const {
  if (modId == NoModId)
    return true;
  if (!isRealMaterial(materialId))
    return false;
  if (!isRealMod(modId))
    return false;

  return getMaterialInfo(materialId)->supportsMods;
}

}
