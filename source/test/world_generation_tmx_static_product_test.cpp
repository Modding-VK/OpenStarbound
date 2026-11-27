#include "StarWorldGenerationTmxStaticProduct.hpp"

#include "gtest/gtest.h"

using namespace Star;

namespace {
WorldGenerationTmxStaticBlueprint makeBlueprint() {
  WorldGenerationTmxStaticBlueprint blueprint;
  blueprint.identity.assetKey = "dungeons/test/map.json";
  blueprint.identity.assetGeneration = 4;
  WorldGenerationTmxStaticLayer fg;
  fg.name = "front";
  fg.kind = WorldGenerationTmxStaticLayerKind::Foreground;
  fg.width = 2;
  fg.height = 2;
  fg.gids = {1u, 2u, 3u, 4u};
  WorldGenerationTmxStaticLayer bg;
  bg.name = "back";
  bg.kind = WorldGenerationTmxStaticLayerKind::Background;
  bg.width = 2;
  bg.height = 2;
  bg.gids = {10u, 0u, 0u, 11u};
  blueprint.layers = {fg, bg};
  blueprint.objects = {
      {"door", {1, 0}, {1, 2}, false, ""},
      {"north", {0, 1}, {1, 1}, true, "up"}};
  return blueprint;
}
}

TEST(WorldGenerationTmxStaticProductTest, BuildsLayersObjectsAndConnectors) {
  auto product = WorldGenerationTmxStaticProduct::tryBuild(makeBlueprint());
  ASSERT_TRUE(product.has_value());
  EXPECT_TRUE(product->active());
  EXPECT_EQ(2u, product->layers().size());
  EXPECT_EQ(2u, product->objects().size());
  ASSERT_EQ(1u, product->connectors().size());
  EXPECT_EQ(String("north"), product->connectors()[0].name);
  EXPECT_EQ(String("up"), product->connectors()[0].direction);
  EXPECT_GT(product->retainedBytesEstimate(), sizeof(WorldGenerationTmxStaticProduct));
}

TEST(WorldGenerationTmxStaticProductTest, ContentHashStableForSameBlueprint) {
  auto a = makeBlueprint();
  auto b = makeBlueprint();
  EXPECT_EQ(worldGenerationTmxStaticContentHash(a), worldGenerationTmxStaticContentHash(b));
  b.layers[0].gids[0] = 99u;
  EXPECT_NE(worldGenerationTmxStaticContentHash(a), worldGenerationTmxStaticContentHash(b));
}

TEST(WorldGenerationTmxStaticProductTest, InvalidLayerSizeRefusesBuild) {
  auto blueprint = makeBlueprint();
  blueprint.layers[0].gids.append(5u);
  EXPECT_FALSE(WorldGenerationTmxStaticProduct::tryBuild(std::move(blueprint)).has_value());
}

TEST(WorldGenerationTmxStaticProductTest, StaleAssetGenerationRefusesConsume) {
  auto blueprint = makeBlueprint();
  auto product = WorldGenerationTmxStaticProduct::tryBuild(blueprint);
  ASSERT_TRUE(product.has_value());
  auto live = product->identity();
  live.assetGeneration = 99;
  EXPECT_FALSE(product->tryConsume(live));
  EXPECT_TRUE(product->active());
}

TEST(WorldGenerationTmxStaticProductTest, MatchingIdentityConsumesWithoutPublishSideEffects) {
  auto blueprint = makeBlueprint();
  auto product = WorldGenerationTmxStaticProduct::tryBuild(blueprint);
  ASSERT_TRUE(product.has_value());
  auto id = product->identity();
  EXPECT_TRUE(product->tryConsume(id));
  // Still holds private data until discard; consume is revalidation only.
  EXPECT_EQ(2u, product->layers().size());
  product->discard();
  EXPECT_FALSE(product->active());
  EXPECT_TRUE(product->layers().empty());
  EXPECT_TRUE(product->connectors().empty());
}

TEST(WorldGenerationTmxStaticProductTest, EmptyLayersRefuseBuild) {
  WorldGenerationTmxStaticBlueprint blueprint;
  blueprint.identity.assetKey = "x";
  blueprint.identity.assetGeneration = 1;
  blueprint.identity.contentHash = 1;
  EXPECT_FALSE(WorldGenerationTmxStaticProduct::tryBuild(std::move(blueprint)).has_value());
}
