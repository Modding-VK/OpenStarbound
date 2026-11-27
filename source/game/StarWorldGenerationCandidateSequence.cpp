#include "StarWorldGenerationCandidateSequence.hpp"

#include <cstring>

namespace Star {

WorldGenerationCandidateRecord worldGenerationCandidateRecord(BiomeItemPlacement const& placement,
    Maybe<WorldGenerationPlacementId> placementId) {
  WorldGenerationCandidateRecord record;
  record.placementId = std::move(placementId);
  record.position = placement.position;
  record.priority = placement.priority;
  record.itemKind = placement.item.typeIndex();
  return record;
}

std::vector<WorldGenerationCandidateRecord> worldGenerationCandidateRecordsFromQueue(
    WorldGenerationPlacementQueue<BiomeItemPlacement> const& queue) {
  std::vector<WorldGenerationCandidateRecord> records;
  records.reserve(queue.entries().size());
  for (auto const& entry : queue.entries())
    records.push_back(worldGenerationCandidateRecord(entry.second, entry.first));
  return records;
}

void sortWorldGenerationCandidateRecords(std::vector<WorldGenerationCandidateRecord>& records) {
  std::stable_sort(records.begin(), records.end(),
      [](WorldGenerationCandidateRecord const& a, WorldGenerationCandidateRecord const& b) {
        return a.priority < b.priority;
      });
}

namespace {
bool sameFloat(float const& a, float const& b) {
  return std::memcmp(&a, &b, sizeof(float)) == 0;
}

bool sameId(Maybe<WorldGenerationPlacementId> const& a, Maybe<WorldGenerationPlacementId> const& b) {
  if (!a && !b)
    return true;
  if (!a || !b)
    return false;
  return *a == *b;
}

uint32_t differentFields(WorldGenerationCandidateRecord const& a, WorldGenerationCandidateRecord const& b) {
  uint32_t result = 0;
  auto check = [&](WorldGenerationCandidateField field, bool same) {
    if (!same)
      result |= static_cast<uint32_t>(field);
  };
  check(WorldGenerationCandidateField::PlacementId, sameId(a.placementId, b.placementId));
  check(WorldGenerationCandidateField::Position, a.position == b.position);
  check(WorldGenerationCandidateField::Priority, sameFloat(a.priority, b.priority));
  check(WorldGenerationCandidateField::ItemKind, a.itemKind == b.itemKind);
  return result;
}
}

WorldGenerationCandidateSequenceComparison compareWorldGenerationCandidateSequences(
    std::vector<WorldGenerationCandidateRecord> const& a,
    std::vector<WorldGenerationCandidateRecord> const& b) {
  WorldGenerationCandidateSequenceComparison result;
  result.valid = true;
  size_t const shared = std::min(a.size(), b.size());
  for (size_t i = 0; i != shared; ++i) {
    ++result.comparedEntries;
    auto fields = differentFields(a[i], b[i]);
    result.differentFields |= fields;
    if (fields) {
      ++result.differentEntries;
      if (!result.firstDifference)
        result.firstDifference = i;
    }
  }
  if (a.size() != b.size()) {
    result.differentFields |= static_cast<uint32_t>(WorldGenerationCandidateField::Length);
    if (!result.firstDifference)
      result.firstDifference = shared;
  }
  return result;
}

namespace {
bool sameStringSet(StringSet const& a, StringSet const& b) {
  if (a.size() != b.size())
    return false;
  for (auto const& value : a) {
    if (!b.contains(value))
      return false;
  }
  return true;
}

bool validBiomeItemMode(WorldGenerationBiomeItemComparisonMode mode) {
  return mode == WorldGenerationBiomeItemComparisonMode::KindOnly
      || mode == WorldGenerationBiomeItemComparisonMode::SupportedPayload;
}
}

WorldGenerationBiomeItemComparison compareWorldGenerationBiomeItems(BiomeItem const& a, BiomeItem const& b,
    WorldGenerationBiomeItemComparisonMode mode) {
  WorldGenerationBiomeItemComparison result;
  result.valid = validBiomeItemMode(mode);
  if (!result.valid)
    return result;
  if (a.typeIndex() != b.typeIndex()) {
    result.differentFields |= static_cast<uint32_t>(WorldGenerationBiomeItemField::Kind);
    return result;
  }
  if (mode == WorldGenerationBiomeItemComparisonMode::KindOnly)
    return result;

  if (auto aNames = a.ptr<MicroDungeonNames>()) {
    auto bNames = b.ptr<MicroDungeonNames>();
    if (!bNames || !sameStringSet(*aNames, *bNames))
      result.differentFields |= static_cast<uint32_t>(WorldGenerationBiomeItemField::Payload);
    return result;
  }
  if (auto aTreasure = a.ptr<TreasureBoxSet>()) {
    auto bTreasure = b.ptr<TreasureBoxSet>();
    if (!bTreasure || *aTreasure != *bTreasure)
      result.differentFields |= static_cast<uint32_t>(WorldGenerationBiomeItemField::Payload);
    return result;
  }

  // Grass/Bush/Tree/ObjectPool: kind matches, payload deferred.
  result.differentFields |= static_cast<uint32_t>(WorldGenerationBiomeItemField::UnsupportedPayload);
  return result;
}

WorldGenerationCandidateSequenceComparison compareWorldGenerationCandidatePlacements(
    std::vector<BiomeItemPlacement> const& a, std::vector<BiomeItemPlacement> const& b,
    std::vector<Maybe<WorldGenerationPlacementId>> const& aIds,
    std::vector<Maybe<WorldGenerationPlacementId>> const& bIds,
    WorldGenerationBiomeItemComparisonMode itemMode) {
  WorldGenerationCandidateSequenceComparison result;
  bool idsOk = (aIds.empty() || aIds.size() == a.size()) && (bIds.empty() || bIds.size() == b.size());
  result.valid = idsOk && validBiomeItemMode(itemMode);
  if (!result.valid)
    return result;

  auto idAt = [](std::vector<Maybe<WorldGenerationPlacementId>> const& ids, size_t index) -> Maybe<WorldGenerationPlacementId> {
    if (ids.empty())
      return {};
    return ids[index];
  };

  size_t const shared = std::min(a.size(), b.size());
  for (size_t i = 0; i != shared; ++i) {
    ++result.comparedEntries;
    auto left = worldGenerationCandidateRecord(a[i], idAt(aIds, i));
    auto right = worldGenerationCandidateRecord(b[i], idAt(bIds, i));
    uint32_t fields = differentFields(left, right);
    auto item = compareWorldGenerationBiomeItems(a[i].item, b[i].item, itemMode);
    if (!item.equal()) {
      if (item.differentFields & static_cast<uint32_t>(WorldGenerationBiomeItemField::Kind))
        fields |= static_cast<uint32_t>(WorldGenerationCandidateField::ItemKind);
      if (item.differentFields
          & (static_cast<uint32_t>(WorldGenerationBiomeItemField::Payload)
              | static_cast<uint32_t>(WorldGenerationBiomeItemField::UnsupportedPayload)))
        fields |= static_cast<uint32_t>(WorldGenerationCandidateField::ItemPayload);
    }
    result.differentFields |= fields;
    if (fields) {
      ++result.differentEntries;
      if (!result.firstDifference)
        result.firstDifference = i;
    }
  }
  if (a.size() != b.size()) {
    result.differentFields |= static_cast<uint32_t>(WorldGenerationCandidateField::Length);
    if (!result.firstDifference)
      result.firstDifference = shared;
  }
  return result;
}

}
