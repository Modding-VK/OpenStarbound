#include "StarWorldRegionEntityHandoff.hpp"
#include "StarGameTypes.hpp"

#include "gtest/gtest.h"

using namespace Star;

namespace {

WorldRegionPartition partition() {
  return WorldRegionPartition(WorldGeometry(256, 128));
}

WorldRegionEntityId entity(uint64_t id = 42, uint64_t incarnation = 1) {
  return {id, incarnation};
}

}

TEST(WorldRegionEntityHandoffTest, SeedsExclusiveOwnerAndCommitsTransfer) {
  WorldRegionEntityHandoffShadow ledger(3, partition());
  ASSERT_TRUE(ledger.seedOwner({entity(), {0, 0}, 1}));
  EXPECT_FALSE(ledger.seedOwner({entity(), {1, 0}, 1}));

  auto id = ledger.prepare(1, entity(), {1, 0});
  ASSERT_TRUE(id);
  EXPECT_EQ(ledger.commit(*id), WorldRegionEntityHandoffOutcome::Accepted);

  auto owner = ledger.owner(entity());
  ASSERT_TRUE(owner);
  EXPECT_EQ(owner->region, Vec2U(1, 0));
  EXPECT_EQ(owner->ownerGeneration, 2u);

  auto record = ledger.handoff(*id);
  ASSERT_TRUE(record);
  EXPECT_EQ(record->state, WorldRegionEntityHandoffState::Committed);
}

TEST(WorldRegionEntityHandoffTest, RejectsStaleOwnerGenerationOnCommit) {
  WorldRegionEntityHandoffShadow ledger(3, partition());
  ASSERT_TRUE(ledger.seedOwner({entity(), {0, 0}, 1}));

  auto id = ledger.prepare(1, entity(), {1, 0});
  ASSERT_TRUE(id);
  ASSERT_TRUE(ledger.bumpOwnerGeneration(entity()));
  EXPECT_EQ(ledger.commit(*id), WorldRegionEntityHandoffOutcome::Stale);

  auto record = ledger.handoff(*id);
  ASSERT_TRUE(record);
  EXPECT_EQ(record->state, WorldRegionEntityHandoffState::Stale);
  auto owner = ledger.owner(entity());
  ASSERT_TRUE(owner);
  EXPECT_EQ(owner->region, Vec2U(0, 0));
  EXPECT_EQ(owner->ownerGeneration, 2u);
}

TEST(WorldRegionEntityHandoffTest, OnlyOnePreparedHandoffPerEntityAndRejectReleasesSlot) {
  WorldRegionEntityHandoffShadow ledger(3, partition());
  ASSERT_TRUE(ledger.seedOwner({entity(), {0, 0}, 1}));

  auto first = ledger.prepare(1, entity(), {1, 0});
  ASSERT_TRUE(first);
  EXPECT_FALSE(ledger.prepare(1, entity(), {2, 0}));

  EXPECT_EQ(ledger.reject(*first), WorldRegionEntityHandoffOutcome::Accepted);
  auto second = ledger.prepare(1, entity(), {2, 0});
  ASSERT_TRUE(second);
  EXPECT_EQ(ledger.commit(*second), WorldRegionEntityHandoffOutcome::Accepted);

  auto owner = ledger.owner(entity());
  ASSERT_TRUE(owner);
  EXPECT_EQ(owner->region, Vec2U(2, 0));
}

TEST(WorldRegionEntityHandoffTest, IdempotentCommitOnSameStepIsDuplicate) {
  WorldRegionEntityHandoffShadow ledger(3, partition());
  ASSERT_TRUE(ledger.seedOwner({entity(), {0, 0}, 1}));
  auto first = ledger.prepare(5, entity(), {1, 0});
  ASSERT_TRUE(first);
  ASSERT_EQ(ledger.commit(*first), WorldRegionEntityHandoffOutcome::Accepted);

  // After commit, same entity cannot prepare again at the same logical step.
  EXPECT_FALSE(ledger.prepare(5, entity(), {0, 0}));
}
