#include "StarWorldGenerationPlatformMatrix.hpp"

#include "gtest/gtest.h"

using namespace Star;

TEST(WorldGenerationPlatformMatrixTest, SupportsDeclaredPlatformsAndDefersLinuxClang) {
  EXPECT_TRUE(worldGenerationPlatformInScope(WorldGenerationPlatform::Windows));
  EXPECT_TRUE(worldGenerationPlatformInScope(WorldGenerationPlatform::LinuxGcc));
  EXPECT_TRUE(worldGenerationPlatformInScope(WorldGenerationPlatform::MacOs));
  EXPECT_FALSE(worldGenerationPlatformInScope(WorldGenerationPlatform::LinuxClangDeferred));
  EXPECT_EQ(worldGenerationPlatformSupport(WorldGenerationPlatform::LinuxClangDeferred).status,
      WorldGenerationCompatStatus::Deferred);
  EXPECT_EQ(worldGenerationSupportedPlatformCount(), 4u);
}

TEST(WorldGenerationPlatformMatrixTest, HardwareComputeRemainsOptionalWithCpuFallback) {
  auto compute = worldGenerationSubsystemSupport(WorldGenerationSubsystem::HardwareCompute);
  EXPECT_EQ(compute.status, WorldGenerationCompatStatus::Optional);
  EXPECT_TRUE(worldGenerationSubsystemRequiresCpuFallback(
      WorldGenerationSubsystem::HardwareCompute));
  EXPECT_FALSE(worldGenerationSubsystemRequiresCpuFallback(
      WorldGenerationSubsystem::Save));
}
