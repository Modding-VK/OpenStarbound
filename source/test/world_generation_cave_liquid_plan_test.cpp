#include "StarWorldGenerationCaveLiquidPlan.hpp"

#include "gtest/gtest.h"

using namespace Star;

namespace {
WorldGenerationCaveLiquidPlanIdentity makeIdentity(RectI sectorTiles = RectI(0, 0, 32, 32),
    float seedDensity = 100.0f) {
  WorldGenerationCaveLiquidPlanIdentity out;
  out.task = {1, 1, {0, 0}, {0, 0}, WorldGenerationTaskStage::CaveLiquidPlan};
  out.tileRevision = 2;
  out.templateRevision = 1;
  out.worldSize = {64, 64};
  out.sectorTiles = sectorTiles;
  out.fillLiquid = 3;
  out.fillMicrodungeons = false;
  out.encloseLiquids = true;
  out.seedDensity = seedDensity;
  return out;
}

WorldGenerationCaveLiquidSample air(LiquidId liquid = EmptyLiquidId, MaterialId background = 1) {
  WorldGenerationCaveLiquidSample cell;
  cell.foreground = EmptyMaterialId;
  cell.background = background;
  cell.liquid = liquid;
  cell.dungeonId = NoDungeonId;
  cell.solidCollision = false;
  return cell;
}

WorldGenerationCaveLiquidSample solid() {
  WorldGenerationCaveLiquidSample cell;
  cell.foreground = 10;
  cell.background = 10;
  cell.liquid = EmptyLiquidId;
  cell.dungeonId = NoDungeonId;
  cell.solidCollision = true;
  return cell;
}

WorldGenerationCaveLiquidSamples filledBounds(RectI bounds, WorldGenerationCaveLiquidSample const& fill) {
  WorldGenerationCaveLiquidSamples samples;
  for (int y = bounds.yMin(); y <= bounds.yMax(); ++y) {
    for (int x = bounds.xMin(); x <= bounds.xMax(); ++x)
      samples[Vec2I(x, y)] = fill;
  }
  return samples;
}

// Independent seed lattice matching the legacy frequency formula.
List<Vec2I> referenceSeeds(RectI sectorTiles, float seedDensity) {
  List<Vec2I> nodes;
  if (!(seedDensity > 0.0f))
    return nodes;
  int frequency = int(100 / seedDensity);
  if (frequency <= 0)
    return nodes;
  for (int y = frequency * (int)floor((double)sectorTiles.min()[1] / frequency); y < sectorTiles.max()[1];
       y += frequency) {
    for (int x = frequency * (int)floor((double)sectorTiles.min()[0] / frequency); x < sectorTiles.max()[0];
         x += frequency) {
      if (sectorTiles.contains(Vec2I(x, y)))
        nodes.append(Vec2I(x, y));
    }
  }
  return nodes;
}
}

TEST(WorldGenerationCaveLiquidPlanTest, SeedsMatchLegacyFrequencyLattice) {
  RectI sector(0, 0, 32, 32);
  auto planned = planWorldGenerationCaveLiquidSeeds(sector, 25.0f);
  auto expected = referenceSeeds(sector, 25.0f);
  sort(expected.begin(), expected.end(), [](Vec2I const& a, Vec2I const& b) {
    return a[1] != b[1] ? a[1] < b[1] : a[0] < b[0];
  });
  ASSERT_EQ(expected.size(), planned.size());
  for (size_t i = 0; i < expected.size(); ++i)
    EXPECT_EQ(expected[i], planned[i]);
}

TEST(WorldGenerationCaveLiquidPlanTest, ZeroDensityProducesEmptyPlan) {
  auto identity = makeIdentity(RectI(0, 0, 32, 32), 0.0f);
  auto bounds = worldGenerationCaveLiquidReadBounds(identity.sectorTiles, identity.worldSize);
  auto samples = filledBounds(bounds, air());
  auto plan = WorldGenerationCaveLiquidPlan::tryPlan(identity, samples);
  ASSERT_TRUE(plan.has_value());
  EXPECT_TRUE(plan->delta().empty());
  EXPECT_TRUE(plan->tryConsume(identity));
}

