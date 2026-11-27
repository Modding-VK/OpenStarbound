#include "StarWorldGenerationCandidateSequence.hpp"
#include "StarBiomePlacementTypes.hpp"
#include "StarWorldGenerationPlacementQueue.hpp"
#include "StarTileDamage.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

#include "gtest/gtest.h"

namespace Star {
// Must match world_generation_container_storage_test.cpp (ODR).
struct TileDamageParametersTestAccess {
  static void put(TileDamageParameters& value, TileDamageType type, float factor) { value.m_damages[type] = factor; }
  static auto const& factors(TileDamageParameters const& value) { return value.m_damages; }
};
}

using namespace Star;

namespace {
using Field = WorldGenerationCandidateField;
uint32_t mask(Field field) { return static_cast<uint32_t>(field); }
WorldGenerationCandidateRecord record(Maybe<WorldGenerationPlacementId> id, Vec2I position, float priority,
    VariantTypeIndex itemKind = InvalidVariantType) {
  return {id, position, priority, itemKind};
}
float bits(uint32_t value) {
  float result;
  static_assert(sizeof(result) == sizeof(value));
  std::memcpy(&result, &value, sizeof(result));
  return result;
}
// -ffast-math / finite-math-only makes std::isnan constant-fold to false and
// may canonicalize NaN payloads on float stores; keep bit patterns explicit.
void storeBits(float& dest, uint32_t value) {
  static_assert(sizeof(dest) == sizeof(value));
  std::memcpy(&dest, &value, sizeof(dest));
}
bool isNanBits(float const& value) {
  uint32_t bits = 0;
  std::memcpy(&bits, &value, sizeof(bits));
  return ((bits & 0x7f800000u) == 0x7f800000u) && ((bits & 0x007fffffu) != 0u);
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
  WorldGenerationCandidateRecord plusZero{};
  WorldGenerationCandidateRecord minusZero{};
  storeBits(plusZero.priority, 0x00000000u);
  storeBits(minusZero.priority, 0x80000000u);
  auto result = compareWorldGenerationCandidateSequences({plusZero}, {minusZero});
  EXPECT_FALSE(result.equal());
  EXPECT_EQ(result.differentFields, mask(Field::Priority));

  WorldGenerationCandidateRecord nanA{};
  WorldGenerationCandidateRecord nanB{};
  storeBits(nanA.priority, 0x7fc00001u);
  storeBits(nanB.priority, 0x7fc00002u);
  ASSERT_TRUE(isNanBits(nanA.priority));
  ASSERT_TRUE(isNanBits(nanB.priority));
  auto nanResult = compareWorldGenerationCandidateSequences({nanA}, {nanB});
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

TEST(WorldGenerationCandidateSequenceTest, MicroDungeonAndTreasurePayloadCompare) {
  using ItemField = WorldGenerationBiomeItemField;
  auto namesA = BiomeItem{MicroDungeonNames(StringSet{"alpha", "beta"})};
  auto namesB = BiomeItem{MicroDungeonNames(StringSet{"beta", "alpha"})};
  auto namesC = BiomeItem{MicroDungeonNames(StringSet{"alpha"})};
  auto sameNames = compareWorldGenerationBiomeItems(namesA, namesB);
  EXPECT_TRUE(sameNames.equal());
  auto diffNames = compareWorldGenerationBiomeItems(namesA, namesC);
  EXPECT_FALSE(diffNames.equal());
  EXPECT_EQ(diffNames.differentFields, static_cast<uint32_t>(ItemField::Payload));

  auto treasureA = BiomeItem{TreasureBoxSet(String("chest"))};
  auto treasureB = BiomeItem{TreasureBoxSet(String("chest"))};
  auto treasureC = BiomeItem{TreasureBoxSet(String("other"))};
  EXPECT_TRUE(compareWorldGenerationBiomeItems(treasureA, treasureB).equal());
  auto treasureDiff = compareWorldGenerationBiomeItems(treasureA, treasureC);
  EXPECT_EQ(treasureDiff.differentFields, static_cast<uint32_t>(ItemField::Payload));

  auto grass = BiomeItem{GrassVariant{}};
  EXPECT_TRUE(compareWorldGenerationBiomeItems(grass, grass).equal());
  EXPECT_TRUE(compareWorldGenerationBiomeItems(grass, grass, WorldGenerationBiomeItemComparisonMode::KindOnly).equal());

  ObjectPool poolA;
  poolA.add(1.0, make_pair(String("obj"), JsonObject{{"a", 1}}));
  ObjectPool poolB = poolA;
  EXPECT_TRUE(compareWorldGenerationBiomeItems(BiomeItem{poolA}, BiomeItem{poolB}).equal());
  poolB.add(2.0, make_pair(String("other"), Json()));
  EXPECT_FALSE(compareWorldGenerationBiomeItems(BiomeItem{poolA}, BiomeItem{poolB}).equal());
}

TEST(WorldGenerationCandidateSequenceTest, GrassBushTreePayloadCompareIncludingTileDamage) {
  GrassVariant grassA;
  grassA.name = "tuft";
  grassA.directory = "/grass/";
  grassA.images = StringList{"a.png", "b.png"};
  grassA.hueShift = 0.25f;
  grassA.ceiling = true;
  GrassVariant grassB = grassA;
  EXPECT_TRUE(compareWorldGenerationBiomeItems(BiomeItem{grassA}, BiomeItem{grassB}).equal());
  grassB.hueShift = 0.5f;
  auto grassDiff = compareWorldGenerationBiomeItems(BiomeItem{grassA}, BiomeItem{grassB});
  EXPECT_EQ(grassDiff.differentFields, static_cast<uint32_t>(WorldGenerationBiomeItemField::Payload));

  GrassVariant grassDamage = grassA;
  TileDamageParametersTestAccess::put(grassDamage.tileDamageParameters, TileDamageType::Blockish, 2.0f);
  EXPECT_FALSE(compareWorldGenerationBiomeItems(BiomeItem{grassA}, BiomeItem{grassDamage}).equal());

  BushVariant bushA;
  bushA.bushName = "scrub";
  bushA.modName = "flower";
  bushA.shapes = {{"base.png", StringList{"m1.png"}}};
  BushVariant bushB = bushA;
  EXPECT_TRUE(compareWorldGenerationBiomeItems(BiomeItem{bushA}, BiomeItem{bushB}).equal());
  bushB.shapes[0].mods = StringList{"m2.png"};
  EXPECT_FALSE(compareWorldGenerationBiomeItems(BiomeItem{bushA}, BiomeItem{bushB}).equal());

  TreeVariant stem;
  stem.stemName = "oak";
  stem.foliageName = "leaves";
  TreePair trees{stem, stem};
  TreePair treesDiff = trees;
  treesDiff.second.foliageName = "other";
  EXPECT_TRUE(compareWorldGenerationBiomeItems(BiomeItem{trees}, BiomeItem{trees}).equal());
  EXPECT_FALSE(compareWorldGenerationBiomeItems(BiomeItem{trees}, BiomeItem{treesDiff}).equal());
}

TEST(WorldGenerationCandidateSequenceTest, PlacementPayloadSequenceDetectsNameSetDiff) {
  BiomeItemPlacement left{BiomeItem{MicroDungeonNames(StringSet{"one", "two"})}, Vec2I(0, 0), 1.0f};
  BiomeItemPlacement right{BiomeItem{MicroDungeonNames(StringSet{"one"})}, Vec2I(0, 0), 1.0f};
  auto result = compareWorldGenerationCandidatePlacements({left}, {right});
  EXPECT_FALSE(result.equal());
  EXPECT_EQ(result.differentFields, mask(Field::ItemPayload));
  EXPECT_EQ(result.firstDifference.value(), 0u);

  auto kindOnly = compareWorldGenerationCandidatePlacements({left}, {right}, {}, {},
      WorldGenerationBiomeItemComparisonMode::KindOnly);
  EXPECT_TRUE(kindOnly.equal());
}

TEST(WorldGenerationCandidateSequenceTest, ItemKindDiffersWithoutComparingPayload) {
  auto grassKind = BiomeItem{GrassVariant{}}.typeIndex();
  auto dungeonKind = BiomeItem{MicroDungeonNames(StringSet{"x"})}.typeIndex();
  ASSERT_NE(grassKind, dungeonKind);

  auto left = record({}, {0, 0}, 1.0f, grassKind);
  auto right = record({}, {0, 0}, 1.0f, dungeonKind);
  auto result = compareWorldGenerationCandidateSequences({left}, {right});
  EXPECT_FALSE(result.equal());
  EXPECT_EQ(result.differentFields, mask(Field::ItemKind));

  BiomeItemPlacement grass{BiomeItem{GrassVariant{}}, Vec2I(1, 2), 0.25f};
  BiomeItemPlacement dungeon{BiomeItem{MicroDungeonNames(StringSet{"y"})}, Vec2I(1, 2), 0.25f};
  auto adapted = compareWorldGenerationCandidateSequences(
      {worldGenerationCandidateRecord(grass)}, {worldGenerationCandidateRecord(dungeon)});
  EXPECT_FALSE(adapted.equal());
  EXPECT_EQ(adapted.differentFields, mask(Field::ItemKind));
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
