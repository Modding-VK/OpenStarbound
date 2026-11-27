#include "StarWorldGenerationEquivalenceVersion.hpp"

#include "gtest/gtest.h"

using namespace Star;

TEST(WorldGenerationEquivalenceVersionTest, CompatibleVersionsRequireMatchingEpochs) {
  WorldGenerationEquivalenceVersion left{2, 1, 3};
  WorldGenerationEquivalenceVersion right{2, 9, 3};
  EXPECT_TRUE(left.compatibleWith(right));
  right.generationEpoch = 4;
  EXPECT_FALSE(left.compatibleWith(right));
}

TEST(WorldGenerationEquivalenceVersionTest, RejectsSilentNormalizationOfDivergence) {
  WorldGenerationEquivalenceVersion version{1, 1, 1};
  WorldGenerationEquivalenceFieldMask compared;
  compared.include(WorldGenerationEquivalenceDomain::Tiles);
  WorldGenerationEquivalenceFieldMask diverged;
  diverged.include(WorldGenerationEquivalenceDomain::Tiles);
  auto rejected = compareWorldGenerationEquivalence(version, version, compared, diverged, false);
  EXPECT_FALSE(rejected.equivalent);
  EXPECT_TRUE(rejected.versionCompatible);

  WorldGenerationEquivalenceFieldMask none;
  auto ok = compareWorldGenerationEquivalence(version, version, compared, none, false);
  EXPECT_TRUE(ok.equivalent);
}

TEST(WorldGenerationEquivalenceVersionTest, MathOrSeedChangeRequiresGenerationBump) {
  EXPECT_TRUE(worldGenerationRequiresGenerationEpochBump(true, false));
  EXPECT_TRUE(worldGenerationRequiresGenerationEpochBump(false, true));
  EXPECT_FALSE(worldGenerationRequiresGenerationEpochBump(false, false));
}
