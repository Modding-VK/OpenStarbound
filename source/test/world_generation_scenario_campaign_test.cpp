#include "StarWorldGenerationScenarioCampaign.hpp"
#include "StarWorldGenerationExecutionProfile.hpp"

#include "gtest/gtest.h"

using namespace Star;

namespace {
WorldGenerationShadowBatchObservation equalHalf(WorldGenerationShadowKernelFamily kernel) {
  WorldGenerationShadowBatchObservation o;
  o.kernel = kernel;
  o.verdict = WorldGenerationShadowCompareVerdict::Equal;
  o.compared = 8;
  o.cpuExecutionMs = 5;
  o.gpuExecutionMs = 2;
  o.transferMs = 0.5;
  o.fenceMs = 0.2;
  o.materializeMs = 0.1;
  return o;
}
}

TEST(WorldGenerationScenarioCampaignTest, RejectsInvalidSkipReasons) {
  WorldGenerationScenarioCampaignRecorder recorder;
  WorldGenerationHardwareCapabilities caps{false};
  EXPECT_FALSE(recorder.record(WorldGenerationValidationScenario::OpenSkyTerrain,
      WorldGenerationScenarioOutcome::SkippedMissingGpu, caps));
  EXPECT_TRUE(recorder.record(WorldGenerationValidationScenario::GpuAbsentSlowLost,
      WorldGenerationScenarioOutcome::SkippedMissingGpu, caps));
  EXPECT_TRUE(recorder.snapshot().attempted(WorldGenerationValidationScenario::GpuAbsentSlowLost));
  EXPECT_FALSE(recorder.snapshot().passed(WorldGenerationValidationScenario::GpuAbsentSlowLost));
}

TEST(WorldGenerationScenarioCampaignTest, CpuOnlyCoverageIgnoresGpuRows) {
  WorldGenerationScenarioCampaignRecorder recorder;
  WorldGenerationHardwareCapabilities caps{false};

  for (uint32_t i = 0; i < worldGenerationScenarioCount(); ++i) {
    auto scenario = static_cast<WorldGenerationValidationScenario>(i);
    auto entry = worldGenerationScenarioEntry(scenario);
    if (entry.requiresGpu) {
      EXPECT_TRUE(recorder.record(scenario, WorldGenerationScenarioOutcome::SkippedMissingGpu, caps));
      continue;
    }
    if (entry.requiresAssets) {
      EXPECT_TRUE(recorder.record(
          scenario, WorldGenerationScenarioOutcome::SkippedMissingAssets, caps));
      continue;
    }
    if (entry.requiresMultiplayer) {
      EXPECT_TRUE(recorder.record(
          scenario, WorldGenerationScenarioOutcome::SkippedMissingMultiplayer, caps));
      continue;
    }
    EXPECT_TRUE(recorder.record(scenario, WorldGenerationScenarioOutcome::Passed, caps));
  }

  EXPECT_TRUE(recorder.snapshot().cpuOnlyMatrixCovered());
  EXPECT_EQ(recorder.snapshot().failedCount(), 0u);
}

TEST(WorldGenerationScenarioCampaignTest, ScaleBridgeMarksGpuScenarios) {
  WorldGenerationScaleMetricsRecorder scale;
  scale.claimCapabilities({true, true, true});
  EXPECT_TRUE(scale.recordShadowBatch(equalHalf(WorldGenerationShadowKernelFamily::DenseUint32HalfSignMask)));
  EXPECT_TRUE(scale.recordShadowBatch(equalHalf(WorldGenerationShadowKernelFamily::DenseUint32HalfIsZero)));
  EXPECT_TRUE(scale.recordShadowBatch(equalHalf(WorldGenerationShadowKernelFamily::DenseUint32HalfIsNonzero)));

  WorldGenerationScenarioCampaignRecorder campaign;
  EXPECT_TRUE(campaign.recordGpuScenariosFromScale(scale.snapshot()));
  EXPECT_TRUE(campaign.snapshot().passed(WorldGenerationValidationScenario::MemoryPressureGpuKinds));
  EXPECT_TRUE(campaign.snapshot().passed(WorldGenerationValidationScenario::GpuAbsentSlowLost));
}

TEST(WorldGenerationScenarioCampaignTest, GovernorPrefersGpuFromScaleGain) {
  WorldGenerationScaleMetricsRecorder scale;
  scale.claimCapabilities({true, true, true});
  EXPECT_TRUE(scale.recordShadowBatch(equalHalf(WorldGenerationShadowKernelFamily::DenseUint32HalfSignMask)));

  WorldGenerationHardwareSelector selector;
  auto binding = bindWorldGenerationExecutionProfile("dynamic");
  WorldGenerationGovernorObservation observation;
  observation.demand = WorldGenerationGovernorDemand::Visible;
  observation.observedDeliveryMs = 10;
  observation.targetDeliveryMs = 20;

  auto advice = adviseWorldGenerationResourcesFromScale(
      binding, observation, selector, scale, WorldGenerationPerformanceMode::Dynamic);
  EXPECT_EQ(advice.preferredBackend, WorldGenerationHardwareRoutingBackend::Gpu);
  EXPECT_EQ(advice.action, WorldGenerationGovernorAction::PreferGpuBackend);
}
