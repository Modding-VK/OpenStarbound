#include "StarWorldGenerationScaleMetrics.hpp"

#include "gtest/gtest.h"

using namespace Star;

namespace {
WorldGenerationShadowBatchObservation equalHalfSignMask() {
  WorldGenerationShadowBatchObservation o;
  o.kernel = WorldGenerationShadowKernelFamily::DenseUint32HalfSignMask;
  o.verdict = WorldGenerationShadowCompareVerdict::Equal;
  o.compared = 8;
  o.divergences = 0;
  o.cpuExecutionMs = 4.0;
  o.gpuExecutionMs = 1.5;
  o.transferMs = 0.4;
  o.fenceMs = 0.2;
  o.materializeMs = 0.1;
  return o;
}
}

TEST(WorldGenerationScaleMetricsTest, RejectsInvalidEqualWithDivergences) {
  WorldGenerationScaleMetricsRecorder recorder;
  auto bad = equalHalfSignMask();
  bad.divergences = 1;
  EXPECT_FALSE(recorder.recordShadowBatch(bad));
  EXPECT_EQ(recorder.snapshot().batchesRecorded, 0u);
}

TEST(WorldGenerationScaleMetricsTest, AggregatesH4TipKernelsAndMeasuredGain) {
  WorldGenerationScaleMetricsRecorder recorder;
  recorder.claimCapabilities({true, true, true});

  EXPECT_TRUE(recorder.recordShadowBatch(equalHalfSignMask()));

  WorldGenerationShadowBatchObservation zero = equalHalfSignMask();
  zero.kernel = WorldGenerationShadowKernelFamily::DenseUint32HalfIsZero;
  zero.anyPendingOpenKernels = true;
  EXPECT_TRUE(recorder.recordShadowBatch(zero));

  WorldGenerationShadowBatchObservation nonzero = equalHalfSignMask();
  nonzero.kernel = WorldGenerationShadowKernelFamily::DenseUint32HalfIsNonzero;
  EXPECT_TRUE(recorder.recordShadowBatch(nonzero));

  auto const& snap = recorder.snapshot();
  EXPECT_EQ(snap.batchesRecorded, 3u);
  EXPECT_EQ(snap.equalBatches, 3u);
  EXPECT_EQ(snap.pendingOpenKernelHits, 1u);
  EXPECT_TRUE(snap.coversH4TipHalfPredicates());
  EXPECT_TRUE(snap.measuredDeliveryGain());

  auto routing = recorder.tryRoutingSample();
  ASSERT_TRUE(routing.ready);
  EXPECT_TRUE(routing.capabilities.gpuComputeAvailable);
  EXPECT_TRUE(routing.measuredDeliveryGain);
  EXPECT_LT(routing.gpuCost.totalMs(), routing.cpuCost.totalMs());
}

TEST(WorldGenerationScaleMetricsTest, RoutingStaysCpuWithoutProbeClaim) {
  WorldGenerationScaleMetricsRecorder recorder;
  recorder.claimCapabilities({true, true, false});
  EXPECT_TRUE(recorder.recordShadowBatch(equalHalfSignMask()));

  WorldGenerationHardwareSelector selector;
  auto choice = selectFromScaleMetrics(
      selector, recorder, WorldGenerationPerformanceMode::Adaptive);
  EXPECT_EQ(choice.backend, WorldGenerationComputeBackend::Cpu);
  EXPECT_EQ(choice.reason, WorldGenerationHardwareSelectionReason::CpuOnlyCapability);
}

TEST(WorldGenerationScaleMetricsTest, SelectsGpuWhenProbeAndGainAgree) {
  WorldGenerationScaleMetricsRecorder recorder;
  recorder.claimCapabilities({true, true, true});
  EXPECT_TRUE(recorder.recordShadowBatch(equalHalfSignMask()));

  WorldGenerationHardwareSelector selector;
  auto choice = selectFromScaleMetrics(
      selector, recorder, WorldGenerationPerformanceMode::Dynamic);
  EXPECT_EQ(choice.backend, WorldGenerationComputeBackend::Gpu);
  EXPECT_EQ(choice.reason, WorldGenerationHardwareSelectionReason::LowerDeliveryCost);
}

TEST(WorldGenerationScaleMetricsTest, DivergedBatchBlocksRoutingSample) {
  WorldGenerationScaleMetricsRecorder recorder;
  recorder.claimCapabilities({true, true, true});
  EXPECT_TRUE(recorder.recordShadowBatch(equalHalfSignMask()));

  WorldGenerationShadowBatchObservation diverged = equalHalfSignMask();
  diverged.kernel = WorldGenerationShadowKernelFamily::DenseUint32Add;
  diverged.verdict = WorldGenerationShadowCompareVerdict::Diverged;
  diverged.divergences = 2;
  EXPECT_TRUE(recorder.recordShadowBatch(diverged));
  EXPECT_FALSE(recorder.tryRoutingSample().ready);
}

TEST(WorldGenerationScaleMetricsTest, FillsDeliveryRecorderForObservedCost) {
  WorldGenerationScaleMetricsRecorder scale;
  scale.claimCapabilities({true, true, true});
  EXPECT_TRUE(scale.recordShadowBatch(equalHalfSignMask()));

  WorldGenerationDeliveryMetricsRecorder delivery;
  ASSERT_TRUE(scale.fillDeliveryRecorder(delivery));
  WorldGenerationDeliveryCost cost;
  ASSERT_TRUE(delivery.tryObservedDeliveryCost(cost));
  EXPECT_DOUBLE_EQ(cost.executionMs, 1.5);
  EXPECT_DOUBLE_EQ(cost.fenceMs, 0.2);
}
