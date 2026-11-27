#include "StarWorldGenerationPublicationPass.hpp"

#include "gtest/gtest.h"

using namespace Star;

namespace {
WorldRegionPartition partition() {
  return WorldRegionPartition(WorldGeometry(Vec2U{128, 128}));
}

struct PlanBundle {
  WorldGenerationStructureCommandPlan plan;
  HashMap<Vec2U, uint64_t> revisions;
};

PlanBundle makePlan(List<WorldGenerationStructureCommandDraft> drafts) {
  WorldGenerationTmxStaticBlueprint blueprint;
  blueprint.identity.assetKey = "dungeons/wt04/map.json";
  blueprint.identity.assetGeneration = 2;
  WorldGenerationTmxStaticLayer layer;
  layer.name = "front";
  layer.width = 1;
  layer.height = 1;
  layer.gids = {1u};
  blueprint.layers = {layer};
  auto staticProduct = WorldGenerationTmxStaticProduct::tryBuild(std::move(blueprint));
  EXPECT_TRUE(staticProduct.has_value());

  WorldGenerationTmxBindingPlanIdentity bindingId;
  bindingId.staticIdentity = staticProduct->identity();
  bindingId.bindingSeed = 11;
  auto binding = WorldGenerationTmxBindingPlan::tryPlan(bindingId, *staticProduct, {});
  EXPECT_TRUE(binding.has_value());

  WorldGenerationStructureCommandPlanIdentity identity;
  identity.bindingIdentity = binding->identity();
  identity.structureEpoch = 4;

  HashMap<Vec2U, uint64_t> revisions;
  for (auto const& draft : drafts) {
    if (auto region = partition().regionForPosition(draft.position))
      revisions[*region] = 1;
  }
  auto plan = WorldGenerationStructureCommandPlan::tryPlan(
      identity, *binding, drafts, {128, 128}, partition(), revisions);
  EXPECT_TRUE(plan.has_value());
  return {std::move(*plan), std::move(revisions)};
}
}

TEST(WorldGenerationPublicationPassTest, BudgetExhaustsBeforeCompletion) {
  List<WorldGenerationStructureCommandDraft> drafts = {
      {WorldGenerationStructureCommandKind::SetForegroundMaterial, {1, 1}, {}, {}, {}, 1, 0, "a"},
      {WorldGenerationStructureCommandKind::SetForegroundMaterial, {2, 1}, {}, {}, {}, 2, 0, "b"},
      {WorldGenerationStructureCommandKind::SetForegroundMaterial, {3, 1}, {}, {}, {}, 3, 0, "c"}};
  auto bundle = makePlan(drafts);
  auto staging = WorldGenerationStructureBatchStaging::tryOpen(bundle.plan, 1);
  ASSERT_TRUE(staging.has_value());
  auto pass = WorldGenerationPublicationPass::tryOpen(std::move(*staging), {1, 32});
  ASSERT_TRUE(pass.has_value());
  EXPECT_EQ(WorldGenerationPublicationPassExit::BudgetExhausted,
      pass->run(bundle.plan.identity(), bundle.revisions, bundle.plan));
  EXPECT_FALSE(pass->frontierConfirmed());
  EXPECT_TRUE(pass->deferredDirty().empty());
  EXPECT_EQ(1u, pass->stats().batchesApplied);
}

TEST(WorldGenerationPublicationPassTest, CompletesAndConfirmsDeferredDirtyWithoutPublish) {
  List<WorldGenerationStructureCommandDraft> drafts = {
      {WorldGenerationStructureCommandKind::SetBackgroundMaterial, {4, 4}, {}, {}, {}, 7, 0, "bg"},
      {WorldGenerationStructureCommandKind::PlaceObject, {5, 4}, {}, {}, {{5, 4}}, 1, 0, "obj"}};
  auto bundle = makePlan(drafts);
  auto staging = WorldGenerationStructureBatchStaging::tryOpen(bundle.plan, 2);
  ASSERT_TRUE(staging.has_value());
  auto pass = WorldGenerationPublicationPass::tryOpen(std::move(*staging), {4, 64});
  ASSERT_TRUE(pass.has_value());
  EXPECT_EQ(WorldGenerationPublicationPassExit::Complete,
      pass->run(bundle.plan.identity(), bundle.revisions, bundle.plan));
  EXPECT_TRUE(pass->frontierConfirmed());
  EXPECT_TRUE(pass->deferredDirty().collisionDirty.contains(Vec2I(4, 4)));
  EXPECT_TRUE(pass->deferredDirty().notificationCells.contains(Vec2I(5, 4)));
  EXPECT_TRUE(pass->staging().committed());
}

