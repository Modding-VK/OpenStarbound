#pragma once

#include "StarWorldTemplateTypes.hpp"
#include "StarMaterialDatabase.hpp"

namespace Star {

// The same borrowed resolver used by the real BaseTiles loop and read-contract
// tests. Not a frozen context or proof of purity/lifetime of its sources.
struct WorldGenerationLookupResolver {
  WorldTemplate const& planet;
  MaterialDatabase const& materials;
  MaterialHue materialHue(BiomeIndex biome, MaterialId material) const { return planet.biomeMaterialHueShift(biome, material); }
  bool supportsMod(MaterialId material, ModId mod) const { return materials.supportsMod(material, mod); }
  MaterialHue modHue(BiomeIndex biome, ModId mod) const { return planet.biomeModHueShift(biome, mod); }
};

}
