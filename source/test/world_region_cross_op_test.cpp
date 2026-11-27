#include "StarWorldRegionCrossOp.hpp"
#include "StarGameTypes.hpp"

#include "gtest/gtest.h"

using namespace Star;

namespace {

WorldRegionPartition partition() {
  return WorldRegionPartition(WorldGeometry(256, 128));
}

// Adjacent regions {0,0} and {1,0}: sectors (1,0) and (2,0) sit on the shared border.
List<Vec2U> borderFootprint() {
  return {{1, 0}, {2, 0}};
}

}

TEST(WorldRegionCrossOpTest, ProposesVersionedIntentUnderCommonLogicalStep) {
  WorldRegionCrossOpShadow ledger(9, partition());
  auto id = ledger.propose(1, {0, 0}, {{1, 0}}, borderFootprint(),
      WorldRegionCrossIntentKind::BorderPublish);
  ASSERT_TRUE(id);
  EXPECT_EQ(id->worldEpoch, 9u);
  EXPECT_EQ(id->sequence, 1u);
  EXPECT_EQ(ledger.currentStep(), 1u);

  auto intent = ledger.intent(*id);
  ASSERT_TRUE(intent);
  EXPECT_EQ(intent->state, WorldRegionCrossIntentState::Proposed);
  EXPECT_EQ(intent->sourceRegion, Vec2U(0, 0));
  ASSERT_EQ(intent->targetRegions.size(), 1u);
  EXPECT_EQ(intent->targetRegions[0], Vec2U(1, 0));
  EXPECT_EQ(intent->footprintSectors, borderFootprint());
}

TEST(WorldRegionCrossOpTest, RejectsOlderStepAndDuplicateFootprintSectors) {
  WorldRegionCrossOpShadow ledger(9, partition());
  ASSERT_TRUE(ledger.propose(3, {0, 0}, {{1, 0}}, borderFootprint()));
  EXPECT_FALSE(ledger.propose(2, {0, 0}, {{1, 0}}, borderFootprint()));
  EXPECT_FALSE(ledger.propose(3, {0, 0}, {{1, 0}}, List<Vec2U>{{1, 0}, {1, 0}}));
  EXPECT_FALSE(ledger.propose(3, {0, 0}, {{1, 0}}, List<Vec2U>{{1, 0}}));
}

TEST(WorldRegionCrossOpTest, ReservesFullFootprintOrConflictsWithoutPartialHold) {
  WorldRegionCrossOpShadow ledger(9, partition());
  auto first = ledger.propose(1, {0, 0}, {{1, 0}}, borderFootprint());
  auto second = ledger.propose(1, {1, 0}, {{0, 0}}, borderFootprint());
  ASSERT_TRUE(first);
  ASSERT_TRUE(second);

  EXPECT_EQ(ledger.reserve(*first), WorldRegionCrossOpOutcome::Accepted);
  EXPECT_EQ(ledger.reservedSectorCount(), 2u);
  EXPECT_EQ(ledger.reserve(*second), WorldRegionCrossOpOutcome::Conflict);
  EXPECT_EQ(ledger.reservedSectorCount(), 2u);

  auto blocked = ledger.intent(*second);
  ASSERT_TRUE(blocked);
  EXPECT_EQ(blocked->state, WorldRegionCrossIntentState::Proposed);
}

TEST(WorldRegionCrossOpTest, CommitReleasesReservationForLaterConflictFreeReserve) {
  WorldRegionCrossOpShadow ledger(9, partition());
  auto first = ledger.propose(1, {0, 0}, {{1, 0}}, borderFootprint());
  auto second = ledger.propose(1, {1, 0}, {{0, 0}}, borderFootprint());
  ASSERT_TRUE(first);
  ASSERT_TRUE(second);
  ASSERT_EQ(ledger.reserve(*first), WorldRegionCrossOpOutcome::Accepted);
  ASSERT_EQ(ledger.reserve(*second), WorldRegionCrossOpOutcome::Conflict);

  EXPECT_EQ(ledger.commit(*first), WorldRegionCrossOpOutcome::Accepted);
  EXPECT_EQ(ledger.reservedSectorCount(), 0u);
  auto committed = ledger.intent(*first);
  ASSERT_TRUE(committed);
  EXPECT_EQ(committed->state, WorldRegionCrossIntentState::Committed);

  EXPECT_EQ(ledger.reserve(*second), WorldRegionCrossOpOutcome::Accepted);
  EXPECT_EQ(ledger.reservedSectorCount(), 2u);
}

TEST(WorldRegionCrossOpTest, FasterCommitDoesNotBypassEarlierConflictDecision) {
  WorldRegionCrossOpShadow ledger(9, partition());
  auto early = ledger.propose(1, {0, 0}, {{1, 0}}, borderFootprint());
  auto late = ledger.propose(1, {1, 0}, {{0, 0}}, borderFootprint());
  ASSERT_TRUE(early);
  ASSERT_TRUE(late);

  // Late intent finishes first in wall-clock terms but still conflicts until
  // the earlier reserved footprint is released by reject/timeout/commit.
  EXPECT_EQ(ledger.reserve(*early), WorldRegionCrossOpOutcome::Accepted);
  EXPECT_EQ(ledger.reserve(*late), WorldRegionCrossOpOutcome::Conflict);
  EXPECT_EQ(ledger.reject(*early), WorldRegionCrossOpOutcome::Accepted);
  EXPECT_EQ(ledger.reserve(*late), WorldRegionCrossOpOutcome::Accepted);
  EXPECT_EQ(ledger.timeout(*late), WorldRegionCrossOpOutcome::Accepted);

  auto timedOut = ledger.intent(*late);
  ASSERT_TRUE(timedOut);
  EXPECT_EQ(timedOut->state, WorldRegionCrossIntentState::TimedOut);
  EXPECT_EQ(ledger.reservedSectorCount(), 0u);
}
