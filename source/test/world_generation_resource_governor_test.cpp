#include "gtest/gtest.h"

#include "StarWorldGenerationResourceGovernor.hpp"

using namespace Star;

TEST(WorldGenerationResourceGovernorTest, BalancedCapsStartsToOneWhenFrontierExists) {
  WorldGenerationResourceGovernor governor(WorldGenerationResourceProfile::Balanced);
  WorldGenerationResourceObservation obs;
  obs.executableFrontier = 8;
  obs.mailboxDepth = 0;
  auto budget = governor.suggest(obs);
  EXPECT_EQ(1u, budget.maxStarts);
  EXPECT_EQ(16u, budget.maxDrainMessages);
}

TEST(WorldGenerationResourceGovernorTest, HighThroughputRaisesStartsAndPrefersDrain) {
  WorldGenerationResourceGovernor governor(WorldGenerationResourceProfile::HighThroughput);
  WorldGenerationResourceObservation obs;
  obs.executableFrontier = 10;
  obs.mailboxDepth = 3;
  auto budget = governor.suggest(obs);
  EXPECT_EQ(4u, budget.maxStarts);
  EXPECT_EQ(3u, budget.maxDrainMessages);
}

TEST(WorldGenerationResourceGovernorTest, OverrunAndRetentionShrinkStarts) {
  WorldGenerationResourceGovernor governor(WorldGenerationResourceProfile::HighThroughput);
  WorldGenerationResourceObservation obs;
  obs.executableFrontier = 10;
  obs.lastPassTicksSpent = 50;
  obs.targetPassTicks = 10;
  auto overrun = governor.suggest(obs);
  EXPECT_EQ(1u, overrun.maxStarts);

  obs.lastPassTicksSpent = 0;
  obs.targetPassTicks = 0;
  obs.retainedProducts = 40;
  auto retained = governor.suggest(obs);
  EXPECT_EQ(1u, retained.maxStarts);
}

TEST(WorldGenerationResourceGovernorTest, EmptyFrontierYieldsZeroStarts) {
  WorldGenerationResourceGovernor governor(WorldGenerationResourceProfile::HighThroughput);
  WorldGenerationResourceObservation obs;
  obs.executableFrontier = 0;
  auto budget = governor.suggest(obs);
  EXPECT_EQ(0u, budget.maxStarts);
}
