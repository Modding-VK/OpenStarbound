#include "StarWorldGenerationDeliveryMetrics.hpp"

#include "gtest/gtest.h"

using namespace Star;

TEST(WorldGenerationDeliveryMetricsTest, StartsEmptyAndTracksMask) {
  WorldGenerationDeliveryMetricsRecorder recorder;
  EXPECT_EQ(recorder.snapshot().recordedCount(), 0u);
  EXPECT_FALSE(recorder.snapshot().allMilestonesRecorded());

  recorder.recordInterest({7, 3, 2, 1.5, 1, false});
  EXPECT_TRUE(recorder.snapshot().has(WorldGenerationDeliveryMilestone::Interest));
  EXPECT_TRUE(recorder.snapshot().interest.recorded);
  EXPECT_EQ(recorder.snapshot().interest.sequence, 3u);
}

TEST(WorldGenerationDeliveryMetricsTest, ObservedCostRequiresCompleteResourceCoverage) {
  WorldGenerationDeliveryMetricsRecorder recorder;
  recorder.recordPreparation({9, 10, 18, WorldGenerationComputeBackend::Cpu, 64, 4, false});
  WorldGenerationDeliveryCost cost;
  EXPECT_FALSE(recorder.tryObservedDeliveryCost(cost));

  WorldGenerationResourceMetric resources;
  resources.usefulCpuMs = 8;
  resources.uniqueRamBytes = 64;
  resources.coverageComplete = false;
  recorder.recordResources(resources);
  EXPECT_FALSE(recorder.tryObservedDeliveryCost(cost));

  resources.coverageComplete = true;
  recorder.recordResources(resources);
  ASSERT_TRUE(recorder.tryObservedDeliveryCost(cost));
  EXPECT_DOUBLE_EQ(cost.executionMs, 8);
  EXPECT_TRUE(cost.valid());
}

TEST(WorldGenerationDeliveryMetricsTest, RecordsEveryMilestoneIndependently) {
  WorldGenerationDeliveryMetricsRecorder recorder;
  recorder.recordInterest({});
  recorder.recordPreparation({});
  recorder.recordEligibility({});
  recorder.recordPublication({});
  recorder.recordDelivery({});
  recorder.recordPassExit({WorldGenerationPassExitReason::Deadline, 3, false});
  WorldGenerationResourceMetric resources;
  resources.coverageComplete = true;
  recorder.recordResources(resources);
  EXPECT_TRUE(recorder.snapshot().allMilestonesRecorded());
  EXPECT_EQ(recorder.snapshot().passExit.reason, WorldGenerationPassExitReason::Deadline);
}
