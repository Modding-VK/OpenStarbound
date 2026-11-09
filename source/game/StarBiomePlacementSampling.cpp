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

WorldGenerationBackingEstimate estimateBiomeItemGenerationBacking(BiomeItem const& value) noexcept {
  WorldGenerationBackingEstimate result;
  auto strings = [&](StringList const& values) {
    result.storage.add(values.capacity(), sizeof(String));
    for (auto const& item : values) result.storage.stringCapacity(item);
  };
  auto tree = [&](TreeVariant const& variant) {
    result.storage.stringCapacity(variant.stemName);
    result.storage.stringCapacity(variant.foliageName);
    result.storage.stringCapacity(variant.stemDirectory);
    result.storage.stringCapacity(variant.foliageDirectory);
    result.storage.add(variant.tileDamageParameters.damageFactorPayloadBytes());
  };
  if (auto names = value.ptr<MicroDungeonNames>()) {
    // Includes occupied String objects: do not add sizeof(String) again.
    result.storage.add(names->bucketStorageBytes());
    for (auto const& name : *names) result.storage.stringCapacity(name);
  } else if (auto treasure = value.ptr<TreasureBoxSet>()) {
    result.storage.stringCapacity(*treasure);
  } else if (auto grass = value.ptr<GrassVariant>()) {
    result.storage.stringCapacity(grass->name);
    result.storage.stringCapacity(grass->directory);
    strings(grass->images);
    result.storage.add(grass->tileDamageParameters.damageFactorPayloadBytes());
    result.opaqueItems = 1;
  } else if (auto bush = value.ptr<BushVariant>()) {
    result.storage.stringCapacity(bush->bushName);
    result.storage.stringCapacity(bush->modName);
    result.storage.stringCapacity(bush->directory);
    result.storage.add(bush->shapes.capacity(), sizeof(BushVariant::BushShape));
    for (auto const& shape : bush->shapes) {
      result.storage.stringCapacity(shape.image);
      strings(shape.mods);
    }
    result.storage.add(bush->tileDamageParameters.damageFactorPayloadBytes());
    result.opaqueItems = 1;
  } else if (auto trees = value.ptr<TreePair>()) {
    tree(trees->first);
    tree(trees->second);
    result.opaqueItems = 1;
  } else if (auto objects = value.ptr<ObjectPool>()) {
    result.storage.add(objects->items().capacity(), sizeof(ObjectPool::ItemsType));
    for (auto const& item : objects->items()) result.storage.stringCapacity(item.second.first);
    result.opaqueItems = 1;
  } else {
    // Preserve unknown/valueless items; never turn incomplete sizing into a
    // filter. Json, map node bookkeeping/malloc overhead and SSO remain caveats.
    result.opaqueItems = 1;
  }
  return result;
}

WorldGenerationBackingEstimate BiomeItemDistribution::generationBackingEstimate() const {
  WorldGenerationBackingEstimate result;
  result.storage.add(m_densityFunction.allocatedStorageBytes());
  result.storage.add(m_modulusDistortion.allocatedStorageBytes());
  result.storage.add(m_randomItems.capacity(), sizeof(BiomeItem));
  result.storage.add(m_weightedItems.capacity(), sizeof(pair<BiomeItem, PerlinF>));
  for (auto const& value : m_randomItems) result.add(estimateBiomeItemGenerationBacking(value));
  for (auto const& weighted : m_weightedItems) {
    result.add(estimateBiomeItemGenerationBacking(weighted.first));
    result.storage.add(weighted.second.allocatedStorageBytes());
  }
  return result;
}

bool collectBiomeItemGenerationJson(BiomeItem const& value, WorldGenerationJsonRoots& roots) {
  auto tree = [&](TreeVariant const& variant) {
    return roots.append(variant.stemSettings) && roots.append(variant.foliageSettings)
        && roots.append(variant.descriptions) && roots.append(variant.stemDropConfig)
        && roots.append(variant.foliageDropConfig);
  };
  if (auto grass = value.ptr<GrassVariant>()) return roots.append(grass->descriptions);
  if (auto bush = value.ptr<BushVariant>()) return roots.append(bush->descriptions);
  if (auto trees = value.ptr<TreePair>()) return tree(trees->first) && tree(trees->second);
  if (auto objects = value.ptr<ObjectPool>()) {
    for (auto const& item : objects->items()) if (!roots.append(item.second.second)) return false;
  }
  return true;
}

bool BiomeItemDistribution::collectGenerationJson(WorldGenerationJsonRoots& roots) const {
  // Cover both physical lists even if only one is selected by the sampler.
  for (auto const& item : m_randomItems) if (!collectBiomeItemGenerationJson(item, roots)) return false;
  for (auto const& item : m_weightedItems) if (!collectBiomeItemGenerationJson(item.first, roots)) return false;
  return true;
}

std::shared_ptr<WorldGenerationJsonSnapshot const> captureBiomeDistributionGenerationJson(
    WorldGenerationMemoryLedger& ledger, BiomeItemDistribution const* values, size_t count,
    bool measure, size_t maximumValues) {
  if (!measure) return {};
  WorldGenerationJsonRoots roots(maximumValues);
  for (size_t i = 0; i < count; ++i) if (!values[i].collectGenerationJson(roots)) break;
  // MicroDungeonNames/default variants need no forest object or ledger token.
  // A truncated empty collection still needs an explicit incomplete result.
  if (roots.empty() && roots.complete()) return {};
  return captureWorldGenerationJsonRoots(ledger, std::move(roots));
}

}
