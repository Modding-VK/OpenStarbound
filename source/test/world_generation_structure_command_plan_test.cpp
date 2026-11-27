#include "StarWorldGenerationStructureCommandPlan.hpp"

#include "gtest/gtest.h"

using namespace Star;

namespace {
WorldRegionPartition partition() {
  return WorldRegionPartition(WorldGeometry(Vec2U{128, 128}));
}

WorldGenerationTmxStaticProduct makeStaticProduct() {
  WorldGenerationTmxStaticBlueprint blueprint;
  blueprint.identity.assetKey = "dungeons/md04/map.json";
  blueprint.identity.assetGeneration = 3;
  WorldGenerationTmxStaticLayer layer;
  layer.name = "front";
  layer.width = 2;
  layer.height = 2;
  layer.gids = {1u, 2u, 3u, 4u};
  blueprint.layers = {layer};
  auto product = WorldGenerationTmxStaticProduct::tryBuild(std::move(blueprint));
  EXPECT_TRUE(product.has_value());
  return std::move(*product);
}

WorldGenerationTmxBindingPlan makeBinding(WorldGenerationTmxStaticProduct const& staticProduct) {
  WorldGenerationTmxBindingPlanIdentity identity;
  identity.staticIdentity = staticProduct.identity();
  identity.bindingSeed = 9;
  auto plan = WorldGenerationTmxBindingPlan::tryPlan(identity, staticProduct, {});
  EXPECT_TRUE(plan.has_value());
  return std::move(*plan);
}

WorldGenerationStructureCommandPlanIdentity makeIdentity(WorldGenerationTmxBindingPlan const& binding) {
  WorldGenerationStructureCommandPlanIdentity out;
  out.bindingIdentity = binding.identity();
  out.structureEpoch = 5;
  return out;
}
}

TEST(WorldGenerationStructureCommandPlanTest, BuildsOrderedCommandsAndFootprint) {
  auto staticProduct = makeStaticProduct();
  auto binding = makeBinding(staticProduct);
  auto identity = makeIdentity(binding);
  List<WorldGenerationStructureCommandDraft> drafts = {
      {WorldGenerationStructureCommandKind::SetForegroundMaterial, {4, 4}, {}, {}, {}, 10, 0, "mat"},
      {WorldGenerationStructureCommandKind::PlaceObject, {5, 4}, {{5, 5}}, {{5, 4}}, {{5, 4}, {6, 4}}, 1, 0, "chest"},
      {WorldGenerationStructureCommandKind::ClearTileEntities, {5, 4}, {{4, 4}, {6, 4}}, {{5, 4}}, {}, 0, 1, "clear"}};
  HashMap<Vec2U, uint64_t> revisions;
  auto region = partition().regionForPosition({4, 4});
  ASSERT_TRUE(region.isValid());
  revisions[*region] = 11;

  auto plan = WorldGenerationStructureCommandPlan::tryPlan(
      identity, binding, drafts, {128, 128}, partition(), revisions);
  ASSERT_TRUE(plan.has_value());
  ASSERT_EQ(3u, plan->commands().size());
  EXPECT_EQ(1u, plan->commands()[0].sequence);
  EXPECT_EQ(WorldGenerationStructureCommandKind::SetForegroundMaterial, plan->commands()[0].kind);
  EXPECT_EQ(WorldGenerationStructureCommandKind::ClearTileEntities, plan->commands()[2].kind);
  EXPECT_FALSE(plan->footprint().empty());
  EXPECT_TRUE(plan->footprint().writeSet.contains(Vec2I(4, 4)));
  EXPECT_TRUE(plan->footprint().entityReach.contains(Vec2I(6, 4)));
  ASSERT_FALSE(plan->reservations().empty());
  EXPECT_EQ(1u, plan->reservations()[0].sequence);
  EXPECT_TRUE(plan->tryConsume(identity, revisions));
}

