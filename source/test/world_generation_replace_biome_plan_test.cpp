#include "StarWorldGenerationReplaceBiomePlan.hpp"

#include "gtest/gtest.h"

using namespace Star;

namespace {
WorldGenerationReplaceBiomePlanIdentity makeIdentity(RectI sectorTiles = RectI(0, 0, 4, 4)) {
  WorldGenerationReplaceBiomePlanIdentity out;
  out.task = {1, 1, {0, 0}, {0, 0}, WorldGenerationTaskStage::FinalizePlan};
  out.tileRevision = 3;
  out.templateRevision = 1;
  out.worldSize = {64, 64};
  out.sectorTiles = sectorTiles;
  return out;
}

WorldGenerationReplaceBiomeBlockSample biomeWithSubs() {
  WorldGenerationReplaceBiomeBlockSample out;
  out.mainBlock = 10;
  out.subBlocks = {20, 21, 22};
  out.materialHueShifts[10] = 5;
  out.materialHueShifts[20] = 7;
  out.materialHueShifts[21] = 8;
  return out;
}
}

TEST(WorldGenerationReplaceBiomePlanTest, ResolvesMainAndSubBlocksWithoutMutation) {
  auto identity = makeIdentity(RectI(0, 0, 2, 1));
  WorldGenerationReplaceBiomeTileSamples tiles;
  WorldGenerationReplaceBiomeTileSample a;
  a.foreground = BiomeMaterialId;
  a.background = Biome1MaterialId;
  a.blockBiomeIndex = 3;
  tiles[Vec2I(0, 0)] = a;
  WorldGenerationReplaceBiomeTileSample b;
  b.foreground = Biome2MaterialId; // -> subBlocks[1] == 21
  b.background = EmptyMaterialId;
  b.blockBiomeIndex = 3;
  tiles[Vec2I(1, 0)] = b;

  WorldGenerationReplaceBiomeBlocksByIndex biomes;
  biomes[3] = biomeWithSubs();

  auto plan = WorldGenerationReplaceBiomePlan::tryPlan(identity, tiles, biomes);
  ASSERT_TRUE(plan.has_value());
  ASSERT_FALSE(plan->delta().empty());

  // Samples untouched.
  EXPECT_EQ(BiomeMaterialId, tiles.ptr(Vec2I(0, 0))->foreground);
  EXPECT_EQ(Biome1MaterialId, tiles.ptr(Vec2I(0, 0))->background);

  // Ordered by x within y: (0,0) then (1,0).
  EXPECT_EQ(Vec2I(0, 0), plan->delta()[0].position);
  EXPECT_EQ(WorldGenerationReplaceBiomeOp::SetForeground, plan->delta()[0].op);
  EXPECT_EQ(10u, plan->delta()[0].material);
  EXPECT_EQ(WorldGenerationReplaceBiomeOp::SetForegroundHue, plan->delta()[1].op);
  EXPECT_EQ(5u, plan->delta()[1].hue);
  EXPECT_EQ(WorldGenerationReplaceBiomeOp::SetBackground, plan->delta()[2].op);
  EXPECT_EQ(20u, plan->delta()[2].material); // Biome1 -> subBlocks[0]
  EXPECT_EQ(WorldGenerationReplaceBiomeOp::SetBackgroundHue, plan->delta()[3].op);
  EXPECT_EQ(7u, plan->delta()[3].hue);

  EXPECT_EQ(Vec2I(1, 0), plan->delta()[4].position);
  EXPECT_EQ(WorldGenerationReplaceBiomeOp::SetForeground, plan->delta()[4].op);
  EXPECT_EQ(21u, plan->delta()[4].material);
  EXPECT_EQ(WorldGenerationReplaceBiomeOp::SetForegroundHue, plan->delta()[5].op);
  EXPECT_EQ(8u, plan->delta()[5].hue);
}

