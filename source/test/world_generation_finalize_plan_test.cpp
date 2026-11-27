#include "StarWorldGenerationFinalizePlan.hpp"
#include "StarWorldGenerationTasks.hpp"

#include "gtest/gtest.h"

using namespace Star;

namespace {
WorldGenerationFinalizePlanIdentity makeIdentity(RectI sectorTiles = RectI(0, 0, 4, 4)) {
  WorldGenerationFinalizePlanIdentity out;
  out.task = {1, 1, {0, 0}, {0, 0}, WorldGenerationTaskStage::FinalizePlan};
  out.tileRevision = 3;
  out.templateRevision = 1;
  out.worldSize = {64, 64};
  out.sectorTiles = sectorTiles;
  return out;
}

WorldGenerationFinalizeSample cell(MaterialId fg = 1, MaterialId bg = 1) {
  WorldGenerationFinalizeSample out;
  out.foreground = fg;
  out.background = bg;
  out.dungeonId = NoDungeonId;
  out.liquidSource = false;
  out.oceanLiquid = EmptyLiquidId;
  out.oceanLiquidLevel = 0;
  return out;
}
}

TEST(WorldGenerationFinalizePlanTest, FinalizePlanStageIsReservable) {
  WorldGenerationTasks tasks(1, WorldRegionPartition(WorldGeometry(Vec2U{160, 128})), 2, 1024);
  auto task = tasks.reserve({0, 0}, WorldGenerationTaskStage::FinalizePlan, 16);
  ASSERT_TRUE(task);
  EXPECT_EQ(WorldGenerationTaskStage::FinalizePlan, task->identity.stage);
}

TEST(WorldGenerationFinalizePlanTest, MarksCollisionDirtyForEverySampledTile) {
  auto identity = makeIdentity(RectI(0, 0, 2, 2));
  WorldGenerationFinalizeSamples samples;
  samples[Vec2I(0, 0)] = cell();
  samples[Vec2I(1, 0)] = cell();
  samples[Vec2I(0, 1)] = cell();
  samples[Vec2I(1, 1)] = cell();
  auto plan = WorldGenerationFinalizePlan::tryPlan(identity, samples);
  ASSERT_TRUE(plan.has_value());
  size_t dirty = 0;
  for (auto const& entry : plan->delta()) {
    if (entry.op == WorldGenerationFinalizeOp::MarkCollisionCacheDirty)
      ++dirty;
  }
  EXPECT_EQ(4u, dirty);
}

TEST(WorldGenerationFinalizePlanTest, ClearsOceanSourceUnderFilledBackground) {
  auto identity = makeIdentity(RectI(0, 0, 1, 1));
  WorldGenerationFinalizeSamples samples;
  auto c = cell(EmptyMaterialId, 5);
  c.liquidSource = true;
  c.oceanLiquid = 3;
  c.oceanLiquidLevel = 10;
  samples[Vec2I(0, 0)] = c;
  auto plan = WorldGenerationFinalizePlan::tryPlan(identity, samples);
  ASSERT_TRUE(plan.has_value());
  bool cleared = false;
  bool pressured = false;
  for (auto const& entry : plan->delta()) {
    if (entry.op == WorldGenerationFinalizeOp::ClearLiquidSource)
      cleared = true;
    if (entry.op == WorldGenerationFinalizeOp::SetLiquidPressure) {
      pressured = true;
      EXPECT_EQ(10.0f, entry.pressure);
    }
  }
  EXPECT_TRUE(cleared);
  EXPECT_TRUE(pressured);
}

TEST(WorldGenerationFinalizePlanTest, ResetsNonRealMaterialColorVariants) {
  auto identity = makeIdentity(RectI(0, 0, 1, 1));
  WorldGenerationFinalizeSamples samples;
  samples[Vec2I(0, 0)] = cell(NullMaterialId, BoundaryMaterialId);
  auto plan = WorldGenerationFinalizePlan::tryPlan(identity, samples);
  ASSERT_TRUE(plan.has_value());
  bool fg = false;
  bool bg = false;
  for (auto const& entry : plan->delta()) {
    if (entry.op == WorldGenerationFinalizeOp::ResetForegroundColorVariant)
      fg = true;
    if (entry.op == WorldGenerationFinalizeOp::ResetBackgroundColorVariant)
      bg = true;
  }
  EXPECT_TRUE(fg);
  EXPECT_TRUE(bg);
}

TEST(WorldGenerationFinalizePlanTest, DeltaStaysOrderedByPositionThenOp) {
  auto identity = makeIdentity(RectI(0, 0, 2, 1));
  WorldGenerationFinalizeSamples samples;
  samples[Vec2I(1, 0)] = cell(NullMaterialId, 1);
  samples[Vec2I(0, 0)] = cell(1, NullMaterialId);
  auto plan = WorldGenerationFinalizePlan::tryPlan(identity, samples);
  ASSERT_TRUE(plan.has_value());
  for (size_t i = 1; i < plan->delta().size(); ++i) {
    auto const& a = plan->delta()[i - 1];
    auto const& b = plan->delta()[i];
    if (a.position != b.position) {
      EXPECT_TRUE(a.position[1] < b.position[1]
          || (a.position[1] == b.position[1] && a.position[0] < b.position[0]));
    } else {
      EXPECT_LE((uint8_t)a.op, (uint8_t)b.op);
    }
  }
}

TEST(WorldGenerationFinalizePlanTest, StaleExpectationRefusesConsume) {
  auto identity = makeIdentity();
  WorldGenerationFinalizeSamples samples;
  samples[Vec2I(0, 0)] = cell();
  auto plan = WorldGenerationFinalizePlan::tryPlan(identity, samples);
  ASSERT_TRUE(plan.has_value());
  auto live = identity;
  live.tileRevision = 99;
  EXPECT_FALSE(plan->tryConsume(live));
  EXPECT_TRUE(plan->active());
}

TEST(WorldGenerationFinalizePlanTest, InvalidIdentityRefusesPlan) {
  auto identity = makeIdentity();
  identity.tileRevision = 0;
  EXPECT_FALSE(WorldGenerationFinalizePlan::tryPlan(identity, {}).has_value());
}
