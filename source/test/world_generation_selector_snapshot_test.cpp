#include "StarWorldGenerationSelectorSnapshot.hpp"
#include "StarRandom.hpp"
#include "gtest/gtest.h"

#include <array>
#include <cstring>
#include <future>

using namespace Star;

namespace {
uint32_t bits(float value) { uint32_t out; std::memcpy(&out, &value, sizeof(out)); return out; }
}

TEST(WorldGenerationSelectorSnapshotTest, RoundTripPreservesFieldsAndFloatBits) {
  for (float height : {0.0f, -0.0f, -2048.5f, 1024.25f})
    for (float commonality : {0.0f, -0.0f, 1.0f, 4.5f}) {
      TerrainSelectorParameters source(65536, height, UINT64_MAX, commonality);
      auto snapshot = WorldGenerationSelectorSnapshot::capture(source);
      auto result = snapshot.materialize();
      EXPECT_EQ(source.worldWidth, result.worldWidth);
      EXPECT_EQ(source.seed, result.seed);
      EXPECT_EQ(bits(source.baseHeight), bits(result.baseHeight));
      EXPECT_EQ(bits(source.commonality), bits(result.commonality));
    }
}

TEST(WorldGenerationSelectorSnapshotTest, OwnerConsumesLegacySeedAndWorkersDoNotConsumeMore) {
  RandomSource reference(918);
  auto expectedSeed = reference.randu64(), expectedNext = reference.randu64();
  Random::init(918);
  TerrainSelectorParameters owner;
  EXPECT_EQ(expectedSeed, owner.seed);
  owner.worldWidth = 64; owner.baseHeight = 16;
  auto snapshot = WorldGenerationSelectorSnapshot::capture(owner.withSeed(1234));
  for (int i = 0; i != 100; ++i) EXPECT_EQ(1234u, snapshot.materialize().seed);
  EXPECT_EQ(expectedNext, Random::randu64());
}

TEST(WorldGenerationSelectorSnapshotTest, SnapshotSurvivesSourceAndSupportsPrivateConcurrentMaterialization) {
  auto snapshot = [] { TerrainSelectorParameters source(64, -3.5f, 99, 0.75f); return WorldGenerationSelectorSnapshot::capture(source); }();
  std::array<std::future<bool>, 3> results;
  for (auto& result : results) result = std::async(std::launch::async, [&snapshot] {
    for (int i = 0; i != 1000; ++i) {
      auto copy = snapshot.materialize();
      if (copy.worldWidth != 64 || copy.baseHeight != -3.5f || copy.seed != 99 || copy.commonality != 0.75f) return false;
    }
    return true;
  });
  for (auto& result : results) EXPECT_TRUE(result.get());
}

TEST(WorldGenerationSelectorSnapshotTest, RegionCaptureMatchesLegacyParametersAndGlobalRng) {
  for (uint64_t seed : {uint64_t(0), uint64_t(1), uint64_t(917), UINT64_MAX}) {
    Random::init(918);
    TerrainSelectorParameters base;
    base.worldWidth = 8192; base.baseHeight = -37;
    auto terrain = base.withSeed(staticRandomU64(seed, "Terrain"));
    auto foreground = base.withSeed(staticRandomU64(seed, "ForegroundCaveSeed"));
    auto background = base.withSeed(staticRandomU64(seed, "BackgroundCave"));
    auto legacyNext = Random::randu64();

    Random::init(918);
    auto snapshots = WorldGenerationRegionSelectorSnapshots::capture(8192, -37, seed);
    EXPECT_EQ(terrain.toJson(), snapshots.terrain.materialize().toJson());
    EXPECT_EQ(foreground.toJson(), snapshots.foregroundCave.materialize().toJson());
    EXPECT_EQ(background.toJson(), snapshots.backgroundCave.materialize().toJson());
    EXPECT_EQ(legacyNext, Random::randu64());
  }
}