TEST(WorldGenerationReplaceBiomePlanTest, MissingSubBlocksFallsBackToMain) {
  auto identity = makeIdentity(RectI(0, 0, 1, 1));
  WorldGenerationReplaceBiomeTileSamples tiles;
  WorldGenerationReplaceBiomeTileSample tile;
  tile.foreground = Biome3MaterialId;
  tile.blockBiomeIndex = 1;
  tiles[Vec2I(0, 0)] = tile;

  WorldGenerationReplaceBiomeBlocksByIndex biomes;
  WorldGenerationReplaceBiomeBlockSample biome;
  biome.mainBlock = 55;
  biome.materialHueShifts[55] = 1;
  biomes[1] = biome;

  auto plan = WorldGenerationReplaceBiomePlan::tryPlan(identity, tiles, biomes);
  ASSERT_TRUE(plan.has_value());
  ASSERT_GE(plan->delta().size(), 2u);
  EXPECT_EQ(WorldGenerationReplaceBiomeOp::SetForeground, plan->delta()[0].op);
  EXPECT_EQ(55u, plan->delta()[0].material);
}

TEST(WorldGenerationReplaceBiomePlanTest, MissingBiomeClearsPlaceholdersAndBiomeMods) {
  auto identity = makeIdentity(RectI(0, 0, 1, 1));
  WorldGenerationReplaceBiomeTileSamples tiles;
  WorldGenerationReplaceBiomeTileSample tile;
  tile.foreground = BiomeMaterialId;
  tile.background = Biome1MaterialId;
  tile.foregroundMod = BiomeModId;
  tile.backgroundMod = UndergroundBiomeModId;
  tile.blockBiomeIndex = 99; // absent
  tiles[Vec2I(0, 0)] = tile;

  auto plan = WorldGenerationReplaceBiomePlan::tryPlan(identity, tiles, {});
  ASSERT_TRUE(plan.has_value());
  bool clearedFg = false;
  bool clearedBg = false;
  bool clearedFgMod = false;
  bool clearedBgMod = false;
  for (auto const& entry : plan->delta()) {
    if (entry.op == WorldGenerationReplaceBiomeOp::ClearForeground)
      clearedFg = true;
    if (entry.op == WorldGenerationReplaceBiomeOp::ClearBackground)
      clearedBg = true;
    if (entry.op == WorldGenerationReplaceBiomeOp::ClearForegroundMod)
      clearedFgMod = true;
    if (entry.op == WorldGenerationReplaceBiomeOp::ClearBackgroundMod)
      clearedBgMod = true;
  }
  EXPECT_TRUE(clearedFg);
  EXPECT_TRUE(clearedBg);
  EXPECT_TRUE(clearedFgMod);
  EXPECT_TRUE(clearedBgMod);
}

TEST(WorldGenerationReplaceBiomePlanTest, NonBiomeMaterialsEmitNothing) {
  auto identity = makeIdentity(RectI(0, 0, 1, 1));
  WorldGenerationReplaceBiomeTileSamples tiles;
  WorldGenerationReplaceBiomeTileSample tile;
  tile.foreground = 12;
  tile.background = 13;
  tile.blockBiomeIndex = 3;
  tiles[Vec2I(0, 0)] = tile;
  WorldGenerationReplaceBiomeBlocksByIndex biomes;
  biomes[3] = biomeWithSubs();
  auto plan = WorldGenerationReplaceBiomePlan::tryPlan(identity, tiles, biomes);
  ASSERT_TRUE(plan.has_value());
  EXPECT_TRUE(plan->delta().empty());
}

TEST(WorldGenerationReplaceBiomePlanTest, StaleIdentityRejectsConsume) {
  auto identity = makeIdentity();
  WorldGenerationReplaceBiomeTileSamples tiles;
  WorldGenerationReplaceBiomeTileSample tile;
  tile.foreground = BiomeMaterialId;
  tile.blockBiomeIndex = 3;
  tiles[Vec2I(0, 0)] = tile;
  WorldGenerationReplaceBiomeBlocksByIndex biomes;
  biomes[3] = biomeWithSubs();
  auto plan = WorldGenerationReplaceBiomePlan::tryPlan(identity, tiles, biomes);
  ASSERT_TRUE(plan.has_value());
  auto stale = identity;
  stale.tileRevision = 99;
  EXPECT_FALSE(plan->tryConsume(stale));
  EXPECT_TRUE(plan->tryConsume(identity));
  plan->discard();
  EXPECT_TRUE(plan->delta().empty());
}

TEST(WorldGenerationReplaceBiomePlanTest, InvalidIdentityRejected) {
  auto identity = makeIdentity();
  identity.tileRevision = 0;
  EXPECT_FALSE(WorldGenerationReplaceBiomePlan::tryPlan(identity, {}, {}).has_value());
}
