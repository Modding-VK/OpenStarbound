#include "StarWorldGenerationShadowIntegration.hpp"

#include <cstring>

#include "gtest/gtest.h"

using namespace Star;

namespace {
ByteArray bytes(char const* text) {
  return ByteArray(text, strlen(text));
}

WorldRegionPartition partition() {
  return WorldRegionPartition(WorldGeometry(Vec2U{128, 128}));
}

struct PlanBundle {
  WorldGenerationStructureCommandPlan plan;
  HashMap<Vec2U, uint64_t> revisions;
};

PlanBundle makePlan(List<WorldGenerationStructureCommandDraft> drafts) {
  WorldGenerationTmxStaticBlueprint blueprint;
  blueprint.identity.assetKey = "dungeons/h3-int/map.json";
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
  bindingId.bindingSeed = 42;
  auto binding = WorldGenerationTmxBindingPlan::tryPlan(bindingId, *staticProduct, {});
  EXPECT_TRUE(binding.has_value());

  WorldGenerationStructureCommandPlanIdentity identity;
  identity.bindingIdentity = binding->identity();
  identity.structureEpoch = 7;

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

TEST(WorldGenerationShadowIntegrationTest, UnconfirmedFrontierRejected) {
  List<WorldGenerationStructureCommandDraft> drafts = {
      {WorldGenerationStructureCommandKind::SetForegroundMaterial, {1, 1}, {}, {}, {}, 1, 0, "a"},
      {WorldGenerationStructureCommandKind::SetForegroundMaterial, {2, 1}, {}, {}, {}, 2, 0, "b"}};
  auto bundle = makePlan(drafts);
  auto staging = WorldGenerationStructureBatchStaging::tryOpen(bundle.plan, 1);
  ASSERT_TRUE(staging.has_value());
  auto pass = WorldGenerationPublicationPass::tryOpen(std::move(*staging), {1, 8});
  ASSERT_TRUE(pass.has_value());
  EXPECT_EQ(WorldGenerationPublicationPassExit::BudgetExhausted,
      pass->run(bundle.plan.identity(), bundle.revisions, bundle.plan));
  EXPECT_FALSE(pass->frontierConfirmed());

  WorldGenerationShadowIntegrationHarness harness("int-a");
  EXPECT_EQ(WorldGenerationShadowIntegrationStatus::FrontierNotConfirmed,
      harness.refuseUnconfirmed(*pass));
  EXPECT_EQ(WorldGenerationShadowIntegrationStatus::FrontierNotConfirmed,
      harness.ingestPublicationFrontier(*pass, {0, 0}, 1, bytes("tiles")));
}

TEST(WorldGenerationShadowIntegrationTest, ConfirmedFrontierCheckpointsAndDeliversPacket) {
  List<WorldGenerationStructureCommandDraft> drafts = {
      {WorldGenerationStructureCommandKind::SetBackgroundMaterial, {4, 4}, {}, {}, {}, 7, 0, "bg"},
      {WorldGenerationStructureCommandKind::PlaceObject, {5, 4}, {}, {}, {{5, 4}}, 1, 0, "obj"}};
  auto bundle = makePlan(drafts);
  auto staging = WorldGenerationStructureBatchStaging::tryOpen(bundle.plan, 2);
  ASSERT_TRUE(staging.has_value());
  auto pass = WorldGenerationPublicationPass::tryOpen(std::move(*staging), {4, 64});
  ASSERT_TRUE(pass.has_value());
  ASSERT_EQ(WorldGenerationPublicationPassExit::Complete,
      pass->run(bundle.plan.identity(), bundle.revisions, bundle.plan));
  ASSERT_TRUE(pass->frontierConfirmed());

  WorldGenerationShadowIntegrationHarness harness("int-b");
  ASSERT_EQ(WorldGenerationShadowIntegrationStatus::Ok,
      harness.ingestPublicationFrontier(*pass, {0, 0}, 1, bytes("tiles-confirmed")));
  ASSERT_EQ(WorldGenerationShadowIntegrationStatus::Ok, harness.commitCheckpointUnit());

  auto tiles = harness.db().readCheckpoint({{0, 0}, String("Tiles")});
  auto frontier = harness.db().readCheckpoint({{0, 0}, String("Frontier")});
  ASSERT_EQ(WorldGenerationShadowDbStatus::Ok, tiles.status);
  ASSERT_EQ(WorldGenerationShadowDbStatus::Ok, frontier.status);
  uint64_t structureEpoch = 0;
  uint64_t bindingSeed = 0;
  uint32_t dirtyCount = 0;
  ASSERT_TRUE(WorldGenerationShadowDbService::decodeFrontierBlob(
      frontier.entry->bytes, structureEpoch, bindingSeed, dirtyCount));
  EXPECT_EQ(7u, structureEpoch);
  EXPECT_EQ(42u, bindingSeed);
  EXPECT_EQ(pass->stats().deferredDirtyCells, dirtyCount);

  ASSERT_EQ(WorldGenerationShadowIntegrationStatus::Ok, harness.deliverInitialPacket());
  EXPECT_TRUE(harness.packets().sectorActive({0, 0}));
  EXPECT_NE(0u, harness.lastInitialPacketId());
  EXPECT_EQ(WorldGenerationShadowPacketPhase::Applied,
      harness.packets().packet(harness.lastInitialPacketId())->phase);
}

TEST(WorldGenerationShadowIntegrationTest, DeliverRequiresCommittedUnit) {
  WorldGenerationShadowIntegrationHarness harness("int-c");
  WorldGenerationShadowIntegrationFrontier frontier;
  frontier.sector = {1, 0};
  frontier.structureEpoch = 3;
  frontier.bindingSeed = 9;
  frontier.dirtyCellCount = 2;
  frontier.tilesContentVersion = 1;
  frontier.tilesBytes = bytes("tiles");
  frontier.confirmed = true;
  ASSERT_EQ(WorldGenerationShadowIntegrationStatus::Ok,
      harness.ingestConfirmedFrontier(std::move(frontier)));
  EXPECT_EQ(WorldGenerationShadowIntegrationStatus::NotCommitted, harness.deliverInitialPacket());
  ASSERT_EQ(WorldGenerationShadowIntegrationStatus::Ok, harness.commitCheckpointUnit());
  ASSERT_EQ(WorldGenerationShadowIntegrationStatus::Ok, harness.deliverInitialPacket());
  EXPECT_TRUE(harness.packets().sectorActive({1, 0}));
}

TEST(WorldGenerationShadowIntegrationTest, ProvisionalFrontierMetadataRejected) {
  WorldGenerationShadowIntegrationHarness harness("int-d");
  WorldGenerationShadowIntegrationFrontier frontier;
  frontier.sector = {2, 0};
  frontier.structureEpoch = 1;
  frontier.bindingSeed = 1;
  frontier.dirtyCellCount = 1;
  frontier.tilesContentVersion = 1;
  frontier.tilesBytes = bytes("tiles");
  frontier.confirmed = false;
  EXPECT_EQ(WorldGenerationShadowIntegrationStatus::FrontierNotConfirmed,
      harness.ingestConfirmedFrontier(std::move(frontier)));
}
