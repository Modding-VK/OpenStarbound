#include "StarCompression.hpp"
#include "StarEncode.hpp"
#include "StarWorldGenerationTmxStaticDecode.hpp"
#include "StarZSTDCompression.hpp"

#include <cstring>
#include <zlib.h>

#include "gtest/gtest.h"

using namespace Star;

namespace {
ByteArray packGidsLittleEndian(List<uint32_t> const& gids) {
  ByteArray out;
  for (uint32_t gid : gids)
    out.append((char const*)&gid, sizeof(gid));
  return out;
}

ByteArray gzipCompress(ByteArray const& in) {
  z_stream stream;
  std::memset(&stream, 0, sizeof(stream));
  if (deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 16 + MAX_WBITS, 8, Z_DEFAULT_STRATEGY) != Z_OK)
    return {};
  stream.next_in = (Bytef*)in.ptr();
  stream.avail_in = (uInt)in.size();
  ByteArray out;
  char buffer[256];
  int result = Z_OK;
  while (result == Z_OK) {
    stream.next_out = (Bytef*)buffer;
    stream.avail_out = sizeof(buffer);
    result = deflate(&stream, Z_FINISH);
    out.append(buffer, sizeof(buffer) - stream.avail_out);
  }
  deflateEnd(&stream);
  if (result != Z_STREAM_END)
    return {};
  return out;
}

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

Json makeCompressedFrontMap(String const& compression, String const& dataB64) {
  auto map = makeMapJson().toObject();
  auto layers = map.get("layers").toArray();
  auto layer = layers[0].toObject();
  layer.set("compression", compression);
  layer.set("data", dataB64);
  layers[0] = layer;
  map.set("layers", layers);
  return map;
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

TEST(WorldGenerationTmxStaticDecodeTest, DecodesZlibBase64TileLayer) {
  auto raw = packGidsLittleEndian({1u, 2u, 3u, 4u});
  auto map = makeCompressedFrontMap("zlib", base64Encode(compressData(raw)));
  WorldGenerationTmxStaticDecodeRequest request;
  request.assetKey = "dungeons/shadow/map.json";
  request.assetGeneration = 1;
  request.mapJson = map;
  auto product = tryDecodeWorldGenerationTmxStaticProduct(std::move(request));
  ASSERT_TRUE(product.has_value());
  EXPECT_EQ((List<uint32_t>{1u, 2u, 3u, 4u}), product->layers()[0].gids);
}

TEST(WorldGenerationTmxStaticDecodeTest, DecodesGzipBase64TileLayer) {
  auto raw = packGidsLittleEndian({1u, 2u, 3u, 4u});
  auto gzipped = gzipCompress(raw);
  ASSERT_FALSE(gzipped.empty());
  auto map = makeCompressedFrontMap("gzip", base64Encode(gzipped));
  WorldGenerationTmxStaticDecodeRequest request;
  request.assetKey = "dungeons/shadow/map.json";
  request.assetGeneration = 1;
  request.mapJson = map;
  auto product = tryDecodeWorldGenerationTmxStaticProduct(std::move(request));
  ASSERT_TRUE(product.has_value());
  EXPECT_EQ((List<uint32_t>{1u, 2u, 3u, 4u}), product->layers()[0].gids);
}

TEST(WorldGenerationTmxStaticDecodeTest, DecodesZstdAndStripsFlipBits) {
  auto raw = packGidsLittleEndian({0xe0000001u, 2u, 3u, 4u});
  auto map = makeCompressedFrontMap("zstd", base64Encode(ZstdCompression::compress(raw)));
  WorldGenerationTmxStaticDecodeRequest request;
  request.assetKey = "dungeons/shadow/map.json";
  request.assetGeneration = 1;
  request.mapJson = map;
  auto product = tryDecodeWorldGenerationTmxStaticProduct(std::move(request));
  ASSERT_TRUE(product.has_value());
  EXPECT_EQ((List<uint32_t>{1u, 2u, 3u, 4u}), product->layers()[0].gids);
}

TEST(WorldGenerationTmxStaticDecodeTest, RejectsCorruptCompressedPayload) {
  auto map = makeCompressedFrontMap("zlib", String("AAAA"));
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
