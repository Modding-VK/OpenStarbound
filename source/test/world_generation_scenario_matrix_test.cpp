#include "StarWorldGenerationScenarioMatrix.hpp"

#include "gtest/gtest.h"

using namespace Star;

TEST(WorldGenerationScenarioMatrixTest, EnumeratesAllDocumentedScenarios) {
  EXPECT_EQ(worldGenerationScenarioCount(), 11u);
  auto sky = worldGenerationScenarioEntry(WorldGenerationValidationScenario::OpenSkyTerrain);
  EXPECT_EQ(sky.focus, WorldGenerationScenarioFocus::Throughput);
  EXPECT_FALSE(sky.requiresGpu);

  auto gpuLost = worldGenerationScenarioEntry(WorldGenerationValidationScenario::GpuAbsentSlowLost);
  EXPECT_TRUE(gpuLost.requiresGpu);
  EXPECT_EQ(gpuLost.focus, WorldGenerationScenarioFocus::CpuFallback);
}

TEST(WorldGenerationScenarioMatrixTest, IdentifiesCpuRunnableSubset) {
  EXPECT_TRUE(worldGenerationScenarioRunnableWithoutGpu(
      WorldGenerationValidationScenario::WorkersAndWorlds));
  EXPECT_FALSE(worldGenerationScenarioRunnableWithoutGpu(
      WorldGenerationValidationScenario::MemoryPressureGpuKinds));
}
