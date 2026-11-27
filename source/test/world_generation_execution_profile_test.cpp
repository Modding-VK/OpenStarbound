#include "StarWorldGenerationExecutionProfile.hpp"

#include "gtest/gtest.h"

using namespace Star;

TEST(WorldGenerationExecutionProfileTest, MapsExistingModesExplicitlyWithoutRename) {
  auto legacy = bindWorldGenerationExecutionProfile("legacy");
  EXPECT_EQ(legacy.existingMode, WorldGenerationPerformanceMode::Legacy);
  EXPECT_EQ(legacy.migration, WorldGenerationProfileMigration::UnchangedExistingMode);
  EXPECT_FALSE(legacy.allowsGpuSelection);
  EXPECT_FALSE(legacy.allowsRegionalOwners);

  auto aggressive = bindWorldGenerationExecutionProfile("aggressive");
  EXPECT_EQ(aggressive.conceptual, WorldGenerationExecutionProfile::HighThroughput);
  EXPECT_EQ(aggressive.migration, WorldGenerationProfileMigration::ExplicitMapToConceptual);
  EXPECT_TRUE(aggressive.allowsGpuSelection);

  auto unknown = bindWorldGenerationExecutionProfile("balanced");
  EXPECT_EQ(unknown.migration, WorldGenerationProfileMigration::RejectedUnknown);
  EXPECT_FALSE(unknown.allowsGpuSelection);
}

TEST(WorldGenerationExecutionProfileTest, RegionalActivationNeedsCheckpointBoundary) {
  auto binding = bindWorldGenerationExecutionProfile(WorldGenerationPerformanceMode::Adaptive);
  binding.allowsRegionalOwners = true;
  EXPECT_FALSE(worldGenerationRegionalActivationAllowed(binding, false));
  EXPECT_TRUE(worldGenerationRegionalActivationAllowed(binding, true));
}

TEST(WorldGenerationExecutionProfileTest, FallbackPrefersCpuOnGpuFailureAndCheckpointOnPartialRegional) {
  auto binding = bindWorldGenerationExecutionProfile("dynamic");
  EXPECT_EQ(worldGenerationSelectFallback(binding, true, false),
      WorldGenerationFallbackKind::CpuComplete);
  EXPECT_EQ(worldGenerationSelectFallback(binding, false, true),
      WorldGenerationFallbackKind::CheckpointReloadRequired);
  EXPECT_EQ(worldGenerationSelectFallback(binding, false, false),
      WorldGenerationFallbackKind::CompatiblePath);
}