TEST(WorldGenerationCaveLiquidPlanTest, EnclosedPocketProducesOrderedPressureDelta) {
  auto identity = makeIdentity(RectI(8, 8, 24, 24), 100.0f);
  auto bounds = worldGenerationCaveLiquidReadBounds(identity.sectorTiles, identity.worldSize);
  auto samples = filledBounds(bounds, solid());
  // Hollow 3x3 air pocket fully enclosed by solids, away from padded bounds edge.
  for (int y = 12; y <= 14; ++y) {
    for (int x = 12; x <= 14; ++x)
      samples[Vec2I(x, y)] = air();
  }
  // Seed lattice at density 100 → frequency 1, every tile in sector is a seed.
  auto plan = WorldGenerationCaveLiquidPlan::tryPlan(identity, samples);
  ASSERT_TRUE(plan.has_value());
  ASSERT_FALSE(plan->delta().empty());
  for (size_t i = 1; i < plan->delta().size(); ++i) {
    auto const& a = plan->delta()[i - 1];
    auto const& b = plan->delta()[i];
    EXPECT_TRUE(a.position[1] < b.position[1]
        || (a.position[1] == b.position[1] && a.position[0] <= b.position[0]));
  }
  for (auto const& entry : plan->delta()) {
    EXPECT_GE(entry.pressure, 1.0f);
    EXPECT_TRUE(RectI(12, 12, 15, 15).contains(entry.position));
  }
}

TEST(WorldGenerationCaveLiquidPlanTest, OpenTopPoisonsUnenclosedAir) {
  auto identity = makeIdentity(RectI(8, 8, 24, 24), 100.0f);
  identity.encloseLiquids = false;
  auto bounds = worldGenerationCaveLiquidReadBounds(identity.sectorTiles, identity.worldSize);
  auto samples = filledBounds(bounds, solid());
  for (int y = 12; y <= 14; ++y) {
    for (int x = 12; x <= 14; ++x) {
      auto cell = air(EmptyLiquidId, EmptyMaterialId);
      samples[Vec2I(x, y)] = cell;
    }
  }
  auto plan = WorldGenerationCaveLiquidPlan::tryPlan(identity, samples);
  ASSERT_TRUE(plan.has_value());
  EXPECT_TRUE(plan->delta().empty());
}

TEST(WorldGenerationCaveLiquidPlanTest, ForeignLiquidPoisonsCandidate) {
  auto identity = makeIdentity(RectI(8, 8, 24, 24), 100.0f);
  auto bounds = worldGenerationCaveLiquidReadBounds(identity.sectorTiles, identity.worldSize);
  auto samples = filledBounds(bounds, solid());
  for (int y = 12; y <= 14; ++y) {
    for (int x = 12; x <= 14; ++x)
      samples[Vec2I(x, y)] = air();
  }
  samples[Vec2I(13, 13)] = air(9, 1);
  auto plan = WorldGenerationCaveLiquidPlan::tryPlan(identity, samples);
  ASSERT_TRUE(plan.has_value());
  for (auto const& entry : plan->delta())
    EXPECT_NE(Vec2I(13, 13), entry.position);
}

TEST(WorldGenerationCaveLiquidPlanTest, StaleExpectationRefusesConsumeWithoutMutatingDelta) {
  auto identity = makeIdentity();
  auto bounds = worldGenerationCaveLiquidReadBounds(identity.sectorTiles, identity.worldSize);
  auto samples = filledBounds(bounds, air());
  auto plan = WorldGenerationCaveLiquidPlan::tryPlan(identity, samples);
  ASSERT_TRUE(plan.has_value());
  size_t before = plan->delta().size();
  auto live = identity;
  live.tileRevision = 99;
  EXPECT_FALSE(plan->tryConsume(live));
  EXPECT_TRUE(plan->active());
  EXPECT_EQ(before, plan->delta().size());
}

TEST(WorldGenerationCaveLiquidPlanTest, InvalidIdentityRefusesPlan) {
  auto identity = makeIdentity();
  identity.tileRevision = 0;
  WorldGenerationCaveLiquidSamples samples;
  EXPECT_FALSE(WorldGenerationCaveLiquidPlan::tryPlan(identity, samples).has_value());
}
