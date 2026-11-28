#include "StarWorldGenerationCompositeContext.hpp"
#include "StarRandom.hpp"
#include "StarWorldTemplateTypes.hpp"
#include "gtest/gtest.h"

#include <array>
#include <cstring>
#include <future>
#include <type_traits>

namespace Star {
struct WorldGenerationCompositeContextTestAccess {
  static WorldTemplatePtr planet(Json config = JsonObject{}, Vec2U size = {128, 128}) {
    auto result = WorldTemplatePtr(new WorldTemplate(WorldTemplate::LookupFixtureTag{}));
    result->m_geometry = WorldGeometry(size);
    result->m_templateConfig = std::move(config);
    result->m_customTerrainBlendSize = 10;
    result->m_customTerrainBlendWeight = 8;
    return result;
  }
};
}

using namespace Star;

namespace {
using Access = WorldGenerationCompositeContextTestAccess;
uint32_t bits(float value) {
  uint32_t out = 0;
  std::memcpy(&out, &value, sizeof(out));
  return out;
}

WorldGenerationSelectorContextIdentity selectorIdentity(uint64_t revision, unsigned width = 128,
    float height = -37.f, uint64_t seed = 917, uint64_t sequence = 1) {
  WorldGenerationSelectorContextIdentity identity;
  identity.worldEpoch = 1;
  identity.sequence = sequence;
  identity.templateRevision = revision;
  identity.worldWidth = width;
  identity.baseHeight = height;
  identity.regionSeed = seed;
  return identity;
}

WorldGenerationBaseTilesIdentity baseTilesIdentity(uint64_t revision, Vec2U worldSize = {128, 128}) {
  WorldGenerationBaseTilesIdentity identity;
  identity.task = {1, 1, {0, 0}, {0, 0}, WorldGenerationTaskStage::BaseTiles};
  identity.tileRevision = 5;
  identity.templateRevision = revision;
  identity.worldSize = worldSize;
  identity.tileRegion = RectI(0, 0, int(std::min<unsigned>(WorldSectorSize, worldSize[0])),
      int(std::min<unsigned>(WorldSectorSize, worldSize[1])));
  return identity;
}
}

static_assert(!std::is_copy_constructible<WorldGenerationCompositeContext>::value,
    "Composite capture must not deep-copy per job");
static_assert(std::is_move_constructible<WorldGenerationCompositeContext>::value,
    "Composite capture must support handoff");

TEST(WorldGenerationCompositeContextTest, TryCaptureComposesSelectorsStaticInputsAndBaseTilesIdentity) {
  auto world = Access::planet(JsonObject{{"biomeTransitionThreshold", 0.75}});
  world->addCustomTerrainRegion(PolyF(RectF::withSize({10, 10}, {10, 10})));
  auto revision = world->generationRevision();
  auto selectors = selectorIdentity(revision);
  auto baseTiles = baseTilesIdentity(revision);

  Random::init(918);
  TerrainSelectorParameters base;
  base.worldWidth = selectors.worldWidth;
  base.baseHeight = selectors.baseHeight;
  auto expectedTerrain = base.withSeed(staticRandomU64(selectors.regionSeed, "Terrain"));
  auto expectedNext = Random::randu64();

  Random::init(918);
  WorldGenerationMemoryLedger ledger;
  auto before = ledger.snapshot();
  auto context = WorldGenerationCompositeContext::tryCapture(ledger, selectors, baseTiles, *world);
  ASSERT_TRUE(context.has_value());
  EXPECT_TRUE(context->matches(selectors, baseTiles));
  EXPECT_EQ(expectedTerrain.toJson(), context->terrain().toJson());
  EXPECT_EQ(bits(0.75f), bits(context->configFloat("biomeTransitionThreshold", 0)));
  EXPECT_EQ(1u, context->staticInputs().customTerrainRegionCount());
  EXPECT_EQ(revision, context->staticInputs().templateRevision());
  EXPECT_EQ(expectedNext, Random::randu64());
  EXPECT_GE(ledger.snapshot().reservedBytes, before.reservedBytes + WorldGenerationRegionSelectorContext::retainedBytes());

  world->addCustomSpaceRegion(PolyF(RectF::withSize({40, 40}, {5, 5})));
  EXPECT_EQ(bits(0.75f), bits(context->configFloat("biomeTransitionThreshold", 0)));
  EXPECT_EQ(1u, context->staticInputs().customTerrainRegionCount());
}

TEST(WorldGenerationCompositeContextTest, MismatchedOrStaleIdentityRefusesWithoutConsumingRng) {
  auto world = Access::planet();
  auto revision = world->generationRevision();
  RandomSource reference(918);
  auto expectedFirst = reference.randu64();

  Random::init(918);
  WorldGenerationMemoryLedger ledger;
  auto badRevision = selectorIdentity(revision + 1);
  EXPECT_FALSE(WorldGenerationCompositeContext::tryCapture(ledger, badRevision, baseTilesIdentity(revision), *world));
  EXPECT_EQ(expectedFirst, Random::randu64());

  Random::init(918);
  auto widthMismatch = selectorIdentity(revision, 64);
  EXPECT_FALSE(WorldGenerationCompositeContext::tryCapture(ledger, widthMismatch, baseTilesIdentity(revision), *world));
  EXPECT_EQ(expectedFirst, Random::randu64());

  Random::init(918);
  world->addCustomTerrainRegion(PolyF(RectF::withSize({0, 0}, {4, 4})));
  EXPECT_FALSE(WorldGenerationCompositeContext::tryCapture(ledger, selectorIdentity(revision),
      baseTilesIdentity(revision), *world));
  EXPECT_EQ(expectedFirst, Random::randu64());
  EXPECT_EQ(0u, ledger.snapshot().resources);
}

