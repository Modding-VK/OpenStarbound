#include "StarWorldGenerationCandidateSequence.hpp"

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
