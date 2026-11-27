#pragma once

#include "StarBiomePlacementTypes.hpp"
#include "StarMaybe.hpp"
#include "StarVector.hpp"
#include "StarWorldGenerationPlacementQueue.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace Star {

// Diagnostic snapshot of one planning candidate. First VAL-02 slice beyond
// BaseTiles: identity/order/priority/position only. BiomeItem payload and
// live-world publication are out of scope.
struct WorldGenerationCandidateRecord {
  static constexpr unsigned ModelVersion = 1;
  Maybe<WorldGenerationPlacementId> placementId;
  Vec2I position{};
  float priority = 0.0f;
};

enum class WorldGenerationCandidateField : uint32_t {
  Length = 1u << 0,
  PlacementId = 1u << 1,
  Position = 1u << 2,
  Priority = 1u << 3
};

struct WorldGenerationCandidateSequenceComparison {
  static constexpr unsigned ModelVersion = 1;
  bool valid = false;
  size_t comparedEntries = 0;
  size_t differentEntries = 0;
  uint32_t differentFields = 0;
  Maybe<size_t> firstDifference;
  bool equal() const {
    return valid && differentEntries == 0
        && (differentFields & static_cast<uint32_t>(WorldGenerationCandidateField::Length)) == 0;
  }
};

// Copies position/priority only; never retains BiomeItem or live queue state.
WorldGenerationCandidateRecord worldGenerationCandidateRecord(BiomeItemPlacement const& placement,
    Maybe<WorldGenerationPlacementId> placementId = {});

// Stable order matching BiomeItemPlacement::operator< (priority ascending).
// Equal priorities keep relative order (stable_sort). Does not mutate item.
void sortWorldGenerationCandidateRecords(std::vector<WorldGenerationCandidateRecord>& records);

// Bit-exact priority (signed zero/NaN payloads). Never memcmp the record:
// padding is not state. Sequences compare in the given order; callers that
// need legacy BiomeItemPlacement sort must sort before comparing.
WorldGenerationCandidateSequenceComparison compareWorldGenerationCandidateSequences(
    std::vector<WorldGenerationCandidateRecord> const& a,
    std::vector<WorldGenerationCandidateRecord> const& b);

}
