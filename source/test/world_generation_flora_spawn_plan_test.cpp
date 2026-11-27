#include "StarWorldGenerationFloraSpawnPlan.hpp"

#include "gtest/gtest.h"

using namespace Star;

namespace {
WorldGenerationFloraSpawnPlanIdentity makeIdentity(RectI sectorTiles = RectI(0, 0, 4, 4)) {
  WorldGenerationFloraSpawnPlanIdentity out;
  out.task = {1, 1, {0, 0}, {0, 0}, WorldGenerationTaskStage::FinalizePlan};
  out.tileRevision = 3;
  out.templateRevision = 1;
  out.worldSeed = 99;
  out.worldSize = {64, 64};
  out.sectorTiles = sectorTiles;
  return out;
}

WorldGenerationFloraTileSample floorTile() {
  WorldGenerationFloraTileSample out;
  out.foreground = 1;
  out.background = EmptyMaterialId;
  out.foregroundMod = NoModId;
  out.backgroundMod = NoModId;
  out.dungeonId = NoDungeonId;
  out.blockBiomeIndex = 7;
  out.aboveForegroundEmpty = true;
  out.aboveBackgroundEmpty = true;
  out.belowForegroundEmpty = false;
  out.belowBackgroundEmpty = true;
  return out;
}
}

TEST(WorldGenerationFloraSpawnPlanTest, SelectsGrassModWithoutWritingTiles) {
  auto identity = makeIdentity(RectI(0, 0, 2, 2));
  WorldGenerationFloraTileSamples tiles;
  tiles[Vec2I(0, 0)] = floorTile();
  tiles[Vec2I(1, 0)] = floorTile();

  WorldGenerationFloraPlaceablesByBiome placeables;
  WorldGenerationFloraPlaceablesSample surface;
  surface.grassMod = 42; // real mod id in harness
  surface.grassModDensity = 1.0f;
  surface.ceilingGrassMod = 43;
  surface.ceilingGrassModDensity = 1.0f;
  placeables[7] = surface;

  auto plan = WorldGenerationFloraSpawnPlan::tryPlan(identity, tiles, placeables);
  ASSERT_TRUE(plan.has_value());
  ASSERT_EQ(2u, plan->grassIntents().size());
  EXPECT_EQ(Vec2I(0, 0), plan->grassIntents()[0].position);
  EXPECT_EQ(TileLayer::Foreground, plan->grassIntents()[0].modLayer);
  EXPECT_EQ(42u, plan->grassIntents()[0].grassModId);
  EXPECT_EQ(Vec2I(1, 0), plan->grassIntents()[1].position);
  // Samples unchanged — plan never mutates owner maps.
  EXPECT_EQ(NoModId, tiles.ptr(Vec2I(0, 0))->foregroundMod);
}

TEST(WorldGenerationFloraSpawnPlanTest, DensityCanRejectGrass) {
  auto identity = makeIdentity(RectI(0, 0, 1, 1));
  WorldGenerationFloraTileSamples tiles;
  tiles[Vec2I(0, 0)] = floorTile();
  WorldGenerationFloraPlaceablesByBiome placeables;
  WorldGenerationFloraPlaceablesSample surface;
  surface.grassMod = 42;
  surface.grassModDensity = 0.0f;
  placeables[7] = surface;
  auto plan = WorldGenerationFloraSpawnPlan::tryPlan(identity, tiles, placeables);
  ASSERT_TRUE(plan.has_value());
  EXPECT_TRUE(plan->grassIntents().empty());
}

TEST(WorldGenerationFloraSpawnPlanTest, SkipsDungeonAndProtectedMods) {
  auto identity = makeIdentity(RectI(0, 0, 2, 1));
  WorldGenerationFloraTileSamples tiles;
  auto dungeon = floorTile();
  dungeon.dungeonId = 1;
  tiles[Vec2I(0, 0)] = dungeon;
  auto protectedMod = floorTile();
  protectedMod.foregroundMod = 100; // not biome/empty sentinel
  tiles[Vec2I(1, 0)] = protectedMod;

  WorldGenerationFloraPlaceablesByBiome placeables;
  WorldGenerationFloraPlaceablesSample surface;
  surface.grassMod = 42;
  surface.grassModDensity = 1.0f;
  placeables[7] = surface;

  auto plan = WorldGenerationFloraSpawnPlan::tryPlan(identity, tiles, placeables);
  ASSERT_TRUE(plan.has_value());
  EXPECT_TRUE(plan->grassIntents().empty());
}

TEST(WorldGenerationFloraSpawnPlanTest, SpawnIntentsOrderedWithoutConstructingEntities) {
  auto identity = makeIdentity(RectI(0, 0, 8, 8));
  WorldGenerationFloraTileSamples tiles;
  tiles[Vec2I(1, 1)] = floorTile();
  tiles[Vec2I(3, 1)] = floorTile();
  tiles[Vec2I(5, 2)] = floorTile();
  auto dungeonTile = floorTile();
  dungeonTile.dungeonId = 9;
  tiles[Vec2I(2, 2)] = dungeonTile;

  List<WorldGenerationFloraSpawnCandidate> candidates = {
      {{5, 2}, WorldGenerationFinalizePlacementKind::Tree, 11, 2, String("treeA")},
      {{1, 1}, WorldGenerationFinalizePlacementKind::Bush, 7, 1, String("bushA")},
      {{2, 2}, WorldGenerationFinalizePlacementKind::Object, 3, 0, String("objDungeon")},
      {{20, 20}, WorldGenerationFinalizePlacementKind::Treasure, 4, 0, String("out")},
      {{3, 1}, WorldGenerationFinalizePlacementKind::Object, 9, 1, String("objA")}};

  WorldGenerationFloraPlaceablesByBiome placeables;
  auto plan = WorldGenerationFloraSpawnPlan::tryPlan(identity, tiles, placeables, candidates);
  ASSERT_TRUE(plan.has_value());
  ASSERT_EQ(3u, plan->spawnIntents().size());
  EXPECT_EQ(Vec2I(1, 1), plan->spawnIntents()[0].position);
  EXPECT_EQ(String("bushA"), plan->spawnIntents()[0].itemKey);
  EXPECT_EQ(Vec2I(3, 1), plan->spawnIntents()[1].position);
  EXPECT_EQ(Vec2I(5, 2), plan->spawnIntents()[2].position);
  EXPECT_TRUE(plan->tryConsume(identity));
  plan->discard();
  EXPECT_TRUE(plan->spawnIntents().empty());
  EXPECT_TRUE(plan->grassIntents().empty());
}

TEST(WorldGenerationFloraSpawnPlanTest, StaleIdentityRejectsConsume) {
  auto identity = makeIdentity();
  WorldGenerationFloraTileSamples tiles;
  tiles[Vec2I(0, 0)] = floorTile();
  auto plan = WorldGenerationFloraSpawnPlan::tryPlan(identity, tiles, {});
  ASSERT_TRUE(plan.has_value());
  auto stale = identity;
  stale.tileRevision = 99;
  EXPECT_FALSE(plan->tryConsume(stale));
  EXPECT_TRUE(plan->active());
}

TEST(WorldGenerationFloraSpawnPlanTest, InvalidIdentityRejected) {
  auto identity = makeIdentity();
  identity.worldSeed = 0;
  EXPECT_FALSE(WorldGenerationFloraSpawnPlan::tryPlan(identity, {}, {}).has_value());
}
