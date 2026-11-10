#include "StarWorldGenerationLookupResolver.hpp"
#include "StarWorldGenerationLookupSources.hpp"
#include "StarWorldGenerationBaseTiles.hpp"
#include "StarBiomeTypes.hpp"
#include "StarWorkerPool.hpp"

#include <array>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <string>
#include <vector>

#include "gtest/gtest.h"

namespace Star {
// Valid C++ objects with controlled metadata for lookup readers ONLY. These
// private seams do not initialize terrain/assets/Root or replace reader bodies.
struct WorldGenerationLookupTestAccess {
  static WorldLayoutPtr layout(List<BiomeConstPtr> biomes) {
    auto result = std::make_shared<WorldLayout>(); result->m_biomes = std::move(biomes); return result;
  }
  static WorldTemplatePtr planet(WorldLayoutPtr layout = {}) {
    auto result = WorldTemplatePtr(new WorldTemplate(WorldTemplate::LookupFixtureTag{}));
    if (layout) result->setWorldLayout(std::move(layout));
    return result;
  }
  static MaterialDatabasePtr materials(bool supports = true) {
    auto result = MaterialDatabasePtr(new MaterialDatabase(MaterialDatabase::LookupFixtureTag{}));
    result->m_materials.resize(23);
    for (MaterialId id : {MaterialId(21), MaterialId(22)}) {
      auto info = std::make_shared<MaterialDatabase::MaterialInfo>();
      info->id = id; info->supportsMods = id == 21 && supports; info->collisionKind = CollisionKind::Block;
      info->foregroundOnly = false; info->soil = false; info->blocksLiquidFlow = true;
      result->m_materials[id] = std::move(info);
    }
    return result;
  }
};
}

