#include "StarWorldGenerationAdoptionGates.hpp"

#include "gtest/gtest.h"

using namespace Star;

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
