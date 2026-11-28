#include "StarWorldGenerationStructureBatchStaging.hpp"

#include "gtest/gtest.h"

using namespace Star;

namespace {
WorldRegionPartition partition() {
  return WorldRegionPartition(WorldGeometry(Vec2U{128, 128}));
}

WorldGenerationStructureCommandPlan makePlan(List<WorldGenerationStructureCommandDraft> drafts) {
  WorldGenerationTmxStaticBlueprint blueprint;
  blueprint.identity.assetKey = "dungeons/md05/map.json";
  blueprint.identity.assetGeneration = 1;
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
  bindingId.bindingSeed = 3;
  auto binding = WorldGenerationTmxBindingPlan::tryPlan(bindingId, *staticProduct, {});
  EXPECT_TRUE(binding.has_value());

  WorldGenerationStructureCommandPlanIdentity identity;
  identity.bindingIdentity = binding->identity();
  identity.structureEpoch = 8;

  HashMap<Vec2U, uint64_t> revisions;
  for (auto const& draft : drafts) {
    if (auto region = partition().regionForPosition(draft.position))
      revisions[*region] = 1;
  }
  auto plan = WorldGenerationStructureCommandPlan::tryPlan(
      identity, *binding, drafts, {128, 128}, partition(), revisions);
  EXPECT_TRUE(plan.has_value());
  return std::move(*plan);
}

HashMap<Vec2U, uint64_t> revisionsFor(List<WorldGenerationStructureCommandDraft> const& drafts) {
  HashMap<Vec2U, uint64_t> revisions;
  for (auto const& draft : drafts) {
    if (auto region = partition().regionForPosition(draft.position))
      revisions[*region] = 1;
  }
  return revisions;
}
}

TEST(WorldGenerationStructureBatchStagingTest, SplitsCommandsIntoOrderedBatches) {
  List<WorldGenerationStructureCommandDraft> drafts = {
      {WorldGenerationStructureCommandKind::SetForegroundMaterial, {1, 1}, {}, {}, {}, 9, 0, "a"},
      {WorldGenerationStructureCommandKind::SetBackgroundMaterial, {2, 1}, {}, {}, {}, 8, 0, "b"},
      {WorldGenerationStructureCommandKind::PlaceObject, {3, 1}, {}, {}, {{3, 1}}, 1, 0, "c"},
      {WorldGenerationStructureCommandKind::MarkTerrain, {4, 1}, {}, {{4, 1}}, {}, 0, 0, "d"}};
  auto plan = makePlan(drafts);
  auto staging = WorldGenerationStructureBatchStaging::tryOpen(plan, 2);
  ASSERT_TRUE(staging.has_value());
  ASSERT_EQ(2u, staging->batches().size());
  EXPECT_EQ(1u, staging->batches()[0].commands[0].sequence);
  EXPECT_EQ(2u, staging->batches()[0].commands[1].sequence);
  EXPECT_EQ(3u, staging->batches()[1].commands[0].sequence);
  EXPECT_EQ(WorldGenerationStructureBatchPhase::Materials, staging->batches()[0].phase);
  EXPECT_EQ(WorldGenerationStructureBatchPhase::Mixed, staging->batches()[1].phase);
}

TEST(WorldGenerationStructureBatchStagingTest, PartialStagingStaysPrivateUntilCommit) {
  List<WorldGenerationStructureCommandDraft> drafts = {
      {WorldGenerationStructureCommandKind::SetForegroundMaterial, {5, 5}, {}, {}, {}, 42, 0, "mat"},
      {WorldGenerationStructureCommandKind::PlaceObject, {6, 5}, {}, {}, {{6, 5}}, 1, 0, "obj"}};
  auto plan = makePlan(drafts);
  auto staging = WorldGenerationStructureBatchStaging::tryOpen(plan, 1);
  ASSERT_TRUE(staging.has_value());
  ASSERT_TRUE(staging->applyNextBatch());
  EXPECT_FALSE(staging->complete());
  EXPECT_FALSE(staging->committed());
  auto cell = staging->stagingCell({5, 5});
  ASSERT_TRUE(cell.isValid());
  EXPECT_EQ(42u, *cell->foregroundMaterial);
  EXPECT_FALSE(staging->tryCommit(plan.identity(), revisionsFor(drafts), plan));
}