using namespace Star;
namespace {
using Access = WorldGenerationLookupTestAccess;
using Observation = WorldGenerationBaseTileObservation;
using Input = WorldGenerationBaseTileInput;
using Context = WorldGenerationBaseTileLookupContext<32>;
struct ControlledAssets {
  GenerationVersionClock clock;
  uint64_t generationRevision() const { return clock.current(); }
};
using Sources = WorldGenerationLookupSources<ControlledAssets, MaterialDatabase>;
using Status = WorldGenerationLookupApplyStatus;
BiomePtr biome(MaterialId main = 21, MaterialHue hue = 37, bool fourMods = false) {
  auto result = std::make_shared<Biome>(); result->mainBlock = main; result->materialHueShift = hue;
  result->surfacePlaceables.grassMod = 31;
  if (fourMods) {
    result->surfacePlaceables.ceilingGrassMod = 32; result->undergroundPlaceables.grassMod = 33;
    result->undergroundPlaceables.ceilingGrassMod = 34;
  }
  return result;
}
struct Fixture {
  std::shared_ptr<GenerationVersionClock> root = std::make_shared<GenerationVersionClock>();
  std::shared_ptr<ControlledAssets> assets = std::make_shared<ControlledAssets>();
  MaterialDatabasePtr materials;
  WorldLayoutPtr layout;
  WorldTemplatePtr planet;
  Fixture(bool supports = true, bool fourMods = false)
    : materials(Access::materials(supports)), layout(Access::layout({biome(21, 37, fourMods), biome(22, 99, fourMods)})), planet(Access::planet(layout)) {}
  WorldGenerationLookupResolver resolver() const { return {*planet, *materials}; }
  Sources sources() const { return Sources::capture(root, root->current(), assets, materials); }
};
Input input() { return {1, 2, true, {21, 31}, {22, 32}, 3, 10, true, 0}; }
void initialize(ServerTile& tile, unsigned seed = 0) {
  tile.foreground = tile.background = NullMaterialId;
  tile.foregroundHueShift = MaterialHue(seed % 255); tile.foregroundMod = 41; tile.foregroundModHueShift = 5; tile.foregroundColorVariant = 2;
  tile.backgroundHueShift = 6; tile.backgroundMod = 42; tile.backgroundModHueShift = 7; tile.backgroundColorVariant = 3;
  tile.collision = CollisionKind::Platform; tile.objectCollision = CollisionKind::None; tile.collisionCacheDirty = false;
  tile.collisionCache.clear(); tile.collisionCache.append(CollisionBlock::nullBlock({1, 2}));
  tile.blockBiomeIndex = 3; tile.environmentBiomeIndex = 4; tile.biomeTransition = false;
  tile.foregroundDamage.reset(); tile.backgroundDamage.reset(); tile.dungeonId = 123;
  tile.liquid = LiquidStore::filled(3, 0.25f, 4.5f); tile.rootSource = Vec2I{7, 8};
}
struct Pair { ServerTile a, b; Pair() { initialize(a); initialize(b); } };
struct Event {
  unsigned kind = 0, biome = 0, material = 0, mod = 0;
  std::array<uint32_t, 14> state{};
  unsigned blockBiome = 0, environmentBiome = 0; bool transition = false;
  bool operator==(Event const& o) const {
    return kind == o.kind && biome == o.biome && material == o.material && mod == o.mod && state == o.state
        && blockBiome == o.blockBiome && environmentBiome == o.environmentBiome && transition == o.transition;
  }
};
struct Trace {
  WorldGenerationLookupResolver reader;
  ServerTile const& tile;
  mutable unsigned count = 0;
  mutable std::array<Event, 6> events{};
  void record(unsigned kind, unsigned biome, unsigned material, unsigned mod) const {
    events.at(count++) = {kind, biome, material, mod, Observation::sourceState(tile), tile.blockBiomeIndex, tile.environmentBiomeIndex, tile.biomeTransition};
  }
  MaterialHue materialHue(BiomeIndex b, MaterialId m) const { record(1, b, m, NoModId); return reader.materialHue(b, m); }
  bool supportsMod(MaterialId m, ModId d) const { record(2, NullBiomeIndex, m, d); return reader.supportsMod(m, d); }
  MaterialHue modHue(BiomeIndex b, ModId d) const { record(3, b, NullMaterialId, d); return reader.modHue(b, d); }
};
// Independent first-party body before extraction, including the original liquid
// branches. It intentionally does NOT call the new application/liquid helpers.
template <typename Resolver>
void legacy(ServerTile& tile, Input const& in, Resolver const& resolver) {
  tile.blockBiomeIndex = in.blockBiome; tile.environmentBiomeIndex = in.environmentBiome; tile.biomeTransition = in.biomeTransition;
  if (tile.foreground == NullMaterialId) {
    tile.foreground = in.foreground.material; tile.foregroundColorVariant = DefaultMaterialColorVariant;
    tile.foregroundHueShift = resolver.materialHue(tile.blockBiomeIndex, tile.foreground);
    if (resolver.supportsMod(tile.foreground, in.foreground.mod)) {
      tile.foregroundMod = in.foreground.mod; tile.foregroundModHueShift = resolver.modHue(tile.blockBiomeIndex, tile.foregroundMod);
    }
  }
  if (tile.background == NullMaterialId) {
    tile.background = in.background.material; tile.backgroundColorVariant = DefaultMaterialColorVariant;
    tile.backgroundHueShift = resolver.materialHue(tile.blockBiomeIndex, tile.background);
    if (resolver.supportsMod(tile.background, in.background.mod)) {
      tile.backgroundMod = in.background.mod; tile.backgroundModHueShift = resolver.modHue(tile.blockBiomeIndex, tile.backgroundMod);
    }
  }
  if (tile.foreground != EmptyMaterialId) tile.liquid = LiquidStore();
  else if (in.oceanLiquid != EmptyLiquidId && in.y < in.oceanLiquidLevel) {
    float pressure = in.oceanLiquidLevel - in.y;
    if (tile.background == EmptyMaterialId) tile.liquid = LiquidStore::endless(in.oceanLiquid, pressure);
    else if (in.encloseLiquids) tile.liquid = LiquidStore::filled(in.oceanLiquid, 1.0f, pressure);
  }
}
Observation sample(Fixture const& fixture, Input in = input()) {
  ServerTile tile; initialize(tile); Observation result; observeWorldGenerationBaseTile(tile, in, fixture.resolver(), result); return result;
}
void sameTrace(Trace const& a, Trace const& b) { EXPECT_EQ(a.count, b.count); EXPECT_TRUE(a.events == b.events); }
}

