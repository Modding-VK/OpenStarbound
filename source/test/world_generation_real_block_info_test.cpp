#include "StarRoot.hpp"
#include "StarWorldTemplateTypes.hpp"
#include "StarWorldGenerationBlockInfo.hpp"
#include "StarWorldGenerationBaseTiles.hpp"
#include "StarWorldGenerationBaseTileInput.hpp"
#include "StarWorldGenerationLookupResolver.hpp"
#include "StarConstantSelector.hpp"
#include "StarFlatSurfaceSelector.hpp"
#include "StarWorldGenerationTestDirectory.hpp"

#include <array>
#include <cstring>
#include <future>
#include <tuple>
#include <vector>

#include "gtest/gtest.h"

namespace Star {
// Layout graph is controlled metadata, not a terrestrial/asset layout builder.
// No fixture constructor/tag for Root, WorldTemplate or selectors: all sampling
// methods and Constant/FlatSurface constructors/getters are production bodies.
struct WorldGenerationRealBlockInfoTestAccess {
  static WorldLayoutPtr layout(bool horizontal = false, bool vertical = false,
      float foregroundCave = -1, float backgroundCave = -1, bool subBlocks = false, bool ores = false) {
    auto out = std::make_shared<WorldLayout>(); out->m_worldSize = {64, 64}; out->m_regionBlending = 8;
    TerrainSelectorParameters params(JsonObject{{"worldWidth", 64}, {"baseHeight", 16}, {"seed", 1234}, {"commonality", 1}});
    auto constant = [&](float value) { return std::make_shared<ConstantSelector>(JsonObject{{"value", value}}, params); };
    out->m_terrainSelectors = {std::make_shared<FlatSurfaceSelector>(JsonObject{}, params),
      constant(foregroundCave), constant(backgroundCave), constant(-1), constant(1)};
    params.baseHeight = 40;
    out->m_terrainSelectors.append(std::make_shared<FlatSurfaceSelector>(JsonObject{}, params));
    out->m_terrainSelectors.append(constant(1));
    for (int i = 0; i != 2; ++i) {
      auto b = std::make_shared<Biome>(); b->mainBlock = MaterialId(21 + i); b->materialHueShift = MaterialHue(9 + i * 18);
      if (subBlocks) b->subBlocks = {MaterialId(21 + i), MaterialId(22 - i)};
      if (ores) { b->ores = {{31, 1}}; b->surfacePlaceables.grassMod = 31; }
      out->m_biomes.append(std::move(b));
    }
    auto region = [&](BiomeIndex biome, TerrainSelectorIndex terrain) {
      auto r = std::make_shared<WorldRegion>(); r->terrainSelectorIndex = terrain;
      r->foregroundCaveSelectorIndex = 2; r->backgroundCaveSelectorIndex = 3;
      r->blockBiomeIndex = r->environmentBiomeIndex = biome;
      // The inherited WorldRegion default omits liquids; initialize every field.
      r->regionLiquids = {0, 0, 0, 0, false, false};
      if (subBlocks) r->subBlockSelectorIndexes = {4, 5};
      if (ores) { r->foregroundOreSelectorIndexes = {7}; r->backgroundOreSelectorIndexes = {7}; }
      return r;
    };
    WorldLayout::WorldLayer lower; lower.yStart = 0; lower.cells.append(region(1, 1));
    if (horizontal) { lower.boundaries.append(32); lower.cells.append(region(2, 1)); }
    out->m_layers.append(std::move(lower));
    if (vertical) {
      WorldLayout::WorldLayer upper; upper.yStart = 32; upper.cells.append(region(2, 6)); out->m_layers.append(std::move(upper));
    }
    return out;
  }
  static WorldRegion& first(WorldLayout& layout) { return *layout.m_layers.first().cells.first(); }
  static size_t cacheSize(WorldTemplate const& world) { return world.m_blockCache.currentSize(); }
  static void clearCache(WorldTemplate& world) { world.m_blockCache.clear(); }
};
}

