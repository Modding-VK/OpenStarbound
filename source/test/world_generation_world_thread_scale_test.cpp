#include "StarWorldGenerationWorldThreadScale.hpp"

#include "gtest/gtest.h"

using namespace Star;

namespace {

WorldGenerationComputeInputIdentity makeScaleIdentity(uint64_t sequence,
    WorldGenerationComputeKernel kernel) {
  WorldGenerationComputeInputIdentity id;
  id.worldEpoch = 7;
  id.sequence = sequence;
  id.kernelVersion = 1;
  id.kernel = kernel;
  return id;
}

}

TEST(WorldGenerationWorldThreadScaleTest, RehearsalPumpNeverYieldsMeasuredGain) {
  WorldGenerationWorldThreadScaleRecorder recorder;
  WorldGenerationWorldThreadPumpObservation o;
  o.runtimeActive = true;
  o.rehearsal = true;
  o.lastPumpCompletions = 2;
  o.deliveryCostsCalibrated = true;
  o.cpuCost = {10, 0, 0, 0};
  o.gpuCost = {1, 0, 0, 0};
  EXPECT_TRUE(recorder.recordPump(o));
  EXPECT_TRUE(recorder.snapshot().worldThreadSignalsReady);
  EXPECT_EQ(recorder.snapshot().rehearsalTicks, 1u);
  EXPECT_EQ(recorder.snapshot().livePumpTicks, 0u);
  EXPECT_FALSE(recorder.snapshot().measuredGainFromLivePump);
  EXPECT_FALSE(recorder.tryRoutingSample().ready);

  WorldGenerationHardwareSelector selector;
  auto choice = selectFromWorldThreadScale(
      selector, recorder, WorldGenerationPerformanceMode::Dynamic);
  EXPECT_EQ(choice.backend, WorldGenerationHardwareRoutingBackend::Cpu);
  EXPECT_EQ(choice.reason, WorldGenerationHardwareSelectionReason::CpuOnlyCapability);
}

TEST(WorldGenerationWorldThreadScaleTest, LivePumpWithGainSelectsGpu) {
  WorldGenerationWorldThreadScaleRecorder recorder;
  WorldGenerationWorldThreadPumpObservation o;
  o.runtimeActive = true;
  o.rehearsal = false;
  o.lastPumpCompletions = 1;
  o.deliveryCostsCalibrated = true;
  // Desktop-shaped gain_pipeline sample (ratio ~20x, not invented from rehearsal).
  o.cpuCost = {20, 0.5, 0.2, 0.1};
  o.gpuCost = {1, 0.5, 0.2, 0.1};
  EXPECT_TRUE(recorder.recordPump(o));
  EXPECT_EQ(recorder.snapshot().livePumpTicks, 1u);
  EXPECT_TRUE(recorder.snapshot().measuredGainFromLivePump);

  auto sample = recorder.tryRoutingSample();
  ASSERT_TRUE(sample.ready);
  EXPECT_TRUE(sample.measuredDeliveryGain);

  WorldGenerationHardwareSelector selector;
  auto choice = selectFromWorldThreadScale(
      selector, recorder, WorldGenerationPerformanceMode::Dynamic);
  EXPECT_EQ(choice.backend, WorldGenerationHardwareRoutingBackend::Gpu);
  EXPECT_EQ(choice.reason, WorldGenerationHardwareSelectionReason::LowerDeliveryCost);
}

TEST(WorldGenerationWorldThreadScaleTest, InactivePumpDoesNotClaimSignals) {
  WorldGenerationWorldThreadScaleRecorder recorder;
  WorldGenerationWorldThreadPumpObservation o;
  o.runtimeActive = false;
  o.lastPumpCompletions = 0;
  EXPECT_TRUE(recorder.recordPump(o));
  EXPECT_FALSE(recorder.snapshot().worldThreadSignalsReady);
  EXPECT_FALSE(recorder.snapshot().sawActiveRuntime);
}

TEST(WorldGenerationWorldThreadScaleTest, RejectsCalibratedInvalidCosts) {
  WorldGenerationWorldThreadScaleRecorder recorder;
  WorldGenerationWorldThreadPumpObservation o;
  o.runtimeActive = true;
  o.rehearsal = false;
  o.deliveryCostsCalibrated = true;
  o.cpuCost = {-1, 0, 0, 0};
  o.gpuCost = {1, 0, 0, 0};
  EXPECT_FALSE(recorder.recordPump(o));
  EXPECT_EQ(recorder.snapshot().pumpTicks, 0u);
}

TEST(WorldGenerationWorldThreadScaleTest, ObserveRealComputeRuntimeRehearsal) {
  // INT tip 191cb6e4: observe the real WorldGenerationComputeRuntime pump.
  WorldGenerationComputeRuntime runtime;
  WorldGenerationComputeRuntime::Options options;
  options.enabled = true;
  options.gpuFenceRehearsal = true;
  options.deferredCompletion = true;
  runtime.startOnWorldThread(options);
  ASSERT_TRUE(runtime.active());
  ASSERT_TRUE(runtime.rehearsal());
  ASSERT_TRUE(runtime.service());

  auto ticket = runtime.service()->submit(
      makeScaleIdentity(4201, WorldGenerationComputeKernel::TerrainFieldMax2),
      List<uint32_t>{0x00200010u}, 0u, WorldGenerationComputeBackend::GpuOpenGlCompute);
  ASSERT_TRUE(ticket);
  EXPECT_EQ(1u, runtime.pumpUpdate());

  auto observation = observeWorldGenerationComputeRuntime(runtime);
  EXPECT_TRUE(observation.runtimeActive);
  EXPECT_TRUE(observation.rehearsal);
  EXPECT_EQ(observation.lastPumpCompletions, 1u);
  EXPECT_FALSE(observation.deliveryCostsCalibrated);

  WorldGenerationWorldThreadScaleRecorder recorder;
  EXPECT_TRUE(recorder.recordPump(observation));
  EXPECT_TRUE(recorder.snapshot().worldThreadSignalsReady);
  EXPECT_EQ(recorder.snapshot().rehearsalTicks, 1u);
  EXPECT_EQ(recorder.snapshot().livePumpTicks, 0u);
  EXPECT_FALSE(recorder.snapshot().measuredGainFromLivePump);
  EXPECT_FALSE(recorder.tryRoutingSample().ready);

  runtime.stopOnWorldThread();
}
