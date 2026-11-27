#include "StarWorldGenerationTmxShadowCache.hpp"

#include "gtest/gtest.h"

using namespace Star;

namespace {
Json makeMap(String const& assetSuffix = "") {
  return JsonObject{
      {"tilewidth", 8},
      {"tileheight", 8},
      {"width", 2},
      {"height", 1},
      {"tilesets", JsonArray{JsonObject{
          {"firstgid", 1},
          {"source", "/tilesets/packed/materials.json"}}}},
      {"layers", JsonArray{JsonObject{
          {"type", "tilelayer"},
          {"name", String("front") + assetSuffix},
          {"width", 2},
          {"height", 1},
          {"data", JsonArray{1, 2}}}}}};
}

Json makeTilesetJson(uint32_t tilecount = 4) {
  return JsonObject{{"tilecount", tilecount}};
}

WorldGenerationTmxShadowAssetStore makeAssets() {
  WorldGenerationTmxShadowAssetStore assets;
  assets.put("/tilesets/packed/materials.json", makeTilesetJson(4));
  return assets;
}

WorldGenerationTmxStaticDecodeRequest makeRequest(String assetKey, uint64_t generation) {
  WorldGenerationTmxStaticDecodeRequest request;
  request.assetKey = std::move(assetKey);
  request.assetGeneration = generation;
  request.mapJson = makeMap();
  return request;
}
}

TEST(WorldGenerationTmxShadowCacheTest, MissThenHitReusesProduct) {
  WorldGenerationTmxShadowCache cache(1 << 20);
  auto assets = makeAssets();

  auto first = cache.tryGetOrDecode(makeRequest("dungeons/a.json", 1), assets);
  ASSERT_EQ(WorldGenerationTmxShadowCacheStatus::MissInserted, first.status);
  ASSERT_NE(nullptr, first.product);
  EXPECT_EQ(String("dungeons/a.json"), first.product->identity().assetKey);

  auto second = cache.tryGetOrDecode(makeRequest("dungeons/a.json", 1), assets);
  ASSERT_EQ(WorldGenerationTmxShadowCacheStatus::Hit, second.status);
  EXPECT_EQ(first.product, second.product);

  auto stats = cache.stats();
  EXPECT_EQ(1u, stats.hits);
  EXPECT_EQ(1u, stats.misses);
  EXPECT_EQ(1u, stats.inserts);
  EXPECT_EQ(1u, stats.entryCount);
  EXPECT_GT(stats.retainedBytes, 0u);
}

TEST(WorldGenerationTmxShadowCacheTest, GenerationChangeIsSeparateKey) {
  WorldGenerationTmxShadowCache cache(1 << 20);
  auto assets = makeAssets();

  ASSERT_EQ(WorldGenerationTmxShadowCacheStatus::MissInserted,
      cache.tryGetOrDecode(makeRequest("dungeons/a.json", 1), assets).status);
  ASSERT_EQ(WorldGenerationTmxShadowCacheStatus::MissInserted,
      cache.tryGetOrDecode(makeRequest("dungeons/a.json", 2), assets).status);
  EXPECT_EQ(2u, cache.size());
  EXPECT_EQ(WorldGenerationTmxShadowCacheStatus::Hit,
      cache.tryGet(WorldGenerationTmxShadowCacheKey{"dungeons/a.json", 1}).status);
}

TEST(WorldGenerationTmxShadowCacheTest, ReloadClearsAndBumpsEpoch) {
  WorldGenerationTmxShadowCache cache(1 << 20);
  auto assets = makeAssets();
  ASSERT_EQ(WorldGenerationTmxShadowCacheStatus::MissInserted,
      cache.tryGetOrDecode(makeRequest("dungeons/a.json", 1), assets).status);
  uint64_t before = cache.reloadEpoch();
  uint64_t after = cache.reload();
  EXPECT_EQ(before + 1, after);
  EXPECT_EQ(0u, cache.size());
  EXPECT_EQ(0u, cache.retainedBytes());
  EXPECT_EQ(WorldGenerationTmxShadowCacheStatus::Missing,
      cache.tryGet(WorldGenerationTmxShadowCacheKey{"dungeons/a.json", 1}).status);
  EXPECT_EQ(1u, cache.stats().reloads);
}