using namespace Star;
namespace {
using Access = WorldGenerationRealBlockInfoTestAccess;
using Inputs = WorldGenerationBlockInfoStaticInputs;
uint32_t bits(float value) { uint32_t out; std::memcpy(&out, &value, sizeof(out)); return out; }
auto fields(WorldTemplate::BlockInfo const& b) {
  return std::make_tuple(b.blockBiomeIndex, b.environmentBiomeIndex, b.biomeTransition, b.terrain,
    b.foregroundCave, b.backgroundCave, b.foreground, b.foregroundMod, b.background, b.backgroundMod,
    b.caveLiquid, bits(b.caveLiquidSeedDensity), b.oceanLiquid, b.oceanLiquidLevel, b.encloseLiquids, b.fillMicrodungeons);
}
// Test adapter composes the captured inputs with REAL layout/noise/selector
// queries. Layout is borrowed and must be private to each reader; not a frozen
// production layout/context or a substitute implementation of those methods.
struct RealSource {
  Inputs const& inputs;
  WorldLayoutConstPtr layout;
  WorldGeometry geometry{Vec2U{64, 64}};
  bool hasLayout() const { return bool(layout); }
  auto getWeighting(int x, int y) const { return layout->getWeighting(x, y); }
  int blendNoiseOffset(uint32_t x, uint32_t y) const {
    if (auto const& noise = layout->blendNoise()) return (int)noise->get(x, y);
    return 0;
  }
  Maybe<Vec2I> blockNoisePosition(uint32_t x, uint32_t y) const {
    if (auto const& noise = layout->blockNoise()) return noise->apply(Vec2I(x, y), geometry.size());
    return {};
  }
  float terrainSelectorValue(TerrainSelectorIndex index, int x, int y) const { return layout->getTerrainSelector(index)->get(x, y); }
  BiomeConstPtr biome(BiomeIndex index) const { return index == NullBiomeIndex ? BiomeConstPtr{} : layout->getBiome(index); }
  float configFloat(char const* key, float fallback) const { return inputs.configFloat(key, fallback); }
  pair<float, float> customTerrainWeighting(int x, int y) const { return inputs.customTerrainWeighting(x, y); }
  WorldTemplate::BlockInfo sample(int x, int y) { return calculateWorldGenerationBlockInfo(*this, geometry.xwrap(x), y); }
};
void initialize(ServerTile& tile) {
  tile.foreground = tile.background = NullMaterialId;
  tile.foregroundHueShift = tile.backgroundHueShift = 0;
  tile.foregroundMod = tile.backgroundMod = NoModId;
  tile.foregroundModHueShift = tile.backgroundModHueShift = 0;
  tile.foregroundColorVariant = tile.backgroundColorVariant = DefaultMaterialColorVariant;
  tile.collision = tile.objectCollision = CollisionKind::None; tile.collisionCacheDirty = false; tile.collisionCache.clear();
  tile.blockBiomeIndex = tile.environmentBiomeIndex = 0; tile.biomeTransition = false;
  tile.foregroundDamage.reset(); tile.backgroundDamage.reset(); tile.dungeonId = 0;
  tile.liquid = LiquidStore(); tile.rootSource = {};
}
WorldGenerationBaseTileInput tileInput(WorldTemplate::BlockInfo const& b, int y) {
  return {b.blockBiomeIndex, b.environmentBiomeIndex, b.biomeTransition, {b.foreground, b.foregroundMod},
    {b.background, b.backgroundMod}, b.oceanLiquid, b.oceanLiquidLevel, b.encloseLiquids, y};
}
class WorldGenerationRealBlockInfoTest : public testing::Test {
protected:
  Star::Test::GenerationTestDirectory source, storage;
  WorldGenerationMemoryLedger ledger;
  Json config = JsonObject{{"customTerrainBlendSize", 4}, {"customTerrainBlendWeight", 32}, {"blockCacheSize", 16},
    {"biomeTransitionThreshold", 0.75}, {"surfaceCaveAttenuationDist", 0}, {"surfaceCaveAttenuationFactor", 1}};
  void SetUp() override {
    ASSERT_EQ(RootBase::singletonPtr(), nullptr);
    source.write("preload.config", "[]"); source.write("_metadata", "{\"name\":\"real-block-info-fixture\"}");
    source.write("metamaterials.config", "[]"); source.write("client.config", "{\"defaultFootstepSound\":\"\"}");
    source.write("world_template.config", config.repr());
    for (int id : {21, 22}) {
      Json material = JsonObject{{"materialId", id}, {"materialName", String("realfixture") + toString(id)}, {"category", "fixture"},
        {"supportsMods", true}, {"damageTable", JsonObject{{"damageFactors", JsonObject{{"blockish", 1}}}, {"damageRecovery", 1}}}};
      source.write(String("probe") + toString(id) + ".material", material.repr());
    }
  }
  void TearDown() override { EXPECT_EQ(RootBase::singletonPtr(), nullptr); }
  std::unique_ptr<Root> root() const {
    Root::Settings s{}; s.assetsSettings.assetTimeToLive = 60; s.assetsSettings.audioDecompressLimit = 0; s.assetsSettings.workerPoolSize = 0;
    s.assetSources = {source.path()}; s.storageDirectory = storage.path();
    s.defaultConfiguration = JsonObject{{"configurationVersion", 1}, {"safe", JsonObject{{"allowRemoteAssets", false}}}};
    s.logLevel = LogLevel::Error; s.quiet = false; s.includeUGC = false;
    return std::make_unique<Root>(s);
  }
  std::unique_ptr<WorldTemplate> planet(WorldLayoutPtr layout = {}) const {
    auto result = std::make_unique<WorldTemplate>(Vec2U{64, 64}); if (layout) result->setWorldLayout(std::move(layout)); return result;
  }
  void grid(WorldTemplate& world, WorldLayoutPtr privateLayout) {
    auto inputs = world.captureBlockInfoStaticInputs(ledger, true); RealSource candidate{inputs, privateLayout};
    for (int x = -8; x <= 72; x += 2) for (int y = -2; y <= 66; y += 2)
      EXPECT_EQ(fields(world.blockInfo(x, y)), fields(candidate.sample(x, y))) << x << ',' << y;
  }
  void sector(WorldTemplate& world, WorldLayoutPtr privateLayout, LiquidId ocean = EmptyLiquidId) {
    auto inputs = world.captureBlockInfoStaticInputs(ledger, true); RealSource candidate{inputs, privateLayout};
    auto materials = Root::singleton().materialDatabase(); WorldGenerationLookupResolver resolver{world, *materials};
    auto expected = std::make_unique<ServerTileSectorArray::Array>(); auto actual = std::make_unique<ServerTileSectorArray::Array>();
    for (int x = 0; x != 32; ++x) for (int y = 0; y != 32; ++y) {
      auto normal = world.blockInfo(x, y), captured = candidate.sample(x, y); EXPECT_EQ(fields(normal), fields(captured));
      initialize((*expected)(x, y)); initialize((*actual)(x, y));
      applyWorldGenerationBaseTile((*expected)(x, y), tileInput(normal, y), resolver);
      applyWorldGenerationBaseTile((*actual)(x, y), tileInput(captured, y), resolver);
    }
    WorldGenerationBaseTilesIdentity identity{{1, 1, {0, 0}, {0, 0}, WorldGenerationTaskStage::BaseTiles},
      1, world.generationRevision(), {64, 64}, RectI(0, 0, 32, 32)};
    auto comparison = compareWorldGenerationBaseTiles(*expected, *actual, identity);
    EXPECT_TRUE(comparison.equal()); EXPECT_EQ(1024u, comparison.comparedTiles);
    EXPECT_EQ(21, (*actual)(8, 4).foreground); EXPECT_EQ(EmptyMaterialId, (*actual)(8, 20).foreground);
    EXPECT_EQ(MaterialHue(9), (*actual)(8, 4).foregroundHueShift);
    EXPECT_EQ(EmptyLiquidId, (*actual)(8, 4).liquid.liquid);
    auto const& liquid = (*actual)(8, 20).liquid;
    EXPECT_EQ(ocean, liquid.liquid);
    EXPECT_EQ(bits(ocean == EmptyLiquidId ? 0.0f : 1.0f), bits(liquid.level));
    EXPECT_EQ(bits(ocean == EmptyLiquidId ? 0.0f : 4.0f), bits(liquid.pressure));
    EXPECT_EQ(ocean != EmptyLiquidId, liquid.source);
  }
};
}

