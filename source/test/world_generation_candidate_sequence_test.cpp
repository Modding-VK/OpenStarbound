#include "StarWorldGenerationCandidateSequence.hpp"
#include "StarBiomePlacementTypes.hpp"
#include "StarWorldGenerationPlacementQueue.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

#include "gtest/gtest.h"

using namespace Star;

namespace {
using Field = WorldGenerationCandidateField;
uint32_t mask(Field field) { return static_cast<uint32_t>(field); }
WorldGenerationCandidateRecord record(Maybe<WorldGenerationPlacementId> id, Vec2I position, float priority) {
  return {id, position, priority};
}
float bits(uint32_t value) {
  float result;
  static_assert(sizeof(result) == sizeof(value));
  std::memcpy(&result, &value, sizeof(result));
  return result;
}
}

TEST(WorldGenerationCandidateSequenceTest, EqualSequencesMatchBitExactPriority) {
  std::vector<WorldGenerationCandidateRecord> a{
      record(WorldGenerationPlacementId(1), {3, 4}, 1.5f),
      record({}, {5, 6}, -0.0f)};
  auto b = a;
  auto result = compareWorldGenerationCandidateSequences(a, b);
  EXPECT_TRUE(result.valid);
  EXPECT_TRUE(result.equal());
  EXPECT_EQ(result.comparedEntries, 2u);
  EXPECT_EQ(result.differentEntries, 0u);
  EXPECT_EQ(result.differentFields, 0u);
  EXPECT_FALSE(result.firstDifference);
}

TEST(WorldGenerationCandidateSequenceTest, LengthMismatchIsReported) {
  std::vector<WorldGenerationCandidateRecord> a{record(WorldGenerationPlacementId(1), {0, 0}, 1.0f)};
  std::vector<WorldGenerationCandidateRecord> b{
      record(WorldGenerationPlacementId(1), {0, 0}, 1.0f),
      record(WorldGenerationPlacementId(2), {1, 0}, 2.0f)};
  auto result = compareWorldGenerationCandidateSequences(a, b);
  EXPECT_TRUE(result.valid);
  EXPECT_FALSE(result.equal());
  EXPECT_EQ(result.comparedEntries, 1u);
  EXPECT_EQ(result.differentEntries, 0u);
  EXPECT_EQ(result.differentFields, mask(Field::Length));
  EXPECT_EQ(result.firstDifference.value(), 1u);
}

TEST(WorldGenerationCandidateSequenceTest, PlacementIdPositionAndPriorityDiffsAreIsolated) {
  auto base = record(WorldGenerationPlacementId(9), {2, 3}, 4.0f);
  {
    auto left = std::vector{base};
    auto right = std::vector{record(WorldGenerationPlacementId(10), {2, 3}, 4.0f)};
    auto result = compareWorldGenerationCandidateSequences(left, right);
    EXPECT_EQ(result.differentFields, mask(Field::PlacementId));
    EXPECT_EQ(result.firstDifference.value(), 0u);
  }
  {
    auto left = std::vector{base};
    auto right = std::vector{record(WorldGenerationPlacementId(9), {8, 3}, 4.0f)};
    auto result = compareWorldGenerationCandidateSequences(left, right);
    EXPECT_EQ(result.differentFields, mask(Field::Position));
  }
  {
    auto left = std::vector{base};
    auto right = std::vector{record(WorldGenerationPlacementId(9), {2, 3}, 5.0f)};
    auto result = compareWorldGenerationCandidateSequences(left, right);
    EXPECT_EQ(result.differentFields, mask(Field::Priority));
  }
}

TEST(WorldGenerationCandidateSequenceTest, SignedZeroAndNaNPayloadsRemainDistinct) {
  auto plusZero = record({}, {0, 0}, 0.0f);
  auto minusZero = record({}, {0, 0}, -0.0f);
  auto result = compareWorldGenerationCandidateSequences({plusZero}, {minusZero});
  EXPECT_FALSE(result.equal());
  EXPECT_EQ(result.differentFields, mask(Field::Priority));

  float nanA = bits(0x7fc00001u);
  float nanB = bits(0x7fc00002u);
  ASSERT_TRUE(std::isnan(nanA));
  ASSERT_TRUE(std::isnan(nanB));
  auto nanResult = compareWorldGenerationCandidateSequences(
      {record({}, {0, 0}, nanA)}, {record({}, {0, 0}, nanB)});
  EXPECT_FALSE(nanResult.equal());
  EXPECT_EQ(nanResult.differentFields, mask(Field::Priority));
}

TEST(WorldGenerationCandidateSequenceTest, RelativeOrderMismatchSurfacesAtFirstIndex) {
  std::vector<WorldGenerationCandidateRecord> sorted{
      record(WorldGenerationPlacementId(1), {0, 0}, 1.0f),
      record(WorldGenerationPlacementId(2), {1, 0}, 2.0f)};
  std::vector<WorldGenerationCandidateRecord> swapped{
      record(WorldGenerationPlacementId(2), {1, 0}, 2.0f),
      record(WorldGenerationPlacementId(1), {0, 0}, 1.0f)};
  auto result = compareWorldGenerationCandidateSequences(sorted, swapped);
  EXPECT_FALSE(result.equal());
  EXPECT_EQ(result.comparedEntries, 2u);
  EXPECT_EQ(result.differentEntries, 2u);
  EXPECT_EQ(result.firstDifference.value(), 0u);
  EXPECT_NE(result.differentFields & mask(Field::PlacementId), 0u);
  EXPECT_NE(result.differentFields & mask(Field::Priority), 0u);
}

