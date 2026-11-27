#include "StarWorldGenerationTmxStaticDecode.hpp"

#include "gtest/gtest.h"

using namespace Star;

namespace {
Json makeMapJson() {
  return JsonObject{
      {"tilewidth", 8},
      {"tileheight", 8},
      {"width", 2},
      {"height", 2},
      {"tilesets", JsonArray{}}, // present but ignored — no Root load
      {"layers", JsonArray{
          JsonObject{
              {"type", "tilelayer"},
              {"name", "front"},
              {"width", 2},
              {"height", 2},
              {"data", JsonArray{1, 2, 3, 4}}},
          JsonObject{
              {"type", "tilelayer"},
              {"name", "back"},
              {"width", 2},
              {"height", 2},
              {"data", JsonArray{10, 0, 0, 11}}},
          JsonObject{
              {"type", "objectgroup"},
              {"name", "objects"},
              {"objects", JsonArray{
                  JsonObject{
                      {"name", "door"},
                      {"x", 8},
                      {"y", 0},
                      {"width", 8},
                      {"height", 16}},
                  JsonObject{
                      {"name", "north"},
                      {"x", 0},
                      {"y", 8},
                      {"width", 8},
                      {"height", 8},
                      {"properties", JsonArray{
                          JsonObject{{"name", "connector"}, {"value", true}},
                          JsonObject{{"name", "direction"}, {"value", "up"}}}}}}}}}}};
}
}

TEST(WorldGenerationTmxStaticDecodeTest, DecodesLayersObjectsAndConnectorsFromJson) {
  WorldGenerationTmxStaticDecodeRequest request;
  request.assetKey = "dungeons/shadow/map.json";
  request.assetGeneration = 2;
  request.mapJson = makeMapJson();

  auto product = tryDecodeWorldGenerationTmxStaticProduct(std::move(request));
  ASSERT_TRUE(product.has_value());
  ASSERT_EQ(2u, product->layers().size());
  EXPECT_EQ(WorldGenerationTmxStaticLayerKind::Foreground, product->layers()[0].kind);
  EXPECT_EQ(WorldGenerationTmxStaticLayerKind::Background, product->layers()[1].kind);
  EXPECT_EQ((List<uint32_t>{1u, 2u, 3u, 4u}), product->layers()[0].gids);
  ASSERT_EQ(2u, product->objects().size());
  EXPECT_EQ(Vec2I(1, 0), product->objects()[0].position);
  EXPECT_EQ(Vec2I(1, 2), product->objects()[0].size);
  ASSERT_EQ(1u, product->connectors().size());
  EXPECT_EQ(String("north"), product->connectors()[0].name);
  EXPECT_EQ(String("up"), product->connectors()[0].direction);
}

TEST(WorldGenerationTmxStaticDecodeTest, IgnoresTilesetsWithoutRoot) {
  auto map = makeMapJson().toObject();
  map.set("tilesets", JsonArray{JsonObject{{"firstgid", 1}, {"source", "tilesets/packed/materials.json"}}});
  WorldGenerationTmxStaticDecodeRequest request;
  request.assetKey = "dungeons/shadow/map.json";
  request.assetGeneration = 1;
  request.mapJson = map;
  auto product = tryDecodeWorldGenerationTmxStaticProduct(std::move(request));
  ASSERT_TRUE(product.has_value());
  EXPECT_TRUE(product->active());
}

TEST(WorldGenerationTmxStaticDecodeTest, RejectsCompressedTileLayer) {
  auto map = makeMapJson().toObject();
  auto layers = map.get("layers").toArray();
  auto layer = layers[0].toObject();
  layer.set("compression", "zlib");
  layer.set("data", "AAAA");
  layers[0] = layer;
  map.set("layers", layers);

  WorldGenerationTmxStaticDecodeRequest request;
  request.assetKey = "dungeons/shadow/map.json";
  request.assetGeneration = 1;
  request.mapJson = map;
  EXPECT_FALSE(tryDecodeWorldGenerationTmxStaticProduct(std::move(request)).has_value());
}

TEST(WorldGenerationTmxStaticDecodeTest, RejectsInvalidTileSize) {
  auto map = makeMapJson().toObject();
  map.set("tilewidth", 16);
  WorldGenerationTmxStaticDecodeRequest request;
  request.assetKey = "dungeons/shadow/map.json";
  request.assetGeneration = 1;
  request.mapJson = map;
  EXPECT_FALSE(tryDecodeWorldGenerationTmxStaticProduct(std::move(request)).has_value());
}

TEST(WorldGenerationTmxStaticDecodeTest, ContentHashMatchesManualBlueprint) {
  WorldGenerationTmxStaticDecodeRequest request;
  request.assetKey = "dungeons/shadow/map.json";
  request.assetGeneration = 4;
  request.mapJson = makeMapJson();
  auto decoded = tryDecodeWorldGenerationTmxStaticBlueprint(request);
  ASSERT_TRUE(decoded.has_value());

  WorldGenerationTmxStaticBlueprint manual;
  manual.identity.assetKey = request.assetKey;
  manual.identity.assetGeneration = request.assetGeneration;
  manual.layers = decoded->layers;
  manual.objects = decoded->objects;
  EXPECT_EQ(worldGenerationTmxStaticContentHash(manual), decoded->identity.contentHash);
}

TEST(WorldGenerationTmxStaticDecodeTest, ConsumeStillPrivate) {
  WorldGenerationTmxStaticDecodeRequest request;
  request.assetKey = "dungeons/shadow/map.json";
  request.assetGeneration = 2;
  request.mapJson = makeMapJson();
  auto product = tryDecodeWorldGenerationTmxStaticProduct(std::move(request));
  ASSERT_TRUE(product.has_value());
  EXPECT_TRUE(product->tryConsume(product->identity()));
  product->discard();
  EXPECT_FALSE(product->active());
}
