#pragma once

// Data-only declarations extracted from StarBiomePlacement.hpp. The original
// header remains a compatibility facade, including its Plant declarations.

#include "StarPerlin.hpp"
#include "StarWeightedPool.hpp"
#include "StarBiMap.hpp"
#include "StarPlantDatabase.hpp"
#include "StarTreasure.hpp"
#include "StarStrongTypedef.hpp"

namespace Star {

STAR_CLASS(BiomeItemDistribution);
struct WorldGenerationBackingEstimate;

STAR_EXCEPTION(BiomeException, StarException);

typedef pair<TreeVariant, TreeVariant> TreePair;
// Weighted pairs of object name / parameters.
typedef WeightedPool<pair<String, Json>> ObjectPool;
strong_typedef(String, TreasureBoxSet);
strong_typedef(StringSet, MicroDungeonNames);
typedef Variant<GrassVariant, BushVariant, TreePair, ObjectPool, TreasureBoxSet, MicroDungeonNames> BiomeItem;
BiomeItem variantToBiomeItem(Json const& store);
Json variantFromBiomeItem(BiomeItem const& biomeItem);

enum class BiomePlacementArea { Surface, Underground };
enum class BiomePlacementMode { Floor, Ceiling, Background, Ocean };
extern EnumMap<BiomePlacementMode> const BiomePlacementModeNames;

struct BiomeItemPlacement {
  BiomeItemPlacement(BiomeItem item, Vec2I position, float priority);
  // Orders by priority.
  bool operator<(BiomeItemPlacement const& rhs) const;
  BiomeItem item;
  Vec2I position;
  float priority;
};

class BiomeItemDistribution {
public:
  struct PeriodicWeightedItem {
    BiomeItem item;
    PerlinF weight;
  };

  static Maybe<BiomeItem> createItem(Json const& itemSettings, RandomSource& rand, float biomeHueShift);
  BiomeItemDistribution();
  BiomeItemDistribution(Json const& config, uint64_t seed, float biomeHueShift = 0.0f);
  BiomeItemDistribution(Json const& store);
  Json toJson() const;
  BiomePlacementMode mode() const;
  List<BiomeItem> allItems() const;
  // Return the best weighted item only when density permits this position.
  Maybe<BiomeItemPlacement> itemToPlace(int x, int y) const;
  // Known backing, not a complete heap bound. Opaque items remain explicit.
  WorldGenerationBackingEstimate generationBackingEstimate() const;

private:
  friend struct BiomeItemDistributionTestAccess;
  // Periodic noise groups items spatially; Random uses the block seed.
  enum class DistributionType { Random, Periodic };
  static EnumMap<DistributionType> const DistributionTypeNames;
  BiomePlacementMode m_mode;
  DistributionType m_distribution;
  float m_priority;
  // Random distribution inputs.
  float m_blockProbability;
  uint64_t m_blockSeed;
  List<BiomeItem> m_randomItems;
  // Periodic density/modulus and weighted noise inputs.
  PerlinF m_densityFunction;
  PerlinF m_modulusDistortion;
  int m_modulus;
  int m_modulusOffset;
  List<pair<BiomeItem, PerlinF>> m_weightedItems;
};

}