TEST(WorldGenerationCandidateSequenceTest, AbsentVersusPresentPlacementIdDiffers) {
  auto withId = record(WorldGenerationPlacementId(3), {1, 1}, 0.5f);
  auto withoutId = record({}, {1, 1}, 0.5f);
  auto result = compareWorldGenerationCandidateSequences({withId}, {withoutId});
  EXPECT_FALSE(result.equal());
  EXPECT_EQ(result.differentFields, mask(Field::PlacementId));
}

TEST(WorldGenerationCandidateSequenceTest, PlacementAdapterCopiesPositionPriorityAndOptionalId) {
  BiomeItemPlacement placement{BiomeItem{MicroDungeonNames(StringSet{"alpha"})}, Vec2I(4, 9), 2.25f};
  auto withoutId = worldGenerationCandidateRecord(placement);
  EXPECT_FALSE(withoutId.placementId);
  EXPECT_EQ(withoutId.position, Vec2I(4, 9));
  EXPECT_EQ(withoutId.priority, 2.25f);

  auto withId = worldGenerationCandidateRecord(placement, WorldGenerationPlacementId(42));
  ASSERT_TRUE(withId.placementId);
  EXPECT_EQ(*withId.placementId, 42u);
  EXPECT_EQ(withId.position, placement.position);
  EXPECT_EQ(withId.priority, placement.priority);
}

TEST(WorldGenerationCandidateSequenceTest, StablePrioritySortMatchesBiomeItemPlacementOrder) {
  BiomeItemPlacement high{BiomeItem{MicroDungeonNames(StringSet{"high"})}, Vec2I(1, 0), 3.0f};
  BiomeItemPlacement midA{BiomeItem{MicroDungeonNames(StringSet{"mid-a"})}, Vec2I(2, 0), 1.0f};
  BiomeItemPlacement midB{BiomeItem{MicroDungeonNames(StringSet{"mid-b"})}, Vec2I(3, 0), 1.0f};
  BiomeItemPlacement low{BiomeItem{MicroDungeonNames(StringSet{"low"})}, Vec2I(0, 0), 0.5f};

  std::vector<BiomeItemPlacement> placements{high, midA, midB, low};
  std::stable_sort(placements.begin(), placements.end());

  std::vector<WorldGenerationCandidateRecord> records{
      worldGenerationCandidateRecord(high, WorldGenerationPlacementId(1)),
      worldGenerationCandidateRecord(midA, WorldGenerationPlacementId(2)),
      worldGenerationCandidateRecord(midB, WorldGenerationPlacementId(3)),
      worldGenerationCandidateRecord(low, WorldGenerationPlacementId(4))};
  sortWorldGenerationCandidateRecords(records);

  ASSERT_EQ(records.size(), placements.size());
  for (size_t i = 0; i != records.size(); ++i) {
    EXPECT_EQ(records[i].position, placements[i].position) << i;
    EXPECT_EQ(records[i].priority, placements[i].priority) << i;
  }
  // Equal priorities keep relative input order (midA before midB).
  EXPECT_EQ(records[1].position, Vec2I(2, 0));
  EXPECT_EQ(records[2].position, Vec2I(3, 0));

  std::vector<WorldGenerationCandidateRecord> expected;
  for (auto const& placement : placements)
    expected.push_back(worldGenerationCandidateRecord(placement));
  // IDs were assigned only on the unsorted path; strip for content compare.
  for (auto& entry : records)
    entry.placementId = {};
  auto result = compareWorldGenerationCandidateSequences(records, expected);
  EXPECT_TRUE(result.equal());
}

TEST(WorldGenerationCandidateSequenceTest, QueueSnapshotPreservesAscendingIdsAfterRetirement) {
  WorldGenerationPlacementQueue<BiomeItemPlacement> queue;
  auto first = queue.append(BiomeItemPlacement{BiomeItem{MicroDungeonNames(StringSet{"a"})}, Vec2I(0, 0), 2.0f});
  auto second = queue.append(BiomeItemPlacement{BiomeItem{MicroDungeonNames(StringSet{"b"})}, Vec2I(1, 0), 1.0f});
  auto third = queue.append(BiomeItemPlacement{BiomeItem{MicroDungeonNames(StringSet{"c"})}, Vec2I(2, 0), 3.0f});

  auto before = worldGenerationCandidateRecordsFromQueue(queue);
  ASSERT_EQ(before.size(), 3u);
  EXPECT_EQ(*before[0].placementId, first);
  EXPECT_EQ(*before[1].placementId, second);
  EXPECT_EQ(*before[2].placementId, third);
  EXPECT_EQ(before[0].priority, 2.0f);
  EXPECT_EQ(before[1].position, Vec2I(1, 0));

  ASSERT_TRUE(queue.take(second));
  auto after = worldGenerationCandidateRecordsFromQueue(queue);
  ASSERT_EQ(after.size(), 2u);
  EXPECT_EQ(*after[0].placementId, first);
  EXPECT_EQ(*after[1].placementId, third);

  auto mismatch = compareWorldGenerationCandidateSequences(before, after);
  EXPECT_FALSE(mismatch.equal());
  EXPECT_NE(mismatch.differentFields & mask(Field::Length), 0u);

  auto sorted = after;
  sortWorldGenerationCandidateRecords(sorted);
  EXPECT_EQ(sorted[0].priority, 2.0f);
  EXPECT_EQ(sorted[1].priority, 3.0f);
  EXPECT_EQ(*sorted[0].placementId, first);
  EXPECT_EQ(*sorted[1].placementId, third);
}
