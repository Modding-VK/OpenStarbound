#include "StarWorldGenerationScaleEvidence.hpp"
#include "StarWorldGenerationDeliveryMetrics.hpp"

#include "gtest/gtest.h"

using namespace Star;

namespace {
WorldGenerationShadowBatchObservation equalHalf(WorldGenerationShadowKernelFamily kernel) {
  WorldGenerationShadowBatchObservation o;
  o.kernel = kernel;
  o.verdict = WorldGenerationShadowCompareVerdict::Equal;
  o.compared = 8;
  o.cpuExecutionMs = 4;
  o.gpuExecutionMs = 1.5;
  o.transferMs = 0.3;
  o.fenceMs = 0.1;
  o.materializeMs = 0.1;
  return o;
}

void fillCpuOnlyCampaign(WorldGenerationScenarioCampaignRecorder& campaign) {
  WorldGenerationHardwareCapabilities caps{false};
  for (uint32_t i = 0; i < worldGenerationScenarioCount(); ++i) {
    auto scenario = static_cast<WorldGenerationValidationScenario>(i);
    auto entry = worldGenerationScenarioEntry(scenario);
    if (entry.requiresGpu) {
      campaign.record(scenario, WorldGenerationScenarioOutcome::SkippedMissingGpu, caps);
    } else if (entry.requiresAssets) {
      campaign.record(scenario, WorldGenerationScenarioOutcome::SkippedMissingAssets, caps);
    } else if (entry.requiresMultiplayer) {
      campaign.record(scenario, WorldGenerationScenarioOutcome::SkippedMissingMultiplayer, caps);
    } else {
      campaign.record(scenario, WorldGenerationScenarioOutcome::Passed, caps);
    }
  }
}
}

TEST(WorldGenerationScaleEvidenceTest, RejectsZeroGenerationEpoch) {
  WorldGenerationScaleMetricsRecorder scale;
  WorldGenerationScenarioCampaignRecorder campaign;
  fillCpuOnlyCampaign(campaign);
  WorldGenerationDeliveryMetricsRecorder delivery;
  auto equivalence = worldGenerationScaleEvidenceEquivalence(0, 1, 1);
  auto bundle = buildWorldGenerationScaleEvidenceFromRecorders(
      scale, campaign, delivery.snapshot(), true, equivalence);
  EXPECT_FALSE(bundle.readyForIntegratorReview);
}

TEST(WorldGenerationScaleEvidenceTest, ReadyWhenCpuCampaignAndGatesBlockGpuAdopt) {
  WorldGenerationScaleMetricsRecorder scale;
  WorldGenerationScenarioCampaignRecorder campaign;
  fillCpuOnlyCampaign(campaign);
  WorldGenerationDeliveryMetricsRecorder delivery;
  auto equivalence = worldGenerationScaleEvidenceEquivalence(1, 1, 1);
  auto bundle = buildWorldGenerationScaleEvidenceFromRecorders(
      scale, campaign, delivery.snapshot(), true, equivalence);
  EXPECT_TRUE(bundle.campaignCpuOnlyCovered);
  EXPECT_TRUE(bundle.gatesAuthoritativeGpuBlocked);
  EXPECT_FALSE(bundle.gatesAdopt);
  EXPECT_TRUE(bundle.readyForIntegratorReview);
  EXPECT_EQ(bundle.platformStatus, WorldGenerationCompatStatus::Supported);
  EXPECT_EQ(bundle.hardwareComputeStatus, WorldGenerationCompatStatus::Optional);
}

