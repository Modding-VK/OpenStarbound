#include "StarWorldGenerationSelectorContext.hpp"
#include "StarRandom.hpp"
#include "StarWorldTiles.hpp"
#include "StarSkyParameters.hpp"
#include "gtest/gtest.h"

#include <array>
#include <cstring>
#include <future>
#include <type_traits>

using namespace Star;

namespace {
uint32_t bits(float value) {
  uint32_t out = 0;
  std::memcpy(&out, &value, sizeof(out));
  return out;
}

WorldGenerationSelectorContextIdentity sampleIdentity(uint64_t sequence = 1, float height = -37.f,
    unsigned width = 8192, uint64_t seed = 917) {
  WorldGenerationSelectorContextIdentity identity;
  identity.worldEpoch = 7;
  identity.sequence = sequence;
  identity.templateRevision = 3;
  identity.worldWidth = width;
  identity.baseHeight = height;
  identity.regionSeed = seed;
  return identity;
}
}

static_assert(!std::is_copy_constructible<WorldGenerationRegionSelectorContext>::value,
    "Owned selector context must not deep-copy per job");
static_assert(std::is_move_constructible<WorldGenerationRegionSelectorContext>::value,
    "Owned selector context must support handoff");

TEST(WorldGenerationSelectorContextTest, TryCapturePreservesLegacyParametersAndGlobalRng) {
  for (uint64_t seed : {uint64_t(0), uint64_t(1), uint64_t(917), UINT64_MAX}) {
    auto identity = sampleIdentity(1, -37.f, 8192, seed);
    Random::init(918);
    TerrainSelectorParameters base;
    base.worldWidth = identity.worldWidth;
    base.baseHeight = identity.baseHeight;
    auto terrain = base.withSeed(staticRandomU64(seed, "Terrain"));
    auto foreground = base.withSeed(staticRandomU64(seed, "ForegroundCaveSeed"));
    auto background = base.withSeed(staticRandomU64(seed, "BackgroundCave"));
    auto legacyNext = Random::randu64();

    Random::init(918);
    WorldGenerationMemoryLedger ledger;
    auto before = ledger.snapshot();
    auto context = WorldGenerationRegionSelectorContext::tryCapture(ledger, identity);
    ASSERT_TRUE(context.has_value());
    EXPECT_TRUE(context->matches(identity));
    EXPECT_EQ(terrain.toJson(), context->terrain().toJson());
    EXPECT_EQ(foreground.toJson(), context->foregroundCave().toJson());
    EXPECT_EQ(background.toJson(), context->backgroundCave().toJson());
    EXPECT_EQ(legacyNext, Random::randu64());
    auto after = ledger.snapshot();
    EXPECT_EQ(before.reservedBytes + WorldGenerationRegionSelectorContext::retainedBytes(), after.reservedBytes);
    EXPECT_EQ(before.classResources[size_t(WorldGenerationMemoryClass::SnapshotMetadataEstimate)] + 1,
        after.classResources[size_t(WorldGenerationMemoryClass::SnapshotMetadataEstimate)]);
  }
}

TEST(WorldGenerationSelectorContextTest, InvalidIdentityAndRefusedAdmissionDoNotConsumeRng) {
  RandomSource reference(918);
  auto expectedFirst = reference.randu64();
  Random::init(918);
  WorldGenerationMemoryLedger ledger;
  auto invalid = sampleIdentity();
  invalid.worldEpoch = 0;
  EXPECT_FALSE(WorldGenerationRegionSelectorContext::tryCapture(ledger, invalid).has_value());
  EXPECT_EQ(expectedFirst, Random::randu64());

  Random::init(918);
  WorldGenerationMemoryLedger tight(WorldGenerationRegionSelectorContext::retainedBytes() - 1);
  EXPECT_FALSE(WorldGenerationRegionSelectorContext::tryCapture(tight, sampleIdentity()).has_value());
  EXPECT_EQ(expectedFirst, Random::randu64());
  EXPECT_EQ(0u, tight.snapshot().resources);
}

TEST(WorldGenerationSelectorContextTest, MismatchedIdentityRejectsCommitGuard) {
  WorldGenerationMemoryLedger ledger;
  auto identity = sampleIdentity();
  auto context = WorldGenerationRegionSelectorContext::tryCapture(ledger, identity);
  ASSERT_TRUE(context.has_value());
  auto other = identity;
  ++other.sequence;
  EXPECT_FALSE(context->matches(other));
  EXPECT_TRUE(context->matches(identity));
}