TEST(WorldGenerationStructureCommandPlanTest, WrapsFootprintAcrossWorldX) {
  auto staticProduct = makeStaticProduct();
  auto binding = makeBinding(staticProduct);
  auto identity = makeIdentity(binding);
  List<WorldGenerationStructureCommandDraft> drafts = {
      {WorldGenerationStructureCommandKind::SetBackgroundMaterial, {127, 2}, {{128, 2}}, {{128, 2}}, {}, 3, 0, "wrap"}};
  HashMap<Vec2U, uint64_t> revisions;
  auto region = partition().regionForPosition({127, 2});
  ASSERT_TRUE(region.isValid());
  revisions[*region] = 1;
  auto wrappedRegion = partition().regionForPosition({0, 2});
  ASSERT_TRUE(wrappedRegion.isValid());
  revisions[*wrappedRegion] = 1;

  auto plan = WorldGenerationStructureCommandPlan::tryPlan(
      identity, binding, drafts, {128, 128}, partition(), revisions);
  ASSERT_TRUE(plan.has_value());
  EXPECT_TRUE(plan->footprint().writeSet.contains(Vec2I(0, 2)));
  EXPECT_TRUE(plan->footprint().readSet.contains(Vec2I(0, 2)));
}

TEST(WorldGenerationStructureCommandPlanTest, StaleRegionRevisionRefusesConsume) {
  auto staticProduct = makeStaticProduct();
  auto binding = makeBinding(staticProduct);
  auto identity = makeIdentity(binding);
  List<WorldGenerationStructureCommandDraft> drafts = {
      {WorldGenerationStructureCommandKind::SpawnNpc, {8, 8}, {}, {}, {{8, 8}}, 0, 0, "npc"}};
  HashMap<Vec2U, uint64_t> revisions;
  auto region = partition().regionForPosition({8, 8});
  ASSERT_TRUE(region.isValid());
  revisions[*region] = 4;
  auto plan = WorldGenerationStructureCommandPlan::tryPlan(
      identity, binding, drafts, {128, 128}, partition(), revisions);
  ASSERT_TRUE(plan.has_value());
  revisions[*region] = 5;
  EXPECT_FALSE(plan->tryConsume(identity, revisions));
  EXPECT_TRUE(plan->active());
}

TEST(WorldGenerationStructureCommandPlanTest, TruncatedFootprintNeverAuthorizesConsume) {
  auto staticProduct = makeStaticProduct();
  auto binding = makeBinding(staticProduct);
  auto identity = makeIdentity(binding);
  List<WorldGenerationStructureCommandDraft> drafts = {
      {WorldGenerationStructureCommandKind::MarkTerrain, {1, 1}, {}, {{1, 1}}, {}, 0, 0, "terrain"}};
  HashMap<Vec2U, uint64_t> revisions;
  auto region = partition().regionForPosition({1, 1});
  ASSERT_TRUE(region.isValid());
  revisions[*region] = 2;
  auto plan = WorldGenerationStructureCommandPlan::tryPlan(
      identity, binding, drafts, {128, 128}, partition(), revisions, true);
  ASSERT_TRUE(plan.has_value());
  EXPECT_TRUE(plan->footprint().truncated);
  EXPECT_FALSE(plan->tryConsume(identity, revisions));
}

TEST(WorldGenerationStructureCommandPlanTest, BindingMismatchRefusesPlan) {
  auto staticProduct = makeStaticProduct();
  auto binding = makeBinding(staticProduct);
  auto identity = makeIdentity(binding);
  identity.bindingIdentity.bindingSeed = 99;
  List<WorldGenerationStructureCommandDraft> drafts = {
      {WorldGenerationStructureCommandKind::AddDrop, {0, 0}, {}, {}, {}, 1, 0, "drop"}};
  EXPECT_FALSE(WorldGenerationStructureCommandPlan::tryPlan(
      identity, binding, drafts, {128, 128}, partition(), {}).has_value());
}

TEST(WorldGenerationStructureCommandPlanTest, DiscardClearsCommandsAndFootprint) {
  auto staticProduct = makeStaticProduct();
  auto binding = makeBinding(staticProduct);
  auto identity = makeIdentity(binding);
  List<WorldGenerationStructureCommandDraft> drafts = {
      {WorldGenerationStructureCommandKind::ConnectWire, {2, 2}, {{2, 3}}, {{2, 2}, {2, 3}}, {}, 0, 0, "wire"}};
  HashMap<Vec2U, uint64_t> revisions;
  auto region = partition().regionForPosition({2, 2});
  ASSERT_TRUE(region.isValid());
  revisions[*region] = 1;
  auto plan = WorldGenerationStructureCommandPlan::tryPlan(
      identity, binding, drafts, {128, 128}, partition(), revisions);
  ASSERT_TRUE(plan.has_value());
  plan->discard();
  EXPECT_FALSE(plan->active());
  EXPECT_TRUE(plan->commands().empty());
  EXPECT_TRUE(plan->footprint().empty());
  EXPECT_TRUE(plan->reservations().empty());
}