TEST(WorldGenerationTmxShadowCacheTest, InvalidateDropsOneEntry) {
  WorldGenerationTmxShadowCache cache(1 << 20);
  auto assets = makeAssets();
  cache.tryGetOrDecode(makeRequest("dungeons/a.json", 1), assets);
  cache.tryGetOrDecode(makeRequest("dungeons/b.json", 1), assets);
  EXPECT_TRUE(cache.invalidate(WorldGenerationTmxShadowCacheKey{"dungeons/a.json", 1}));
  EXPECT_EQ(1u, cache.size());
  EXPECT_EQ(WorldGenerationTmxShadowCacheStatus::Missing,
      cache.tryGet(WorldGenerationTmxShadowCacheKey{"dungeons/a.json", 1}).status);
  EXPECT_EQ(WorldGenerationTmxShadowCacheStatus::Hit,
      cache.tryGet(WorldGenerationTmxShadowCacheKey{"dungeons/b.json", 1}).status);
}

TEST(WorldGenerationTmxShadowCacheTest, BudgetEvictsOldestOnInsert) {
  auto assets = makeAssets();
  auto probe = tryDecodeWorldGenerationTmxStaticProductWithAssets(makeRequest("dungeons/a.json", 1), assets);
  ASSERT_TRUE(probe.has_value());
  size_t oneBytes = probe->retainedBytesEstimate();
  ASSERT_GT(oneBytes, 0u);
  WorldGenerationTmxShadowCache cache(oneBytes + oneBytes / 2);

  auto a = cache.tryGetOrDecode(makeRequest("dungeons/a.json", 1), assets);
  ASSERT_TRUE(a.status == WorldGenerationTmxShadowCacheStatus::MissInserted
      || a.status == WorldGenerationTmxShadowCacheStatus::MissEvictedInsert);
  auto b = cache.tryGetOrDecode(makeRequest("dungeons/b.json", 1), assets);
  ASSERT_EQ(WorldGenerationTmxShadowCacheStatus::MissEvictedInsert, b.status);
  EXPECT_EQ(1u, cache.size());
  EXPECT_EQ(WorldGenerationTmxShadowCacheStatus::Missing,
      cache.tryGet(WorldGenerationTmxShadowCacheKey{"dungeons/a.json", 1}).status);
  EXPECT_EQ(WorldGenerationTmxShadowCacheStatus::Hit,
      cache.tryGet(WorldGenerationTmxShadowCacheKey{"dungeons/b.json", 1}).status);
  EXPECT_GE(cache.stats().evictions, 1u);
}

TEST(WorldGenerationTmxShadowCacheTest, SingleEntryLargerThanBudgetRefuses) {
  auto assets = makeAssets();
  WorldGenerationTmxShadowCache cache(16);
  auto result = cache.tryGetOrDecode(makeRequest("dungeons/a.json", 1), assets);
  EXPECT_EQ(WorldGenerationTmxShadowCacheStatus::OverBudget, result.status);
  EXPECT_EQ(0u, cache.size());
}

TEST(WorldGenerationTmxShadowCacheTest, DecodeFailureDoesNotInsert) {
  WorldGenerationTmxShadowCache cache(1 << 20);
  WorldGenerationTmxShadowAssetStore emptyAssets;
  auto result = cache.tryGetOrDecode(makeRequest("dungeons/a.json", 1), emptyAssets);
  EXPECT_EQ(WorldGenerationTmxShadowCacheStatus::DecodeFailed, result.status);
  EXPECT_EQ(0u, cache.size());
}

TEST(WorldGenerationTmxShadowCacheTest, InvalidKeyRejected) {
  WorldGenerationTmxShadowCache cache(1 << 20);
  WorldGenerationTmxShadowCacheKey key;
  EXPECT_EQ(WorldGenerationTmxShadowCacheStatus::InvalidKey, cache.tryGet(key).status);
}
