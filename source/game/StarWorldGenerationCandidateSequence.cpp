#include "StarWorldGenerationCandidateSequence.hpp"

#include <cstring>

namespace Star {

WorldGenerationCandidateRecord worldGenerationCandidateRecord(BiomeItemPlacement const& placement,
    Maybe<WorldGenerationPlacementId> placementId) {
  WorldGenerationCandidateRecord record;
  record.placementId = std::move(placementId);
  record.position = placement.position;
  record.priority = placement.priority;
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

}