TEST(WorldGenerationCompositeContextTest, RefusedSelectorAdmissionLeavesStaticInputsUncaptured) {
  auto world = Access::planet(JsonObject{{"biomeTransitionThreshold", 0.5}});
  auto revision = world->generationRevision();
  RandomSource reference(918);
  auto expectedFirst = reference.randu64();
  Random::init(918);
  WorldGenerationMemoryLedger tight(WorldGenerationRegionSelectorContext::retainedBytes() - 1);
  EXPECT_FALSE(WorldGenerationCompositeContext::tryCapture(tight, selectorIdentity(revision),
      baseTilesIdentity(revision), *world));
  EXPECT_EQ(expectedFirst, Random::randu64());
  EXPECT_EQ(0u, tight.snapshot().resources);
}

TEST(WorldGenerationCompositeContextTest, SurvivesSourceDestructionAndPrivateConcurrentReads) {
  WorldGenerationMemoryLedger ledger;
  std::optional<WorldGenerationCompositeContext> context;
  {
    auto world = Access::planet(JsonObject{{"surfaceCaveAttenuationDist", 12}});
    world->addCustomTerrainRegion(PolyF(RectF::withSize({0, 10}, {10, 10})));
    auto revision = world->generationRevision();
    context = WorldGenerationCompositeContext::tryCapture(ledger, selectorIdentity(revision, 128, -0.0f, 99),
        baseTilesIdentity(revision), *world);
    ASSERT_TRUE(context.has_value());
  }
  EXPECT_EQ(bits(12.f), bits(context->configFloat("surfaceCaveAttenuationDist", 0)));
  EXPECT_EQ(bits(-0.0f), bits(context->terrain().baseHeight));
  EXPECT_EQ(1u, context->staticInputs().customTerrainRegionCount());

  std::array<std::future<bool>, 3> results;
  for (auto& result : results) {
    result = std::async(std::launch::async, [&context] {
      for (int i = 0; i != 500; ++i) {
        if (context->terrain().worldWidth != 128 || bits(context->configFloat("surfaceCaveAttenuationDist", 0)) != bits(12.f)
            || context->customTerrainWeighting(5, 15).second <= 0)
          return false;
      }
      return true;
    });
  }
  for (auto& result : results)
    EXPECT_TRUE(result.get());
}

TEST(WorldGenerationCompositeContextTest, DestroyingCompositeReleasesSelectorAdmission) {
  auto world = Access::planet();
  auto revision = world->generationRevision();
  WorldGenerationMemoryLedger ledger(WorldGenerationRegionSelectorContext::retainedBytes());
  {
    auto context = WorldGenerationCompositeContext::tryCapture(ledger, selectorIdentity(revision),
        baseTilesIdentity(revision), *world, false);
    ASSERT_TRUE(context.has_value());
    EXPECT_EQ(WorldGenerationRegionSelectorContext::retainedBytes(), ledger.snapshot().reservedBytes);
  }
  EXPECT_EQ(0u, ledger.snapshot().reservedBytes);
  EXPECT_TRUE(WorldGenerationCompositeContext::tryCapture(ledger, selectorIdentity(revision, 128, -37.f, 917, 2),
      baseTilesIdentity(revision), *world, false).has_value());
}

TEST(WorldGenerationCompositeContextTest, ReconstructPureInputsMatchesMaterializedSelectors) {
  auto world = Access::planet(JsonObject{{"biomeTransitionThreshold", 0.5}});
  auto revision = world->generationRevision();
  auto selectors = selectorIdentity(revision);
  auto baseTiles = baseTilesIdentity(revision);

  Random::init(441);
  WorldGenerationMemoryLedger ledger;
  auto context = WorldGenerationCompositeContext::tryCapture(ledger, selectors, baseTiles, *world);
  ASSERT_TRUE(context.has_value());
  auto nextAfterCapture = Random::randu64();

  // Poison global RNG; PureKernel reconstruction must ignore it and not draw.
  Random::init(7);
  auto pure = context->reconstructPureInputs();
  EXPECT_EQ(1u, WorldGenerationCompositeContext::PureInputs::ModelVersion);
  EXPECT_EQ(context->terrain().toJson(), pure.terrain.toJson());
  EXPECT_EQ(context->foregroundCave().toJson(), pure.foregroundCave.toJson());
  EXPECT_EQ(context->backgroundCave().toJson(), pure.backgroundCave.toJson());
  EXPECT_TRUE(pure.baseTiles.matches(baseTiles));
  EXPECT_EQ(selectors.worldWidth, pure.terrain.worldWidth);
  EXPECT_EQ(bits(selectors.baseHeight), bits(pure.terrain.baseHeight));

  auto again = context->reconstructPureInputs();
  EXPECT_EQ(pure.terrain.toJson(), again.terrain.toJson());
  EXPECT_EQ(pure.baseTiles, again.baseTiles);

  // Same post-capture sequence after reconstruct (no extra global draws).
  Random::init(441);
  WorldGenerationMemoryLedger verify;
  ASSERT_TRUE(WorldGenerationCompositeContext::tryCapture(verify, selectors, baseTiles, *world).has_value());
  EXPECT_EQ(nextAfterCapture, Random::randu64());
}
