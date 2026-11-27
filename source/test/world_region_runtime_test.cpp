#include "StarWorldRegionRuntime.hpp"
#include "StarGameTypes.hpp"

#include "gtest/gtest.h"

using namespace Star;

namespace {

WorldRegionPartition partition() {
  return WorldRegionPartition(WorldGeometry(256, 128));
}

}

TEST(WorldRegionRuntimeTest, CoordinatorRoutesInterestWithoutCentralPublish) {
  WorldRegionRuntimeCoordinator runtime(21, partition());
  EXPECT_EQ(runtime.noteInterest({0, 0}, WorldRegionMemberLevel::Queued),
      WorldRegionRuntimeInterestResult::IgnoredQueued);
  EXPECT_EQ(0u, runtime.ownerCount());
  EXPECT_EQ(runtime.noteInterest({0, 0}, WorldRegionMemberLevel::Tiles), WorldRegionRuntimeInterestResult::Noted);
  EXPECT_EQ(1u, runtime.ownerCount());
  auto owner = runtime.activeOwner({0, 0});
  ASSERT_TRUE(owner);
  EXPECT_EQ(21u, owner->worldEpoch);
  EXPECT_EQ(Vec2U(0, 0), owner->region);
}

TEST(WorldRegionRuntimeTest, ClockAgesBorderSnapshotsWithoutOwningSimulation) {
  WorldRegionRuntimeCoordinator runtime(21, partition());
  ASSERT_EQ(runtime.noteInterest({0, 0}, WorldRegionMemberLevel::Tiles), WorldRegionRuntimeInterestResult::Noted);
  EXPECT_EQ(0u, runtime.clock());
  EXPECT_EQ(1u, runtime.advanceClock());
  EXPECT_EQ(runtime.publishBorder({0, 0}, {{{0, 0}, true, 5}}), WorldRegionBorderCaptureResult::Captured);
  auto border = runtime.border({0, 0});
  ASSERT_TRUE(border);
  EXPECT_EQ(1u, border->captureStep);
  EXPECT_EQ(1u, border->logicalAge);
  auto stillActive = runtime.activeOwner({0, 0});
  ASSERT_TRUE(stillActive);
  EXPECT_EQ(*stillActive, border->owner);
  EXPECT_EQ(2u, runtime.advanceClock());
  EXPECT_EQ(1u, runtime.border({0, 0})->logicalAge);
}

TEST(WorldRegionRuntimeTest, DrainHonorsPendingWorkThroughCoordinator) {
  WorldRegionRuntimeCoordinator runtime(21, partition());
  ASSERT_EQ(runtime.noteInterest({0, 0}, WorldRegionMemberLevel::Entities), WorldRegionRuntimeInterestResult::Noted);
  EXPECT_TRUE(runtime.addPendingWork({0, 0}));
  EXPECT_EQ(runtime.releaseInterest({0, 0}), WorldRegionOwnerReleaseResult::DrainStarted);
  EXPECT_FALSE(runtime.activeOwner({0, 0}));
  EXPECT_EQ(runtime.completeDrain({0, 0}), WorldRegionOwnerReleaseResult::PendingWork);
  EXPECT_TRUE(runtime.clearPendingWork({0, 0}));
  EXPECT_EQ(runtime.completeDrain({0, 0}), WorldRegionOwnerReleaseResult::OwnerRetired);
  EXPECT_EQ(0u, runtime.ownerCount());
}

TEST(WorldRegionRuntimeTest, CreationFailureKeepsLegacyCentralOwnershipPath) {
  WorldRegionRuntimeCoordinator runtime(21, partition(), false);
  EXPECT_EQ(runtime.noteInterest({0, 0}, WorldRegionMemberLevel::Tiles),
      WorldRegionRuntimeInterestResult::CreationFailed);
  EXPECT_EQ(0u, runtime.ownerCount());
  EXPECT_FALSE(runtime.activeOwner({0, 0}));
}

TEST(WorldRegionRuntimeTest, CoordinatorExposesLocalInventoryAndSharedCheckpoint) {
  WorldRegionRuntimeCoordinator runtime(21, partition());
  ASSERT_EQ(runtime.noteInterest({0, 0}, WorldRegionMemberLevel::Tiles), WorldRegionRuntimeInterestResult::Noted);
  ASSERT_EQ(runtime.noteInterest({2, 0}, WorldRegionMemberLevel::Tiles), WorldRegionRuntimeInterestResult::Noted);
  auto kinds = worldRegionLocalKindMask(WorldRegionLocalKind::Tiles)
      | worldRegionLocalKindMask(WorldRegionLocalKind::Collision);
  EXPECT_EQ(runtime.declareLocalKinds({0, 0}, kinds), WorldRegionLocalDeclareResult::Declared);
  EXPECT_EQ(runtime.declareLocalKinds({1, 0}, kinds), WorldRegionLocalDeclareResult::Declared);
  ASSERT_TRUE(runtime.localInventory({0, 0}));
  EXPECT_EQ(runtime.confirmCheckpoint({0, 0}, 1), WorldRegionCheckpointNoteResult::Noted);
  EXPECT_EQ(runtime.advanceCheckpoint(1), WorldRegionCheckpointAdvanceResult::WaitingOnOwners);
  EXPECT_EQ(runtime.confirmCheckpoint({1, 0}, 1), WorldRegionCheckpointNoteResult::Noted);
  EXPECT_EQ(runtime.advanceCheckpoint(1), WorldRegionCheckpointAdvanceResult::Advanced);
  EXPECT_EQ(1u, runtime.committedCheckpoint());
}
