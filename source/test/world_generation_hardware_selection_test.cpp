#include "StarWorldGenerationHardwareSelection.hpp"

#include "gtest/gtest.h"

using namespace Star;

namespace {
WorldGenerationDeliveryCost cpu(double executionMs, double materializeMs = 0) {
  return {executionMs, 0, 0, materializeMs};
}
WorldGenerationDeliveryCost gpu(double executionMs, double transferMs, double fenceMs = 0,
    double materializeMs = 0) {
  return {executionMs, transferMs, fenceMs, materializeMs};
}
}

TEST(WorldGenerationHardwareSelectionTest, ParsesExistingPerformanceModesWithoutRenaming) {
  EXPECT_EQ(parseWorldGenerationPerformanceMode("legacy"), WorldGenerationPerformanceMode::Legacy);
  EXPECT_EQ(parseWorldGenerationPerformanceMode("adaptive"), WorldGenerationPerformanceMode::Adaptive);
  EXPECT_EQ(parseWorldGenerationPerformanceMode("aggressive"), WorldGenerationPerformanceMode::Aggressive);
  EXPECT_EQ(parseWorldGenerationPerformanceMode("dynamic"), WorldGenerationPerformanceMode::Dynamic);
  EXPECT_EQ(parseWorldGenerationPerformanceMode("balanced"), WorldGenerationPerformanceMode::Unknown);
  EXPECT_FALSE(worldGenerationPerformanceModeAllowsGpu(WorldGenerationPerformanceMode::Legacy));
  EXPECT_TRUE(worldGenerationPerformanceModeAllowsGpu(WorldGenerationPerformanceMode::Adaptive));
}

TEST(WorldGenerationHardwareSelectionTest, AbsentGpuCapabilityAlwaysSelectsCpu) {
  WorldGenerationHardwareSelector selector;
  WorldGenerationHardwareCapabilities caps{false};
  auto choice = selector.select(caps, WorldGenerationPerformanceMode::Aggressive,
      cpu(20), gpu(1, 1));
  EXPECT_EQ(choice.backend, WorldGenerationHardwareRoutingBackend::Cpu);
  EXPECT_EQ(choice.reason, WorldGenerationHardwareSelectionReason::CpuOnlyCapability);
  EXPECT_DOUBLE_EQ(choice.selectedCostMs, 20);
}

TEST(WorldGenerationHardwareSelectionTest, LegacyForcesCpuEvenWhenGpuIsCheaper) {
  WorldGenerationHardwareSelector selector;
  WorldGenerationHardwareCapabilities caps{true};
  auto choice = selector.select(caps, WorldGenerationPerformanceMode::Legacy,
      cpu(50), gpu(2, 2));
  EXPECT_EQ(choice.backend, WorldGenerationHardwareRoutingBackend::Cpu);
  EXPECT_EQ(choice.reason, WorldGenerationHardwareSelectionReason::LegacyForcesCpu);
}

TEST(WorldGenerationHardwareSelectionTest, SelectsGpuWhenFullDeliveryCostIsLower) {
  WorldGenerationHardwareSelector selector;
  WorldGenerationHardwareCapabilities caps{true};
  auto choice = selector.select(caps, WorldGenerationPerformanceMode::Adaptive,
      cpu(40), gpu(5, 4, 1, 2), {0, 0});
  EXPECT_EQ(choice.backend, WorldGenerationHardwareRoutingBackend::Gpu);
  EXPECT_EQ(choice.reason, WorldGenerationHardwareSelectionReason::LowerDeliveryCost);
  EXPECT_DOUBLE_EQ(choice.selectedCostMs, 12);
}

TEST(WorldGenerationHardwareSelectionTest, HysteresisHoldsUntilMarginBeaten) {
  WorldGenerationHardwareSelector selector(WorldGenerationHardwareRoutingBackend::Cpu);
  WorldGenerationHardwareCapabilities caps{true};
  WorldGenerationHardwareSelectionLimits limits{5, 0};

  auto hold = selector.select(caps, WorldGenerationPerformanceMode::Dynamic,
      cpu(10), gpu(8, 0), limits);
  EXPECT_EQ(hold.backend, WorldGenerationHardwareRoutingBackend::Cpu);
  EXPECT_EQ(hold.reason, WorldGenerationHardwareSelectionReason::HysteresisHold);

  auto switchBackend = selector.select(caps, WorldGenerationPerformanceMode::Dynamic,
      cpu(10), gpu(4, 0), limits);
  EXPECT_EQ(switchBackend.backend, WorldGenerationHardwareRoutingBackend::Gpu);
  EXPECT_EQ(switchBackend.reason, WorldGenerationHardwareSelectionReason::LowerDeliveryCost);
  EXPECT_DOUBLE_EQ(switchBackend.selectedCostMs, 4);
}

TEST(WorldGenerationHardwareSelectionTest, DeadlinePrefersCpuWhenGpuMissesIt) {
  WorldGenerationHardwareSelector selector;
  WorldGenerationHardwareCapabilities caps{true};
  auto choice = selector.select(caps, WorldGenerationPerformanceMode::Aggressive,
      cpu(8), gpu(2, 20), {0, 10});
  EXPECT_EQ(choice.backend, WorldGenerationHardwareRoutingBackend::Cpu);
  EXPECT_EQ(choice.reason, WorldGenerationHardwareSelectionReason::DeadlinePrefersCpu);
  EXPECT_DOUBLE_EQ(choice.selectedCostMs, 8);
}

TEST(WorldGenerationHardwareSelectionTest, InvalidCostsDoNotFlipTheStickyBackend) {
  WorldGenerationHardwareSelector selector(WorldGenerationHardwareRoutingBackend::Gpu);
  WorldGenerationHardwareCapabilities caps{true};
  WorldGenerationDeliveryCost bad{-1, 0, 0, 0};
  auto choice = selector.select(caps, WorldGenerationPerformanceMode::Adaptive,
      cpu(3), bad);
  EXPECT_EQ(choice.backend, WorldGenerationHardwareRoutingBackend::Gpu);
  EXPECT_EQ(choice.reason, WorldGenerationHardwareSelectionReason::InvalidCost);
  EXPECT_EQ(selector.current(), WorldGenerationHardwareRoutingBackend::Gpu);
}