TEST(WorldGenerationLookupReadersTest, NoLayoutAndNullBiomeReturnZeroWithoutLookingUpMetadata) {
  auto planet = Access::planet();
  EXPECT_EQ(planet->biomeMaterialHueShift(255, 21), 0u); EXPECT_EQ(planet->biomeModHueShift(255, 31), 0u);
  Fixture fixture; EXPECT_EQ(fixture.planet->biomeMaterialHueShift(NullBiomeIndex, 21), 0u);
  EXPECT_EQ(fixture.planet->biomeModHueShift(NullBiomeIndex, NoModId), 0u);
}

TEST(WorldGenerationLookupReadersTest, OnlyTheMainMaterialGetsTheBlockBiomeHue) {
  Fixture fixture; EXPECT_EQ(fixture.resolver().materialHue(1, 21), 37u); EXPECT_EQ(fixture.resolver().materialHue(1, 22), 0u);
  EXPECT_EQ(fixture.resolver().materialHue(2, 22), 99u); EXPECT_EQ(fixture.resolver().materialHue(2, 21), 0u);
}

TEST(WorldGenerationLookupReadersTest, AllFourPlaceableModSlotsUseTheSameRealBiomeHue) {
  Fixture fixture(true, true);
  for (ModId mod : {ModId(31), ModId(32), ModId(33), ModId(34)}) EXPECT_EQ(fixture.resolver().modHue(1, mod), 37u);
  EXPECT_EQ(fixture.resolver().modHue(1, 35), 0u); EXPECT_EQ(fixture.resolver().modHue(1, NoModId), 0u);
}

TEST(WorldGenerationLookupReadersTest, NoModCanHaveANonzeroHueInTheActualReader) {
  Fixture fixture; EXPECT_EQ(fixture.resolver().modHue(1, NoModId), 37u);
  EXPECT_EQ(fixture.resolver().modHue(2, NoModId), 99u);
}

TEST(WorldGenerationLookupReadersTest, NoModShortCircuitsEvenMissingAndMetaMaterials) {
  Fixture fixture;
  for (MaterialId material : {MaterialId(5), MaterialId(99), EmptyMaterialId, NullMaterialId})
    EXPECT_TRUE(fixture.materials->supportsMod(material, NoModId));
}

TEST(WorldGenerationLookupReadersTest, MetaMaterialAndMetaModRefuseBeforeMissingMaterialLookup) {
  Fixture fixture;
  EXPECT_FALSE(fixture.materials->supportsMod(EmptyMaterialId, 31)); EXPECT_FALSE(fixture.materials->supportsMod(NullMaterialId, 31));
  EXPECT_FALSE(fixture.materials->supportsMod(99, BiomeModId)); EXPECT_FALSE(fixture.materials->supportsMod(99, UndergroundBiomeModId));
}

TEST(WorldGenerationLookupReadersTest, SupportDependsOnMaterialNotMembershipInTheLoadedModDatabase) {
  Fixture fixture; EXPECT_TRUE(fixture.materials->supportsMod(21, 31)); EXPECT_TRUE(fixture.materials->supportsMod(21, 32000));
  EXPECT_FALSE(fixture.materials->supportsMod(22, 31)); EXPECT_FALSE(fixture.materials->supportsMod(22, 32000));
}

TEST(WorldGenerationLookupReadersTest, MissingSparseAndOutOfRangeMaterialsThrowTheRealException) {
  Fixture fixture;
  for (MaterialId material : {MaterialId(5), MaterialId(99)}) {
    EXPECT_THROW(fixture.materials->supportsMod(material, 31), MaterialException);
    try { fixture.materials->supportsMod(material, 31); FAIL(); }
    catch (MaterialException const& e) { EXPECT_NE(std::string(e.what()).find("No such material id:"), std::string::npos); }
  }
}

