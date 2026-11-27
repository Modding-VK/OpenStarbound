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

// Diagnostic snapshot of one planning candidate. VAL-02 beyond BaseTiles:
// identity/order/priority/itemKind. Live-world publication stays out of scope.
struct WorldGenerationCandidateRecord {
  static constexpr unsigned ModelVersion = 2;
  Maybe<WorldGenerationPlacementId> placementId;
  Vec2I position{};
  float priority = 0.0f;
  // BiomeItem Variant typeIndex only — not payload contents.
  VariantTypeIndex itemKind = InvalidVariantType;
};

enum class WorldGenerationCandidateField : uint32_t {
  Length = 1u << 0,
  PlacementId = 1u << 1,
  Position = 1u << 2,
  Priority = 1u << 3,
  ItemKind = 1u << 4,
  ItemPayload = 1u << 5
};

struct WorldGenerationCandidateSequenceComparison {
  static constexpr unsigned ModelVersion = 3;
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

enum class WorldGenerationBiomeItemField : uint32_t {
  Kind = 1u << 0,
  Payload = 1u << 1,
  // Same kind, but Grass/Bush/Tree/ObjectPool content is not compared yet.
  UnsupportedPayload = 1u << 2
};

enum class WorldGenerationBiomeItemComparisonMode {
  KindOnly,
  // All BiomeItem kinds including plant TileDamageParameters (bit-exact factors).
  // ObjectPool compares weight/name/Json entries in order.
  SupportedPayload
};

struct WorldGenerationBiomeItemComparison {
  static constexpr unsigned ModelVersion = 1;
  bool valid = false;
  uint32_t differentFields = 0;
  bool equal() const { return valid && differentFields == 0; }
};

// Copies position/priority only; never retains BiomeItem or live queue state.
WorldGenerationCandidateRecord worldGenerationCandidateRecord(BiomeItemPlacement const& placement,
    Maybe<WorldGenerationPlacementId> placementId = {});

// Owner-thread snapshot: ascending placement IDs, no BiomeItem copy retained.
// Does not sort by priority; callers may sortWorldGenerationCandidateRecords.
std::vector<WorldGenerationCandidateRecord> worldGenerationCandidateRecordsFromQueue(
    WorldGenerationPlacementQueue<BiomeItemPlacement> const& queue);

// Stable order matching BiomeItemPlacement::operator< (priority ascending).
// Equal priorities keep relative order (stable_sort). Does not mutate item.
void sortWorldGenerationCandidateRecords(std::vector<WorldGenerationCandidateRecord>& records);

// Bit-exact priority (signed zero/NaN payloads). Never memcmp the record:
// padding is not state. Sequences compare in the given order; callers that
// need legacy BiomeItemPlacement sort must sort before comparing.
WorldGenerationCandidateSequenceComparison compareWorldGenerationCandidateSequences(
    std::vector<WorldGenerationCandidateRecord> const& a,
    std::vector<WorldGenerationCandidateRecord> const& b);

// Diagnostic BiomeItem compare. Never calls Root/Assets; never changes selection.
WorldGenerationBiomeItemComparison compareWorldGenerationBiomeItems(BiomeItem const& a, BiomeItem const& b,
    WorldGenerationBiomeItemComparisonMode mode = WorldGenerationBiomeItemComparisonMode::SupportedPayload);

// Record fields + BiomeItem payload in lockstep. Optional parallel id lists must
// match placement length when present; empty id vector means all ids absent.
WorldGenerationCandidateSequenceComparison compareWorldGenerationCandidatePlacements(
    std::vector<BiomeItemPlacement> const& a, std::vector<BiomeItemPlacement> const& b,
    std::vector<Maybe<WorldGenerationPlacementId>> const& aIds = {},
    std::vector<Maybe<WorldGenerationPlacementId>> const& bIds = {},
    WorldGenerationBiomeItemComparisonMode itemMode = WorldGenerationBiomeItemComparisonMode::SupportedPayload);

}