TEST(WorldGenerationPublicationPassTest, StaleIdentityDiscardsBeforeEffects) {
  List<WorldGenerationStructureCommandDraft> drafts = {
      {WorldGenerationStructureCommandKind::MarkTerrain, {6, 6}, {}, {{6, 6}}, {}, 0, 0, "mark"}};
  auto bundle = makePlan(drafts);
  auto staging = WorldGenerationStructureBatchStaging::tryOpen(bundle.plan, 1);
  ASSERT_TRUE(staging.has_value());
  auto pass = WorldGenerationPublicationPass::tryOpen(std::move(*staging), {2, 8});
  ASSERT_TRUE(pass.has_value());
  auto live = bundle.plan.identity();
  live.structureEpoch = 99;
  EXPECT_EQ(WorldGenerationPublicationPassExit::StaleDiscarded,
      pass->run(live, bundle.revisions, bundle.plan));
  EXPECT_FALSE(pass->active());
  EXPECT_TRUE(pass->deferredDirty().empty());
  EXPECT_FALSE(pass->frontierConfirmed());
}

TEST(WorldGenerationPublicationPassTest, StaleRevisionAfterStagingDiscardsFrontier) {
  List<WorldGenerationStructureCommandDraft> drafts = {
      {WorldGenerationStructureCommandKind::SpawnNpc, {8, 8}, {}, {}, {{8, 8}}, 0, 0, "npc"}};
  auto bundle = makePlan(drafts);
  auto staging = WorldGenerationStructureBatchStaging::tryOpen(bundle.plan, 1);
  ASSERT_TRUE(staging.has_value());
  auto pass = WorldGenerationPublicationPass::tryOpen(std::move(*staging), {2, 8});
  ASSERT_TRUE(pass.has_value());
  auto region = partition().regionForPosition({8, 8});
  ASSERT_TRUE(region.isValid());
  bundle.revisions[*region] = 50;
  EXPECT_EQ(WorldGenerationPublicationPassExit::StaleDiscarded,
      pass->run(bundle.plan.identity(), bundle.revisions, bundle.plan));
  EXPECT_FALSE(pass->frontierConfirmed());
  EXPECT_TRUE(pass->deferredDirty().empty());
}

TEST(WorldGenerationPublicationPassTest, MultiPassBudgetEventuallyCompletes) {
  List<WorldGenerationStructureCommandDraft> drafts = {
      {WorldGenerationStructureCommandKind::SetLiquid, {1, 2}, {}, {}, {}, 3, 0, "a"},
      {WorldGenerationStructureCommandKind::SetLiquid, {2, 2}, {}, {}, {}, 4, 0, "b"}};
  auto bundle = makePlan(drafts);
  auto staging = WorldGenerationStructureBatchStaging::tryOpen(bundle.plan, 1);
  ASSERT_TRUE(staging.has_value());
  auto pass = WorldGenerationPublicationPass::tryOpen(std::move(*staging), {1, 8});
  ASSERT_TRUE(pass.has_value());
  EXPECT_EQ(WorldGenerationPublicationPassExit::BudgetExhausted,
      pass->run(bundle.plan.identity(), bundle.revisions, bundle.plan));
  EXPECT_EQ(WorldGenerationPublicationPassExit::Complete,
      pass->run(bundle.plan.identity(), bundle.revisions, bundle.plan));
  EXPECT_TRUE(pass->frontierConfirmed());
  EXPECT_EQ(2u, pass->stats().batchesApplied);
}
