#include "StarWorldGenerationTmxBindingPlan.hpp"

#include "gtest/gtest.h"

using namespace Star;

namespace {
WorldGenerationTmxStaticProduct makeStaticProduct() {
  WorldGenerationTmxStaticBlueprint blueprint;
  blueprint.identity.assetKey = "dungeons/bind/map.json";
  blueprint.identity.assetGeneration = 2;
  WorldGenerationTmxStaticLayer layer;
  layer.name = "front";
  layer.width = 1;
  layer.height = 1;
  layer.gids = {7u};
  blueprint.layers = {layer};
  auto product = WorldGenerationTmxStaticProduct::tryBuild(std::move(blueprint));
  EXPECT_TRUE(product.has_value());
  return std::move(*product);
}

WorldGenerationTmxBindingPlanIdentity makeIdentity(WorldGenerationTmxStaticIdentity staticId) {
  WorldGenerationTmxBindingPlanIdentity out;
  out.staticIdentity = std::move(staticId);
  out.bindingSeed = 42;
  return out;
}
}

TEST(WorldGenerationTmxBindingPlanTest, RecordsOrderedChoicesWithoutFacadeWrites) {
  auto staticProduct = makeStaticProduct();
  auto identity = makeIdentity(staticProduct.identity());
  List<WorldGenerationTmxBindingRequest> requests = {
      {WorldGenerationTmxBindingKind::RandomBrush, {0, 0}, "floor", 3, 1},
      {WorldGenerationTmxBindingKind::NpcBrush, {0, 0}, "guard", 2, 0},
      {WorldGenerationTmxBindingKind::ConnectorPick, {1, 0}, "east", 4, 2}};
  auto plan = WorldGenerationTmxBindingPlan::tryPlan(identity, staticProduct, requests);
  ASSERT_TRUE(plan.has_value());
  ASSERT_EQ(3u, plan->choices().size());
  EXPECT_EQ(1u, plan->choices()[0].sequence);
  EXPECT_EQ(WorldGenerationTmxBindingKind::RandomBrush, plan->choices()[0].kind);
  EXPECT_EQ(2u, plan->choices()[1].sequence);
  EXPECT_EQ(WorldGenerationTmxBindingKind::NpcBrush, plan->choices()[1].kind);
  EXPECT_EQ(3u, plan->choices()[2].sequence);
  EXPECT_TRUE(plan->tryConsume(identity));
}

TEST(WorldGenerationTmxBindingPlanTest, RejectsOutOfRangeSelection) {
  auto staticProduct = makeStaticProduct();
  auto identity = makeIdentity(staticProduct.identity());
  List<WorldGenerationTmxBindingRequest> requests = {
      {WorldGenerationTmxBindingKind::RandomBrush, {0, 0}, "bad", 2, 5}};
  auto plan = WorldGenerationTmxBindingPlan::tryPlan(identity, staticProduct, requests);
  ASSERT_TRUE(plan.has_value());
  EXPECT_TRUE(plan->choices().empty());
}

TEST(WorldGenerationTmxBindingPlanTest, StaleStaticIdentityRefusesPlan) {
  auto staticProduct = makeStaticProduct();
  auto identity = makeIdentity(staticProduct.identity());
  identity.staticIdentity.assetGeneration = 99;
  List<WorldGenerationTmxBindingRequest> requests;
  EXPECT_FALSE(WorldGenerationTmxBindingPlan::tryPlan(identity, staticProduct, requests).has_value());
}

TEST(WorldGenerationTmxBindingPlanTest, StaleBindingSeedRefusesConsume) {
  auto staticProduct = makeStaticProduct();
  auto identity = makeIdentity(staticProduct.identity());
  auto plan = WorldGenerationTmxBindingPlan::tryPlan(identity, staticProduct, {});
  ASSERT_TRUE(plan.has_value());
  auto live = identity;
  live.bindingSeed = 7;
  EXPECT_FALSE(plan->tryConsume(live));
  EXPECT_TRUE(plan->active());
}

TEST(WorldGenerationTmxBindingPlanTest, DiscardClearsChoices) {
  auto staticProduct = makeStaticProduct();
  auto identity = makeIdentity(staticProduct.identity());
  List<WorldGenerationTmxBindingRequest> requests = {
      {WorldGenerationTmxBindingKind::NpcBrush, {0, 0}, "a", 1, 0}};
  auto plan = WorldGenerationTmxBindingPlan::tryPlan(identity, staticProduct, requests);
  ASSERT_TRUE(plan.has_value());
  plan->discard();
  EXPECT_FALSE(plan->active());
  EXPECT_TRUE(plan->choices().empty());
}