TEST(WorldGenerationLookupReadersTest, InvalidNonNullBiomeThrowsTheInheritedLayoutException) {
  Fixture fixture;
  EXPECT_THROW(fixture.resolver().materialHue(3, 21), StarException);
  EXPECT_THROW(fixture.resolver().modHue(3, 31), StarException);
  EXPECT_THROW(fixture.layout->getBiome(NullBiomeIndex), StarException);
}

TEST(WorldGenerationLookupReadersTest, CaptureAndCatalogMatchIndependentBodyWithRealReaders) {
  Fixture fixture; auto sources = fixture.sources(); auto scope = sources.scope(1, fixture.planet->generationRevision()); ASSERT_TRUE(scope);
  Pair pair; Trace reference{fixture.resolver(), pair.a}, recorded{fixture.resolver(), pair.b}; Observation observation;
  legacy(pair.a, input(), reference); observeWorldGenerationBaseTile(pair.b, input(), recorded, observation);
  sameTrace(reference, recorded); EXPECT_TRUE(compareWorldGenerationBaseTile(pair.a, pair.b).equal()); ASSERT_TRUE(observation.valid());
  Context context(*scope); ASSERT_TRUE(context.add(*scope, observation)); ASSERT_TRUE(context.seal()); ServerTile replay; initialize(replay);
  EXPECT_EQ(sources.apply(context, replay, input(), 1, fixture.planet->generationRevision()), Status::Applied);
  EXPECT_TRUE(compareWorldGenerationBaseTile(pair.a, replay).equal());
}

TEST(WorldGenerationLookupReadersTest, ApplicationUsesBlockBiomeNotEnvironmentBiomeForHues) {
  Fixture fixture; ServerTile tile; initialize(tile); auto in = input(); in.foreground.mod = NoModId;
  applyWorldGenerationBaseTile(tile, in, fixture.resolver()); EXPECT_EQ(tile.blockBiomeIndex, 1u); EXPECT_EQ(tile.environmentBiomeIndex, 2u);
  EXPECT_EQ(tile.foregroundHueShift, 37u); EXPECT_EQ(tile.foregroundModHueShift, 37u);
}

TEST(WorldGenerationLookupReadersTest, AirAndNoModKeepTheRealHueQueryAndFinalLiquidDecision) {
  Fixture fixture; auto in = input(); in.foreground = in.background = {EmptyMaterialId, NoModId}; Pair pair; Observation observation;
  legacy(pair.a, in, fixture.resolver()); observeWorldGenerationBaseTile(pair.b, in, fixture.resolver(), observation);
  ASSERT_TRUE(observation.valid()); EXPECT_EQ(observation.count, 6u); EXPECT_EQ(pair.b.foregroundModHueShift, 37u);
  EXPECT_TRUE(pair.b.liquid.source); EXPECT_EQ(pair.b.liquid.pressure, 10.0f); EXPECT_TRUE(compareWorldGenerationBaseTile(pair.a, pair.b).equal());
}

TEST(WorldGenerationLookupReadersTest, RealMaterialFailurePreservesTheSamePartialWritesAndTrace) {
  Fixture fixture;
  for (bool background : {false, true}) {
    Pair pair; auto in = input(); if (background) in.background.material = 99; else in.foreground.material = 99;
    Trace reference{fixture.resolver(), pair.a}, recorded{fixture.resolver(), pair.b}; Observation observation;
    EXPECT_THROW(legacy(pair.a, in, reference), MaterialException);
    EXPECT_THROW(observeWorldGenerationBaseTile(pair.b, in, recorded, observation), MaterialException);
    sameTrace(reference, recorded); EXPECT_TRUE(compareWorldGenerationBaseTile(pair.a, pair.b).equal()); EXPECT_FALSE(observation.valid());
    EXPECT_EQ(observation.count, background ? 4u : 1u); EXPECT_EQ(pair.b.liquid.liquid, 3u);
  }
}

