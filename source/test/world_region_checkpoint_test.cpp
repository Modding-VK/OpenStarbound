#include "StarWorldRegionCheckpoint.hpp"
#include "StarGameTypes.hpp"

#include "gtest/gtest.h"

using namespace Star;

namespace {

WorldRegionPartition partition() {
  return WorldRegionPartition(WorldGeometry(256, 128));
}

}

TEST(WorldRegionCheckpointTest, BarrierWaitsForEveryActiveOwner) {
  WorldRegionOwnerRegistry owners(2, partition());
  ASSERT_EQ(owners.admit({0, 0}, WorldRegionMemberLevel::Tiles), WorldRegionOwnerAdmitResult::Created);
  ASSERT_EQ(owners.admit({2, 0}, WorldRegionMemberLevel::Tiles), WorldRegionOwnerAdmitResult::Created);
  WorldRegionCheckpointBarrier barrier(owners);
  auto a = *owners.identity({0, 0});
  auto b = *owners.identity({1, 0});

  EXPECT_EQ(barrier.noteConfirmed(a, 1), WorldRegionCheckpointNoteResult::Noted);
  EXPECT_EQ(barrier.tryAdvance(1), WorldRegionCheckpointAdvanceResult::WaitingOnOwners);
  EXPECT_EQ(1u, barrier.laggingOwners(1).size());
  EXPECT_EQ(barrier.noteConfirmed(b, 1), WorldRegionCheckpointNoteResult::Noted);
  EXPECT_EQ(barrier.tryAdvance(1), WorldRegionCheckpointAdvanceResult::Advanced);
  EXPECT_EQ(1u, barrier.committedStep());
}

TEST(WorldRegionCheckpointTest, FasterOwnerCannotSkipSharedCheckpoint) {
  WorldRegionOwnerRegistry owners(2, partition());
  ASSERT_EQ(owners.admit({0, 0}, WorldRegionMemberLevel::Entities), WorldRegionOwnerAdmitResult::Created);
  ASSERT_EQ(owners.admit({2, 0}, WorldRegionMemberLevel::Entities), WorldRegionOwnerAdmitResult::Created);
  WorldRegionCheckpointBarrier barrier(owners);
  auto a = *owners.identity({0, 0});
  auto b = *owners.identity({1, 0});
  ASSERT_EQ(barrier.noteConfirmed(a, 1), WorldRegionCheckpointNoteResult::Noted);
  ASSERT_EQ(barrier.noteConfirmed(b, 1), WorldRegionCheckpointNoteResult::Noted);
  ASSERT_EQ(barrier.tryAdvance(1), WorldRegionCheckpointAdvanceResult::Advanced);

  EXPECT_EQ(barrier.noteConfirmed(a, 3), WorldRegionCheckpointNoteResult::Noted);
  EXPECT_EQ(barrier.tryAdvance(3), WorldRegionCheckpointAdvanceResult::WaitingOnOwners);
  EXPECT_EQ(1u, barrier.committedStep());
  EXPECT_EQ(barrier.noteConfirmed(b, 2), WorldRegionCheckpointNoteResult::Noted);
  EXPECT_EQ(barrier.tryAdvance(3), WorldRegionCheckpointAdvanceResult::WaitingOnOwners);
  EXPECT_EQ(barrier.noteConfirmed(b, 3), WorldRegionCheckpointNoteResult::Noted);
  EXPECT_EQ(barrier.tryAdvance(3), WorldRegionCheckpointAdvanceResult::Advanced);
  EXPECT_EQ(3u, barrier.committedStep());
}

TEST(WorldRegionCheckpointTest, DrainingOwnerIsExcludedFromBarrier) {
  WorldRegionOwnerRegistry owners(2, partition());
  ASSERT_EQ(owners.admit({0, 0}, WorldRegionMemberLevel::Tiles), WorldRegionOwnerAdmitResult::Created);
  ASSERT_EQ(owners.admit({2, 0}, WorldRegionMemberLevel::Tiles), WorldRegionOwnerAdmitResult::Created);
  WorldRegionCheckpointBarrier barrier(owners);
  auto a = *owners.identity({0, 0});
  auto b = *owners.identity({1, 0});
  ASSERT_EQ(barrier.noteConfirmed(a, 1), WorldRegionCheckpointNoteResult::Noted);
  ASSERT_EQ(owners.release({2, 0}), WorldRegionOwnerReleaseResult::DrainStarted);
  EXPECT_EQ(barrier.noteConfirmed(b, 1), WorldRegionCheckpointNoteResult::RegionDraining);
  EXPECT_EQ(barrier.tryAdvance(1), WorldRegionCheckpointAdvanceResult::Advanced);
  EXPECT_EQ(1u, barrier.committedStep());
}