TEST(WorldGenerationScaleEvidenceTest, ShadowComparisonAlsoUnlocksReview) {
  WorldGenerationScaleMetricsRecorder scale;
  scale.claimCapabilities({true, true, true});
  EXPECT_TRUE(scale.recordShadowBatch(equalHalf(WorldGenerationShadowKernelFamily::DenseUint32HalfSignMask)));
  EXPECT_TRUE(scale.recordShadowBatch(equalHalf(WorldGenerationShadowKernelFamily::DenseUint32HalfIsZero)));
  EXPECT_TRUE(scale.recordShadowBatch(equalHalf(WorldGenerationShadowKernelFamily::DenseUint32HalfIsNonzero)));

  WorldGenerationScenarioCampaignRecorder campaign;
  EXPECT_TRUE(campaign.recordGpuScenariosFromScale(scale.snapshot()));

  WorldGenerationDeliveryMetricsRecorder delivery;
  ASSERT_TRUE(scale.fillDeliveryRecorder(delivery));
  auto equivalence = worldGenerationScaleEvidenceEquivalence(2, 1, 1);
  auto bundle = buildWorldGenerationScaleEvidenceFromRecorders(
      scale, campaign, delivery.snapshot(), true, equivalence);
  EXPECT_TRUE(bundle.shadowOutputComparison);
  EXPECT_TRUE(bundle.scaleMeasuredGain);
  EXPECT_FALSE(bundle.gatesAdopt);
  EXPECT_TRUE(bundle.readyForIntegratorReview);
}

TEST(WorldGenerationScaleEvidenceTest, CampaignFailureBlocksReview) {
  WorldGenerationScaleMetricsRecorder scale;
  WorldGenerationScenarioCampaignRecorder campaign;
  WorldGenerationHardwareCapabilities caps{false};
  EXPECT_TRUE(campaign.record(WorldGenerationValidationScenario::OpenSkyTerrain,
      WorldGenerationScenarioOutcome::Failed, caps));
  WorldGenerationDeliveryMetricsRecorder delivery;
  auto equivalence = worldGenerationScaleEvidenceEquivalence(1, 1, 1);
  auto bundle = buildWorldGenerationScaleEvidenceFromRecorders(
      scale, campaign, delivery.snapshot(), true, equivalence);
  EXPECT_TRUE(bundle.campaignHasFailures);
  EXPECT_FALSE(bundle.readyForIntegratorReview);
}

TEST(WorldGenerationScaleEvidenceTest, ProfileAdviceNeverSilentRename) {
  WorldGenerationScaleMetricsRecorder scale;
  WorldGenerationScenarioCampaignRecorder campaign;
  fillCpuOnlyCampaign(campaign);
  WorldGenerationDeliveryMetricsRecorder delivery;
  auto equivalence = worldGenerationScaleEvidenceEquivalence(1, 1, 1);
  auto bundle = buildWorldGenerationScaleEvidenceFromRecorders(
      scale, campaign, delivery.snapshot(), true, equivalence);

  auto legacy = recommendWorldGenerationProfileFromScaleEvidence(
      bundle, WorldGenerationPerformanceMode::Legacy);
  EXPECT_EQ(legacy.advice, WorldGenerationScaleProfileAdvice::KeepExisting);
  EXPECT_TRUE(legacy.requiresExplicitUserOptIn);
  EXPECT_FALSE(legacy.allowsGpuSelection);

  auto unknown = recommendWorldGenerationProfileFromScaleEvidence(
      bundle, WorldGenerationPerformanceMode::Unknown);
  EXPECT_EQ(unknown.advice, WorldGenerationScaleProfileAdvice::RejectUnknown);

  auto aggressive = recommendWorldGenerationProfileFromScaleEvidence(
      bundle, WorldGenerationPerformanceMode::Aggressive);
  EXPECT_EQ(aggressive.advice, WorldGenerationScaleProfileAdvice::PreferHighThroughput);
  EXPECT_EQ(aggressive.suggested, WorldGenerationExecutionProfile::HighThroughput);
  EXPECT_TRUE(aggressive.requiresExplicitUserOptIn);
}