TEST(WorldGenerationLookupReadersTest, RealBiomeFailureLeavesOnlyLegacyMetadataMaterialAndColorWrites) {
  Fixture fixture; Pair pair; auto in = input(); in.blockBiome = 3; Observation observation;
  Trace reference{fixture.resolver(), pair.a}, recorded{fixture.resolver(), pair.b};
  EXPECT_THROW(legacy(pair.a, in, reference), StarException);
  EXPECT_THROW(observeWorldGenerationBaseTile(pair.b, in, recorded, observation), StarException);
  sameTrace(reference, recorded); EXPECT_TRUE(compareWorldGenerationBaseTile(pair.a, pair.b).equal()); EXPECT_EQ(observation.count, 0u); EXPECT_FALSE(observation.complete);
}

TEST(WorldGenerationLookupReadersTest, MissingCoverageDoesNotCallReadersAndLegacyFallbackKeepsItsExceptionState) {
  Fixture fixture; auto sources = fixture.sources(); auto scope = sources.scope(1, fixture.planet->generationRevision()); ASSERT_TRUE(scope);
  Context context(*scope); ASSERT_TRUE(context.add(*scope, sample(fixture))); ASSERT_TRUE(context.seal()); Pair pair; auto in = input(); in.blockBiome = 3;
  Trace reference{fixture.resolver(), pair.a}, recorded{fixture.resolver(), pair.b};
  EXPECT_THROW(legacy(pair.a, in, reference), StarException);
  auto before = Observation::sourceState(pair.b);
  EXPECT_EQ(sources.apply(context, pair.b, in, 1, fixture.planet->generationRevision()), Status::Refused);
  EXPECT_EQ(Observation::sourceState(pair.b), before); EXPECT_EQ(pair.b.blockBiomeIndex, 3u);
  EXPECT_EQ(pair.b.environmentBiomeIndex, 4u); EXPECT_FALSE(pair.b.biomeTransition);
  EXPECT_THROW(legacy(pair.b, in, recorded), StarException); sameTrace(reference, recorded); EXPECT_TRUE(compareWorldGenerationBaseTile(pair.a, pair.b).equal());
}

TEST(WorldGenerationLookupReadersTest, ActualLayoutReplacementAdvancesRevisionAndRefusesOldCatalogBeforeWriting) {
  Fixture fixture; auto sources = fixture.sources(); auto previous = fixture.planet->generationRevision(); auto scope = sources.scope(1, previous); ASSERT_TRUE(scope);
  Context context(*scope); ASSERT_TRUE(context.add(*scope, sample(fixture))); ASSERT_TRUE(context.seal());
  fixture.planet->setWorldLayout(Access::layout({biome(21, 172)})); EXPECT_EQ(fixture.planet->generationRevision(), previous + 1);
  EXPECT_EQ(fixture.resolver().materialHue(1, 21), 172u); EXPECT_EQ(fixture.layout->getBiome(1)->materialHueShift, 37u);
  Pair pair; EXPECT_EQ(sources.apply(context, pair.b, input(), 1, fixture.planet->generationRevision()), Status::Refused);
  EXPECT_TRUE(compareWorldGenerationBaseTile(pair.a, pair.b).equal());
}

TEST(WorldGenerationLookupReadersTest, NoModAllowsACompleteObservedOutputForAMissingRealMaterialDefinition) {
  Fixture fixture; auto in = input(); in.foreground = {99, NoModId}; Pair pair; Observation observation;
  legacy(pair.a, in, fixture.resolver()); observeWorldGenerationBaseTile(pair.b, in, fixture.resolver(), observation);
  ASSERT_TRUE(observation.valid()); EXPECT_EQ(pair.b.foreground, 99u); EXPECT_EQ(pair.b.foregroundModHueShift, 37u);
  EXPECT_TRUE(compareWorldGenerationBaseTile(pair.a, pair.b).equal());
  auto sources = fixture.sources(); auto scope = sources.scope(1, fixture.planet->generationRevision()); ASSERT_TRUE(scope);
  Context context(*scope); ASSERT_TRUE(context.add(*scope, observation)); ASSERT_TRUE(context.seal()); ServerTile actual; initialize(actual);
  EXPECT_EQ(sources.apply(context, actual, in, 1, fixture.planet->generationRevision()), Status::Applied);
  EXPECT_TRUE(compareWorldGenerationBaseTile(pair.a, actual).equal());
}

