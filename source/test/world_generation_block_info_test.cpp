#include "StarWorldGenerationBlockInfo.hpp"
#include "gtest/gtest.h"

#include <array>
#include <algorithm>
#include <cstring>
#include <future>
#include <stdexcept>
#include <string>
#include <tuple>
#include <typeinfo>
#include <type_traits>
#include <vector>

namespace Star {
// Controlled metadata only. Factory/custom terrain/Json/ledger bodies are real;
// no normal WorldTemplate constructor, layout/Root/terrain selector simulation.
struct WorldGenerationBlockInfoStaticInputsTestAccess {
  static WorldTemplatePtr planet(Json config) {
    auto result = WorldTemplatePtr(new WorldTemplate(WorldTemplate::LookupFixtureTag{}));
    result->m_templateConfig = std::move(config); result->m_geometry = WorldGeometry(Vec2U{100, 100});
    result->m_customTerrainBlendSize = 10; result->m_customTerrainBlendWeight = 8;
    return result;
  }
  static void replaceConfig(WorldTemplate& world, Json config) {
    world.m_templateConfig = std::move(config); ++world.m_generationRevision;
  }
};
}

using namespace Star;
namespace {

// Scripted input/query boundary, not a WorldLayout/Perlin implementation or a
// replacement production symbol. Distinct instances keep traces independent.
struct Region {
  TerrainSelectorIndex terrainSelectorIndex = 1;
  TerrainSelectorIndex foregroundCaveSelectorIndex = 2;
  TerrainSelectorIndex backgroundCaveSelectorIndex = 3;
  BiomeIndex blockBiomeIndex = 1, environmentBiomeIndex = 2;
  List<TerrainSelectorIndex> subBlockSelectorIndexes{4, 5};
  List<TerrainSelectorIndex> foregroundOreSelectorIndexes{6, 7};
  List<TerrainSelectorIndex> backgroundOreSelectorIndexes{8, 9};
  WorldRegionLiquids regionLiquids{3, 0.25f, 4, 50, false, true};
};
struct Weighting { float weight; int xValue; Region const* region; };
struct Fixture {
  std::vector<std::string> trace;
  size_t failAt = 0, weightingCall = 0;
  bool layoutPresent = true, blendPresent = false, blockPresent = false;
  float blendValue = 3.75f;
  Vec2I blockPosition{17, 23};
  WorldGeometry geometry{Vec2U{2048, 1024}};
  Region flatRegion, blockRegion;
  std::array<List<Weighting>, 3> weights;
  std::array<float, 10> values{{0, 10, -1, -1, -1, 1, -1, 1, -1, 1}};
  pair<float, float> custom{0, 0};
  BiomePtr blockBiome = std::make_shared<Biome>();
  Json config = JsonObject();

  void event(std::string value) {
    trace.push_back(std::move(value));
    if (failAt && trace.size() == failAt)
      throw std::runtime_error("query failure");
  }
  static std::string coords(int x, int y) { return std::to_string(x) + ":" + std::to_string(y); }
  struct Selector {
    Fixture* f; TerrainSelectorIndex index;
    float get(int x, int y) const {
      f->event("value:" + std::to_string(index) + ":" + coords(x, y));
      return f->values.at(index);
    }
  };
  struct Blend {
    Fixture* f;
    float get(uint32_t x, uint32_t y) const { f->event("blend.get:" + coords(x, y)); return f->blendValue; }
  };
  struct Noise {
    Fixture* f;
    Vec2I apply(Vec2I p, Vec2U size) const {
      f->event("block.apply:" + coords(p[0], p[1]) + ":" + coords(size[0], size[1]));
      return f->blockPosition;
    }
  };
  struct Layout {
    Fixture* f;
    List<Weighting> getWeighting(int x, int y) const {
      f->event("weighting:" + coords(x, y)); return f->weights.at(f->weightingCall++);
    }
    Maybe<Blend> blendNoise() const {
      f->event("blend"); if (f->blendPresent) return Blend{f}; return {};
    }
    Maybe<Noise> blockNoise() const {
      f->event("block"); if (f->blockPresent) return Noise{f}; return {};
    }
    std::shared_ptr<Selector> getTerrainSelector(TerrainSelectorIndex index) const {
      f->event("selector:" + std::to_string(index)); return std::make_shared<Selector>(Selector{f, index});
    }
  } layout{this};
  struct Config {
    Fixture* f;
    float getFloat(char const* key, float fallback) const {
      f->event(std::string("config:") + key); return f->config.getFloat(key, fallback);
    }
  } configuration{this};