TEST(WorldGenerationStructureBatchStagingTest, CommitRequiresAllBatchesAndFreshReservations) {
  List<WorldGenerationStructureCommandDraft> drafts = {
      {WorldGenerationStructureCommandKind::SetLiquid, {7, 7}, {}, {}, {}, 3, 0, "liq"},
      {WorldGenerationStructureCommandKind::SpawnNpc, {8, 7}, {}, {}, {{8, 7}}, 0, 0, "npc"}};
  auto plan = makePlan(drafts);
  auto staging = WorldGenerationStructureBatchStaging::tryOpen(plan, 1);
  ASSERT_TRUE(staging.has_value());
  EXPECT_TRUE(staging->applyAllRemaining());
  EXPECT_TRUE(staging->complete());
  auto revisions = revisionsFor(drafts);
  EXPECT_TRUE(staging->tryCommit(plan.identity(), revisions, plan));
  EXPECT_TRUE(staging->committed());
  EXPECT_FALSE(staging->applyNextBatch());
}

TEST(WorldGenerationStructureBatchStagingTest, StaleRevisionBlocksCommitAfterFullStaging) {
  List<WorldGenerationStructureCommandDraft> drafts = {
      {WorldGenerationStructureCommandKind::MarkSpace, {9, 9}, {}, {{9, 9}}, {}, 0, 0, "space"}};
  auto plan = makePlan(drafts);
  auto staging = WorldGenerationStructureBatchStaging::tryOpen(plan, 4);
  ASSERT_TRUE(staging.has_value());
  EXPECT_TRUE(staging->applyAllRemaining());
  auto revisions = revisionsFor(drafts);
  auto region = partition().regionForPosition({9, 9});
  ASSERT_TRUE(region.isValid());
  revisions[*region] = 99;
  EXPECT_FALSE(staging->tryCommit(plan.identity(), revisions, plan));
  EXPECT_FALSE(staging->committed());
}

TEST(WorldGenerationStructureBatchStagingTest, ResumeTokenContinuesWithoutReplay) {
  List<WorldGenerationStructureCommandDraft> drafts = {
      {WorldGenerationStructureCommandKind::SetForegroundMaterial, {1, 2}, {}, {}, {}, 1, 0, "a"},
      {WorldGenerationStructureCommandKind::SetForegroundMaterial, {2, 2}, {}, {}, {}, 2, 0, "b"},
      {WorldGenerationStructureCommandKind::SetForegroundMaterial, {3, 2}, {}, {}, {}, 3, 0, "c"}};
  auto plan = makePlan(drafts);
  auto staging = WorldGenerationStructureBatchStaging::tryOpen(plan, 1);
  ASSERT_TRUE(staging.has_value());
  EXPECT_TRUE(staging->applyNextBatch());
  auto token = staging->resumeToken();
  EXPECT_EQ(1u, token.nextBatchIndex);
  EXPECT_TRUE(staging->resumeFrom(token));
  EXPECT_TRUE(staging->applyNextBatch());
  EXPECT_EQ(2u, staging->nextBatchIndex());
  EXPECT_EQ(2u, staging->appliedCommands());
}

TEST(WorldGenerationStructureBatchStagingTest, DiscardClearsInvisibleStaging) {
  List<WorldGenerationStructureCommandDraft> drafts = {
      {WorldGenerationStructureCommandKind::PlaceFlora, {4, 4}, {}, {}, {{4, 4}}, 0, 0, "flora"}};
  auto plan = makePlan(drafts);
  auto staging = WorldGenerationStructureBatchStaging::tryOpen(plan, 1);
  ASSERT_TRUE(staging.has_value());
  EXPECT_TRUE(staging->applyAllRemaining());
  EXPECT_TRUE(staging->stagingCell({4, 4}).isValid());
  staging->discard();
  EXPECT_FALSE(staging->active());
  EXPECT_FALSE(staging->stagingCell({4, 4}).isValid());
  EXPECT_TRUE(staging->batches().empty());
}
