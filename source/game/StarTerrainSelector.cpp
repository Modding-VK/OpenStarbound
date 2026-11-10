#include "StarTerrainDatabase.hpp"
#include "StarRandom.hpp"

namespace Star {

// Inherited bodies relocated unchanged for real sampling tests and normal
// generation to use the same implementation, without linking unrelated factories.
TerrainSelectorParameters::TerrainSelectorParameters() {
  seed = Random::randu64();
  worldWidth = 0;
  commonality = 1.0f;
}

TerrainSelectorParameters::TerrainSelectorParameters(Json const& v) {
  worldWidth = v.getUInt("worldWidth");
  baseHeight = v.getFloat("baseHeight");
  seed = v.getUInt("seed");
  commonality = v.getFloat("commonality");
}

TerrainSelectorParameters::TerrainSelectorParameters(unsigned worldWidth, float baseHeight, uint64_t seed, float commonality)
  : worldWidth(worldWidth), baseHeight(baseHeight), seed(seed), commonality(commonality) {}

Json TerrainSelectorParameters::toJson() const {
  return JsonObject{
      {"worldWidth", worldWidth}, {"baseHeight", baseHeight}, {"seed", seed}, {"commonality", commonality}};
}

TerrainSelectorParameters TerrainSelectorParameters::withSeed(uint64_t seed) const {
  TerrainSelectorParameters copy = *this;
  copy.seed = seed;
  return copy;
}

TerrainSelectorParameters TerrainSelectorParameters::withCommonality(float commonality) const {
  TerrainSelectorParameters copy = *this;
  copy.commonality = commonality;
  return copy;
}

TerrainSelector::TerrainSelector(String type, Json config, TerrainSelectorParameters parameters)
  : type(std::move(type)), config(std::move(config)), parameters(std::move(parameters)) {}

TerrainSelector::~TerrainSelector() {}

}