  Fixture() {
    weights[0] = {{1, 11, &flatRegion}};
    weights[1] = {{1, 29, &blockRegion}};
    weights[2] = {{0.25f, 17, &blockRegion}};
    blockRegion.blockBiomeIndex = 3;
    blockRegion.environmentBiomeIndex = 4;
    blockRegion.regionLiquids = {5, 0.75f, 6, 100, true, false};
    blockBiome->mainBlock = 21; blockBiome->subBlocks = {22, 23};
    blockBiome->ores = {{31, 0.5f}, {32, 0.25f}};
  }
  Fixture(Fixture const&) = delete;
  Fixture& operator=(Fixture const&) = delete;
  bool hasLayout() const { return layoutPresent; }
  auto getWeighting(int x, int y) { return layout.getWeighting(x, y); }
  int blendNoiseOffset(uint32_t x, uint32_t y) {
    if (auto noise = layout.blendNoise()) return (int)noise->get(x, y);
    return 0;
  }
  Maybe<Vec2I> blockNoisePosition(uint32_t x, uint32_t y) {
    if (auto noise = layout.blockNoise()) return noise->apply(Vec2I(x, y), geometry.size());
    return {};
  }
  float configFloat(char const* key, float fallback) { return configuration.getFloat(key, fallback); }
  float terrainSelectorValue(TerrainSelectorIndex index, int x, int y) { return layout.getTerrainSelector(index)->get(x, y); }
  pair<float, float> customTerrainWeighting(int x, int y) { event("custom:" + coords(x, y)); return custom; }
  BiomeConstPtr biome(BiomeIndex index) { event("biome:" + std::to_string(index)); return blockBiome; }
};

// Frozen inherited algorithm, independent of the new kernel. Kept separately
// so changes to branching/order/arithmetic are not self-comparisons.
WorldTemplate::BlockInfo legacyBlockInfo(Fixture& f, uint32_t x, uint32_t y);

uint32_t bits(float value) { uint32_t result; std::memcpy(&result, &value, sizeof(result)); return result; }
auto fields(WorldTemplate::BlockInfo const& b) {
  return std::make_tuple(b.blockBiomeIndex, b.environmentBiomeIndex, b.biomeTransition,
    b.terrain, b.foregroundCave, b.backgroundCave, b.foreground, b.foregroundMod,
    b.background, b.backgroundMod, b.caveLiquid, bits(b.caveLiquidSeedDensity),
    b.oceanLiquid, b.oceanLiquidLevel, b.encloseLiquids, b.fillMicrodungeons);
}
template <typename Configure>
WorldTemplate::BlockInfo compare(Configure configure, uint32_t x = 7, uint32_t y = 13) {
  Fixture old, current; configure(old); configure(current);
  auto expected = legacyBlockInfo(old, x, y);
  auto actual = calculateWorldGenerationBlockInfo(current, x, y);
  EXPECT_EQ(fields(expected), fields(actual)); EXPECT_EQ(old.trace, current.trace);
  return actual;
}
template <typename Configure>
void compareFailure(Configure configure) {
  Fixture old, current; configure(old); configure(current);
  bool oldFailed = false, currentFailed = false;
  std::string oldType, currentType, oldMessage, currentMessage;
  try { legacyBlockInfo(old, 7, 13); } catch (std::exception const& e) {
    oldFailed = true; oldType = typeid(e).name(); oldMessage = e.what();
  }
  try { calculateWorldGenerationBlockInfo(current, 7, 13); } catch (std::exception const& e) {
    currentFailed = true; currentType = typeid(e).name(); currentMessage = e.what();
  }
  EXPECT_TRUE(oldFailed); EXPECT_TRUE(currentFailed); EXPECT_EQ(old.trace, current.trace);
  EXPECT_EQ(oldType, currentType); EXPECT_EQ(oldMessage, currentMessage);
}

TEST(WorldGenerationBlockInfoTest, NoLayoutDoesNotQuery) {
  auto b = compare([](Fixture& f) { f.layoutPresent = false; });
  EXPECT_EQ(fields(WorldTemplate::BlockInfo{}), fields(b));
  Fixture f; f.layoutPresent = false; calculateWorldGenerationBlockInfo(f, 0, 0); EXPECT_TRUE(f.trace.empty());
}
TEST(WorldGenerationBlockInfoTest, EmptyFlatStillEvaluatesNoiseBeforeReturn) {
  compare([](Fixture& f) { f.weights[0].clear(); f.blendPresent = f.blockPresent = true; });
  Fixture f; f.weights[0].clear(); calculateWorldGenerationBlockInfo(f, 7, 13);
  EXPECT_EQ((std::vector<std::string>{"weighting:7:13", "blend", "block"}), f.trace);
}
TEST(WorldGenerationBlockInfoTest, EmptyBlockStillQueriesTransition) {
  auto b = compare([](Fixture& f) { f.blockPresent = true; f.weights[1].clear(); });
  EXPECT_EQ(fields(WorldTemplate::BlockInfo{}), fields(b));
}
TEST(WorldGenerationBlockInfoTest, NoNoiseUsesOnlyFlatWeighting) {
  auto b = compare([](Fixture&) {}); EXPECT_TRUE(b.terrain); EXPECT_EQ(1, b.blockBiomeIndex);
  EXPECT_EQ(23, b.foreground); EXPECT_EQ(32, b.foregroundMod);
}
TEST(WorldGenerationBlockInfoTest, NoisyBiomesUseIndependentFlatAndBlockInputs) {
  auto b = compare([](Fixture& f) { f.blendPresent = f.blockPresent = true; });
  EXPECT_EQ(3, b.blockBiomeIndex); EXPECT_EQ(2, b.environmentBiomeIndex);
  Fixture f; f.blendPresent = f.blockPresent = true; calculateWorldGenerationBlockInfo(f, 7, 13);
  EXPECT_NE(f.trace.end(), std::find(f.trace.begin(), f.trace.end(), "weighting:20:23"));
  EXPECT_NE(f.trace.end(), std::find(f.trace.begin(), f.trace.end(), "value:4:26:23"));
  EXPECT_NE(f.trace.end(), std::find(f.trace.begin(), f.trace.end(), "value:6:7:13"));
}
TEST(WorldGenerationBlockInfoTest, TransitionThresholdIsStrict) {
  EXPECT_FALSE(compare([](Fixture& f) { f.config = JsonObject{{"biomeTransitionThreshold", 1}}; }).biomeTransition);
  EXPECT_TRUE(compare([](Fixture& f) { f.config = JsonObject{{"biomeTransitionThreshold", 1.25}}; }).biomeTransition);
}
TEST(WorldGenerationBlockInfoTest, WeightingPreservesOrderAndNullSelectors) {
  compare([](Fixture& f) { f.weights[0] = {{0.7f, -9, &f.flatRegion}, {0.3f, 900, &f.blockRegion}}; f.blockRegion.terrainSelectorIndex = 0; });
}
TEST(WorldGenerationBlockInfoTest, WeightedCancellationMustNotBeReassociated) {
  auto b = compare([](Fixture& f) {
    f.blockRegion.terrainSelectorIndex = 2; f.values[1] = 2.0e20f; f.values[2] = -2.0e20f;
    f.weights[0] = {{0.5f, 11, &f.flatRegion}, {0.5f, 12, &f.blockRegion}, {5.0e-21f, 13, &f.flatRegion}};
  });
  EXPECT_TRUE(b.terrain);
}
TEST(WorldGenerationBlockInfoTest, CustomTerrainCanCreateSolidSpace) {
  EXPECT_TRUE(compare([](Fixture& f) { f.values[1] = -10; f.custom = {20, 1}; }).terrain);
}
TEST(WorldGenerationBlockInfoTest, CustomSpaceCanRemoveSolidTerrain) {
  EXPECT_FALSE(compare([](Fixture& f) { f.custom = {-20, 1}; }).terrain);
}
TEST(WorldGenerationBlockInfoTest, NonpositiveTerrainSkipsCavesAndAttenuation) {
  for (float value : {-1.0f, 0.0f}) compare([&](Fixture& f) { f.values[1] = value; f.config = JsonObject{{"surfaceCaveAttenuationDist", "invalid"}}; });
}
TEST(WorldGenerationBlockInfoTest, AttenuationFactorIsLazyAtStrictBoundary) {
  compare([](Fixture& f) { f.config = JsonObject{{"surfaceCaveAttenuationDist", 10}, {"surfaceCaveAttenuationFactor", "invalid"}}; });
  EXPECT_FALSE(compare([](Fixture& f) { f.values[2] = f.values[3] = 1; f.config = JsonObject{{"surfaceCaveAttenuationDist", 12}, {"surfaceCaveAttenuationFactor", 2}}; }).foregroundCave);
}
TEST(WorldGenerationBlockInfoTest, ForegroundCaveStillBuildsBackground) {
  auto b = compare([](Fixture& f) { f.values[2] = 1; });
  EXPECT_EQ(EmptyMaterialId, b.foreground); EXPECT_EQ(23, b.background); EXPECT_EQ(NoModId, b.foregroundMod);
}
TEST(WorldGenerationBlockInfoTest, BackgroundCaveDoesNotRemoveSolidForegroundBackground) {
  auto b = compare([](Fixture& f) { f.values[3] = 1; });
  EXPECT_EQ(23, b.foreground); EXPECT_EQ(23, b.background); EXPECT_EQ(NoModId, b.backgroundMod);
}
TEST(WorldGenerationBlockInfoTest, BothCavesStillConsultSubBlocksButSkipOres) {
  auto b = compare([](Fixture& f) { f.values[2] = f.values[3] = 1; });
  EXPECT_EQ(EmptyMaterialId, b.foreground); EXPECT_EQ(EmptyMaterialId, b.background);
}
TEST(WorldGenerationBlockInfoTest, LiquidMetadataComesFromFlatNotBlockRegion) {
  auto b = compare([](Fixture& f) { f.blockPresent = true; });
  EXPECT_EQ(3, b.caveLiquid); EXPECT_EQ(bits(0.25f), bits(b.caveLiquidSeedDensity)); EXPECT_EQ(4, b.oceanLiquid); EXPECT_EQ(50, b.oceanLiquidLevel);
}
TEST(WorldGenerationBlockInfoTest, OceanEnclosureHasStrictHeightBoundary) {
  auto set = [](Fixture& f) { f.values[1] = -1; f.flatRegion.regionLiquids.encloseLiquids = true; };
  EXPECT_TRUE(compare(set, 7, 49).terrain); EXPECT_TRUE(compare(set, 7, 49).foregroundCave);
  EXPECT_FALSE(compare(set, 7, 50).terrain);
}
TEST(WorldGenerationBlockInfoTest, AbsentBiomeDoesNotQueryMaterialSelectors) {
  auto b = compare([](Fixture& f) { f.blockBiome.reset(); }); EXPECT_TRUE(b.terrain); EXPECT_EQ(EmptyMaterialId, b.foreground);
}
TEST(WorldGenerationBlockInfoTest, SubBlockFirstMatchSkipsLaterInvalidIndex) {
  EXPECT_EQ(22, compare([](Fixture& f) { f.values[4] = 1; f.flatRegion.subBlockSelectorIndexes = {4}; }).foreground);
}
TEST(WorldGenerationBlockInfoTest, OreFirstMatchSkipsLaterInvalidIndex) {
  auto b = compare([](Fixture& f) { f.values[6] = f.values[8] = 1; f.flatRegion.foregroundOreSelectorIndexes = {6}; f.flatRegion.backgroundOreSelectorIndexes = {8}; });
  EXPECT_EQ(31, b.foregroundMod); EXPECT_EQ(31, b.backgroundMod);
}
TEST(WorldGenerationBlockInfoTest, EmptySelectorListsLeaveMainMaterialAndNoMod) {
  auto b = compare([](Fixture& f) { f.flatRegion.subBlockSelectorIndexes.clear(); f.flatRegion.foregroundOreSelectorIndexes.clear(); f.flatRegion.backgroundOreSelectorIndexes.clear(); });
  EXPECT_EQ(21, b.foreground); EXPECT_EQ(NoModId, b.foregroundMod);
}
TEST(WorldGenerationBlockInfoTest, InvalidUsedConfigFailsAtSameQuery) {
  compareFailure([](Fixture& f) { f.config = JsonObject{{"biomeTransitionThreshold", "invalid"}}; });
  compareFailure([](Fixture& f) { f.config = JsonObject{{"surfaceCaveAttenuationDist", "invalid"}}; });
  compareFailure([](Fixture& f) { f.config = JsonObject{{"surfaceCaveAttenuationDist", 12}, {"surfaceCaveAttenuationFactor", "invalid"}}; });
}
TEST(WorldGenerationBlockInfoTest, ShortSubBlockListFailsOnlyWhenReached) {
  compareFailure([](Fixture& f) { f.flatRegion.subBlockSelectorIndexes = {4}; });
}
TEST(WorldGenerationBlockInfoTest, ShortOreListFailsOnlyWhenReached) {
  compareFailure([](Fixture& f) { f.flatRegion.foregroundOreSelectorIndexes = {6}; });
}
TEST(WorldGenerationBlockInfoTest, EmptyTransitionListFailsWithoutEagerConfigValidation) {
  compareFailure([](Fixture& f) { f.blockPresent = true; f.weights[2].clear(); });
}
TEST(WorldGenerationBlockInfoTest, EveryQueryFailureStopsAtSameBoundary) {
  Fixture probe; probe.blendPresent = probe.blockPresent = true; calculateWorldGenerationBlockInfo(probe, 7, 13);
  for (size_t i = 1; i <= probe.trace.size(); ++i) {
    SCOPED_TRACE(i); compareFailure([&](Fixture& f) { f.blendPresent = f.blockPresent = true; f.failAt = i; });
  }
}
TEST(WorldGenerationBlockInfoTest, SignedCoordinateConversionPreservesExistingPath) {
  compare([](Fixture& f) { f.blendPresent = f.blockPresent = true; }, uint32_t(-1), uint32_t(-7));
}
TEST(WorldGenerationBlockInfoTest, DeterministicInputMatrixMatchesAllFieldsAndQueries) {
  // 4,096 input combinations in ONE GTest case, not 4,096 unique tests.
  for (unsigned mask = 0; mask != 4096; ++mask) {
    SCOPED_TRACE(mask);
    compare([&](Fixture& f) {
      f.blendPresent = mask & 1; f.blockPresent = mask & 2;
      f.values[1] = mask & 4 ? -1 : 10; f.values[2] = mask & 8 ? 1 : -1; f.values[3] = mask & 16 ? 1 : -1;
      f.values[4] = mask & 32 ? 1 : -1; f.values[6] = mask & 64 ? 1 : -1; f.values[8] = mask & 128 ? 1 : -1;
      f.custom = mask & 256 ? pair<float, float>{-2, 0.5f} : pair<float, float>{0, 0};
      f.flatRegion.regionLiquids.encloseLiquids = mask & 512;
      f.config = JsonObject{{"surfaceCaveAttenuationDist", mask & 1024 ? 12 : 0}, {"biomeTransitionThreshold", mask & 2048 ? 1.25 : 0}};
    });
  }
}

using StaticInputs = WorldGenerationBlockInfoStaticInputs;
using StaticAccess = WorldGenerationBlockInfoStaticInputsTestAccess;
static_assert(!std::is_copy_constructible<StaticInputs>::value, "No deep copy per job");
static_assert(std::is_move_constructible<StaticInputs>::value, "Capture supports handoff");

struct CapturedFixture : Fixture {
  explicit CapturedFixture(StaticInputs const& inputs) : inputs(inputs) {}
  float configFloat(char const* key, float fallback) {
    event(std::string("config:") + key); return inputs.configFloat(key, fallback);
  }
  pair<float, float> customTerrainWeighting(int x, int y) {
    event("custom:" + coords(x, y)); return inputs.customTerrainWeighting(x, y);
  }
  StaticInputs const& inputs;
};
struct BlockResult {
  WorldTemplate::BlockInfo value;
  bool failed = false;
  std::string type, message;
};
template <typename Calculate> BlockResult evaluate(Calculate calculate) {
  BlockResult result;
  try { result.value = calculate(); }
  catch (std::exception const& e) { result.failed = true; result.type = typeid(e).name(); result.message = e.what(); }
  return result;
}
template <typename Configure>
BlockResult compareStatic(Json config, StaticInputs const& inputs, Configure configure, int x = 7, int y = 13) {
  Fixture old; CapturedFixture current(inputs);
  old.config = std::move(config);
  // Isolate config/control-flow equivalence: both receive the same custom
  // weighting as an input. The 26 custom-terrain cases independently compare
  // its real geometry/math to the inherited reference, not this fixture.
  old.custom = inputs.customTerrainWeighting(x, y);
  configure(old); configure(current);
  auto expected = evaluate([&] { return legacyBlockInfo(old, x, y); });
  auto actual = evaluate([&] { return calculateWorldGenerationBlockInfo(current, x, y); });
  EXPECT_EQ(expected.failed, actual.failed); EXPECT_EQ(expected.type, actual.type); EXPECT_EQ(expected.message, actual.message);
  if (!expected.failed && !actual.failed) {
    EXPECT_EQ(fields(expected.value), fields(actual.value));
  }
  EXPECT_EQ(old.trace, current.trace);
  return actual;
}
auto captureConfig(Json config, WorldGenerationMemoryLedger& ledger, bool measure = true, size_t limit = 32768) {
  return StaticAccess::planet(std::move(config))->captureBlockInfoStaticInputs(ledger, measure, limit);
}
Json numericConfig() { return JsonObject{{"biomeTransitionThreshold", 0.75}, {"surfaceCaveAttenuationDist", 12}, {"surfaceCaveAttenuationFactor", 2}}; }
TEST(WorldGenerationBlockInfoStaticInputsTest, EmptyObjectPreservesCallerFallbacks) {
  WorldGenerationMemoryLedger ledger; auto inputs = captureConfig(JsonObject{}, ledger);
  EXPECT_EQ(bits(0), bits(inputs.configFloat("biomeTransitionThreshold", 0)));
  EXPECT_EQ(bits(1), bits(inputs.configFloat("surfaceCaveAttenuationFactor", 1)));
  EXPECT_EQ(bits(-0.0f), bits(inputs.configFloat("missing", -0.0f)));
}
TEST(WorldGenerationBlockInfoStaticInputsTest, NullValuesKeepFallbackRatherThanThrow) {
  WorldGenerationMemoryLedger ledger; Json config = JsonObject{{"biomeTransitionThreshold", Json()}};
  auto inputs = captureConfig(config, ledger);
  EXPECT_EQ(bits(0.25f), bits(inputs.configFloat("biomeTransitionThreshold", 0.25f)));
  EXPECT_FALSE(compareStatic(config, inputs, [](Fixture&) {}).failed);
}
TEST(WorldGenerationBlockInfoStaticInputsTest, IntegerFloatAndSignedZeroKeepGetFloatBits) {
  WorldGenerationMemoryLedger ledger;
  for (Json value : {Json(12), Json(0.1), Json(-0.0), Json(-17.5)}) {
    Json config = JsonObject{{"biomeTransitionThreshold", value}}; auto inputs = captureConfig(config, ledger);
    EXPECT_EQ(bits(config.getFloat("biomeTransitionThreshold", 0)), bits(inputs.configFloat("biomeTransitionThreshold", 0)));
  }
}
TEST(WorldGenerationBlockInfoStaticInputsTest, InvalidValueTypesAreNotValidatedAtCapture) {
  WorldGenerationMemoryLedger ledger;
  for (Json value : {Json("bad"), Json(false), Json(JsonArray{}), Json(JsonObject{})}) {
    auto inputs = captureConfig(JsonObject{{"surfaceCaveAttenuationFactor", value}}, ledger);
    EXPECT_TRUE(inputs.jsonStorageMeasured()); EXPECT_THROW(inputs.configFloat("surfaceCaveAttenuationFactor", 1), JsonException);
  }
}
TEST(WorldGenerationBlockInfoStaticInputsTest, InvalidRootTypesAreRetainedUntilAQuery) {
  WorldGenerationMemoryLedger ledger;
  for (Json config : {Json(), Json(3), Json(false), Json("bad root"), Json(JsonArray{})}) {
    auto inputs = captureConfig(config, ledger);
    EXPECT_FALSE(compareStatic(config, inputs, [](Fixture& f) { f.layoutPresent = false; }).failed);
    EXPECT_TRUE(compareStatic(config, inputs, [](Fixture&) {}).failed);
  }
}
TEST(WorldGenerationBlockInfoStaticInputsTest, JsonModelCompletenessDoesNotValidateSchema) {
  WorldGenerationMemoryLedger ledger; auto inputs = captureConfig(Json(3), ledger);
  EXPECT_TRUE(inputs.jsonStorageModelComplete()); EXPECT_THROW(inputs.configFloat("biomeTransitionThreshold", 0), JsonException);
}
TEST(WorldGenerationBlockInfoStaticInputsTest, NoLayoutSkipsEvenInvalidConfig) {
  WorldGenerationMemoryLedger ledger; auto config = Json("invalid root"); auto inputs = captureConfig(config, ledger);
  EXPECT_FALSE(compareStatic(config, inputs, [](Fixture& f) { f.layoutPresent = false; }).failed);
}
TEST(WorldGenerationBlockInfoStaticInputsTest, EmptyWeightsSkipInvalidConfig) {
  WorldGenerationMemoryLedger ledger; auto config = Json("invalid root"); auto inputs = captureConfig(config, ledger);
  EXPECT_FALSE(compareStatic(config, inputs, [](Fixture& f) { f.weights[0].clear(); f.blockPresent = true; }).failed);
}
TEST(WorldGenerationBlockInfoStaticInputsTest, NonpositiveTerrainSkipsInvalidAttenuation) {
  WorldGenerationMemoryLedger ledger;
  Json config = JsonObject{{"surfaceCaveAttenuationDist", "bad"}, {"surfaceCaveAttenuationFactor", "bad"}};
  auto inputs = captureConfig(config, ledger);
  EXPECT_FALSE(compareStatic(config, inputs, [](Fixture& f) { f.values[1] = 0; }).failed);
}
TEST(WorldGenerationBlockInfoStaticInputsTest, StrictDistanceBoundarySkipsInvalidFactor) {
  WorldGenerationMemoryLedger ledger;
  Json config = JsonObject{{"surfaceCaveAttenuationDist", 10}, {"surfaceCaveAttenuationFactor", "bad"}};
  auto inputs = captureConfig(config, ledger);
  EXPECT_FALSE(compareStatic(config, inputs, [](Fixture&) {}).failed);
}
TEST(WorldGenerationBlockInfoStaticInputsTest, ReachedInvalidThresholdHasSameExceptionAndTrace) {
  WorldGenerationMemoryLedger ledger; Json config = JsonObject{{"biomeTransitionThreshold", "bad"}};
  auto inputs = captureConfig(config, ledger); EXPECT_TRUE(compareStatic(config, inputs, [](Fixture&) {}).failed);
}
TEST(WorldGenerationBlockInfoStaticInputsTest, ReachedInvalidDistanceHasSameExceptionAndTrace) {
  WorldGenerationMemoryLedger ledger; Json config = JsonObject{{"surfaceCaveAttenuationDist", "bad"}};
  auto inputs = captureConfig(config, ledger); EXPECT_TRUE(compareStatic(config, inputs, [](Fixture&) {}).failed);
}
TEST(WorldGenerationBlockInfoStaticInputsTest, ReachedInvalidFactorHasSameExceptionAndTrace) {
  WorldGenerationMemoryLedger ledger;
  Json config = JsonObject{{"surfaceCaveAttenuationDist", 12}, {"surfaceCaveAttenuationFactor", "bad"}};
  auto inputs = captureConfig(config, ledger); EXPECT_TRUE(compareStatic(config, inputs, [](Fixture&) {}).failed);
}
TEST(WorldGenerationBlockInfoStaticInputsTest, ConfigDefaultsAndCapturedGeometryComposeInKernel) {
  WorldGenerationMemoryLedger ledger; auto world = StaticAccess::planet(JsonObject{});
  world->addCustomTerrainRegion(PolyF(RectF::withSize({0, 10}, {10, 10})));
  auto inputs = world->captureBlockInfoStaticInputs(ledger, true); world.reset();
  auto result = compareStatic(JsonObject{}, inputs, [](Fixture& f) { f.values[1] = -10; });
  EXPECT_FALSE(result.failed); EXPECT_TRUE(result.value.terrain); EXPECT_EQ(1u, inputs.customTerrainRegionCount());
}
TEST(WorldGenerationBlockInfoStaticInputsTest, SourceReplacementDoesNotChangeOldConfiguration) {
  WorldGenerationMemoryLedger ledger; auto world = StaticAccess::planet(numericConfig());
  auto old = world->captureBlockInfoStaticInputs(ledger, true);
  StaticAccess::replaceConfig(*world, JsonObject{{"biomeTransitionThreshold", 0.25}});
  EXPECT_EQ(bits(0.75f), bits(old.configFloat("biomeTransitionThreshold", 0)));
  auto current = world->captureBlockInfoStaticInputs(ledger, true);
  EXPECT_EQ(bits(0.25f), bits(current.configFloat("biomeTransitionThreshold", 0)));
  EXPECT_EQ(0u, old.templateRevision()); EXPECT_EQ(1u, current.templateRevision());
}
TEST(WorldGenerationBlockInfoStaticInputsTest, SourceClearDoesNotChangeCapturedCustomTerrain) {
  WorldGenerationMemoryLedger ledger; auto world = StaticAccess::planet(numericConfig());
  world->addCustomTerrainRegion(PolyF(RectF::withSize({0, 10}, {10, 10})));
  auto inputs = world->captureBlockInfoStaticInputs(ledger, true); world->clearCustomTerrains();
  EXPECT_EQ(bits(8), bits(inputs.customTerrainWeighting(7, 13).first)); EXPECT_EQ(1u, inputs.templateRevision());
  EXPECT_EQ(0u, world->captureBlockInfoStaticInputs(ledger, true).customTerrainRegionCount());
}
TEST(WorldGenerationBlockInfoStaticInputsTest, ConfigAndTemplateSurviveOnlyForTheirOwnLifetimes) {
  WorldGenerationMemoryLedger ledger; std::weak_ptr<WorldTemplate> worldOwner; std::weak_ptr<JsonObject const> jsonOwner;
  auto inputs = [&] {
    Json config = numericConfig(); jsonOwner = config.objectPtr(); auto world = StaticAccess::planet(config); worldOwner = world;
    return std::make_unique<StaticInputs>(world->captureBlockInfoStaticInputs(ledger, true));
  }();
  EXPECT_TRUE(worldOwner.expired()); EXPECT_FALSE(jsonOwner.expired()); EXPECT_GT(ledger.snapshot().estimatedBytes, 0u);
  EXPECT_EQ(bits(0.75f), bits(inputs->configFloat("biomeTransitionThreshold", 0)));
  inputs.reset(); EXPECT_TRUE(jsonOwner.expired()); EXPECT_EQ(0u, ledger.snapshot().reservedBytes);
}
TEST(WorldGenerationBlockInfoStaticInputsTest, UnmeasuredCaptureRetainsDataWithoutJsonCharges) {
  WorldGenerationMemoryLedger ledger; auto inputs = captureConfig(numericConfig(), ledger, false);
  EXPECT_FALSE(inputs.jsonStorageMeasured()); EXPECT_FALSE(inputs.jsonStorageModelComplete());
  EXPECT_EQ(0u, ledger.snapshot().reservedBytes); EXPECT_EQ(bits(12), bits(inputs.configFloat("surfaceCaveAttenuationDist", 0)));
}
TEST(WorldGenerationBlockInfoStaticInputsTest, FullJsonOwnerIsChargedOnceAcrossTwoCaptures) {
  WorldGenerationMemoryLedger ledger; auto world = StaticAccess::planet(numericConfig());
  auto first = std::make_unique<StaticInputs>(world->captureBlockInfoStaticInputs(ledger, true));
  auto bytes = ledger.snapshot().estimatedBytes; ASSERT_GT(bytes, 0u);
  auto second = std::make_unique<StaticInputs>(world->captureBlockInfoStaticInputs(ledger, true));
  EXPECT_TRUE(first->jsonStorageModelComplete()); EXPECT_TRUE(second->jsonStorageModelComplete());
  EXPECT_EQ(bytes, ledger.snapshot().estimatedBytes); EXPECT_EQ(1u, ledger.snapshot().sharedResources);
  first.reset(); EXPECT_EQ(bytes, ledger.snapshot().estimatedBytes); second.reset(); EXPECT_EQ(0u, ledger.snapshot().estimatedBytes);
  // Untracked source ownership remains outside the tracked-handle scope.
  EXPECT_EQ(bits(0.75f), bits(world->captureBlockInfoStaticInputs(ledger, false).configFloat("biomeTransitionThreshold", 0)));
}
TEST(WorldGenerationBlockInfoStaticInputsTest, StructuralMeasurementRetainsAndCountsUnusedValues) {
  WorldGenerationMemoryLedger ledger; Json leaf(std::string(2048, 'x'));
  Json config = numericConfig().set("unused", JsonObject{{"a", leaf}, {"b", leaf}});
  auto inputs = captureConfig(config, ledger); EXPECT_TRUE(inputs.jsonStorageModelComplete());
  EXPECT_EQ(3u, ledger.snapshot().sharedResources); // root object + nested object + shared String
  EXPECT_GT(ledger.snapshot().estimatedBytes, 2048u);
  EXPECT_EQ(bits(12), bits(inputs.configFloat("surfaceCaveAttenuationDist", 0)));
}
TEST(WorldGenerationBlockInfoStaticInputsTest, ZeroTraversalLimitDoesNotFilterConfiguration) {
  WorldGenerationMemoryLedger ledger; auto inputs = captureConfig(numericConfig(), ledger, true, 0);
  EXPECT_TRUE(inputs.jsonStorageMeasured()); EXPECT_FALSE(inputs.jsonStorageModelComplete()); EXPECT_EQ(0u, ledger.snapshot().reservedBytes);
  EXPECT_EQ(bits(12), bits(inputs.configFloat("surfaceCaveAttenuationDist", 0)));
}
TEST(WorldGenerationBlockInfoStaticInputsTest, PartialTraversalDoesNotRegisterPartialRootBytes) {
  WorldGenerationMemoryLedger ledger; auto inputs = captureConfig(numericConfig(), ledger, true, 1);
  EXPECT_FALSE(inputs.jsonStorageModelComplete()); EXPECT_EQ(0u, ledger.snapshot().reservedBytes);
  EXPECT_EQ(bits(2), bits(inputs.configFloat("surfaceCaveAttenuationFactor", 1)));
}
TEST(WorldGenerationBlockInfoStaticInputsTest, RefusedJsonChargeDoesNotHideOrConvertInputs) {
  WorldGenerationMemoryLedger ledger(0); auto inputs = captureConfig(numericConfig(), ledger);
  EXPECT_FALSE(inputs.jsonStorageModelComplete()); EXPECT_EQ(0u, ledger.snapshot().reservedBytes);
  EXPECT_FALSE(compareStatic(numericConfig(), inputs, [](Fixture&) {}).failed);
}
TEST(WorldGenerationBlockInfoStaticInputsTest, PartialCaptureCannotPoisonLaterCanonicalCharge) {
  WorldGenerationMemoryLedger ledger; auto world = StaticAccess::planet(numericConfig());
  auto partial = world->captureBlockInfoStaticInputs(ledger, true, 1);
  auto full = world->captureBlockInfoStaticInputs(ledger, true);
  EXPECT_FALSE(partial.jsonStorageModelComplete()); EXPECT_TRUE(full.jsonStorageModelComplete()); EXPECT_EQ(1u, ledger.snapshot().sharedResources);
}
TEST(WorldGenerationBlockInfoStaticInputsTest, CaptureOutlivesOriginalLedgerFacade) {
  auto ledger = std::make_unique<WorldGenerationMemoryLedger>(); auto view = *ledger;
  auto inputs = std::make_unique<StaticInputs>(captureConfig(numericConfig(), *ledger)); ledger.reset();
  EXPECT_GT(view.snapshot().reservedBytes, 0u); EXPECT_EQ(bits(12), bits(inputs->configFloat("surfaceCaveAttenuationDist", 0)));
  inputs.reset(); EXPECT_EQ(0u, view.snapshot().reservedBytes);
}
TEST(WorldGenerationBlockInfoStaticInputsTest, MoveHandoffKeepsJsonAndCustomTerrain) {
  WorldGenerationMemoryLedger ledger; auto world = StaticAccess::planet(numericConfig());
  world->addCustomTerrainRegion(PolyF(RectF::withSize({0, 10}, {10, 10})));
  auto first = world->captureBlockInfoStaticInputs(ledger, true); auto second = std::move(first); world.reset();
  EXPECT_EQ(bits(12), bits(second.configFloat("surfaceCaveAttenuationDist", 0)));
  EXPECT_EQ(bits(8), bits(second.customTerrainWeighting(7, 13).first));
}
TEST(WorldGenerationBlockInfoStaticInputsTest, ThreeReadersEvaluateKernelAfterTemplateDestruction) {
  WorldGenerationMemoryLedger ledger; auto config = numericConfig(); auto inputs = captureConfig(config, ledger);
  Fixture old; old.config = config; old.custom = inputs.customTerrainWeighting(7, 13); auto expected = fields(legacyBlockInfo(old, 7, 13));
  std::array<std::future<WorldTemplate::BlockInfo>, 3> results;
  for (auto& result : results) result = std::async(std::launch::async, [&inputs] { CapturedFixture f(inputs); return calculateWorldGenerationBlockInfo(f, 7, 13); });
  for (auto& result : results) EXPECT_EQ(expected, fields(result.get()));
}
TEST(WorldGenerationBlockInfoStaticInputsTest, EveryQueryFailureStillHasTheSamePrefix) {
  WorldGenerationMemoryLedger ledger; auto config = numericConfig(); auto inputs = captureConfig(config, ledger);
  CapturedFixture probe(inputs); probe.blendPresent = probe.blockPresent = true; calculateWorldGenerationBlockInfo(probe, 7, 13);
  for (size_t i = 1; i <= probe.trace.size(); ++i) {
    SCOPED_TRACE(i);
    EXPECT_TRUE(compareStatic(config, inputs, [&](Fixture& f) { f.blendPresent = f.blockPresent = true; f.failAt = i; }).failed);
  }
}
TEST(WorldGenerationBlockInfoStaticInputsTest, ComposedKernelMatrixKeepsFieldsExceptionsAndQueries) {
  // 1,024 combinations in ONE test: captured JSON/custom terrain are real;
  // weighting/noise/selectors remain scripted boundaries, not real sectors.
  for (unsigned mask = 0; mask < 1024; ++mask) {
    SCOPED_TRACE(mask); WorldGenerationMemoryLedger ledger;
    Json config = JsonObject{{"biomeTransitionThreshold", 0.5},
      {"surfaceCaveAttenuationDist", mask & 128 ? Json("invalid") : Json(mask & 256 ? 12 : 0)},
      {"surfaceCaveAttenuationFactor", mask & 512 ? Json("invalid") : Json(2)}};
    auto world = StaticAccess::planet(config);
    if (mask & 64) world->addCustomSpaceRegion(PolyF(RectF::withSize({0, 10}, {10, 10})));
    auto inputs = world->captureBlockInfoStaticInputs(ledger, false); world.reset();
    compareStatic(config, inputs, [&](Fixture& f) {
      f.layoutPresent = !(mask & 1); if (mask & 2) f.weights[0].clear();
      f.blockPresent = mask & 4; f.values[1] = mask & 8 ? -1 : 10;
      f.values[2] = mask & 16 ? 1 : -1; f.values[3] = mask & 32 ? 1 : -1;
    });
  }
}

// Definition below is deliberately the inherited pre-extraction calculation.
WorldTemplate::BlockInfo legacyBlockInfo(Fixture& f, uint32_t x, uint32_t y) {
  using BlockInfo = WorldTemplate::BlockInfo;
  auto m_layout = f.layoutPresent ? &f.layout : nullptr;
  auto& m_templateConfig = f.configuration;
  auto const& m_geometry = f.geometry;
  auto customTerrainWeighting = [&](int a, int b) { return f.customTerrainWeighting(a, b); };
  auto biome = [&](BiomeIndex index) { return f.biome(index); };
  BlockInfo blockInfo;

  if (!m_layout)
    return blockInfo;

  // The environment biome is calculated with weighting based on the flat coordinates.
  List<Weighting> flatWeighting = m_layout->getWeighting(x, y);

  // The block biome is calculated optionally with higher frequency noise
  // added to prevent straight lines appearing on the boundaries of
  // regions.

  int blendNoiseOffset = 0;
  if (auto const& blendNoise = m_layout->blendNoise())
    blendNoiseOffset = (int)blendNoise->get(x, y);

  Vec2I blockPos;
  List<Weighting> blockWeighting;
  List<Weighting> transitionWeighting;
  if (auto const& blockNoise = m_layout->blockNoise()) {
    blockPos = blockNoise->apply(Vec2I(x, y), m_geometry.size());
    blockWeighting = m_layout->getWeighting(blockPos[0] + blendNoiseOffset, blockPos[1]);
    transitionWeighting = m_layout->getWeighting(blockPos[0], blockPos[1]);
  } else {
    blockPos = Vec2I(x, y);
    blockWeighting = flatWeighting;
    transitionWeighting = flatWeighting;
  }

  if (flatWeighting.empty() || blockWeighting.empty())
    return blockInfo;

  auto const& primaryFlatWeighting = flatWeighting.first();
  auto const& primaryBlockWeighting = blockWeighting.first();

  blockInfo.blockBiomeIndex = primaryBlockWeighting.region->blockBiomeIndex;
  blockInfo.environmentBiomeIndex = primaryFlatWeighting.region->environmentBiomeIndex;

  blockInfo.biomeTransition = transitionWeighting.first().weight < m_templateConfig.getFloat("biomeTransitionThreshold", 0);

  float terrainSelect = 0.0f;
  float foregroundCaveSelect = 0.0f;
  float backgroundCaveSelect = 0.0f;

  // Terrain weighting uses the flat weighting, and weights each selector
  // to blend among them.
  for (auto const& weighting : flatWeighting) {
    if (weighting.region->terrainSelectorIndex != NullTerrainSelectorIndex) {
      auto const& terrainSelector = m_layout->getTerrainSelector(weighting.region->terrainSelectorIndex);
      float select = terrainSelector->get(weighting.xValue, y) * weighting.weight;
      terrainSelect += select;
    }
  }

  // This is a bit of a cheat. Since customTerrainWeighting is always flat,
  // there are some odd effects that come from linearly interpolating from
  // the generally non-flat terrain sources to flat regions of space.  By
  // using an interpolator that has an exaggerated S curve between the
  // points, this hides some of these effects.
  auto ctweighting = customTerrainWeighting(x, y);
  terrainSelect = quintic2(ctweighting.second, terrainSelect, ctweighting.first);

  if (terrainSelect > 0.0f) {
    blockInfo.terrain = true;

    for (auto const& weighting : flatWeighting) {
      if (weighting.region->foregroundCaveSelectorIndex != NullTerrainSelectorIndex) {
        auto const& foregroundCaveSelector = m_layout->getTerrainSelector(weighting.region->foregroundCaveSelectorIndex);
        foregroundCaveSelect += foregroundCaveSelector->get(weighting.xValue, y) * weighting.weight;
      }

      if (weighting.region->backgroundCaveSelectorIndex != NullTerrainSelectorIndex) {
        auto const& backgroundCaveSelector = m_layout->getTerrainSelector(weighting.region->backgroundCaveSelectorIndex);
        backgroundCaveSelect += backgroundCaveSelector->get(weighting.xValue, y) * weighting.weight;
      }
    }

    auto surfaceCaveAttenuationDist = m_templateConfig.getFloat("surfaceCaveAttenuationDist", 0);
    if (terrainSelect < surfaceCaveAttenuationDist) {
      auto surfaceCaveAttenuationFactor = m_templateConfig.getFloat("surfaceCaveAttenuationFactor", 1);
      foregroundCaveSelect -= (surfaceCaveAttenuationDist - terrainSelect) * surfaceCaveAttenuationFactor;
      backgroundCaveSelect -= (surfaceCaveAttenuationDist - terrainSelect) * surfaceCaveAttenuationFactor;
    }
  }

  blockInfo.foregroundCave = foregroundCaveSelect > 0.0f;
  blockInfo.backgroundCave = backgroundCaveSelect > 0.0f;

  auto const& regionLiquids = primaryFlatWeighting.region->regionLiquids;
  blockInfo.caveLiquid = regionLiquids.caveLiquid;
  blockInfo.caveLiquidSeedDensity = regionLiquids.caveLiquidSeedDensity;
  blockInfo.oceanLiquid = regionLiquids.oceanLiquid;
  blockInfo.oceanLiquidLevel = regionLiquids.oceanLiquidLevel;
  blockInfo.encloseLiquids = regionLiquids.encloseLiquids;
  blockInfo.fillMicrodungeons = regionLiquids.fillMicrodungeons;

  if (!blockInfo.terrain && blockInfo.encloseLiquids && (int)y < blockInfo.oceanLiquidLevel) {
    blockInfo.terrain = true;
    blockInfo.foregroundCave = true;
  }

  if (blockInfo.terrain) {
    if (auto blockBiome = biome(blockInfo.blockBiomeIndex)) {
      if (!blockInfo.foregroundCave) {
        blockInfo.foreground = blockBiome->mainBlock;
        blockInfo.background = blockInfo.foreground;
      } else if (!blockInfo.backgroundCave) {
        blockInfo.background = blockBiome->mainBlock;
      }

      // subBlock, foregroundOre, and backgroundOre selectors can be empty
      // if they are not enabled, otherwise they will always have the
      // correct count

      if (!primaryBlockWeighting.region->subBlockSelectorIndexes.empty()) {
        for (size_t i = 0; i < blockBiome->subBlocks.size(); ++i) {
          auto const& selector = m_layout->getTerrainSelector(primaryBlockWeighting.region->subBlockSelectorIndexes.at(i));
          if (selector->get(primaryBlockWeighting.xValue - blendNoiseOffset, blockPos[1]) > 0.0f) {
            if (!blockInfo.foregroundCave) {
              blockInfo.foreground = blockBiome->subBlocks.at(i);
              blockInfo.background = blockInfo.foreground;
            } else if (!blockInfo.backgroundCave) {
              blockInfo.background = blockBiome->subBlocks.at(i);
            }

            break;
          }
        }
      }

      if (!blockInfo.foregroundCave && !primaryBlockWeighting.region->foregroundOreSelectorIndexes.empty()) {
        for (size_t i = 0; i < blockBiome->ores.size(); ++i) {
          auto const& selector = m_layout->getTerrainSelector(primaryBlockWeighting.region->foregroundOreSelectorIndexes.at(i));
          if (selector->get(x, y) > 0.0f) {
            blockInfo.foregroundMod = blockBiome->ores.at(i).first;
            break;
          }
        }
      }

      if (!blockInfo.backgroundCave && !primaryBlockWeighting.region->backgroundOreSelectorIndexes.empty()) {
        for (size_t i = 0; i < blockBiome->ores.size(); ++i) {
          auto const& selector = m_layout->getTerrainSelector(primaryBlockWeighting.region->backgroundOreSelectorIndexes.at(i));
          if (selector->get(x, y) > 0.0f) {
            blockInfo.backgroundMod = blockBiome->ores.at(i).first;
            break;
          }
        }
      }
    }
  }

  return blockInfo;
}

} // namespace
