#include "StarWorldRegionBorder.hpp"
#include "StarGameTypes.hpp"

#include "gtest/gtest.h"

using namespace Star;

namespace {

WorldRegionPartition partition() {
  return WorldRegionPartition(WorldGeometry(256, 128));
}

List<WorldRegionBorderCell> sampleBorder() {
  return {{{0, 0}, true, 11}, {{31, 0}, true, 12}, {{32, 0}, false, 0}};
}

}

TEST(WorldRegionBorderTest, AbsentRegionIsUnavailableNotEmptyTerrain) {
  WorldRegionOwnerRegistry owners(7, partition());
  WorldRegionBorderLedger ledger(owners);
  EXPECT_EQ(ledger.capture({0, 0}, 1, 1, sampleBorder()), WorldRegionBorderCaptureResult::RegionAbsent);
  EXPECT_FALSE(ledger.latest({0, 0}));
  EXPECT_EQ(0u, ledger.snapshotCount());
}

TEST(WorldRegionBorderTest, ActiveOwnerPublishesImmutableBorderSnapshot) {
  WorldRegionOwnerRegistry owners(7, partition());
  ASSERT_EQ(owners.admit({0, 0}, WorldRegionMemberLevel::Tiles), WorldRegionOwnerAdmitResult::Created);
  WorldRegionBorderLedger ledger(owners);
  EXPECT_EQ(ledger.capture({0, 0}, 3, 9, sampleBorder()), WorldRegionBorderCaptureResult::Captured);
  auto snapshot = ledger.latest({0, 0});
  ASSERT_TRUE(snapshot);
  auto identity = owners.identity({0, 0});
  ASSERT_TRUE(identity);
  EXPECT_EQ(*identity, snapshot->owner);
  EXPECT_EQ(3u, snapshot->captureStep);
  EXPECT_EQ(9u, snapshot->logicalAge);
  ASSERT_EQ(3u, snapshot->cells.size());
  EXPECT_TRUE(snapshot->cells[0].present);
  EXPECT_FALSE(snapshot->cells[2].present);
  EXPECT_EQ(0u, snapshot->cells[2].contentRevision);
}

TEST(WorldRegionBorderTest, DrainingOwnerCannotPublishNewBorder) {
  WorldRegionOwnerRegistry owners(7, partition());
  ASSERT_EQ(owners.admit({0, 0}, WorldRegionMemberLevel::Tiles), WorldRegionOwnerAdmitResult::Created);
  WorldRegionBorderLedger ledger(owners);
  ASSERT_EQ(ledger.capture({0, 0}, 1, 1, sampleBorder()), WorldRegionBorderCaptureResult::Captured);
  ASSERT_EQ(owners.release({0, 0}), WorldRegionOwnerReleaseResult::DrainStarted);
  EXPECT_EQ(ledger.capture({0, 0}, 2, 2, sampleBorder()), WorldRegionBorderCaptureResult::RegionDraining);
  auto previous = ledger.latest({0, 0});
  ASSERT_TRUE(previous);
  EXPECT_EQ(1u, previous->captureStep);
}

TEST(WorldRegionBorderTest, EmptyCellListIsRejected) {
  WorldRegionOwnerRegistry owners(7, partition());
  ASSERT_EQ(owners.admit({0, 0}, WorldRegionMemberLevel::Entities), WorldRegionOwnerAdmitResult::Created);
  WorldRegionBorderLedger ledger(owners);
  EXPECT_EQ(ledger.capture({0, 0}, 1, 1, {}), WorldRegionBorderCaptureResult::EmptyCells);
}

TEST(WorldRegionBorderTest, PerimeterUsesPartialGeometryBounds) {
  // Width 96 => two full sectors + a partial third; region (0,0) is 64 tiles wide.
  WorldRegionPartition partial(WorldGeometry(96, 64));
  auto positions = WorldRegionBorderOutline::perimeterPositions(partial, {0, 0});
  ASSERT_TRUE(positions);
  EXPECT_FALSE(positions->empty());
  for (auto const& position : *positions) {
    auto region = partial.regionForPosition(position);
    ASSERT_TRUE(region);
    EXPECT_EQ(Vec2U(0, 0), *region);
  }
  auto skeleton = WorldRegionBorderOutline::unavailableSkeleton(*positions);
  ASSERT_EQ(positions->size(), skeleton.size());
  EXPECT_FALSE(skeleton[0].present);
}

TEST(WorldRegionBorderTest, ForeignCellOutsideOwnerRegionIsRejected) {
  WorldRegionOwnerRegistry owners(7, partition());
  ASSERT_EQ(owners.admit({0, 0}, WorldRegionMemberLevel::Tiles), WorldRegionOwnerAdmitResult::Created);
  WorldRegionBorderLedger ledger(owners);
  // Tile in neighboring region (1,0) must not be published as this owner's border.
  EXPECT_EQ(ledger.capture({0, 0}, 1, 1, {{{64, 0}, true, 1}}), WorldRegionBorderCaptureResult::ForeignCell);
  EXPECT_FALSE(ledger.latest({0, 0}));
}
