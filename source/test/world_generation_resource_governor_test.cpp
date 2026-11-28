#include "gtest/gtest.h"

#include "StarWorldGenerationResourceGovernor.hpp"
#include "StarWorldGenerationHardwareSelection.hpp"
#include "StarWorldGenerationExecutionProfile.hpp"

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

// --- H5 scale governor advice (merged) ---

namespace {
WorldGenerationHardwareSelection cpuDeadline() {
  return {WorldGenerationComputeBackend::Cpu,
      WorldGenerationHardwareSelectionReason::DeadlinePrefersCpu, 8};
}
WorldGenerationHardwareSelection gpuChoice() {
  return {WorldGenerationComputeBackend::Gpu,
      WorldGenerationHardwareSelectionReason::LowerDeliveryCost, 4};
}
}

TEST(WorldGenerationResourceGovernorTest, DrainsUnderMemoryOrRenderPressure) {
  auto binding = bindWorldGenerationExecutionProfile("aggressive");
  WorldGenerationGovernorObservation observation;
  observation.demand = WorldGenerationGovernorDemand::Visible;
  observation.memoryPressure = true;
  auto advice = adviseWorldGenerationResources(binding, observation, cpuDeadline());
  EXPECT_EQ(advice.action, WorldGenerationGovernorAction::DrainWorkers);
  EXPECT_FALSE(advice.allowsGrowth);
}

TEST(WorldGenerationResourceGovernorTest, GrowsOnlyForThroughputProfilesWithVisibleLag) {
  auto aggressive = bindWorldGenerationExecutionProfile("aggressive");
  WorldGenerationGovernorObservation observation;
  observation.demand = WorldGenerationGovernorDemand::Visible;
  observation.observedDeliveryMs = 20;
  observation.targetDeliveryMs = 10;
  auto grow = adviseWorldGenerationResources(aggressive, observation, cpuDeadline());
  EXPECT_EQ(grow.action, WorldGenerationGovernorAction::PreferCpuBackend);

  // Deadline preference wins over growth when hardware already forced CPU.
  observation.renderPressure = false;
  observation.memoryPressure = false;
  WorldGenerationHardwareSelection plainCpu{WorldGenerationComputeBackend::Cpu,
      WorldGenerationHardwareSelectionReason::LowerDeliveryCost, 12};
  auto lagGrow = adviseWorldGenerationResources(aggressive, observation, plainCpu);
  EXPECT_EQ(lagGrow.action, WorldGenerationGovernorAction::GrowConcurrency);

  auto legacy = bindWorldGenerationExecutionProfile("legacy");
  auto hold = adviseWorldGenerationResources(legacy, observation, plainCpu);
  EXPECT_EQ(hold.action, WorldGenerationGovernorAction::Hold);
}

TEST(WorldGenerationResourceGovernorTest, PrefersGpuWhenHardwareSelectionDoes) {
  auto binding = bindWorldGenerationExecutionProfile("dynamic");
  WorldGenerationGovernorObservation observation;
  observation.demand = WorldGenerationGovernorDemand::Speculative;
  auto advice = adviseWorldGenerationResources(binding, observation, gpuChoice());
  EXPECT_EQ(advice.action, WorldGenerationGovernorAction::PreferGpuBackend);
  EXPECT_EQ(advice.preferredBackend, WorldGenerationComputeBackend::Gpu);
}
