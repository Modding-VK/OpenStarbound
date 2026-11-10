#include "StarRandom.hpp"
#include "gtest/gtest.h"

using namespace Star;

TEST(AutomaticRandomSeedTest, AutomaticInitializationAndEntropyUseDefinedArithmetic) {
  // ASan/UBSan exercise the real automatic seed path; no timestamp/RNG stubs.
  Random::init();
  Random::addEntropy();
  RandomSource local;
  local.init();
  local.addEntropy();
  auto bytes = local.randBytes(16);
  EXPECT_EQ(bytes.size(), 16u);
}

TEST(AutomaticRandomSeedTest, ExplicitGlobalSeedAndExplicitEntropyKeepTheLegacySequence) {
  Random::init(31415926);
  for (uint32_t expected : {2950892229u, 1418047276u, 3790079132u, 445970691u, 2728181679u})
    EXPECT_EQ(Random::randu32(), expected);
  Random::addEntropy(27182818);
  for (uint32_t expected : {3255590103u, 678168874u, 3814633989u, 4070190392u, 265077625u})
    EXPECT_EQ(Random::randu32(), expected);
}

TEST(AutomaticRandomSeedTest, DefaultSourceStillConsumesExactlyOneSeedFromTheExplicitGlobalSequence) {
  RandomSource reference(9);
  RandomSource expected(reference.randu64());
  Random::init(9);
  RandomSource actual;
  EXPECT_EQ(Random::randu64(), reference.randu64());
  for (unsigned i = 0; i < 32; ++i) EXPECT_EQ(actual.randu64(), expected.randu64());
}
