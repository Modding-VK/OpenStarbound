#include "StarWorldRegionRecovery.hpp"
#include "StarGameTypes.hpp"

#include "gtest/gtest.h"

using namespace Star;

namespace {

WorldRegionPartition partition() {
  return WorldRegionPartition(WorldGeometry(256, 128));
}

}

TEST(WorldRegionRecoveryTest, FailureInvalidatesUnconfirmedIntentsAndKeepsPartialMark) {
  WorldRegionRecoveryShadow ledger(11, partition());
  ASSERT_TRUE(ledger.noteUnconfirmed(1, {0, 0}, 10));
  ASSERT_TRUE(ledger.noteUnconfirmed(1, {0, 0}, 11));
  EXPECT_EQ(2u, ledger.unconfirmedCount(1, {0, 0}));
  EXPECT_TRUE(ledger.regionPartiallyPublished({0, 0}));

  auto failure = ledger.fail(1, {0, 0});
  ASSERT_TRUE(failure);
  EXPECT_EQ(failure->invalidatedIntentSequences.size(), 2u);
  EXPECT_EQ(ledger.unconfirmedCount(1, {0, 0}), 0u);
  auto failed = ledger.stepHealth(1);
  ASSERT_TRUE(failed);
  EXPECT_EQ(*failed, WorldRegionStepHealth::Failed);
  // Must not look reverted after failure alone.
  EXPECT_TRUE(ledger.regionPartiallyPublished({0, 0}));
  EXPECT_FALSE(ledger.noteUnconfirmed(1, {0, 0}, 12));
}

TEST(WorldRegionRecoveryTest, CheckpointRequiresClearedUnconfirmedThenEnablesLegacyReturn) {
  WorldRegionRecoveryShadow ledger(11, partition());
  ASSERT_TRUE(ledger.noteUnconfirmed(2, {1, 0}, 20));
  EXPECT_FALSE(ledger.checkpoint(2, {{1, 0}}));

  ASSERT_TRUE(ledger.fail(2, {1, 0}));
  auto id = ledger.checkpoint(2, {{1, 0}});
  ASSERT_TRUE(id);
  auto health = ledger.stepHealth(2);
  ASSERT_TRUE(health);
  EXPECT_EQ(*health, WorldRegionStepHealth::Checkpointed);
  EXPECT_FALSE(ledger.regionPartiallyPublished({1, 0}));

  EXPECT_TRUE(ledger.returnToLegacy(*id));
  EXPECT_EQ(ledger.mode(), WorldRegionRecoveryMode::Legacy);
}

TEST(WorldRegionRecoveryTest, ReloadLegacyClearsPartialPublishWithoutCheckpoint) {
  WorldRegionRecoveryShadow ledger(11, partition());
  ASSERT_TRUE(ledger.noteUnconfirmed(3, {0, 1}, 30));
  ASSERT_TRUE(ledger.fail(3, {0, 1}));
  EXPECT_TRUE(ledger.regionPartiallyPublished({0, 1}));

  EXPECT_TRUE(ledger.reloadLegacy());
  EXPECT_EQ(ledger.mode(), WorldRegionRecoveryMode::Legacy);
  EXPECT_FALSE(ledger.regionPartiallyPublished({0, 1}));
  EXPECT_FALSE(ledger.stepHealth(3).isValid());
}