TEST(WorldGenerationScaleEvidenceTest, TipPrefixParsesEightHexDigits) {
  EXPECT_EQ(worldGenerationTipPrefixFromHex8("da430a98"), 0xda430a98u);
  EXPECT_EQ(worldGenerationTipPrefixFromHex8("DA430A98"), 0xda430a98u);
  EXPECT_EQ(worldGenerationTipPrefixFromHex8("short"), 0u);
  EXPECT_EQ(worldGenerationTipPrefixFromHex8(nullptr), 0u);
}

TEST(WorldGenerationScaleEvidenceTest, DesktopAttestationGatesReviewWhenRecorded) {
  WorldGenerationScaleMetricsRecorder scale;
  WorldGenerationScenarioCampaignRecorder campaign;
  fillCpuOnlyCampaign(campaign);
  WorldGenerationDeliveryMetricsRecorder delivery;
  auto equivalence = worldGenerationScaleEvidenceEquivalence(1, 1, 1);
  auto bundle = buildWorldGenerationScaleEvidenceFromRecorders(
      scale, campaign, delivery.snapshot(), true, equivalence);
  EXPECT_TRUE(bundle.readyForIntegratorReview);
  EXPECT_FALSE(bundle.desktopAttested);

  uint32_t tip = worldGenerationTipPrefixFromHex8("da430a98");
  WorldGenerationDesktopProbeAttestation bad;
  bad.recorded = true;
  bad.tipPrefix = tip;
  bad.testsPassed = 45;
  bad.testsExpected = 46;
  EXPECT_FALSE(applyWorldGenerationDesktopAttestation(bundle, bad, tip));
  EXPECT_FALSE(bundle.readyForIntegratorReview);

  bundle = buildWorldGenerationScaleEvidenceFromRecorders(
      scale, campaign, delivery.snapshot(), true, equivalence);
  WorldGenerationDesktopProbeAttestation ok;
  ok.recorded = true;
  ok.tipPrefix = tip;
  ok.testsPassed = 46;
  ok.testsExpected = 46;
  ok.whatIfBootstrapOk = true;
  EXPECT_TRUE(applyWorldGenerationDesktopAttestation(bundle, ok, tip));
  EXPECT_TRUE(bundle.desktopAttested);
  EXPECT_TRUE(bundle.readyForIntegratorReview);
}

TEST(WorldGenerationScaleEvidenceTest, MeasuredGainPrefersDynamicBias) {
  WorldGenerationScaleMetricsRecorder scale;
  scale.claimCapabilities({true, true, true});
  EXPECT_TRUE(scale.recordShadowBatch(equalHalf(WorldGenerationShadowKernelFamily::DenseUint32HalfSignMask)));
  EXPECT_TRUE(scale.recordShadowBatch(equalHalf(WorldGenerationShadowKernelFamily::DenseUint32HalfIsZero)));
  EXPECT_TRUE(scale.recordShadowBatch(equalHalf(WorldGenerationShadowKernelFamily::DenseUint32HalfIsNonzero)));
  WorldGenerationScenarioCampaignRecorder campaign;
  EXPECT_TRUE(campaign.recordGpuScenariosFromScale(scale.snapshot()));
  WorldGenerationDeliveryMetricsRecorder delivery;
  ASSERT_TRUE(scale.fillDeliveryRecorder(delivery));
  auto equivalence = worldGenerationScaleEvidenceEquivalence(2, 1, 1);
  auto bundle = buildWorldGenerationScaleEvidenceFromRecorders(
      scale, campaign, delivery.snapshot(), true, equivalence);

  auto rec = recommendWorldGenerationProfileFromScaleEvidence(
      bundle, WorldGenerationPerformanceMode::Adaptive);
  EXPECT_EQ(rec.advice, WorldGenerationScaleProfileAdvice::PreferDynamicBias);
  EXPECT_EQ(rec.suggested, WorldGenerationExecutionProfile::DynamicBias);
  EXPECT_TRUE(rec.allowsGpuSelection);
  EXPECT_TRUE(rec.requiresExplicitUserOptIn);
}