TEST_F(WorldGenerationRealBlockInfoTest, NormalConstructorLoadsRealAssetConfigAndSamplesNoLayout) {
  auto r = root(); auto world = planet(); EXPECT_EQ(Vec2U(64, 64), world->size());
  EXPECT_EQ(fields(WorldTemplate::BlockInfo{}), fields(world->blockInfo(7, 13))); EXPECT_EQ(1u, Access::cacheSize(*world));
}
TEST_F(WorldGenerationRealBlockInfoTest, FlatSurfaceUsesRealSelectorAndLayoutWeighting) {
  auto r = root(); auto world = planet(Access::layout()); grid(*world, Access::layout());
  EXPECT_TRUE(world->blockInfo(8, 4).terrain); EXPECT_FALSE(world->blockInfo(8, 20).terrain);
}
TEST_F(WorldGenerationRealBlockInfoTest, HorizontalBiomeTransitionsAndWrapAgree) {
  auto r = root(); auto world = planet(Access::layout(true)); grid(*world, Access::layout(true));
  EXPECT_EQ(1, world->blockInfo(16, 4).blockBiomeIndex); EXPECT_EQ(2, world->blockInfo(48, 4).blockBiomeIndex);
  EXPECT_TRUE(world->blockInfo(32, 4).biomeTransition); EXPECT_EQ(fields(world->blockInfo(63, 4)), fields(world->blockInfo(-1, 4)));
}
TEST_F(WorldGenerationRealBlockInfoTest, VerticalLayerBlendingUsesTheRealCells) {
  auto r = root(); auto world = planet(Access::layout(false, true)); grid(*world, Access::layout(false, true));
  EXPECT_EQ(2, world->blockInfo(16, 38).blockBiomeIndex); EXPECT_TRUE(world->blockInfo(16, 38).terrain);
}
TEST_F(WorldGenerationRealBlockInfoTest, ForegroundCaveLeavesActualBackgroundMaterial) {
  auto r = root(); auto world = planet(Access::layout(false, false, 1)); grid(*world, Access::layout(false, false, 1));
  auto b = world->blockInfo(8, 4); EXPECT_EQ(EmptyMaterialId, b.foreground); EXPECT_EQ(21, b.background);
}
TEST_F(WorldGenerationRealBlockInfoTest, BothCavesRemainEmpty) {
  auto r = root(); auto world = planet(Access::layout(false, false, 1, 1)); grid(*world, Access::layout(false, false, 1, 1));
  auto b = world->blockInfo(8, 4); EXPECT_EQ(EmptyMaterialId, b.foreground); EXPECT_EQ(EmptyMaterialId, b.background);
}
TEST_F(WorldGenerationRealBlockInfoTest, RealSubBlockAndOreSelectorsKeepFirstMatch) {
  auto r = root(); auto world = planet(Access::layout(false, false, -1, -1, true, true));
  grid(*world, Access::layout(false, false, -1, -1, true, true)); auto b = world->blockInfo(8, 4);
  EXPECT_EQ(22, b.foreground); EXPECT_EQ(31, b.foregroundMod); EXPECT_EQ(31, b.backgroundMod);
}
TEST_F(WorldGenerationRealBlockInfoTest, CacheHitDoesNotNeedRootButMissRetainsLegacyFailure) {
  auto r = root(); auto world = planet(Access::layout()); auto before = fields(world->blockInfo(8, 4)); r.reset();
  EXPECT_EQ(before, fields(world->blockInfo(8, 4))); EXPECT_THROW(world->blockInfo(9, 4), RootException);
}
TEST_F(WorldGenerationRealBlockInfoTest, CacheLimitEvictsTheOldestComputedPoint) {
  config = config.set("blockCacheSize", 2); source.write("world_template.config", config.repr());
  auto r = root(); auto world = planet(Access::layout()); world->blockInfo(1, 4); auto recent = fields(world->blockInfo(2, 4)); world->blockInfo(3, 4);
  EXPECT_EQ(2u, Access::cacheSize(*world)); r.reset(); EXPECT_EQ(recent, fields(world->blockInfo(2, 4))); EXPECT_THROW(world->blockInfo(1, 4), RootException);
}
TEST_F(WorldGenerationRealBlockInfoTest, CustomSpaceInvalidatesRealCacheAndOldCaptureStaysOld) {
  auto r = root(); auto world = planet(Access::layout()); auto old = world->captureBlockInfoStaticInputs(ledger, true);
  EXPECT_TRUE(world->blockInfo(7, 4).terrain); world->addCustomSpaceRegion(PolyF(RectF::withSize({0, 0}, {10, 10})));
  EXPECT_EQ(0u, Access::cacheSize(*world)); EXPECT_FALSE(world->blockInfo(7, 4).terrain);
  RealSource previous{old, Access::layout()}; EXPECT_TRUE(previous.sample(7, 4).terrain); grid(*world, Access::layout());
}
TEST_F(WorldGenerationRealBlockInfoTest, ClearCustomTerrainAndSetLayoutInvalidateRealCache) {
  auto r = root(); auto world = planet(Access::layout()); world->addCustomSpaceRegion(PolyF(RectF::withSize({0, 0}, {10, 10})));
  EXPECT_FALSE(world->blockInfo(7, 4).terrain); world->clearCustomTerrains(); EXPECT_EQ(0u, Access::cacheSize(*world)); EXPECT_TRUE(world->blockInfo(7, 4).terrain);
  world->setWorldLayout(Access::layout(false, false, 1, 1)); EXPECT_EQ(0u, Access::cacheSize(*world)); EXPECT_EQ(EmptyMaterialId, world->blockInfo(7, 4).foreground);
}
TEST_F(WorldGenerationRealBlockInfoTest, RealConfigFailureIsNotInsertedInCache) {
  config = config.set("biomeTransitionThreshold", "bad"); source.write("world_template.config", config.repr());
  auto r = root(); auto world = planet(Access::layout()); EXPECT_THROW(world->blockInfo(7, 4), JsonException); EXPECT_EQ(0u, Access::cacheSize(*world));
  EXPECT_THROW(world->blockInfo(7, 4), JsonException); EXPECT_EQ(0u, Access::cacheSize(*world));
  auto inputs = world->captureBlockInfoStaticInputs(ledger, true); RealSource candidate{inputs, Access::layout()}; EXPECT_THROW(candidate.sample(7, 4), JsonException);
}
TEST_F(WorldGenerationRealBlockInfoTest, NegativeYReturnsBeforeInvalidConfigIsQueried) {
  config = config.set("biomeTransitionThreshold", "bad"); source.write("world_template.config", config.repr());
  auto r = root(); auto world = planet(Access::layout()); EXPECT_EQ(fields(WorldTemplate::BlockInfo{}), fields(world->blockInfo(7, -1)));
}
TEST_F(WorldGenerationRealBlockInfoTest, UnreachedInvalidFactorRemainsLazyOnNormalPath) {
  config = config.set("surfaceCaveAttenuationDist", 0).set("surfaceCaveAttenuationFactor", "bad"); source.write("world_template.config", config.repr());
  auto r = root(); auto world = planet(Access::layout()); grid(*world, Access::layout());
}
TEST_F(WorldGenerationRealBlockInfoTest, InvalidRealSelectorIndexPropagatesWithoutCachedPartialResult) {
  auto r = root(); auto layout = Access::layout(); Access::first(*layout).terrainSelectorIndex = 999; auto world = planet(layout);
  EXPECT_THROW(world->blockInfo(7, 4), StarException); EXPECT_EQ(0u, Access::cacheSize(*world));
}
TEST_F(WorldGenerationRealBlockInfoTest, HotReloadDoesNotReplaceExistingTemplateConfigJson) {
  auto r = root(); auto world = planet(Access::layout()); auto old = fields(world->blockInfo(7, 4));
  config = config.set("biomeTransitionThreshold", "bad"); source.write("world_template.config", config.repr()); r->hotReload();
  Access::clearCache(*world); EXPECT_EQ(old, fields(world->blockInfo(7, 4)));
  auto fresh = planet(Access::layout()); EXPECT_THROW(fresh->blockInfo(7, 4), JsonException);
}
TEST_F(WorldGenerationRealBlockInfoTest, ThreePrivateRealLayoutsSampleAfterTemplateAndRootDestruction) {
  auto r = root(); auto world = planet(Access::layout(true, true)); auto inputs = world->captureBlockInfoStaticInputs(ledger, true);
  std::vector<decltype(fields(WorldTemplate::BlockInfo{}))> expected;
  for (int x = -4; x != 68; ++x) expected.push_back(fields(world->blockInfo(x, 12)));
  world.reset(); r.reset(); std::array<std::future<decltype(expected)>, 3> results;
  for (auto& result : results) result = std::async(std::launch::async, [&inputs] {
    RealSource candidate{inputs, Access::layout(true, true)}; decltype(expected) out;
    for (int x = -4; x != 68; ++x)
      out.push_back(fields(candidate.sample(x, 12)));
    return out;
  });
  for (auto& result : results) EXPECT_EQ(expected, result.get());
}
TEST_F(WorldGenerationRealBlockInfoTest, BaseTiles32x32MatchesAll25FieldsUsingRealMaterialParserAndResolver) {
  auto r = root(); auto world = planet(Access::layout()); sector(*world, Access::layout());
}
TEST_F(WorldGenerationRealBlockInfoTest, BaseTiles32x32WithOceanMetadataMatchesTheNormalSamplingRoute) {
  auto r = root(); auto normal = Access::layout(), captured = Access::layout();
  Access::first(*normal).regionLiquids = Access::first(*captured).regionLiquids = {0, 0, 3, 24, false, false};
  auto world = planet(normal); sector(*world, captured, 3);
}
