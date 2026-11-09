// Read-only sampling extracted from StarBiomePlacement.cpp; no Root/assets
// dependency. Keep selection/copy order equivalent to the inherited path.
#include "StarBiomePlacementTypes.hpp"
#include "StarWorldGenerationMemoryEstimate.hpp"
#include "StarStaticRandom.hpp"

namespace Star {

BiomeItemPlacement::BiomeItemPlacement(BiomeItem item, Vec2I position, float priority)
  : item(std::move(item)), position(position), priority(priority) {}

bool BiomeItemPlacement::operator<(BiomeItemPlacement const& rhs) const {
  return priority < rhs.priority;
}

BiomeItemDistribution::BiomeItemDistribution() {
  m_mode = BiomePlacementMode::Floor;
  m_distribution = DistributionType::Random;
  m_modulus = 1;
  m_modulusOffset = 0;
  m_blockSeed = 0;
  m_blockProbability = 0.0f;
  m_priority = 0.0f;
}

BiomePlacementMode BiomeItemDistribution::mode() const {
  return m_mode;
}

List<BiomeItem> BiomeItemDistribution::allItems() const {
  if (m_distribution == DistributionType::Random) {
    return m_randomItems;
  } else if (m_distribution == DistributionType::Periodic) {
    List<BiomeItem> items;
    for (auto const& pair : m_weightedItems)
      items.append(pair.first);
    return items;
  } else {
    return {};
  }
}

Maybe<BiomeItemPlacement> BiomeItemDistribution::itemToPlace(int x, int y) const {
  if (m_distribution == DistributionType::Random) {
    if (staticRandomFloat(x, y, m_blockSeed) <= m_blockProbability)
      return BiomeItemPlacement{staticRandomValueFrom(m_randomItems, x, y, m_blockSeed), Vec2I(x, y), m_priority};
  } else if (m_distribution == DistributionType::Periodic) {
    BiomeItem const* biomeItem = nullptr;
    if (m_densityFunction.get(x, y) > 0) {
      if ((int)(x + m_modulusOffset + m_modulusDistortion.get(x, y)) % m_modulus == 0) {
        float maxWeight = lowest<float>();
        for (auto const& weightedItem : m_weightedItems) {
          float weight = weightedItem.second.get(x, y);
          if (weight > maxWeight) {
            maxWeight = weight;
            biomeItem = &weightedItem.first;
          }
        }
      }
    }
    if (biomeItem)
      return BiomeItemPlacement{*biomeItem, Vec2I(x, y), m_priority};
  }
  return {};
}

WorldGenerationBackingEstimate BiomeItemDistribution::generationBackingEstimate() const {
  WorldGenerationBackingEstimate result;
  result.storage.add(m_densityFunction.allocatedStorageBytes());
  result.storage.add(m_modulusDistortion.allocatedStorageBytes());
  result.storage.add(m_randomItems.capacity(), sizeof(BiomeItem));
  result.storage.add(m_weightedItems.capacity(), sizeof(pair<BiomeItem, PerlinF>));
  auto item = [&](BiomeItem const& value) {
    if (auto names = value.ptr<MicroDungeonNames>()) {
      result.storage.add(names->size(), sizeof(String));
      for (auto const& name : *names) result.storage.stringCapacity(name);
    } else {
      // Plants/object pools/treasure internals remain explicitly outside this
      // model. Inline representations are in the containing vector already.
      ++result.opaqueItems;
    }
  };
  for (auto const& value : m_randomItems) item(value);
  for (auto const& weighted : m_weightedItems) {
    item(weighted.first);
    result.storage.add(weighted.second.allocatedStorageBytes());
  }
  return result;
}

}
