#include "StarWorldGenerationAdoptionGates.hpp"
#include "StarWorldGenerationScaleMetrics.hpp"

#include "gtest/gtest.h"

using namespace Star;

namespace {
WorldGenerationShadowBatchObservation equalKernel(WorldGenerationShadowKernelFamily kernel) {
  WorldGenerationShadowBatchObservation o;
  o.kernel = kernel;
  o.verdict = WorldGenerationShadowCompareVerdict::Equal;
  o.compared = 8;
  o.cpuExecutionMs = 4.0;
  o.gpuExecutionMs = 1.5;
  o.transferMs = 0.4;
  o.fenceMs = 0.2;
  o.materializeMs = 0.1;
  return o;
}
}

TEST(WorldGenerationAdoptionGatesTest, PartialEvidenceNeverAdopts) {
  WorldGenerationAdoptionEvidence evidence;
  evidence.knownLimitsDocumented = true;
  evidence.cpuFallbackComplete = true;
  auto decision = evaluateWorldGenerationAdoption(evidence);
  EXPECT_FALSE(decision.adopt);
  EXPECT_FALSE(decision.coverageComplete);
  EXPECT_TRUE(decision.has(WorldGenerationAdoptionGate::KnownLimitsDocumented));
  EXPECT_FALSE(decision.has(WorldGenerationAdoptionGate::EquivalentBenchmark));
}

TEST(WorldGenerationAdoptionGatesTest, FullCpuGatesAdoptWithoutAuthoritativeGpu) {
  WorldGenerationAdoptionEvidence evidence;
  evidence.invariantsPassed = true;
  evidence.outputComparisonPassed = true;
  evidence.equivalentBenchmarkPassed = true;
  evidence.memoryBounded = true;
  evidence.failureRecoveryCovered = true;
  evidence.knownLimitsDocumented = true;
  evidence.cpuFallbackComplete = true;
  auto decision = evaluateWorldGenerationAdoption(evidence);
  EXPECT_TRUE(decision.coverageComplete);
  EXPECT_TRUE(decision.adopt);
}

TEST(WorldGenerationAdoptionGatesTest, AuthoritativeGpuAlsoNeedsMeasuredGain) {
  WorldGenerationAdoptionEvidence evidence;
  evidence.invariantsPassed = true;
  evidence.outputComparisonPassed = true;
  evidence.equivalentBenchmarkPassed = true;
  evidence.memoryBounded = true;
  evidence.failureRecoveryCovered = true;
  evidence.knownLimitsDocumented = true;
  evidence.cpuFallbackComplete = true;
  evidence.gpuBackendAuthoritative = true;
  evidence.measuredDeliveryGain = false;
  EXPECT_FALSE(evaluateWorldGenerationAdoption(evidence).adopt);
  evidence.measuredDeliveryGain = true;
  EXPECT_TRUE(evaluateWorldGenerationAdoption(evidence).adopt);
}

TEST(WorldGenerationAdoptionGatesTest, PartialH5HelperKeepsAdoptionClosed) {
  WorldGenerationDeliveryMetricsRecorder recorder;
  WorldGenerationResourceMetric resources;
  resources.coverageComplete = true;
  recorder.recordResources(resources);
  auto evidence = worldGenerationAdoptionFromPartialH5(
      recorder.snapshot(), WorldGenerationHardwareCapabilities{false}, true);
  EXPECT_TRUE(evidence.knownLimitsDocumented);
  EXPECT_FALSE(evaluateWorldGenerationAdoption(evidence).adopt);
  EXPECT_GE(worldGenerationRunnableCpuScenarioCount(), 8u);
}

TEST(WorldGenerationAdoptionGatesTest, ScaleMetricsFillsComparisonAndGainWithoutAdopt) {
  WorldGenerationScaleMetricsRecorder scale;
  scale.claimCapabilities({true, true, true});
  EXPECT_TRUE(scale.recordShadowBatch(equalKernel(WorldGenerationShadowKernelFamily::DenseUint32HalfSignMask)));
  EXPECT_TRUE(scale.recordShadowBatch(equalKernel(WorldGenerationShadowKernelFamily::DenseUint32HalfIsZero)));
  EXPECT_TRUE(scale.recordShadowBatch(equalKernel(WorldGenerationShadowKernelFamily::DenseUint32HalfIsNonzero)));

  WorldGenerationDeliveryMetricsRecorder delivery;
  ASSERT_TRUE(scale.fillDeliveryRecorder(delivery));

  auto evidence = worldGenerationAdoptionFromScaleMetrics(
      scale.snapshot(), delivery.snapshot(), true);
  EXPECT_TRUE(evidence.outputComparisonPassed);
  EXPECT_TRUE(evidence.measuredDeliveryGain);
  EXPECT_TRUE(evidence.memoryBounded);
  EXPECT_FALSE(evidence.gpuBackendAuthoritative);
  EXPECT_FALSE(evidence.equivalentBenchmarkPassed);
  EXPECT_FALSE(evaluateWorldGenerationAdoption(evidence).adopt);

  auto summary = summarizeWorldGenerationScaleGates(evidence);
  EXPECT_TRUE(summary.shadowOutputComparison);
  EXPECT_TRUE(summary.measuredDeliveryGain);
  EXPECT_TRUE(summary.authoritativeGpuBlocked);
  EXPECT_FALSE(summary.adopt);
}

TEST(WorldGenerationAdoptionGatesTest, ScaleMetricsRequiresHalfPredicateCoverage) {
  WorldGenerationScaleMetricsRecorder scale;
  scale.claimCapabilities({true, true, true});
  EXPECT_TRUE(scale.recordShadowBatch(equalKernel(WorldGenerationShadowKernelFamily::DenseUint32Xor)));

  WorldGenerationDeliveryMetricsRecorder delivery;
  ASSERT_TRUE(scale.fillDeliveryRecorder(delivery));
  auto evidence = worldGenerationAdoptionFromScaleMetrics(
      scale.snapshot(), delivery.snapshot(), true);
  EXPECT_FALSE(evidence.outputComparisonPassed);
  EXPECT_FALSE(evidence.measuredDeliveryGain);
}