TEST(WorldGenerationLookupReadersTest, ControlledLifecycleReplacementRetainsRealOldDefinitionsWithoutRebuildingTemplate) {
  Fixture fixture; auto sources = fixture.sources(); auto scope = sources.scope(1, fixture.planet->generationRevision()); ASSERT_TRUE(scope);
  Context context(*scope); ASSERT_TRUE(context.add(*scope, sample(fixture))); ASSERT_TRUE(context.seal());
  std::weak_ptr<MaterialDatabase> previous = fixture.materials; auto templateRevision = fixture.planet->generationRevision();
  { auto change = fixture.root->change(); fixture.materials = Access::materials(false); change.complete(); }
  EXPECT_FALSE(sources.valid()); EXPECT_FALSE(previous.expired()); EXPECT_TRUE(sources.materials()->supportsMod(21, 31));
  EXPECT_FALSE(fixture.materials->supportsMod(21, 31)); EXPECT_EQ(fixture.planet->generationRevision(), templateRevision);
  Pair pair; EXPECT_EQ(fixture.sources().apply(context, pair.b, input(), 1, templateRevision), Status::Refused);
  EXPECT_TRUE(compareWorldGenerationBaseTile(pair.a, pair.b).equal()); sources = {}; EXPECT_TRUE(previous.expired());
}

TEST(WorldGenerationLookupReadersTest, BranchMatrixComparesActualReadersCatalogTapeAndIndependentBody) {
  Fixture supported, unsupported(false);
  for (unsigned flags = 0; flags < 4096; ++flags) {
    auto const& fixture = flags & 64 ? supported : unsupported; auto sources = fixture.sources();
    auto scope = sources.scope(1, fixture.planet->generationRevision()); ASSERT_TRUE(scope);
    Pair pair; ServerTile catalog, replay; initialize(catalog); initialize(replay); auto in = input();
    pair.a.foreground = pair.b.foreground = catalog.foreground = replay.foreground = flags & 1 ? NullMaterialId : MaterialId(1);
    pair.a.background = pair.b.background = catalog.background = replay.background = flags & 2 ? NullMaterialId : MaterialId(2);
    in.foreground = {flags & 4 ? EmptyMaterialId : MaterialId(21), flags & 16 ? NoModId : ModId(31)};
    in.background = {flags & 8 ? EmptyMaterialId : MaterialId(22), flags & 32 ? NoModId : ModId(32)};
    in.blockBiome = flags & 128 ? NullBiomeIndex : BiomeIndex(1); in.biomeTransition = flags & 256;
    in.oceanLiquid = flags & 512 ? EmptyLiquidId : LiquidId(3); in.encloseLiquids = flags & 1024; in.y = flags & 2048 ? 10 : 0;
    Trace reference{fixture.resolver(), pair.a}, recorded{fixture.resolver(), pair.b}; Observation observation;
    legacy(pair.a, in, reference); observeWorldGenerationBaseTile(pair.b, in, recorded, observation); sameTrace(reference, recorded);
    ASSERT_TRUE(compareWorldGenerationBaseTile(pair.a, pair.b).equal()); Context context(*scope);
    ASSERT_TRUE(context.add(*scope, observation)); ASSERT_TRUE(context.seal());
    ASSERT_EQ(sources.apply(context, catalog, in, 1, fixture.planet->generationRevision()), Status::Applied);
    ASSERT_TRUE(replayWorldGenerationBaseTile(replay, in, observation));
    ASSERT_TRUE(compareWorldGenerationBaseTile(pair.a, catalog).equal()); ASSERT_TRUE(compareWorldGenerationBaseTile(pair.a, replay).equal());
  }
}