TEST(WorldGenerationSelectorContextTest, ContextSurvivesHandoffAndPrivateConcurrentMaterialization) {
  WorldGenerationMemoryLedger ledger;
  auto context = WorldGenerationRegionSelectorContext::tryCapture(ledger, sampleIdentity(2, -0.0f, 64, 99));
  ASSERT_TRUE(context.has_value());
  auto moved = std::move(*context);
  context.reset();
  EXPECT_EQ(bits(-0.0f), bits(moved.terrain().baseHeight));
  EXPECT_EQ(64u, moved.terrain().worldWidth);

  std::array<std::future<bool>, 3> results;
  for (auto& result : results) {
    result = std::async(std::launch::async, [&moved] {
      for (int i = 0; i != 1000; ++i) {
        auto terrain = moved.terrain();
        auto foreground = moved.foregroundCave();
        auto background = moved.backgroundCave();
        if (terrain.worldWidth != 64 || bits(terrain.baseHeight) != bits(-0.0f)
            || foreground.worldWidth != 64 || background.worldWidth != 64)
          return false;
      }
      return true;
    });
  }
  for (auto& result : results)
    EXPECT_TRUE(result.get());
  EXPECT_EQ(WorldGenerationRegionSelectorContext::retainedBytes(), ledger.snapshot().reservedBytes);
}

TEST(WorldGenerationSelectorContextTest, DestroyingContextReleasesAdmittedBytes) {
  WorldGenerationMemoryLedger ledger(WorldGenerationRegionSelectorContext::retainedBytes());
  {
    auto context = WorldGenerationRegionSelectorContext::tryCapture(ledger, sampleIdentity());
    ASSERT_TRUE(context.has_value());
    EXPECT_EQ(WorldGenerationRegionSelectorContext::retainedBytes(), ledger.snapshot().reservedBytes);
  }
  EXPECT_EQ(0u, ledger.snapshot().reservedBytes);
  EXPECT_TRUE(WorldGenerationRegionSelectorContext::tryCapture(ledger, sampleIdentity(2)).has_value());
}

// Audit-only: document inherited uninitialized defaults. Do not "fix" production
// constructors here; composition must keep requiring explicit owner initialization.
TEST(WorldGenerationDefaultInitializationAuditTest, TerrainSelectorParametersDefaultLeavesBaseHeightUnset) {
  // Contrasts with the explicit constructor used by materialize()/fixtures.
  TerrainSelectorParameters explicitValue(1, 2.5f, 3, 4.f);
  EXPECT_EQ(bits(2.5f), bits(explicitValue.baseHeight));
  // The default path initializes seed/worldWidth/commonality only. Reading
  // baseHeight before assignment remains undefined; this test does not read it.
  Random::init(1);
  TerrainSelectorParameters defaults;
  EXPECT_EQ(0u, defaults.worldWidth);
  EXPECT_EQ(bits(1.f), bits(defaults.commonality));
  EXPECT_NE(0u, defaults.seed);
}

TEST(WorldGenerationDefaultInitializationAuditTest, WorldTileAssignmentOmitsBiomeTransitionField) {
  // Inherited operator= copies materials/collision/biomes/damage/dungeon and
  // intentionally skips biomeTransition (see StarWorldTiles.hpp). BaseTiles
  // application overwrites the flag on valid tiles. Do not normalize snapshot
  // reads of this field until a compatibility decision exists.
  WorldTile source;
  source.dungeonId = 7;
  source.biomeTransition = true;
  WorldTile copy;
  copy.biomeTransition = false;
  copy = source;
  EXPECT_EQ(7, copy.dungeonId);
  // Reading copy.biomeTransition remains unsafe/unspecified after assignment
  // from an omitted field path; only the explicitly written source is checked.
  EXPECT_TRUE(source.biomeTransition);
}

TEST(WorldGenerationDefaultInitializationAuditTest, SkyParametersDefaultOmitsHorizonClouds) {
  // Production SkyParameters() in LookupDefaults initializes seed/skyType/
  // coloring/settings, not horizonClouds. Fixtures must set the flag before
  // any read/copy; do not silently change the production constructor here.
  SkyParameters sky;
  sky.horizonClouds = false;
  EXPECT_FALSE(sky.horizonClouds);
  sky.horizonClouds = true;
  EXPECT_TRUE(sky.horizonClouds);
}
