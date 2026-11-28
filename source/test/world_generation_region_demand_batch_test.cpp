#include "StarWorldGenerationRegionDemandBatch.hpp"

#include "gtest/gtest.h"

using namespace Star;

TEST(WorldGenerationRegionDemandBatchTest, PlansWholeRegionBeforeAnyWait) {
  WorldGenerationRegionDemandBatch batch({2, 4}, {2, 2}, 9);
  EXPECT_EQ(4u, batch.planRegion());
  EXPECT_EQ(4u, batch.size());
  EXPECT_EQ(4u, batch.count(WorldGenerationRegionDemandState::Planned));
  EXPECT_EQ(0u, batch.planRegion());
}

TEST(WorldGenerationRegionDemandBatchTest, AdmitAllThenReadyWithoutYieldLoop) {
  WorldGenerationRegionDemandBatch batch({0, 0}, {2, 1}, 3);
  ASSERT_EQ(2u, batch.planRegion());
  EXPECT_EQ(2u, batch.admitAllPlanned());
  EXPECT_TRUE(batch.markReady({0, 0}));
  EXPECT_TRUE(batch.markReady({1, 0}));
  EXPECT_TRUE(batch.allReady());
  EXPECT_TRUE(batch.allTerminal());
}

TEST(WorldGenerationRegionDemandBatchTest, FailureAndCancelAreTerminal) {
  WorldGenerationRegionDemandBatch batch({0, 0}, {2, 1}, 1);
  ASSERT_EQ(2u, batch.planRegion());
  ASSERT_TRUE(batch.admit({0, 0}));
  ASSERT_TRUE(batch.admit({1, 0}));
  EXPECT_TRUE(batch.markFailed({0, 0}));
  EXPECT_TRUE(batch.cancel({1, 0}));
  EXPECT_FALSE(batch.allReady());
  EXPECT_TRUE(batch.allTerminal());
  EXPECT_EQ(WorldGenerationRegionDemandState::Failed, batch.state({0, 0}));
  EXPECT_EQ(WorldGenerationRegionDemandState::Cancelled, batch.state({1, 0}));
}

TEST(WorldGenerationRegionDemandBatchTest, CannotReadyUnadmittedOrCancelReady) {
  WorldGenerationRegionDemandBatch batch({0, 0}, {1, 1}, 1);
  ASSERT_EQ(1u, batch.planRegion());
  EXPECT_FALSE(batch.markReady({0, 0}));
  ASSERT_TRUE(batch.admit({0, 0}));
  ASSERT_TRUE(batch.markReady({0, 0}));
  EXPECT_FALSE(batch.cancel({0, 0}));
  EXPECT_TRUE(batch.allReady());
}
