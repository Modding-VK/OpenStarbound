// Asset-free value constructors relocated unchanged. Asset/config constructors
// remain in their original units; private tags below are ONLY fixture seams.
#include "StarWorldTemplateTypes.hpp"
#include "StarBiomeTypes.hpp"
#include "StarMaterialDatabase.hpp"

namespace Star {

WorldLayout::WorldLayout() : m_regionBlending(0.0f) {}

BiomePlaceables::BiomePlaceables() {
  grassMod = NoModId;
  grassModDensity = 0.0f;
  ceilingGrassMod = NoModId;
  ceilingGrassModDensity = 0.0f;
}

Biome::Biome() {
  mainBlock = EmptyMaterialId;
  hueShift = 0.0f;
  materialHueShift = MaterialHue();
}

SpawnProfile::SpawnProfile() {}

SkyParameters::SkyParameters() : seed(), skyType(SkyType::Barren), skyColoring(makeRight(Color::Black)), settings(JsonObject()) {}

MaterialDatabase::MaterialInfo::MaterialInfo() : id(NullMaterialId), tillableMod(NoModId), falling(), cascading() {}

// These objects contain only explicitly supplied lookup metadata, NOT complete
// worlds/databases. No Root, asset parsing, automatic RNG or terrain selectors.
// Normal production constructors DO NOT delegate to these fixture constructors.
MaterialDatabase::MaterialDatabase(LookupFixtureTag) {}

WorldTemplate::WorldTemplate(LookupFixtureTag)
  : m_customTerrainBlendSize(0), m_customTerrainBlendWeight(0), m_seed(0), m_geometry(Vec2U{32, 32}) {
  // The inherited SkyParameters default omits this flag. Do not read/copy it
  // uninitialized in fixtures, or silently change the production constructor.
  m_skyParameters.horizonClouds = false;
}

}