TEST(WorldGenerationLookupReadersTest, SharedRealReadersAndCatalogMatchSerialTilesAcrossFourWorkers) {
  Fixture fixture; auto sources = fixture.sources(); auto revision = fixture.planet->generationRevision(); auto scope = sources.scope(1, revision); ASSERT_TRUE(scope);
  Context context(*scope); ASSERT_TRUE(context.add(*scope, sample(fixture))); ASSERT_TRUE(context.seal());
  auto expected = std::make_unique<ServerTileSectorArray::Array>(), actual = std::make_unique<ServerTileSectorArray::Array>();
  auto readerOutput = std::make_unique<ServerTileSectorArray::Array>();
  for (unsigned i = 0; i < 1024; ++i) {
    auto& a = (*expected)(i / 32, i % 32); auto& b = (*actual)(i / 32, i % 32); initialize(a, i); initialize(b, i);
    a.foreground = b.foreground = i % 2 ? NullMaterialId : EmptyMaterialId; a.background = b.background = i % 3 ? NullMaterialId : MaterialId(2);
    auto& direct = (*readerOutput)(i / 32, i % 32); initialize(direct, i);
    direct.foreground = b.foreground; direct.background = b.background;
    auto in = input(); in.y = int(i % 32); legacy(a, in, fixture.resolver());
  }
  WorkerPool pool("Actual lookup reader contracts", 4); std::vector<WorkerPoolPromise<bool>> promises;
  for (unsigned part = 0; part < 4; ++part) promises.push_back(pool.addProducer<bool>([&, part] {
    for (unsigned i = part * 256; i < (part + 1) * 256; ++i) {
      auto in = input(); in.y = int(i % 32);
      applyWorldGenerationBaseTile((*readerOutput)(i / 32, i % 32), in, fixture.resolver());
      if (sources.apply(context, (*actual)(i / 32, i % 32), in, 1, revision) != Status::Applied) return false;
    }
    return true;
  }));
  for (auto& promise : promises) { EXPECT_TRUE(promise.get()); } pool.finish();
  WorldGenerationBaseTilesIdentity identity{{1, 1, {0, 0}, {0, 0}, WorldGenerationTaskStage::BaseTiles}, 5, 7, {32, 32}, RectI(0, 0, 32, 32)};
  EXPECT_TRUE(compareWorldGenerationBaseTiles(*expected, *actual, identity).equal());
  EXPECT_TRUE(compareWorldGenerationBaseTiles(*expected, *readerOutput, identity).equal());
}

// Explicit opt-in diagnostic, not a timing assertion or a game/asset-loading
// throughput benchmark. Controlled metadata, actual production reader bodies.
TEST(WorldGenerationLookupReadersTest, DISABLED_SyntheticRealReaderCosts) {
  constexpr unsigned count = 8192, rounds = 9; Fixture fixture; auto resolver = fixture.resolver();
  auto sources = fixture.sources(); auto revision = fixture.planet->generationRevision(); auto scope = sources.scope(1, revision); ASSERT_TRUE(scope);
  Context context(*scope); ASSERT_TRUE(context.add(*scope, sample(fixture))); ASSERT_TRUE(context.seal());
  std::vector<ServerTile> tiles(count); std::array<std::array<double, rounds>, 3> timings{}; size_t checksum = 0; auto in = input();
  for (unsigned round = 0; round < rounds; ++round) for (unsigned position = 0; position < 3; ++position) {
    unsigned mode = (round + position) % 3; for (auto& tile : tiles) initialize(tile);
    auto begin = std::chrono::steady_clock::now();
    for (auto& tile : tiles) {
      if (mode == 0) applyWorldGenerationBaseTile(tile, in, resolver);
      else if (mode == 1) checksum += context.apply(tile, in, *scope);
      else checksum += sources.apply(context, tile, in, 1, revision) == Status::Applied;
    }
    timings[mode][round] = std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - begin).count() / count;
    for (auto const& tile : tiles) checksum += tile.foregroundHueShift + tile.backgroundModHueShift;
  }
  for (unsigned mode = 0; mode < 3; ++mode) {
    std::sort(timings[mode].begin(), timings[mode].end());
    std::printf("RealLookupReaderCost mode=%u tiles=%u rounds=%u medianNs=%.2f minNs=%.2f maxNs=%.2f contextBytes=%zu entries=%zu activeEntryBytes=%zu\n",
        mode, count, rounds, timings[mode][rounds / 2], timings[mode].front(), timings[mode].back(), sizeof(Context), context.size(), context.activeEntryBytes());
  }
  EXPECT_EQ(checksum, size_t(rounds) * count * (3 * (37 + 7) + 2));
}
