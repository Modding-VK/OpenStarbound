#include "StarPlantDatabase.hpp"
#include "StarPlant.hpp"

namespace Star {

PlantPtr PlantDatabase::createPlant(TreeVariant const& treeVariant, uint64_t seed) const {
  try {
    return make_shared<Plant>(treeVariant, seed);
  } catch (std::exception const& e) {
    throw PlantDatabaseException(strf("Error constructing plant from tree variant stem: {} foliage: {}", treeVariant.stemName, treeVariant.foliageName), e);
  }
}

PlantPtr PlantDatabase::createPlant(GrassVariant const& grassVariant, uint64_t seed) const {
  try {
    return make_shared<Plant>(grassVariant, seed);
  } catch (std::exception const& e) {
    throw PlantDatabaseException(strf("Error constructing plant from grass variant name: {}", grassVariant.name), e);
  }
}

PlantPtr PlantDatabase::createPlant(BushVariant const& bushVariant, uint64_t seed) const {
  try {
    return make_shared<Plant>(bushVariant, seed);
  } catch (std::exception const& e) {
    throw PlantDatabaseException(
        strf("Error constructing plant from bush variant name: {} mod: {}", bushVariant.bushName, bushVariant.modName),
        e);
  }
}

}
