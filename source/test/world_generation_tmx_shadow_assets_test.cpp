#include "StarWorldGenerationTmxShadowAssets.hpp"

#include "gtest/gtest.h"

using namespace Star;

namespace {
Json makeMapWithTileset(String const& source) {
  return JsonObject{
      {"tilewidth", 8},
      {"tileheight", 8},
      {"width", 2},
      {"height", 1},
      {"tilesets", JsonArray{JsonObject{{"firstgid", 1}, {"source", source}}}},
      {"layers", JsonArray{JsonObject{
          {"type", "tilelayer"},
          {"name", "front"},
          {"width", 2},
          {"height", 1},
          {"data", JsonArray{1, 2}}}}}};
}

Json makeTilesetJson(uint32_t tilecount = 4) {
  return JsonObject{
      {"tilecount", tilecount},
      {"tileproperties", JsonObject{
          {"0", JsonObject{{"material", "dirt"}}},
          {"1", JsonObject{{"material", "stone"}}}}}};
}
}

TEST(WorldGenerationTmxShadowAssetsTest, NormalizesTilesetPath) {
  EXPECT_EQ(String("/tilesets/packed/materials.json"),
      worldGenerationTmxTilesetAssetPath("../../../../packed/tilesets/packed/materials.json"));
  EXPECT_EQ(String("/tilesets/packed/materials.json"),
      worldGenerationTmxTilesetAssetPath("/tilesets/packed/materials.json"));
}

TEST(WorldGenerationTmxShadowAssetsTest, ResolvesTilesetFromFixtureStore) {
  WorldGenerationTmxShadowAssetStore assets;
  assets.put("/tilesets/packed/materials.json", makeTilesetJson(8));

  WorldGenerationTmxStaticDecodeRequest request;
  request.assetKey = "dungeons/shadow/map.json";
  request.assetGeneration = 3;
  request.mapJson = makeMapWithTileset("../../../../packed/tilesets/packed/materials.json");

  auto product = tryDecodeWorldGenerationTmxStaticProductWithAssets(std::move(request), assets);
  ASSERT_TRUE(product.has_value());
  ASSERT_EQ(1u, product->tilesetRefs().size());
  EXPECT_EQ(1u, product->tilesetRefs()[0].firstGid);
  EXPECT_EQ(String("/tilesets/packed/materials.json"), product->tilesetRefs()[0].assetPath);
  EXPECT_EQ(8u, product->tilesetRefs()[0].tileCount);
  EXPECT_EQ((List<uint32_t>{1u, 2u}), product->layers()[0].gids);
}

TEST(WorldGenerationTmxShadowAssetsTest, MissingTilesetAssetFails) {
  WorldGenerationTmxShadowAssetStore assets;
  WorldGenerationTmxStaticDecodeRequest request;
  request.assetKey = "dungeons/shadow/map.json";
  request.assetGeneration = 1;
  request.mapJson = makeMapWithTileset("/tilesets/packed/materials.json");
  EXPECT_FALSE(tryDecodeWorldGenerationTmxStaticProductWithAssets(std::move(request), assets).has_value());
}

TEST(WorldGenerationTmxShadowAssetsTest, EmbeddedTilesetUnsupported) {
  WorldGenerationTmxShadowAssetStore assets;
  auto map = makeMapWithTileset("/tilesets/packed/materials.json").toObject();
  map.set("tilesets", JsonArray{JsonObject{{"firstgid", 1}, {"name", "embedded"}, {"tilecount", 2}}});
  WorldGenerationTmxStaticDecodeRequest request;
  request.assetKey = "dungeons/shadow/map.json";
  request.assetGeneration = 1;
  request.mapJson = map;
  EXPECT_FALSE(tryDecodeWorldGenerationTmxStaticProductWithAssets(std::move(request), assets).has_value());
}

TEST(WorldGenerationTmxShadowAssetsTest, TilesetContentChangeInvalidatesHash) {
  WorldGenerationTmxShadowAssetStore assetsA;
  assetsA.put("/tilesets/packed/materials.json", makeTilesetJson(4));
  WorldGenerationTmxShadowAssetStore assetsB;
  assetsB.put("/tilesets/packed/materials.json", makeTilesetJson(5));

  WorldGenerationTmxStaticDecodeRequest requestA;
  requestA.assetKey = "dungeons/shadow/map.json";
  requestA.assetGeneration = 1;
  requestA.mapJson = makeMapWithTileset("/tilesets/packed/materials.json");
  WorldGenerationTmxStaticDecodeRequest requestB = requestA;

  auto a = tryDecodeWorldGenerationTmxStaticProductWithAssets(std::move(requestA), assetsA);
  auto b = tryDecodeWorldGenerationTmxStaticProductWithAssets(std::move(requestB), assetsB);
  ASSERT_TRUE(a.has_value());
  ASSERT_TRUE(b.has_value());
  EXPECT_NE(a->identity().contentHash, b->identity().contentHash);
}

TEST(WorldGenerationTmxShadowAssetsTest, EmptyTilesetsStillDecodes) {
  WorldGenerationTmxShadowAssetStore assets;
  WorldGenerationTmxStaticDecodeRequest request;
  request.assetKey = "dungeons/shadow/map.json";
  request.assetGeneration = 2;
  request.mapJson = JsonObject{
      {"tilewidth", 8},
      {"tileheight", 8},
      {"width", 1},
      {"height", 1},
      {"tilesets", JsonArray{}},
      {"layers", JsonArray{JsonObject{
          {"type", "tilelayer"},
          {"name", "front"},
          {"width", 1},
          {"height", 1},
          {"data", JsonArray{1}}}}}};
  auto product = tryDecodeWorldGenerationTmxStaticProductWithAssets(std::move(request), assets);
  ASSERT_TRUE(product.has_value());
  EXPECT_TRUE(product->tilesetRefs().empty());
}
