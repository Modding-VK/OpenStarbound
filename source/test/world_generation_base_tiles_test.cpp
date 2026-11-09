#include "StarWorldGenerationBaseTiles.hpp"
#include "StarWorldGenerationBaseTileInput.hpp"
#include "StarWorldGenerationBaseTileObservation.hpp"
#include "StarWorldGenerationBaseTileLookupContext.hpp"
#include "StarWorldGenerationMemory.hpp"
#include "StarWorkerPool.hpp"

#include <array>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <functional>
#include <limits>
#include <stdexcept>
#include <type_traits>

#include "gtest/gtest.h"

namespace Star {
struct TileDamageStatusTestAccess {
  static void change(TileDamageStatus& state, unsigned field) {
    switch (field) {
      case 0: state.m_damagePercentage = 0.25f; break;
      case 1: state.m_damageEffectTimeFactor = 2.0f; break;
      case 2: state.m_harvested = true; break;
      case 3: state.m_damageSourcePosition[0] = 1.0f; break;
      case 4: state.m_damageSourcePosition[1] = 1.0f; break;
      case 5: state.m_damageType = TileDamageType::Blockish; break;
      case 6: state.m_damageEffectPercentage = 0.5f; break;
    }
  }
};
}

using namespace Star;
namespace {
using Identity = WorldGenerationBaseTilesIdentity;
using Field = WorldGenerationBaseTileField;
using Mode = WorldGenerationBaseTileComparisonMode;
uint32_t mask(Field field) { return static_cast<uint32_t>(field); }
Identity baseTicket() {
  return {{1, 1, {0, 0}, {0, 0}, WorldGenerationTaskStage::BaseTiles}, 5, 7, {128, 128}, RectI(0, 0, 32, 32)};
}
void populate(ServerTile& tile, unsigned seed = 0) {
  tile.foreground = MaterialId(1 + seed % 100); tile.foregroundHueShift = MaterialHue(seed % 255);
  tile.foregroundMod = NoModId; tile.foregroundModHueShift = 5; tile.foregroundColorVariant = 2;
  tile.background = EmptyMaterialId; tile.backgroundHueShift = 6; tile.backgroundMod = NoModId;
  tile.backgroundModHueShift = 7; tile.backgroundColorVariant = 3;
  tile.collision = CollisionKind::Platform; tile.objectCollision = CollisionKind::None;
  tile.collisionCacheDirty = false; tile.collisionCache.clear();
  tile.collisionCache.append(CollisionBlock::nullBlock({1, 2}));
  tile.blockBiomeIndex = 3; tile.environmentBiomeIndex = 4; tile.biomeTransition = bool(seed % 2);
  tile.foregroundDamage.reset(); tile.backgroundDamage.reset(); tile.dungeonId = 123;
  tile.liquid = LiquidStore::filled(3, 0.25f, 4.5f); tile.rootSource = Vec2I{7, 8};
}
struct Pair { ServerTile a, b; Pair() { populate(a); populate(b); } };
void expectOnly(Pair const& pair, Field field) {
  auto result = compareWorldGenerationBaseTile(pair.a, pair.b);
  EXPECT_TRUE(result.valid); EXPECT_FALSE(result.equal()); EXPECT_EQ(result.comparedTiles, 1u);
  EXPECT_EQ(result.differentTiles, 1u); EXPECT_EQ(result.differentFields, mask(field));
}
void fill(ServerTileSectorArray::Array& array, unsigned width = 32, unsigned height = 32) {
  for (unsigned x = 0; x < width; ++x)
    for (unsigned y = 0; y < height; ++y) populate(array(x, y), x * 32 + y);
}
float bits(uint32_t value) { float result; static_assert(sizeof(result) == sizeof(value)); std::memcpy(&result, &value, sizeof(result)); return result; }
void legacyLiquid(LiquidStore& store, WorldGenerationBaseTileLiquidInput const& in) {
  if (in.foreground != EmptyMaterialId) store = LiquidStore();
  else if (in.oceanLiquid != EmptyLiquidId && in.y < in.oceanLiquidLevel) {
    float pressure = in.oceanLiquidLevel - in.y;
    if (in.background == EmptyMaterialId) store = LiquidStore::endless(in.oceanLiquid, pressure);
    else if (in.encloseLiquids) store = LiquidStore::filled(in.oceanLiquid, 1.0f, pressure);
  }
}
}

TEST(WorldGenerationBaseTilesTest, DefaultIdentityIsNotAValidResultTicket) {
  Identity empty{}; EXPECT_FALSE(empty.valid()); EXPECT_FALSE(empty.matches(empty));
}

TEST(WorldGenerationBaseTilesTest, CanonicalFullSectorIdentityIsValid) {
  auto id = baseTicket(); EXPECT_TRUE(id.valid()); EXPECT_TRUE(id.matches(id));
}

TEST(WorldGenerationBaseTilesTest, PartialWorldEdgeUsesClippedBounds) {
  auto id = baseTicket(); id.worldSize = {35, 33}; id.task.sector = {1, 1}; id.tileRegion = RectI(32, 32, 35, 33);
  EXPECT_TRUE(id.valid()); id.tileRegion = RectI(32, 32, 64, 64); EXPECT_FALSE(id.valid());
}

TEST(WorldGenerationBaseTilesTest, EmptyAndUnrepresentableWorldDimensionsAreInvalid) {
  for (auto size : {Vec2U{0, 128}, Vec2U{128, 0}, Vec2U{unsigned(std::numeric_limits<int>::max()) + 1u, 128}}) {
    auto id = baseTicket(); id.worldSize = size; EXPECT_FALSE(id.valid());
  }
}

TEST(WorldGenerationBaseTilesTest, NoncanonicalSectorsAndWrongRegionsAreInvalid) {
  auto id = baseTicket(); id.task.sector = {4, 0}; EXPECT_FALSE(id.valid());
  id = baseTicket(); id.task.region = {1, 0}; EXPECT_FALSE(id.valid());
  id.task.sector = {std::numeric_limits<unsigned>::max(), 0}; EXPECT_FALSE(id.valid());
}

TEST(WorldGenerationBaseTilesTest, BoundsCannotShiftOrOmitValidTiles) {
  for (auto region : {RectI(1, 0, 32, 32), RectI(0, 0, 31, 32), RectI(0, 0, 32, 31), RectI(-1, 0, 32, 32)}) {
    auto id = baseTicket(); id.tileRegion = region; EXPECT_FALSE(id.valid());
  }
}

TEST(WorldGenerationBaseTilesTest, EpochSequenceAndStageMustIdentifyABaseTilesAdmission) {
  auto id = baseTicket(); id.task.worldEpoch = 0; EXPECT_FALSE(id.valid());
  id = baseTicket(); id.task.sequence = 0; EXPECT_FALSE(id.valid());
  id = baseTicket(); id.task.stage = WorldGenerationTaskStage::MicroDungeonPlan; EXPECT_FALSE(id.valid());
}

TEST(WorldGenerationBaseTilesTest, UnsupportedVersionsCannotMatchEvenIfBothTicketsAgree) {
  auto id = baseTicket(); ++id.productVersion; EXPECT_FALSE(id.valid()); EXPECT_FALSE(id.matches(id));
  id = baseTicket(); ++id.liquidVersion; EXPECT_FALSE(id.valid()); EXPECT_FALSE(id.matches(id));
}

TEST(WorldGenerationBaseTilesTest, EveryTaskIdentityComponentDistinguishesTheResult) {
  auto expected = baseTicket();
  std::array<std::function<void(Identity&)>, 5> changes{{
      [](auto& v) { ++v.task.worldEpoch; }, [](auto& v) { ++v.task.sequence; },
      [](auto& v) { ++v.task.sector[0]; }, [](auto& v) { ++v.task.region[0]; },
      [](auto& v) { v.task.stage = WorldGenerationTaskStage::CaveLiquidPlan; }}};
  for (auto change : changes) { auto result = expected; change(result); EXPECT_FALSE(expected.matches(result)); }
}

TEST(WorldGenerationBaseTilesTest, RevisionsGeometryAndKernelVersionsDistinguishTheResult) {
  auto expected = baseTicket();
  std::array<std::function<void(Identity&)>, 6> changes{{
      [](auto& v) { ++v.tileRevision; }, [](auto& v) { ++v.templateRevision; },
      [](auto& v) { ++v.worldSize[0]; }, [](auto& v) { v.tileRegion = RectI(0, 0, 31, 32); },
      [](auto& v) { ++v.productVersion; }, [](auto& v) { ++v.liquidVersion; }}};
  for (auto change : changes) { auto result = expected; change(result); EXPECT_FALSE(expected.matches(result)); }
  expected.tileRevision = 0; expected.templateRevision = std::numeric_limits<uint64_t>::max(); EXPECT_TRUE(expected.matches(expected));
}

TEST(WorldGenerationBaseTilesTest, IdentityCopyIsExactAndDoesNotEncodePayloadOrPointers) {
  auto first = baseTicket(), copy = first; EXPECT_EQ(first, copy); EXPECT_TRUE(first.matches(copy));
  Pair pair; pair.b.liquid.pressure = 10;
  EXPECT_TRUE(first.matches(copy)); EXPECT_FALSE(compareWorldGenerationBaseTile(pair.a, pair.b).equal());
}

TEST(WorldGenerationBaseTilesTest, ReadmissionInTheSameSectorGetsADifferentTaskIdentity) {
  WorldGenerationTasks tasks(1, WorldRegionPartition(WorldGeometry(Vec2U{128, 128})), 1, 1024);
  auto old = tasks.reserve({0, 0}, WorldGenerationTaskStage::BaseTiles, 1); ASSERT_TRUE(old);
  auto a = baseTicket(); a.task = old->identity; ASSERT_TRUE(old->start()); ASSERT_TRUE(old->ready());
  ASSERT_TRUE(tasks.retire(old, WorldGenerationTaskOutcome::Stale));
  auto next = tasks.reserve({0, 0}, WorldGenerationTaskStage::BaseTiles, 1); ASSERT_TRUE(next);
  auto b = a; b.task = next->identity; EXPECT_FALSE(a.matches(b));
}

TEST(WorldGenerationBaseTilesTest, ANewWorldEpochCannotReuseTheSameSequenceAndRevisions) {
  auto a = baseTicket(), b = a; ++b.task.worldEpoch; EXPECT_TRUE(b.valid()); EXPECT_FALSE(a.matches(b));
}

TEST(WorldGenerationBaseTilesTest, WorkerEchoesTheAdmissionTicketRatherThanAFreshOwnerObservation) {
  auto expected = baseTicket(); WorkerPool pool("BaseTiles identity contract", 1);
  auto promise = pool.addProducer<Identity>([ticket = expected] { return ticket; });
  ++expected.tileRevision; EXPECT_FALSE(expected.matches(promise.get())); pool.finish();
}

TEST(WorldGenerationBaseTilesTest, FullyInitializedTilesCompareEqualWithoutSerializing) {
  Pair pair; auto result = compareWorldGenerationBaseTile(pair.a, pair.b);
  EXPECT_TRUE(result.equal()); EXPECT_EQ(result.comparedTiles, 1u); EXPECT_FALSE(result.firstDifference);
}

TEST(WorldGenerationBaseTilesTest, EveryPublicScalarFieldHasItsOwnDifferenceBit) {
  using Change = std::pair<Field, std::function<void(ServerTile&)>>;
  std::array<Change, 21> changes{{
      {Field::Foreground, [](auto& t) { ++t.foreground; }}, {Field::ForegroundHue, [](auto& t) { ++t.foregroundHueShift; }},
      {Field::ForegroundMod, [](auto& t) { t.foregroundMod = 1; }}, {Field::ForegroundModHue, [](auto& t) { ++t.foregroundModHueShift; }},
      {Field::ForegroundColor, [](auto& t) { ++t.foregroundColorVariant; }}, {Field::Background, [](auto& t) { t.background = 1; }},
      {Field::BackgroundHue, [](auto& t) { ++t.backgroundHueShift; }}, {Field::BackgroundMod, [](auto& t) { t.backgroundMod = 1; }},
      {Field::BackgroundModHue, [](auto& t) { ++t.backgroundModHueShift; }}, {Field::BackgroundColor, [](auto& t) { ++t.backgroundColorVariant; }},
      {Field::Collision, [](auto& t) { t.collision = CollisionKind::Block; }}, {Field::ObjectCollision, [](auto& t) { t.objectCollision = CollisionKind::Block; }},
      {Field::BlockBiome, [](auto& t) { ++t.blockBiomeIndex; }}, {Field::EnvironmentBiome, [](auto& t) { ++t.environmentBiomeIndex; }},
      {Field::BiomeTransition, [](auto& t) { t.biomeTransition = !t.biomeTransition; }}, {Field::Dungeon, [](auto& t) { ++t.dungeonId; }},
      {Field::LiquidId, [](auto& t) { ++t.liquid.liquid; }}, {Field::LiquidLevel, [](auto& t) { t.liquid.level = 0.5f; }},
      {Field::LiquidPressure, [](auto& t) { t.liquid.pressure = 5.0f; }}, {Field::LiquidSource, [](auto& t) { t.liquid.source = true; }},
      {Field::RootSource, [](auto& t) { t.rootSource = Vec2I{8, 8}; }}}};
  for (auto const& change : changes) { Pair pair; change.second(pair.b); expectOnly(pair, change.first); }
}

TEST(WorldGenerationBaseTilesTest, MultipleDifferencesAccumulateWithoutDuplicatingTheTile) {
  Pair pair; ++pair.b.foreground; ++pair.b.dungeonId; pair.b.liquid.pressure = 6;
  auto result = compareWorldGenerationBaseTile(pair.a, pair.b);
  EXPECT_EQ(result.differentTiles, 1u); EXPECT_EQ(result.differentFields, mask(Field::Foreground) | mask(Field::Dungeon) | mask(Field::LiquidPressure));
  ASSERT_TRUE(result.firstDifference); EXPECT_EQ(*result.firstDifference, Vec2U(0, 0));
}

TEST(WorldGenerationBaseTilesTest, ComparingDoesNotNormalizeOrMutateEitherCache) {
  Pair pair; auto size = pair.a.collisionCache[0].poly.vertexes().size();
  EXPECT_TRUE(compareWorldGenerationBaseTile(pair.a, pair.b).equal());
  EXPECT_EQ(pair.a.collisionCache[0].poly.vertexes().size(), size); EXPECT_FALSE(pair.a.collisionCacheDirty);
  EXPECT_TRUE(compareWorldGenerationBaseTile(pair.a, pair.b).equal());
}

TEST(WorldGenerationBaseTilesTest, IgnoringCacheIsExplicitAndSkipsOnlyCacheFields) {
  Pair pair; pair.b.collisionCacheDirty = true; pair.b.collisionCache.clear();
  auto exact = compareWorldGenerationBaseTile(pair.a, pair.b); EXPECT_EQ(exact.differentFields, mask(Field::CollisionDirty) | mask(Field::CollisionCache));
  EXPECT_TRUE(compareWorldGenerationBaseTile(pair.a, pair.b, Mode::IgnoreCollisionCache).equal());
}

TEST(WorldGenerationBaseTilesTest, IgnoringCacheStillComparesBothCollisionKinds) {
  Pair pair; pair.b.collision = CollisionKind::Block; pair.b.objectCollision = CollisionKind::Platform;
  auto result = compareWorldGenerationBaseTile(pair.a, pair.b, Mode::IgnoreCollisionCache);
  EXPECT_EQ(result.differentFields, mask(Field::Collision) | mask(Field::ObjectCollision));
}

TEST(WorldGenerationBaseTilesTest, CacheEntryCountIsObservable) {
  Pair pair; pair.b.collisionCache.clear(); expectOnly(pair, Field::CollisionCache);
}

TEST(WorldGenerationBaseTilesTest, CacheKindAndSpaceAreObservable) {
  Pair pair; pair.b.collisionCache[0].kind = CollisionKind::Block; expectOnly(pair, Field::CollisionCache);
  pair.b.collisionCache[0].kind = pair.a.collisionCache[0].kind; ++pair.b.collisionCache[0].space[1]; expectOnly(pair, Field::CollisionCache);
}

TEST(WorldGenerationBaseTilesTest, EveryCacheBoundsCoordinateIsObservable) {
  for (auto bounds : {RectF(0, 2, 2, 3), RectF(1, 1, 2, 3), RectF(1, 2, 3, 3), RectF(1, 2, 2, 4)}) {
    Pair pair; pair.b.collisionCache[0].polyBounds = bounds; expectOnly(pair, Field::CollisionCache);
  }
}

TEST(WorldGenerationBaseTilesTest, PolygonVertexCountIsObservable) {
  Pair pair; pair.b.collisionCache[0].poly.add({3, 4}); expectOnly(pair, Field::CollisionCache);
}

TEST(WorldGenerationBaseTilesTest, PolygonOrderCoordinatesAndSignedZeroAreObservable) {
  Pair pair; auto& vertices = pair.b.collisionCache[0].poly.vertexes(); std::swap(vertices[0], vertices[1]); expectOnly(pair, Field::CollisionCache);
  Pair zero; zero.a.collisionCache[0].poly.vertexes()[0][0] = 0.0f; zero.b.collisionCache[0].poly.vertexes()[0][0] = -0.0f;
  expectOnly(zero, Field::CollisionCache);
}

TEST(WorldGenerationBaseTilesTest, EveryForegroundDamageStateFieldIsCompared) {
  for (unsigned field = 0; field < 7; ++field) { Pair pair; TileDamageStatusTestAccess::change(pair.b.foregroundDamage, field); expectOnly(pair, Field::ForegroundDamage); }
}

TEST(WorldGenerationBaseTilesTest, BackgroundDamageIncludesHiddenEffectTimeEvenWithNoVisibleDamage) {
  Pair pair; TileDamageStatusTestAccess::change(pair.b.backgroundDamage, 1);
  EXPECT_EQ(pair.a.backgroundDamage.damagePercentage(), pair.b.backgroundDamage.damagePercentage());
  EXPECT_EQ(pair.a.backgroundDamage.damageEffectPercentage(), pair.b.backgroundDamage.damageEffectPercentage());
  expectOnly(pair, Field::BackgroundDamage);
}

TEST(WorldGenerationBaseTilesTest, DerivedDamageEffectIsComparedWithoutRecomputingIt) {
  Pair pair; TileDamageStatusTestAccess::change(pair.b.foregroundDamage, 6); expectOnly(pair, Field::ForegroundDamage);
  EXPECT_EQ(pair.b.foregroundDamage.damageEffectPercentage(), 0.5f);
}

TEST(WorldGenerationBaseTilesTest, LiquidFloatBitsPreserveZeroSignsAndNanPayloads) {
  Pair pair; pair.a.liquid.level = 0.0f; pair.b.liquid.level = -0.0f; expectOnly(pair, Field::LiquidLevel);
  pair.a.liquid.level = pair.b.liquid.level = bits(0x7fc00001u); EXPECT_TRUE(compareWorldGenerationBaseTile(pair.a, pair.b).equal());
  pair.b.liquid.level = bits(0x7fc00002u); expectOnly(pair, Field::LiquidLevel);
}

TEST(WorldGenerationBaseTilesTest, RootSourcePresenceIsNotTheSameAsTheZeroCoordinate) {
  Pair pair; pair.a.rootSource = {}; pair.b.rootSource = Vec2I{0, 0}; expectOnly(pair, Field::RootSource);
}

TEST(WorldGenerationBaseTilesTest, CopyNormalizationRequiresAnExplicitComparisonMode) {
  Pair pair; ServerTile copy(pair.a);
  // Legacy copy omits biomeTransition; initialize it explicitly in this test,
  // which isolates cache normalization rather than reading an undefined bool.
  copy.biomeTransition = pair.a.biomeTransition;
  EXPECT_FALSE(compareWorldGenerationBaseTile(pair.a, copy).equal());
  EXPECT_TRUE(compareWorldGenerationBaseTile(pair.a, copy, Mode::IgnoreCollisionCache).equal());
}

TEST(WorldGenerationBaseTilesTest, UnknownComparisonModeFailsClosed) {
  Pair pair; auto result = compareWorldGenerationBaseTile(pair.a, pair.b, static_cast<Mode>(255));
  EXPECT_FALSE(result.valid); EXPECT_FALSE(result.equal()); EXPECT_EQ(result.comparedTiles, 0u);
}

TEST(WorldGenerationBaseTilesTest, FullSectorComparisonVisitsAllInitializedTiles) {
  auto a = std::make_unique<ServerTileSectorArray::Array>(), b = std::make_unique<ServerTileSectorArray::Array>(); fill(*a); fill(*b);
  auto result = compareWorldGenerationBaseTiles(*a, *b, baseTicket()); EXPECT_TRUE(result.equal()); EXPECT_EQ(result.comparedTiles, 1024u);
}

TEST(WorldGenerationBaseTilesTest, SectorComparisonReportsTheFirstXMajorDifferenceAndAllMasks) {
  auto a = std::make_unique<ServerTileSectorArray::Array>(), b = std::make_unique<ServerTileSectorArray::Array>(); fill(*a); fill(*b);
  ++(*b)(2, 1).foreground; ++(*b)(1, 30).dungeonId;
  auto result = compareWorldGenerationBaseTiles(*a, *b, baseTicket()); EXPECT_EQ(result.comparedTiles, 1024u); EXPECT_EQ(result.differentTiles, 2u);
  EXPECT_EQ(result.differentFields, mask(Field::Foreground) | mask(Field::Dungeon)); ASSERT_TRUE(result.firstDifference); EXPECT_EQ(*result.firstDifference, Vec2U(1, 30));
}

TEST(WorldGenerationBaseTilesTest, PartialSectorDoesNotReadUninitializedCellsOutsideTheWorld) {
  auto a = std::make_unique<ServerTileSectorArray::Array>(), b = std::make_unique<ServerTileSectorArray::Array>(); fill(*a, 3, 1); fill(*b, 3, 1);
  auto id = baseTicket(); id.worldSize = {35, 33}; id.task.sector = {1, 1}; id.tileRegion = RectI(32, 32, 35, 33);
  (*b)(10, 0).foreground = 999;
  auto result = compareWorldGenerationBaseTiles(*a, *b, id); EXPECT_TRUE(result.equal()); EXPECT_EQ(result.comparedTiles, 3u);
}

TEST(WorldGenerationBaseTilesTest, InvalidIdentityRejectsArraysBeforeReadingTheirUninitializedFields) {
  auto a = std::make_unique<ServerTileSectorArray::Array>(), b = std::make_unique<ServerTileSectorArray::Array>();
  auto result = compareWorldGenerationBaseTiles(*a, *b, Identity{}); EXPECT_FALSE(result.valid); EXPECT_FALSE(result.equal()); EXPECT_EQ(result.comparedTiles, 0u);
}

TEST(WorldGenerationBaseTilesTest, CachePolicyNeverHidesARealSectorDataDifference) {
  auto a = std::make_unique<ServerTileSectorArray::Array>(), b = std::make_unique<ServerTileSectorArray::Array>(); fill(*a); fill(*b);
  (*b)(0, 0).collisionCache.clear(); ++(*b)(31, 31).foreground;
  auto result = compareWorldGenerationBaseTiles(*a, *b, baseTicket(), Mode::IgnoreCollisionCache); EXPECT_EQ(result.differentTiles, 1u); EXPECT_EQ(result.differentFields, mask(Field::Foreground));
}

TEST(WorldGenerationBaseTilesTest, KeepRequiresThePriorLiquidEvenWithTheSameTicketAndScalarInput) {
  Pair pair; pair.b.liquid.pressure = 9;
  WorldGenerationBaseTileLiquidInput in{EmptyMaterialId, MaterialId(1), 3, 10, false, 0};
  applyWorldGenerationBaseTileLiquid(pair.a.liquid, in); applyWorldGenerationBaseTileLiquid(pair.b.liquid, in);
  expectOnly(pair, Field::LiquidPressure); EXPECT_TRUE(baseTicket().matches(baseTicket()));
}

TEST(WorldGenerationBaseTilesTest, RealLiquidAdapterMatchesLegacyWhilePreservingAllOtherTileFields) {
  for (unsigned flags = 0; flags < 32; ++flags) {
    Pair pair; WorldGenerationBaseTileLiquidInput in{flags & 1 ? EmptyMaterialId : MaterialId(1),
        flags & 2 ? EmptyMaterialId : MaterialId(2), LiquidId(flags & 4 ? 3 : 0), 10, bool(flags & 8), flags & 16 ? 10 : 0};
    legacyLiquid(pair.a.liquid, in); applyWorldGenerationBaseTileLiquid(pair.b.liquid, in);
    EXPECT_TRUE(compareWorldGenerationBaseTile(pair.a, pair.b).equal());
  }
}

TEST(WorldGenerationBaseTilesTest, StaleTaggedResultRetainsItsPrivatePayloadUntilPhysicalDrain) {
  WorldGenerationMemoryLedger ledger; WorldGenerationTasks tasks(1, WorldRegionPartition(WorldGeometry(Vec2U{128, 128})), 1, sizeof(ServerTileSectorArray::Array));
  auto task = tasks.reserve({0, 0}, WorldGenerationTaskStage::BaseTiles, sizeof(ServerTileSectorArray::Array)); ASSERT_TRUE(task);
  auto ticket = baseTicket(); ticket.task = task->identity;
  using Payload = WorldGenerationTrackedResource<ServerTileSectorArray::ArrayPtr>;
  struct Result { Identity identity; std::shared_ptr<Payload> tiles; };
  WorkerPool pool("Tagged BaseTiles payload contract", 1);
  {
    auto tiles = makeWorldGenerationTrackedResource<ServerTileSectorArray::ArrayPtr>(ledger, WorldGenerationMemoryClass::BaseTilesArray, sizeof(ServerTileSectorArray::Array));
    tiles->value = std::make_unique<ServerTileSectorArray::Array>(); fill(*tiles->value);
    auto promise = pool.addProducer<Result>([task, ticket, tiles] { EXPECT_TRUE(task->start()); EXPECT_TRUE(task->ready()); return Result{ticket, tiles}; });
    tiles.reset(); auto owner = ticket; ++owner.tileRevision;
    auto const& result = promise.get(); EXPECT_FALSE(owner.matches(result.identity));
    EXPECT_TRUE(tasks.retire(task, WorldGenerationTaskOutcome::Stale)); pool.finish();
    EXPECT_EQ(ledger.snapshot().resources, 1u); EXPECT_TRUE(result.tiles->value);
  }
  EXPECT_EQ(ledger.snapshot().resources, 0u);
}

namespace {
using TileInput = WorldGenerationBaseTileInput;
TileInput tileInput() { return {11, 77, true, {21, 31}, {22, 32}, 3, 10, true, 0}; }
uint32_t floatBits(float value) { uint32_t result; std::memcpy(&result, &value, sizeof(result)); return result; }
enum class Lookup { MaterialHue, SupportsMod, ModHue };
struct LookupEvent {
  Lookup kind{};
  unsigned biome = 0, material = 0, mod = 0;
  std::array<uint32_t, 17> state{};
  bool operator==(LookupEvent const& other) const {
    return kind == other.kind && biome == other.biome && material == other.material && mod == other.mod && state == other.state;
  }
};
struct RecordingResolver {
  ServerTile const& tile;
  bool support = true;
  unsigned throwAt = 0;
  mutable unsigned count = 0;
  mutable std::array<LookupEvent, 6> events{};
  void record(Lookup kind, unsigned biome, unsigned material, unsigned mod) const {
    if (count == events.size()) throw std::logic_error("too many BaseTiles lookups");
    events[count++] = {kind, biome, material, mod,
        {tile.blockBiomeIndex, tile.environmentBiomeIndex, unsigned(tile.biomeTransition),
          tile.foreground, tile.foregroundHueShift, tile.foregroundMod, tile.foregroundModHueShift, tile.foregroundColorVariant,
          tile.background, tile.backgroundHueShift, tile.backgroundMod, tile.backgroundModHueShift, tile.backgroundColorVariant,
          tile.liquid.liquid, floatBits(tile.liquid.level), floatBits(tile.liquid.pressure), unsigned(tile.liquid.source)}};
    if (count == throwAt) throw std::runtime_error("controlled lookup failure");
  }
  static MaterialHue materialValue(BiomeIndex biome, MaterialId material) { return MaterialHue((unsigned(biome) * 13u + unsigned(material) * 3u + 17u) & 255u); }
  static MaterialHue modValue(BiomeIndex biome, ModId mod) { return MaterialHue((unsigned(biome) * 7u + unsigned(mod) * 5u + 23u) & 255u); }
  MaterialHue materialHue(BiomeIndex biome, MaterialId material) const { record(Lookup::MaterialHue, biome, material, NoModId); return materialValue(biome, material); }
  bool supportsMod(MaterialId material, ModId mod) const {
    record(Lookup::SupportsMod, NullBiomeIndex, material, mod);
    return mod == NoModId || (isRealMaterial(material) && isRealMod(mod) && support);
  }
  MaterialHue modHue(BiomeIndex biome, ModId mod) const { record(Lookup::ModHue, biome, NullMaterialId, mod); return modValue(biome, mod); }
};
struct FrozenResolver {
  MaterialHue material = 19, mod = 27;
  bool support = true;
  MaterialHue materialHue(BiomeIndex, MaterialId) const { return material; }
  bool supportsMod(MaterialId id, ModId candidate) const { return candidate == NoModId || (isRealMaterial(id) && isRealMod(candidate) && support); }
  MaterialHue modHue(BiomeIndex, ModId) const { return mod; }
};
// First-party legacy body kept independent of the new sequencing/liquid helpers.
template <typename Resolver>
void legacyBaseTile(ServerTile& tile, TileInput const& in, Resolver const& resolver) {
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
void missing(Pair& pair) { pair.a.foreground = pair.b.foreground = NullMaterialId; pair.a.background = pair.b.background = NullMaterialId; }
void sameTrace(RecordingResolver const& a, RecordingResolver const& b) { EXPECT_EQ(a.count, b.count); EXPECT_TRUE(a.events == b.events); }
}

TEST(WorldGenerationBaseTileInputsTest, ScalarInputHasNoOwningResourcesOrDynamicLifetime) {
  static_assert(std::is_trivially_copyable<TileInput>::value);
  static_assert(std::is_standard_layout<TileInput>::value);
  TileInput input; EXPECT_EQ(input.blockBiome, NullBiomeIndex); EXPECT_EQ(input.foreground.material, EmptyMaterialId);
  EXPECT_EQ(input.foreground.mod, NoModId); EXPECT_EQ(input.oceanLiquid, EmptyLiquidId);
}

TEST(WorldGenerationBaseTileInputsTest, BiomeMetadataAlwaysChangesEvenWhenBothLayersAlreadyExist) {
  Pair pair; RecordingResolver resolver{pair.b}; auto input = tileInput(); applyWorldGenerationBaseTile(pair.b, input, resolver);
  EXPECT_EQ(resolver.count, 0u); EXPECT_EQ(pair.b.blockBiomeIndex, 11u); EXPECT_EQ(pair.b.environmentBiomeIndex, 77u); EXPECT_TRUE(pair.b.biomeTransition);
}

TEST(WorldGenerationBaseTileInputsTest, ExistingForegroundRetainsAllFiveFieldsAndOnlyBackgroundQueries) {
  Pair pair; pair.a.background = pair.b.background = NullMaterialId; auto input = tileInput(); RecordingResolver a{pair.a}, b{pair.b};
  legacyBaseTile(pair.a, input, a); applyWorldGenerationBaseTile(pair.b, input, b);
  EXPECT_TRUE(compareWorldGenerationBaseTile(pair.a, pair.b).equal()); sameTrace(a, b); EXPECT_EQ(b.count, 3u);
  EXPECT_EQ(pair.b.foreground, 1u); EXPECT_EQ(pair.b.foregroundHueShift, 0u); EXPECT_EQ(pair.b.foregroundColorVariant, 2u);
}

TEST(WorldGenerationBaseTileInputsTest, ExistingBackgroundRetainsAllFiveFieldsAndOnlyForegroundQueries) {
  Pair pair; pair.a.foreground = pair.b.foreground = NullMaterialId; auto input = tileInput(); RecordingResolver a{pair.a}, b{pair.b};
  legacyBaseTile(pair.a, input, a); applyWorldGenerationBaseTile(pair.b, input, b);
  EXPECT_TRUE(compareWorldGenerationBaseTile(pair.a, pair.b).equal()); sameTrace(a, b); EXPECT_EQ(b.count, 3u);
  EXPECT_EQ(pair.b.background, EmptyMaterialId); EXPECT_EQ(pair.b.backgroundHueShift, 6u); EXPECT_EQ(pair.b.backgroundColorVariant, 3u);
}

TEST(WorldGenerationBaseTileInputsTest, BothMissingLayersResolveForegroundBeforeBackgroundInSixCalls) {
  Pair pair; missing(pair); RecordingResolver resolver{pair.b}; applyWorldGenerationBaseTile(pair.b, tileInput(), resolver);
  ASSERT_EQ(resolver.count, 6u);
  for (unsigned offset : {0u, 3u}) { EXPECT_EQ(resolver.events[offset].kind, Lookup::MaterialHue); EXPECT_EQ(resolver.events[offset + 1].kind, Lookup::SupportsMod); EXPECT_EQ(resolver.events[offset + 2].kind, Lookup::ModHue); }
  EXPECT_EQ(resolver.events[0].material, 21u); EXPECT_EQ(resolver.events[3].material, 22u);
  EXPECT_EQ(resolver.events[2].mod, 31u); EXPECT_EQ(resolver.events[5].mod, 32u);
}

TEST(WorldGenerationBaseTileInputsTest, UnsupportedForegroundModPreservesTheOldModAndHue) {
  Pair pair; pair.b.foreground = NullMaterialId; pair.b.foregroundMod = 99; pair.b.foregroundModHueShift = 123;
  RecordingResolver resolver{pair.b, false}; applyWorldGenerationBaseTile(pair.b, tileInput(), resolver);
  EXPECT_EQ(resolver.count, 2u); EXPECT_EQ(pair.b.foregroundMod, 99u); EXPECT_EQ(pair.b.foregroundModHueShift, 123u);
}

TEST(WorldGenerationBaseTileInputsTest, UnsupportedBackgroundModPreservesTheOldModAndHue) {
  Pair pair; pair.b.background = NullMaterialId; pair.b.backgroundMod = 99; pair.b.backgroundModHueShift = 123;
  RecordingResolver resolver{pair.b, false}; applyWorldGenerationBaseTile(pair.b, tileInput(), resolver);
  EXPECT_EQ(resolver.count, 2u); EXPECT_EQ(pair.b.backgroundMod, 99u); EXPECT_EQ(pair.b.backgroundModHueShift, 123u);
}

TEST(WorldGenerationBaseTileInputsTest, NoModStillQueriesSupportAndModHueEvenForAir) {
  Pair pair; pair.b.foreground = NullMaterialId; auto input = tileInput(); input.foreground = {EmptyMaterialId, NoModId};
  RecordingResolver resolver{pair.b, false}; applyWorldGenerationBaseTile(pair.b, input, resolver);
  EXPECT_EQ(resolver.count, 3u); EXPECT_EQ(pair.b.foregroundMod, NoModId);
  EXPECT_EQ(pair.b.foregroundModHueShift, RecordingResolver::modValue(input.blockBiome, NoModId));
}

TEST(WorldGenerationBaseTileInputsTest, SkippingNoModHueWouldChangeTheLegacyOutput) {
  Pair pair; missing(pair); auto input = tileInput(); input.foreground.mod = input.background.mod = NoModId;
  RecordingResolver resolver{pair.b}; applyWorldGenerationBaseTile(pair.b, input, resolver);
  EXPECT_EQ(resolver.count, 6u); EXPECT_NE(pair.b.foregroundModHueShift, 5u); EXPECT_NE(pair.b.backgroundModHueShift, 7u);
}

TEST(WorldGenerationBaseTileInputsTest, ExistingAirIsInitializedAndDoesNotRequestForegroundReplacement) {
  Pair pair; pair.b.foreground = EmptyMaterialId; RecordingResolver resolver{pair.b}; applyWorldGenerationBaseTile(pair.b, tileInput(), resolver);
  EXPECT_EQ(resolver.count, 0u); EXPECT_EQ(pair.b.foreground, EmptyMaterialId); EXPECT_EQ(pair.b.foregroundHueShift, 0u);
}

TEST(WorldGenerationBaseTileInputsTest, ProposedNullMaterialRetainsTheLegacyLookupBehavior) {
  Pair pair; pair.b.foreground = NullMaterialId; auto input = tileInput(); input.foreground.material = NullMaterialId;
  RecordingResolver resolver{pair.b}; applyWorldGenerationBaseTile(pair.b, input, resolver);
  EXPECT_EQ(resolver.count, 2u); EXPECT_EQ(pair.b.foreground, NullMaterialId); EXPECT_EQ(pair.b.liquid.liquid, EmptyLiquidId);
}

TEST(WorldGenerationBaseTileInputsTest, HueLookupsUseTheNewBlockBiomeNeverTheEnvironmentBiome) {
  Pair pair; missing(pair); RecordingResolver resolver{pair.b}; applyWorldGenerationBaseTile(pair.b, tileInput(), resolver);
  for (unsigned i : {0u, 2u, 3u, 5u}) EXPECT_EQ(resolver.events[i].biome, 11u);
  EXPECT_NE(pair.b.blockBiomeIndex, pair.b.environmentBiomeIndex);
}

TEST(WorldGenerationBaseTileInputsTest, TransitionFlagIsExplicitAndOverwritesPriorState) {
  Pair pair; pair.b.biomeTransition = true; auto input = tileInput(); input.biomeTransition = false;
  applyWorldGenerationBaseTile(pair.b, input, FrozenResolver{}); EXPECT_FALSE(pair.b.biomeTransition);
}

TEST(WorldGenerationBaseTileInputsTest, LiquidUsesTheResolvedMaterialsAfterBothLayers) {
  Pair pair; missing(pair); auto input = tileInput(); input.foreground.material = input.background.material = EmptyMaterialId;
  RecordingResolver resolver{pair.b}; applyWorldGenerationBaseTile(pair.b, input, resolver);
  EXPECT_EQ(pair.b.liquid.liquid, 3u); EXPECT_TRUE(pair.b.liquid.source); EXPECT_EQ(pair.b.liquid.pressure, 10.0f);
}

TEST(WorldGenerationBaseTileInputsTest, EveryLookupFailurePreservesLegacyPartialWritesAndStopsFurtherQueries) {
  for (unsigned point = 1; point <= 6; ++point) {
    Pair pair; missing(pair); auto input = tileInput(); RecordingResolver a{pair.a, true, point}, b{pair.b, true, point};
    EXPECT_THROW(legacyBaseTile(pair.a, input, a), std::runtime_error);
    EXPECT_THROW(applyWorldGenerationBaseTile(pair.b, input, b), std::runtime_error);
    EXPECT_TRUE(compareWorldGenerationBaseTile(pair.a, pair.b).equal()); sameTrace(a, b); EXPECT_EQ(b.count, point);
    EXPECT_EQ(pair.b.liquid.liquid, 3u); EXPECT_EQ(pair.b.liquid.pressure, 4.5f);
  }
}

TEST(WorldGenerationBaseTileInputsTest, UnsupportedRealModsNeverAskForTheirHue) {
  Pair pair; missing(pair); RecordingResolver resolver{pair.b, false}; applyWorldGenerationBaseTile(pair.b, tileInput(), resolver);
  EXPECT_EQ(resolver.count, 4u); for (unsigned i = 0; i < resolver.count; ++i) EXPECT_NE(resolver.events[i].kind, Lookup::ModHue);
}

TEST(WorldGenerationBaseTileInputsTest, ResolverIsBorrowedWithoutCopyingItsState) {
  struct Noncopyable : FrozenResolver { Noncopyable() = default; Noncopyable(Noncopyable const&) = delete; } resolver;
  Pair pair; missing(pair); applyWorldGenerationBaseTile(pair.b, tileInput(), resolver);
  EXPECT_EQ(pair.b.foregroundHueShift, 19u); EXPECT_EQ(pair.b.foregroundModHueShift, 27u);
}

TEST(WorldGenerationBaseTileInputsTest, DamageCollisionDungeonAndRootAreNotTouched) {
  Pair pair; missing(pair); TileDamageStatusTestAccess::change(pair.a.foregroundDamage, 1); TileDamageStatusTestAccess::change(pair.b.foregroundDamage, 1);
  auto input = tileInput(); RecordingResolver a{pair.a}, b{pair.b}; legacyBaseTile(pair.a, input, a); applyWorldGenerationBaseTile(pair.b, input, b);
  EXPECT_TRUE(compareWorldGenerationBaseTile(pair.a, pair.b).equal()); EXPECT_FALSE(pair.b.collisionCacheDirty);
  EXPECT_EQ(pair.b.dungeonId, 123u); ASSERT_TRUE(pair.b.rootSource); EXPECT_EQ(*pair.b.rootSource, Vec2I(7, 8));
}

TEST(WorldGenerationBaseTileInputsTest, BranchMatrixMatchesAllTileFieldsAndLookupTraces) {
  for (unsigned flags = 0; flags < 4096; ++flags) {
    Pair pair; auto input = tileInput();
    pair.a.foreground = pair.b.foreground = flags & 1 ? NullMaterialId : MaterialId(1);
    pair.a.background = pair.b.background = flags & 2 ? NullMaterialId : MaterialId(2);
    input.foreground = {flags & 4 ? EmptyMaterialId : MaterialId(21), flags & 16 ? NoModId : ModId(31)};
    input.background = {flags & 8 ? EmptyMaterialId : MaterialId(22), flags & 32 ? NoModId : ModId(32)};
    input.blockBiome = flags & 128 ? NullBiomeIndex : BiomeIndex(11); input.biomeTransition = flags & 256;
    input.oceanLiquid = flags & 512 ? EmptyLiquidId : LiquidId(3); input.encloseLiquids = flags & 1024; input.y = flags & 2048 ? 10 : 0;
    RecordingResolver a{pair.a, bool(flags & 64)}, b{pair.b, bool(flags & 64)};
    legacyBaseTile(pair.a, input, a); applyWorldGenerationBaseTile(pair.b, input, b);
    ASSERT_TRUE(compareWorldGenerationBaseTile(pair.a, pair.b).equal()); sameTrace(a, b);
  }
}

TEST(WorldGenerationBaseTileInputsTest, ExplicitInputIsNotModifiedByPreparation) {
  Pair pair; missing(pair); auto input = tileInput(), before = input; applyWorldGenerationBaseTile(pair.b, input, FrozenResolver{});
  EXPECT_EQ(input.blockBiome, before.blockBiome); EXPECT_EQ(input.environmentBiome, before.environmentBiome); EXPECT_EQ(input.biomeTransition, before.biomeTransition);
  EXPECT_EQ(input.foreground.material, before.foreground.material); EXPECT_EQ(input.foreground.mod, before.foreground.mod);
  EXPECT_EQ(input.background.material, before.background.material); EXPECT_EQ(input.background.mod, before.background.mod);
  EXPECT_EQ(input.oceanLiquid, before.oceanLiquid); EXPECT_EQ(input.oceanLiquidLevel, before.oceanLiquidLevel); EXPECT_EQ(input.encloseLiquids, before.encloseLiquids); EXPECT_EQ(input.y, before.y);
}

TEST(WorldGenerationBaseTileInputsTest, ImmutableResolverAndPartitionedTilesMatchTheSerialProducerBody) {
  auto expected = std::make_unique<ServerTileSectorArray::Array>(), actual = std::make_unique<ServerTileSectorArray::Array>(); fill(*expected); fill(*actual);
  std::array<TileInput, 1024> inputs; FrozenResolver const resolver;
  for (unsigned i = 0; i < 1024; ++i) {
    auto& a = (*expected)(i / 32, i % 32); auto& b = (*actual)(i / 32, i % 32);
    a.foreground = b.foreground = i % 2 ? NullMaterialId : EmptyMaterialId; a.background = b.background = i % 3 ? NullMaterialId : MaterialId(2);
    inputs[i] = tileInput(); inputs[i].y = int(i % 32); inputs[i].foreground.material = i % 5 ? EmptyMaterialId : MaterialId(21);
    legacyBaseTile(a, inputs[i], resolver);
  }
  WorkerPool pool("Explicit BaseTiles inputs contract", 4); std::vector<WorkerPoolPromise<bool>> promises;
  for (unsigned part = 0; part < 4; ++part) promises.push_back(pool.addProducer<bool>([&, part] {
    for (unsigned i = part * 256; i < (part + 1) * 256; ++i) applyWorldGenerationBaseTile((*actual)(i / 32, i % 32), inputs[i], resolver);
    return true;
  }));
  for (auto& promise : promises) { EXPECT_TRUE(promise.get()); }
  pool.finish();
  EXPECT_TRUE(compareWorldGenerationBaseTiles(*expected, *actual, baseTicket()).equal());
}

TEST(WorldGenerationBaseTileInputsTest, APartiallyWrittenTileIsNotASafeFreshRetryInput) {
  Pair pair; missing(pair); auto input = tileInput(); RecordingResolver fail{pair.b, true, 1};
  EXPECT_THROW(applyWorldGenerationBaseTile(pair.b, input, fail), std::runtime_error);
  FrozenResolver resolver; applyWorldGenerationBaseTile(pair.b, input, resolver); legacyBaseTile(pair.a, input, resolver);
  EXPECT_FALSE(compareWorldGenerationBaseTile(pair.a, pair.b).equal());
}

TEST(WorldGenerationBaseTileInputsTest, ScalarSampleAloneDoesNotCloseTheResolverOrAssetContext) {
  Pair pair; missing(pair); auto input = tileInput(); FrozenResolver a, b; b.material = 99;
  applyWorldGenerationBaseTile(pair.a, input, a); applyWorldGenerationBaseTile(pair.b, input, b);
  EXPECT_FALSE(compareWorldGenerationBaseTile(pair.a, pair.b).equal());
}

TEST(WorldGenerationBaseTileInputsTest, ObserversSeeTheSameStagedWritesBeforeEachQuery) {
  Pair pair; missing(pair); RecordingResolver resolver{pair.b}; auto input = tileInput(); applyWorldGenerationBaseTile(pair.b, input, resolver);
  ASSERT_EQ(resolver.count, 6u);
  EXPECT_EQ(resolver.events[0].state[0], 11u); EXPECT_EQ(resolver.events[0].state[1], 77u); EXPECT_EQ(resolver.events[0].state[2], 1u);
  EXPECT_EQ(resolver.events[0].state[3], 21u); EXPECT_EQ(resolver.events[0].state[7], DefaultMaterialColorVariant); EXPECT_EQ(resolver.events[0].state[8], NullMaterialId);
  EXPECT_EQ(resolver.events[1].state[4], RecordingResolver::materialValue(11, 21));
  EXPECT_EQ(resolver.events[2].state[5], 31u); EXPECT_EQ(resolver.events[2].state[6], 5u);
  EXPECT_EQ(resolver.events[3].state[8], 22u); EXPECT_EQ(resolver.events[3].state[12], DefaultMaterialColorVariant);
  for (unsigned i = 0; i < 6; ++i) EXPECT_EQ(resolver.events[i].state[15], floatBits(4.5f));
}

namespace {
using Observation = WorldGenerationBaseTileObservation;
using ObservedLookup = WorldGenerationBaseTileLookup;
Observation capturedTile() {
  ServerTile tile; populate(tile); tile.foreground = tile.background = NullMaterialId;
  Observation observation; observeWorldGenerationBaseTile(tile, tileInput(), FrozenResolver{}, observation);
  return observation;
}
void expectCaptureRefused(Observation const& observation) {
  Pair pair; missing(pair);
  EXPECT_FALSE(replayWorldGenerationBaseTile(pair.b, tileInput(), observation));
  EXPECT_TRUE(compareWorldGenerationBaseTile(pair.a, pair.b).equal());
}
}

TEST(WorldGenerationBaseTileObservationTest, DefaultCaptureIsInvalidAndHasValueOnlyStorage) {
  static_assert(std::is_trivially_copyable<Observation>::value);
  static_assert(std::is_standard_layout<Observation>::value);
  Observation observation; EXPECT_FALSE(observation.valid()); expectCaptureRefused(observation);
}

TEST(WorldGenerationBaseTileObservationTest, ExistingLayersProduceACompleteEmptyTrace) {
  Pair pair; Observation observation;
  observeWorldGenerationBaseTile(pair.a, tileInput(), FrozenResolver{}, observation);
  EXPECT_TRUE(observation.valid()); EXPECT_EQ(observation.count, 0u);
  EXPECT_TRUE(replayWorldGenerationBaseTile(pair.b, tileInput(), observation));
  EXPECT_TRUE(compareWorldGenerationBaseTile(pair.a, pair.b).equal());
}

TEST(WorldGenerationBaseTileObservationTest, BothMissingLayersCaptureSixOrderedKeyedValues) {
  auto observation = capturedTile(); ASSERT_TRUE(observation.valid()); ASSERT_EQ(observation.count, 6u);
  EXPECT_EQ(observation.lookups[0].kind, ObservedLookup::MaterialHue); EXPECT_EQ(observation.lookups[0].biome, 11u);
  EXPECT_EQ(observation.lookups[0].material, 21u); EXPECT_EQ(observation.lookups[0].value, 19u);
  EXPECT_EQ(observation.lookups[1].kind, ObservedLookup::SupportsMod); EXPECT_EQ(observation.lookups[1].mod, 31u);
  EXPECT_EQ(observation.lookups[2].kind, ObservedLookup::ModHue); EXPECT_EQ(observation.lookups[2].value, 27u);
  EXPECT_EQ(observation.lookups[3].material, 22u); EXPECT_EQ(observation.lookups[5].mod, 32u);
}

TEST(WorldGenerationBaseTileObservationTest, UnsupportedModsDoNotRecordUnperformedHueQueries) {
  Pair pair; missing(pair); FrozenResolver resolver; resolver.support = false; Observation observation;
  observeWorldGenerationBaseTile(pair.a, tileInput(), resolver, observation);
  ASSERT_TRUE(observation.valid()); EXPECT_EQ(observation.count, 4u);
  EXPECT_TRUE(replayWorldGenerationBaseTile(pair.b, tileInput(), observation));
  EXPECT_TRUE(compareWorldGenerationBaseTile(pair.a, pair.b).equal());
}

TEST(WorldGenerationBaseTileObservationTest, NoModHueIsCapturedEvenForAir) {
  Pair pair; missing(pair); auto input = tileInput(); input.foreground = input.background = {EmptyMaterialId, NoModId};
  Observation observation; observeWorldGenerationBaseTile(pair.a, input, FrozenResolver{}, observation);
  ASSERT_TRUE(observation.valid()); EXPECT_EQ(observation.count, 6u);
  EXPECT_TRUE(replayWorldGenerationBaseTile(pair.b, input, observation));
  EXPECT_EQ(pair.b.foregroundModHueShift, 27u); EXPECT_EQ(pair.b.backgroundModHueShift, 27u);
  EXPECT_TRUE(compareWorldGenerationBaseTile(pair.a, pair.b).equal());
}

TEST(WorldGenerationBaseTileObservationTest, ExistingForegroundSkipsItsThreeObservations) {
  Pair pair; pair.a.background = pair.b.background = NullMaterialId; Observation observation;
  observeWorldGenerationBaseTile(pair.a, tileInput(), FrozenResolver{}, observation);
  ASSERT_TRUE(observation.valid()); EXPECT_EQ(observation.count, 3u); EXPECT_EQ(observation.lookups[0].material, 22u);
  EXPECT_TRUE(replayWorldGenerationBaseTile(pair.b, tileInput(), observation));
  EXPECT_TRUE(compareWorldGenerationBaseTile(pair.a, pair.b).equal());
}

TEST(WorldGenerationBaseTileObservationTest, ExistingBackgroundSkipsItsThreeObservations) {
  Pair pair; pair.a.foreground = pair.b.foreground = NullMaterialId; Observation observation;
  observeWorldGenerationBaseTile(pair.a, tileInput(), FrozenResolver{}, observation);
  ASSERT_TRUE(observation.valid()); EXPECT_EQ(observation.count, 3u); EXPECT_EQ(observation.lookups[0].material, 21u);
  EXPECT_TRUE(replayWorldGenerationBaseTile(pair.b, tileInput(), observation));
  EXPECT_TRUE(compareWorldGenerationBaseTile(pair.a, pair.b).equal());
}

TEST(WorldGenerationBaseTileObservationTest, CapturingDoesNotChangeTheLegacyQueryTraceOrStagedWrites) {
  Pair pair; missing(pair); RecordingResolver a{pair.a}, b{pair.b}; Observation observation;
  legacyBaseTile(pair.a, tileInput(), a); observeWorldGenerationBaseTile(pair.b, tileInput(), b, observation);
  ASSERT_TRUE(observation.valid()); sameTrace(a, b); EXPECT_TRUE(compareWorldGenerationBaseTile(pair.a, pair.b).equal());
}

TEST(WorldGenerationBaseTileObservationTest, EveryThrowRecordsOnlyReturnedValuesAndPreservesPartialWrites) {
  for (unsigned point = 1; point <= 6; ++point) {
    Pair pair; missing(pair); RecordingResolver a{pair.a, true, point}, b{pair.b, true, point}; Observation observation;
    EXPECT_THROW(legacyBaseTile(pair.a, tileInput(), a), std::runtime_error);
    EXPECT_THROW(observeWorldGenerationBaseTile(pair.b, tileInput(), b, observation), std::runtime_error);
    EXPECT_FALSE(observation.complete); EXPECT_FALSE(observation.valid()); EXPECT_EQ(observation.count, point - 1);
    sameTrace(a, b); EXPECT_TRUE(compareWorldGenerationBaseTile(pair.a, pair.b).equal()); expectCaptureRefused(observation);
  }
}

TEST(WorldGenerationBaseTileObservationTest, FailedCaptureCannotLeaveAnEarlierSuccessfulCaptureValid) {
  auto observation = capturedTile(); ASSERT_TRUE(observation.valid()); Pair pair; missing(pair); RecordingResolver resolver{pair.b, true, 1};
  EXPECT_THROW(observeWorldGenerationBaseTile(pair.b, tileInput(), resolver, observation), std::runtime_error);
  EXPECT_FALSE(observation.complete); EXPECT_EQ(observation.count, 0u); expectCaptureRefused(observation);
}

TEST(WorldGenerationBaseTileObservationTest, EveryRelevantOriginalFieldIsCheckedBeforeAnyWrite) {
  std::array<std::function<void(ServerTile&)>, 14> changes{{
      [](auto& t) { t.foreground = 3; }, [](auto& t) { ++t.foregroundHueShift; }, [](auto& t) { t.foregroundMod = 3; },
      [](auto& t) { ++t.foregroundModHueShift; }, [](auto& t) { ++t.foregroundColorVariant; },
      [](auto& t) { t.background = 3; }, [](auto& t) { ++t.backgroundHueShift; }, [](auto& t) { t.backgroundMod = 3; },
      [](auto& t) { ++t.backgroundModHueShift; }, [](auto& t) { ++t.backgroundColorVariant; },
      [](auto& t) { ++t.liquid.liquid; }, [](auto& t) { t.liquid.level = -0.0f; },
      [](auto& t) { t.liquid.pressure = bits(0x7fc00001u); }, [](auto& t) { t.liquid.source = !t.liquid.source; }}};
  auto observation = capturedTile();
  for (auto change : changes) {
    Pair pair; missing(pair); change(pair.a); change(pair.b);
    EXPECT_FALSE(replayWorldGenerationBaseTile(pair.b, tileInput(), observation));
    EXPECT_TRUE(compareWorldGenerationBaseTile(pair.a, pair.b).equal());
  }
}

TEST(WorldGenerationBaseTileObservationTest, EveryScalarInputFieldIsCheckedBeforeAnyWrite) {
  std::array<std::function<void(TileInput&)>, 11> changes{{
      [](auto& v) { ++v.blockBiome; }, [](auto& v) { ++v.environmentBiome; }, [](auto& v) { v.biomeTransition = !v.biomeTransition; },
      [](auto& v) { ++v.foreground.material; }, [](auto& v) { ++v.foreground.mod; },
      [](auto& v) { ++v.background.material; }, [](auto& v) { ++v.background.mod; },
      [](auto& v) { ++v.oceanLiquid; }, [](auto& v) { ++v.oceanLiquidLevel; },
      [](auto& v) { v.encloseLiquids = !v.encloseLiquids; }, [](auto& v) { ++v.y; }}};
  auto observation = capturedTile();
  for (auto change : changes) {
    auto input = tileInput(); change(input); Pair pair; missing(pair);
    EXPECT_FALSE(replayWorldGenerationBaseTile(pair.b, input, observation));
    EXPECT_TRUE(compareWorldGenerationBaseTile(pair.a, pair.b).equal());
  }
}

TEST(WorldGenerationBaseTileObservationTest, AllThreeUnsupportedVersionsAreRejectedBeforeAnyWrite) {
  auto observation = capturedTile(); ++observation.modelVersion; expectCaptureRefused(observation);
  observation = capturedTile(); ++observation.inputVersion; expectCaptureRefused(observation);
  observation = capturedTile(); ++observation.liquidVersion; expectCaptureRefused(observation);
}

TEST(WorldGenerationBaseTileObservationTest, OversizedCountsAreRejectedWithoutReadingPastTheFixedTrace) {
  auto observation = capturedTile(); observation.count = 7; expectCaptureRefused(observation);
  observation.count = std::numeric_limits<unsigned>::max(); expectCaptureRefused(observation);
}

TEST(WorldGenerationBaseTileObservationTest, EveryTruncatedTraceIsRejectedBeforeAnyWrite) {
  for (unsigned count = 0; count < 6; ++count) { auto observation = capturedTile(); observation.count = count; expectCaptureRefused(observation); }
}

TEST(WorldGenerationBaseTileObservationTest, ExtraQueriesOnAnOtherwiseCompleteTraceAreRejected) {
  Pair pair; Observation observation; observeWorldGenerationBaseTile(pair.a, tileInput(), FrozenResolver{}, observation);
  observation.count = 1; observation.lookups[0] = capturedTile().lookups[0];
  EXPECT_FALSE(replayWorldGenerationBaseTile(pair.b, tileInput(), observation));
  EXPECT_EQ(pair.b.blockBiomeIndex, 3u); EXPECT_EQ(pair.b.environmentBiomeIndex, 4u);
}

TEST(WorldGenerationBaseTileObservationTest, EveryQueryKindAndArgumentIsPreflightedIncludingTheLastQuery) {
  for (unsigned i = 0; i < 6; ++i) {
    auto observation = capturedTile(); observation.lookups[i].kind = ObservedLookup::Invalid; expectCaptureRefused(observation);
    observation = capturedTile(); ++observation.lookups[i].biome; expectCaptureRefused(observation);
    observation = capturedTile(); ++observation.lookups[i].material; expectCaptureRefused(observation);
    observation = capturedTile(); ++observation.lookups[i].mod; expectCaptureRefused(observation);
  }
  auto observation = capturedTile(); std::swap(observation.lookups[0], observation.lookups[3]); expectCaptureRefused(observation);
}

TEST(WorldGenerationBaseTileObservationTest, OutOfRangeHueAndBooleanValuesAreRejectedBeforeAnyWrite) {
  for (unsigned i = 0; i < 6; ++i) {
    auto observation = capturedTile(); observation.lookups[i].value = i % 3 == 1 ? 2u : 256u; expectCaptureRefused(observation);
  }
}

TEST(WorldGenerationBaseTileObservationTest, BranchMatrixMatchesTheIndependentLegacyBodyAndCapturedTrace) {
  for (unsigned flags = 0; flags < 4096; ++flags) {
    Pair pair; ServerTile replay; populate(replay); auto input = tileInput();
    pair.a.foreground = pair.b.foreground = replay.foreground = flags & 1 ? NullMaterialId : MaterialId(1);
    pair.a.background = pair.b.background = replay.background = flags & 2 ? NullMaterialId : MaterialId(2);
    input.foreground = {flags & 4 ? EmptyMaterialId : MaterialId(21), flags & 16 ? NoModId : ModId(31)};
    input.background = {flags & 8 ? EmptyMaterialId : MaterialId(22), flags & 32 ? NoModId : ModId(32)};
    input.blockBiome = flags & 128 ? NullBiomeIndex : BiomeIndex(11); input.biomeTransition = flags & 256;
    input.oceanLiquid = flags & 512 ? EmptyLiquidId : LiquidId(3); input.encloseLiquids = flags & 1024; input.y = flags & 2048 ? 10 : 0;
    RecordingResolver a{pair.a, bool(flags & 64)}, b{pair.b, bool(flags & 64)}; Observation observation;
    legacyBaseTile(pair.a, input, a); observeWorldGenerationBaseTile(pair.b, input, b, observation);
    ASSERT_TRUE(observation.valid()); sameTrace(a, b);
    ASSERT_TRUE(replayWorldGenerationBaseTile(replay, input, observation));
    ASSERT_TRUE(compareWorldGenerationBaseTile(pair.a, pair.b).equal());
    ASSERT_TRUE(compareWorldGenerationBaseTile(pair.a, replay).equal());
  }
}

TEST(WorldGenerationBaseTileObservationTest, OneImmutableObservationCanServeIndependentCursorsAcrossFourWorkers) {
  auto expected = std::make_unique<ServerTileSectorArray::Array>(), actual = std::make_unique<ServerTileSectorArray::Array>();
  auto observation = capturedTile();
  for (unsigned x = 0; x < 32; ++x) for (unsigned y = 0; y < 32; ++y) {
    auto& a = (*expected)(x, y); auto& b = (*actual)(x, y); populate(a); populate(b);
    a.foreground = a.background = b.foreground = b.background = NullMaterialId;
    legacyBaseTile(a, tileInput(), FrozenResolver{});
  }
  WorkerPool pool("Frozen BaseTiles observations contract", 4); std::vector<WorkerPoolPromise<bool>> promises;
  for (unsigned part = 0; part < 4; ++part) promises.push_back(pool.addProducer<bool>([&, part] {
    for (unsigned i = part * 256; i < (part + 1) * 256; ++i)
      if (!replayWorldGenerationBaseTile((*actual)(i / 32, i % 32), tileInput(), observation)) return false;
    return true;
  }));
  for (auto& promise : promises) { EXPECT_TRUE(promise.get()); }
  pool.finish(); EXPECT_TRUE(compareWorldGenerationBaseTiles(*expected, *actual, baseTicket()).equal());
  EXPECT_TRUE(observation.valid()); EXPECT_EQ(observation.count, 6u);
}

TEST(WorldGenerationBaseTileObservationTest, LiquidGuardAndKeepPreserveSignedZeroAndNanPayloadBits) {
  Pair pair; pair.a.foreground = pair.b.foreground = EmptyMaterialId;
  pair.a.liquid.level = pair.b.liquid.level = -0.0f; pair.a.liquid.pressure = pair.b.liquid.pressure = bits(0x7fc00123u);
  auto input = tileInput(); input.encloseLiquids = false; input.oceanLiquid = EmptyLiquidId; Observation observation;
  observeWorldGenerationBaseTile(pair.a, input, FrozenResolver{}, observation);
  EXPECT_TRUE(replayWorldGenerationBaseTile(pair.b, input, observation));
  EXPECT_TRUE(compareWorldGenerationBaseTile(pair.a, pair.b).equal());
  pair.b.liquid.level = 0.0f; EXPECT_FALSE(replayWorldGenerationBaseTile(pair.b, input, observation));
}

TEST(WorldGenerationBaseTileObservationTest, CaptureAcceptsInputAliasedToTheOutputBeforeResettingIt) {
  Pair pair; missing(pair); Observation observation; observation.input = tileInput();
  observeWorldGenerationBaseTile(pair.a, observation.input, FrozenResolver{}, observation);
  EXPECT_TRUE(replayWorldGenerationBaseTile(pair.b, tileInput(), observation));
  EXPECT_TRUE(compareWorldGenerationBaseTile(pair.a, pair.b).equal());
}

TEST(WorldGenerationBaseTileObservationTest, ReplayDoesNotRetainOrConsultTheOriginalResolver) {
  Pair pair; missing(pair); Observation observation;
  { RecordingResolver resolver{pair.a}; observeWorldGenerationBaseTile(pair.a, tileInput(), resolver, observation); }
  EXPECT_TRUE(replayWorldGenerationBaseTile(pair.b, tileInput(), observation));
  EXPECT_TRUE(compareWorldGenerationBaseTile(pair.a, pair.b).equal());
}

TEST(WorldGenerationBaseTileObservationTest, RepeatedReplayStartsFromFreshOriginalsNotAPersistentCursor) {
  auto observation = capturedTile();
  for (unsigned i = 0; i < 10; ++i) {
    Pair pair; missing(pair); legacyBaseTile(pair.a, tileInput(), FrozenResolver{});
    EXPECT_TRUE(replayWorldGenerationBaseTile(pair.b, tileInput(), observation));
    EXPECT_TRUE(compareWorldGenerationBaseTile(pair.a, pair.b).equal());
  }
}

TEST(WorldGenerationBaseTileObservationTest, PartialTileIsRejectedEvenWhenTheProposedMaterialRemainsNull) {
  Pair pair; missing(pair); auto input = tileInput(); input.foreground.material = NullMaterialId; Observation observation;
  observeWorldGenerationBaseTile(pair.a, input, FrozenResolver{}, observation);
  RecordingResolver fail{pair.b, true, 1}; EXPECT_THROW(applyWorldGenerationBaseTile(pair.b, input, fail), std::runtime_error);
  EXPECT_EQ(pair.b.foreground, NullMaterialId); auto before = Observation::sourceState(pair.b);
  EXPECT_FALSE(replayWorldGenerationBaseTile(pair.b, input, observation)); EXPECT_EQ(Observation::sourceState(pair.b), before);
}

TEST(WorldGenerationBaseTileObservationTest, UnrelatedInitializedFieldsPassThroughAndAreNotAContentKey) {
  Pair pair; missing(pair); auto observation = capturedTile();
  for (auto* tile : {&pair.a, &pair.b}) {
    tile->rootSource = Vec2I{99, 55}; tile->dungeonId = 88; tile->collisionCacheDirty = true;
    TileDamageStatusTestAccess::change(tile->foregroundDamage, 1);
  }
  legacyBaseTile(pair.a, tileInput(), FrozenResolver{});
  EXPECT_TRUE(replayWorldGenerationBaseTile(pair.b, tileInput(), observation));
  EXPECT_TRUE(compareWorldGenerationBaseTile(pair.a, pair.b).equal()); EXPECT_EQ(pair.b.dungeonId, 88u);
}

TEST(WorldGenerationBaseTileObservationTest, EqualQueryKeysCanHaveDifferentPositionDependentReturnedValues) {
  struct Resolver {
    mutable unsigned cursor = 0;
    MaterialHue materialHue(BiomeIndex, MaterialId) const { return MaterialHue(++cursor); }
    bool supportsMod(MaterialId, ModId) const { return ++cursor != 2; }
    MaterialHue modHue(BiomeIndex, ModId) const { return MaterialHue(++cursor); }
  } resolver;
  Pair pair; missing(pair); auto input = tileInput(); input.background = input.foreground; Observation observation;
  observeWorldGenerationBaseTile(pair.a, input, resolver, observation); ASSERT_TRUE(observation.valid()); EXPECT_EQ(observation.count, 5u);
  EXPECT_TRUE(replayWorldGenerationBaseTile(pair.b, input, observation)); EXPECT_NE(pair.b.foregroundHueShift, pair.b.backgroundHueShift);
  EXPECT_TRUE(compareWorldGenerationBaseTile(pair.a, pair.b).equal());
}

// Explicit opt-in micro-cost experiment, not a gameplay throughput benchmark or
// a timing assertion in CI. Preallocate/initialize outside the measured ranges.
TEST(WorldGenerationBaseTileObservationTest, DISABLED_SyntheticObservationCosts) {
  constexpr unsigned count = 8192, rounds = 9;
  std::vector<ServerTile> tiles(count); std::vector<Observation> observations(count);
  std::array<std::array<double, rounds>, 3> timings{}; auto input = tileInput(); FrozenResolver resolver;
  size_t checksum = 0;
  for (unsigned round = 0; round < rounds; ++round) for (unsigned position = 0; position < 3; ++position) {
    unsigned mode = (round + position) % 3;
    for (auto& tile : tiles) { populate(tile); tile.foreground = tile.background = NullMaterialId; }
    if (mode == 2) {
      auto observation = capturedTile(); for (auto& value : observations) value = observation;
    }
    auto begin = std::chrono::steady_clock::now();
    for (unsigned i = 0; i < count; ++i) {
      if (mode == 0) applyWorldGenerationBaseTile(tiles[i], input, resolver);
      else if (mode == 1) observeWorldGenerationBaseTile(tiles[i], input, resolver, observations[i]);
      else checksum += replayWorldGenerationBaseTile(tiles[i], input, observations[i]);
    }
    timings[mode][round] = std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - begin).count() / count;
    for (auto const& tile : tiles) checksum += tile.foregroundHueShift + tile.backgroundModHueShift;
  }
  for (unsigned mode = 0; mode < 3; ++mode) {
    std::sort(timings[mode].begin(), timings[mode].end());
    std::printf("BaseTileObservationCost mode=%u tiles=%u rounds=%u medianNs=%.2f minNs=%.2f maxNs=%.2f captureBytes=%zu\n",
        mode, count, rounds, timings[mode][rounds / 2], timings[mode].front(), timings[mode].back(), sizeof(Observation));
  }
  EXPECT_EQ(checksum, size_t(rounds) * count * (3 * (19 + 27) + 1));
}

namespace {
using LookupScope = WorldGenerationBaseTileLookupScope;
using LookupContext = WorldGenerationBaseTileLookupContext<32>;
LookupScope lookupScope() { return {1, 7, 3, 5, 9}; }
template <typename Resolver>
Observation lookupSample(TileInput input, unsigned missingLayers, Resolver const& resolver) {
  ServerTile tile; populate(tile);
  if (missingLayers & 1) tile.foreground = NullMaterialId;
  if (missingLayers & 2) tile.background = NullMaterialId;
  Observation observation; observeWorldGenerationBaseTile(tile, input, resolver, observation); return observation;
}
}

TEST(WorldGenerationBaseTileLookupContextTest, ContextMustBeSealedBeforeItCanWriteAnything) {
  LookupContext context(lookupScope()); EXPECT_FALSE(context.valid()); EXPECT_EQ(context.size(), 0u);
  ASSERT_TRUE(context.add(lookupScope(), capturedTile())); Pair pair; missing(pair);
  EXPECT_FALSE(context.apply(pair.b, tileInput(), lookupScope())); EXPECT_TRUE(compareWorldGenerationBaseTile(pair.a, pair.b).equal());
}

TEST(WorldGenerationBaseTileLookupContextTest, ExplicitScopeRejectsMissingGenerationsAndUnsupportedVersions) {
  std::array<std::function<void(LookupScope&)>, 7> changes{{
      [](auto& s) { s.worldEpoch = 0; }, [](auto& s) { s.assetsGeneration = 0; }, [](auto& s) { s.materialsGeneration = 0; },
      [](auto& s) { s.sourceEpoch = 0; }, [](auto& s) { ++s.modelVersion; }, [](auto& s) { ++s.inputVersion; }, [](auto& s) { ++s.liquidVersion; }}};
  for (auto change : changes) {
    auto scope = lookupScope(); change(scope); LookupContext context(scope);
    EXPECT_FALSE(context.add(scope, capturedTile())); EXPECT_FALSE(context.seal()); EXPECT_FALSE(context.valid());
    Pair pair; missing(pair); EXPECT_FALSE(context.apply(pair.b, tileInput(), scope)); EXPECT_TRUE(compareWorldGenerationBaseTile(pair.a, pair.b).equal());
  }
}

TEST(WorldGenerationBaseTileLookupContextTest, EveryScopeComponentGuardsBothAdmissionAndApplication) {
  std::array<std::function<void(LookupScope&)>, 8> changes{{
      [](auto& s) { ++s.worldEpoch; }, [](auto& s) { ++s.templateRevision; }, [](auto& s) { ++s.assetsGeneration; },
      [](auto& s) { ++s.materialsGeneration; }, [](auto& s) { ++s.sourceEpoch; }, [](auto& s) { ++s.modelVersion; }, [](auto& s) { ++s.inputVersion; }, [](auto& s) { ++s.liquidVersion; }}};
  for (auto change : changes) {
    LookupContext context(lookupScope()); auto stale = lookupScope(); change(stale);
    EXPECT_FALSE(context.add(stale, capturedTile())); EXPECT_EQ(context.size(), 0u);
    ASSERT_TRUE(context.add(lookupScope(), capturedTile())); ASSERT_TRUE(context.seal()); Pair pair; missing(pair);
    EXPECT_FALSE(context.apply(pair.b, tileInput(), stale)); EXPECT_TRUE(compareWorldGenerationBaseTile(pair.a, pair.b).equal());
  }
}

TEST(WorldGenerationBaseTileLookupContextTest, ZeroTemplateRevisionIsAnExplicitValidRevision) {
  auto scope = lookupScope(); scope.templateRevision = 0; LookupContext context(scope);
  EXPECT_TRUE(context.add(scope, capturedTile())); EXPECT_TRUE(context.seal()); Pair pair; missing(pair);
  EXPECT_TRUE(context.apply(pair.b, tileInput(), scope));
}

TEST(WorldGenerationBaseTileLookupContextTest, SealedCatalogMatchesTheIndependentBodyAndFrozenReplay) {
  auto observation = capturedTile(); LookupContext context(lookupScope()); ASSERT_TRUE(context.add(lookupScope(), observation)); ASSERT_TRUE(context.seal());
  Pair pair; missing(pair); ServerTile replay; populate(replay); replay.foreground = replay.background = NullMaterialId;
  legacyBaseTile(pair.a, tileInput(), FrozenResolver{}); ASSERT_TRUE(context.apply(pair.b, tileInput(), lookupScope()));
  ASSERT_TRUE(replayWorldGenerationBaseTile(replay, tileInput(), observation));
  EXPECT_TRUE(compareWorldGenerationBaseTile(pair.a, pair.b).equal()); EXPECT_TRUE(compareWorldGenerationBaseTile(pair.a, replay).equal());
}

TEST(WorldGenerationBaseTileLookupContextTest, EachUniqueQueryUsesOneEightByteWordNotAPerTileTape) {
  LookupContext context(lookupScope()); ASSERT_TRUE(context.add(lookupScope(), capturedTile()));
  EXPECT_EQ(context.size(), 6u); EXPECT_EQ(context.activeEntryBytes(), 48u); EXPECT_EQ(context.reservedEntryBytes(), 256u);
  EXPECT_FALSE(std::is_copy_constructible<LookupContext>::value);
  for (unsigned i = 0; i < 1024; ++i) ASSERT_TRUE(context.add(lookupScope(), capturedTile()));
  EXPECT_EQ(context.size(), 6u); EXPECT_EQ(context.activeEntryBytes(), 48u);
}

TEST(WorldGenerationBaseTileLookupContextTest, IdenticalForegroundAndBackgroundKeysDeduplicateWithinOneSample) {
  auto input = tileInput(); input.background = input.foreground; auto sample = lookupSample(input, 3, FrozenResolver{});
  ASSERT_EQ(sample.count, 6u); LookupContext context(lookupScope()); ASSERT_TRUE(context.add(lookupScope(), sample));
  EXPECT_EQ(context.size(), 3u); ASSERT_TRUE(context.seal()); Pair pair; missing(pair);
  legacyBaseTile(pair.a, input, FrozenResolver{}); EXPECT_TRUE(context.apply(pair.b, input, lookupScope()));
  EXPECT_TRUE(compareWorldGenerationBaseTile(pair.a, pair.b).equal());
}

TEST(WorldGenerationBaseTileLookupContextTest, UnionSharesSupportKeysAcrossBiomesWithoutMixingHueKeys) {
  LookupContext context(lookupScope()); auto input = tileInput(); ASSERT_TRUE(context.add(lookupScope(), capturedTile()));
  input.blockBiome = 99; ASSERT_TRUE(context.add(lookupScope(), lookupSample(input, 3, FrozenResolver{})));
  EXPECT_EQ(context.size(), 10u); ASSERT_TRUE(context.seal()); Pair pair; missing(pair);
  legacyBaseTile(pair.a, input, FrozenResolver{}); EXPECT_TRUE(context.apply(pair.b, input, lookupScope()));
  EXPECT_TRUE(compareWorldGenerationBaseTile(pair.a, pair.b).equal());
}

TEST(WorldGenerationBaseTileLookupContextTest, CapacityRefusalDoesNotPartiallyInsertANewSample) {
  WorldGenerationBaseTileLookupContext<6> context(lookupScope()); ASSERT_TRUE(context.add(lookupScope(), capturedTile()));
  auto input = tileInput(); input.blockBiome = 99;
  EXPECT_FALSE(context.add(lookupScope(), lookupSample(input, 3, FrozenResolver{}))); EXPECT_EQ(context.size(), 6u);
  EXPECT_FALSE(context.conflicted()); ASSERT_TRUE(context.seal()); Pair pair; missing(pair);
  EXPECT_FALSE(context.apply(pair.b, input, lookupScope())); EXPECT_TRUE(compareWorldGenerationBaseTile(pair.a, pair.b).equal());
  EXPECT_TRUE(context.apply(pair.b, tileInput(), lookupScope()));
}

TEST(WorldGenerationBaseTileLookupContextTest, TooSmallFirstSampleLeavesTheBuilderEmptyAndUsable) {
  WorldGenerationBaseTileLookupContext<5> context(lookupScope()); EXPECT_FALSE(context.add(lookupScope(), capturedTile()));
  EXPECT_EQ(context.size(), 0u); EXPECT_FALSE(context.conflicted()); auto input = tileInput(); input.background = input.foreground;
  EXPECT_TRUE(context.add(lookupScope(), lookupSample(input, 3, FrozenResolver{}))); EXPECT_EQ(context.size(), 3u); EXPECT_TRUE(context.seal());
}

TEST(WorldGenerationBaseTileLookupContextTest, ZeroCapacityHandlesCompleteEmptySamplesWithoutPointerArithmetic) {
  WorldGenerationBaseTileLookupContext<0> context(lookupScope());
  EXPECT_TRUE(context.add(lookupScope(), lookupSample(tileInput(), 0, FrozenResolver{}))); EXPECT_TRUE(context.seal());
  Pair pair; legacyBaseTile(pair.a, tileInput(), FrozenResolver{});
  EXPECT_TRUE(context.apply(pair.b, tileInput(), lookupScope())); EXPECT_TRUE(compareWorldGenerationBaseTile(pair.a, pair.b).equal());
}

TEST(WorldGenerationBaseTileLookupContextTest, EmptySealedContextRefusesUncoveredMissingLayersBeforeMetadataWrites) {
  LookupContext context(lookupScope()); ASSERT_TRUE(context.seal()); Pair pair; missing(pair);
  EXPECT_FALSE(context.apply(pair.b, tileInput(), lookupScope())); EXPECT_TRUE(compareWorldGenerationBaseTile(pair.a, pair.b).equal());
}

TEST(WorldGenerationBaseTileLookupContextTest, IncompleteOrMalformedObservationsCannotAddEntries) {
  LookupContext context(lookupScope()); auto sample = capturedTile(); sample.complete = false;
  EXPECT_FALSE(context.add(lookupScope(), sample)); sample = capturedTile(); sample.lookups[5].value = 256;
  EXPECT_FALSE(context.add(lookupScope(), sample)); EXPECT_EQ(context.size(), 0u); EXPECT_FALSE(context.conflicted());
  EXPECT_TRUE(context.add(lookupScope(), capturedTile())); EXPECT_TRUE(context.seal());
}

TEST(WorldGenerationBaseTileLookupContextTest, ConflictingResultsForAnyQueryKindPermanentlyPoisonTheBuilder) {
  for (unsigned kind = 0; kind < 3; ++kind) {
    LookupContext context(lookupScope()); ASSERT_TRUE(context.add(lookupScope(), capturedTile())); FrozenResolver resolver;
    if (kind == 0) ++resolver.material;
    else if (kind == 1) resolver.support = false;
    else ++resolver.mod;
    EXPECT_FALSE(context.add(lookupScope(), lookupSample(tileInput(), 3, resolver)));
    EXPECT_TRUE(context.conflicted()); EXPECT_EQ(context.size(), 6u); EXPECT_FALSE(context.seal());
    EXPECT_FALSE(context.add(lookupScope(), capturedTile())); Pair pair; missing(pair);
    EXPECT_FALSE(context.apply(pair.b, tileInput(), lookupScope())); EXPECT_TRUE(compareWorldGenerationBaseTile(pair.a, pair.b).equal());
  }
}

TEST(WorldGenerationBaseTileLookupContextTest, PositionDependentValuesForEqualKeysCannotBeSilentlyMemoized) {
  struct Resolver {
    mutable unsigned cursor = 0;
    MaterialHue materialHue(BiomeIndex, MaterialId) const { return MaterialHue(++cursor); }
    bool supportsMod(MaterialId, ModId) const { return true; }
    MaterialHue modHue(BiomeIndex, ModId) const { return MaterialHue(++cursor); }
  } resolver;
  auto input = tileInput(); input.background = input.foreground; auto sample = lookupSample(input, 3, resolver); ASSERT_TRUE(sample.valid());
  LookupContext context(lookupScope()); EXPECT_FALSE(context.add(lookupScope(), sample));
  EXPECT_TRUE(context.conflicted()); EXPECT_EQ(context.size(), 0u); EXPECT_FALSE(context.seal());
}

TEST(WorldGenerationBaseTileLookupContextTest, SealPreventsFurtherMutationAndCanBeRepeatedWithoutWriting) {
  LookupContext context(lookupScope()); ASSERT_TRUE(context.add(lookupScope(), capturedTile())); ASSERT_TRUE(context.seal());
  EXPECT_FALSE(context.add(lookupScope(), capturedTile())); EXPECT_EQ(context.size(), 6u); EXPECT_TRUE(context.seal()); EXPECT_TRUE(context.valid());
}

TEST(WorldGenerationBaseTileLookupContextTest, MissingLastBackgroundHueIsRefusedBeforeAnyLayerOrBiomeWrite) {
  LookupContext context(lookupScope()); auto input = tileInput();
  ASSERT_TRUE(context.add(lookupScope(), lookupSample(input, 1, FrozenResolver{})));
  auto alternate = input; alternate.background.mod = 34; FrozenResolver unsupported; unsupported.support = false;
  ASSERT_TRUE(context.add(lookupScope(), lookupSample(alternate, 2, unsupported)));
  alternate = input; alternate.blockBiome = 99; ASSERT_TRUE(context.add(lookupScope(), lookupSample(alternate, 2, FrozenResolver{})));
  ASSERT_TRUE(context.seal()); Pair pair; missing(pair);
  EXPECT_FALSE(context.apply(pair.b, input, lookupScope())); EXPECT_TRUE(compareWorldGenerationBaseTile(pair.a, pair.b).equal());
}

TEST(WorldGenerationBaseTileLookupContextTest, RejectedCoverageAllowsTheLegacyFailurePathWithIdenticalPartialState) {
  LookupContext context(lookupScope()); ASSERT_TRUE(context.seal()); Pair pair; missing(pair); RecordingResolver a{pair.a, true, 6}, b{pair.b, true, 6};
  EXPECT_THROW(legacyBaseTile(pair.a, tileInput(), a), std::runtime_error);
  ASSERT_FALSE(context.apply(pair.b, tileInput(), lookupScope()));
  EXPECT_THROW(legacyBaseTile(pair.b, tileInput(), b), std::runtime_error);
  sameTrace(a, b); EXPECT_TRUE(compareWorldGenerationBaseTile(pair.a, pair.b).equal());
}

TEST(WorldGenerationBaseTileLookupContextTest, NoModForAirStillRequiresAndPreservesItsObservedHue) {
  auto input = tileInput(); input.foreground = input.background = {EmptyMaterialId, NoModId}; LookupContext context(lookupScope());
  ASSERT_TRUE(context.add(lookupScope(), lookupSample(input, 3, FrozenResolver{}))); EXPECT_EQ(context.size(), 3u); ASSERT_TRUE(context.seal());
  Pair pair; missing(pair); legacyBaseTile(pair.a, input, FrozenResolver{});
  EXPECT_TRUE(context.apply(pair.b, input, lookupScope())); EXPECT_EQ(pair.b.foregroundModHueShift, 27u);
  EXPECT_TRUE(compareWorldGenerationBaseTile(pair.a, pair.b).equal());
}

TEST(WorldGenerationBaseTileLookupContextTest, UnsupportedModsNeedNoHueEntryAndPreservePriorModState) {
  FrozenResolver resolver; resolver.support = false; LookupContext context(lookupScope());
  ASSERT_TRUE(context.add(lookupScope(), lookupSample(tileInput(), 3, resolver))); EXPECT_EQ(context.size(), 4u); ASSERT_TRUE(context.seal());
  Pair pair; missing(pair); legacyBaseTile(pair.a, tileInput(), resolver); EXPECT_TRUE(context.apply(pair.b, tileInput(), lookupScope()));
  EXPECT_TRUE(compareWorldGenerationBaseTile(pair.a, pair.b).equal()); EXPECT_EQ(pair.b.foregroundModHueShift, 5u);
}

TEST(WorldGenerationBaseTileLookupContextTest, PackedKeysDoNotAliasAtBiomeMaterialOrModBoundaries) {
  WorldGenerationBaseTileLookupContext<128> context(lookupScope()); auto input = tileInput();
  for (BiomeIndex biome : {BiomeIndex(0), BiomeIndex(1), BiomeIndex(255)})
    for (MaterialId material : {MaterialId(0), MaterialId(21), EmptyMaterialId})
      for (ModId mod : {ModId(31), BiomeModId, NoModId}) {
        input.blockBiome = biome; input.foreground = {material, mod};
        ServerTile tile; populate(tile); tile.foreground = NullMaterialId; RecordingResolver resolver{tile}; Observation sample;
        observeWorldGenerationBaseTile(tile, input, resolver, sample); ASSERT_TRUE(context.add(lookupScope(), sample));
      }
  ASSERT_TRUE(context.seal());
  for (BiomeIndex biome : {BiomeIndex(0), BiomeIndex(1), BiomeIndex(255)})
    for (MaterialId material : {MaterialId(0), MaterialId(21), EmptyMaterialId})
      for (ModId mod : {ModId(31), BiomeModId, NoModId}) {
        input.blockBiome = biome; input.foreground = {material, mod}; Pair pair; pair.a.foreground = pair.b.foreground = NullMaterialId;
        RecordingResolver resolver{pair.a}; legacyBaseTile(pair.a, input, resolver);
        ASSERT_TRUE(context.apply(pair.b, input, lookupScope())); ASSERT_TRUE(compareWorldGenerationBaseTile(pair.a, pair.b).equal());
      }
}

TEST(WorldGenerationBaseTileLookupContextTest, SampleOrderDoesNotChangeTheDeduplicatedCatalogResults) {
  auto input = tileInput(); input.blockBiome = 99; auto a = capturedTile(), b = lookupSample(input, 3, FrozenResolver{});
  LookupContext first(lookupScope()), second(lookupScope());
  ASSERT_TRUE(first.add(lookupScope(), a)); ASSERT_TRUE(first.add(lookupScope(), b));
  ASSERT_TRUE(second.add(lookupScope(), b)); ASSERT_TRUE(second.add(lookupScope(), a));
  ASSERT_TRUE(first.seal()); ASSERT_TRUE(second.seal()); EXPECT_EQ(first.size(), second.size());
  for (auto const& in : {tileInput(), input}) {
    Pair pair; missing(pair); ASSERT_TRUE(first.apply(pair.a, in, lookupScope())); ASSERT_TRUE(second.apply(pair.b, in, lookupScope()));
    EXPECT_TRUE(compareWorldGenerationBaseTile(pair.a, pair.b).equal());
  }
}

TEST(WorldGenerationBaseTileLookupContextTest, BranchMatrixMatchesIndependentLegacyAndTapeWithoutRetainingPerTileInputs) {
  for (unsigned flags = 0; flags < 4096; ++flags) {
    Pair pair; ServerTile replay, expected; populate(replay); populate(expected); auto input = tileInput();
    pair.a.foreground = pair.b.foreground = replay.foreground = expected.foreground = flags & 1 ? NullMaterialId : MaterialId(1);
    pair.a.background = pair.b.background = replay.background = expected.background = flags & 2 ? NullMaterialId : MaterialId(2);
    input.foreground = {flags & 4 ? EmptyMaterialId : MaterialId(21), flags & 16 ? NoModId : ModId(31)};
    input.background = {flags & 8 ? EmptyMaterialId : MaterialId(22), flags & 32 ? NoModId : ModId(32)};
    input.blockBiome = flags & 128 ? NullBiomeIndex : BiomeIndex(11); input.biomeTransition = flags & 256;
    input.oceanLiquid = flags & 512 ? EmptyLiquidId : LiquidId(3); input.encloseLiquids = flags & 1024; input.y = flags & 2048 ? 10 : 0;
    RecordingResolver resolver{pair.a, bool(flags & 64)}, reference{expected, bool(flags & 64)}; Observation observation;
    legacyBaseTile(expected, input, reference);
    observeWorldGenerationBaseTile(pair.a, input, resolver, observation); LookupContext context(lookupScope());
    ASSERT_TRUE(context.add(lookupScope(), observation)); ASSERT_TRUE(context.seal());
    ASSERT_TRUE(context.apply(pair.b, input, lookupScope())); ASSERT_TRUE(replayWorldGenerationBaseTile(replay, input, observation));
    ASSERT_TRUE(compareWorldGenerationBaseTile(pair.a, pair.b).equal()); ASSERT_TRUE(compareWorldGenerationBaseTile(pair.a, replay).equal());
    ASSERT_TRUE(compareWorldGenerationBaseTile(pair.a, expected).equal()); sameTrace(resolver, reference);
  }
}

TEST(WorldGenerationBaseTileLookupContextTest, OneSealedContextServesFourWorkersAndDifferentPrivateInitialTiles) {
  LookupContext context(lookupScope()); ASSERT_TRUE(context.add(lookupScope(), capturedTile())); ASSERT_TRUE(context.seal());
  auto expected = std::make_unique<ServerTileSectorArray::Array>(), actual = std::make_unique<ServerTileSectorArray::Array>(); fill(*expected); fill(*actual);
  for (unsigned i = 0; i < 1024; ++i) {
    auto& a = (*expected)(i / 32, i % 32); auto& b = (*actual)(i / 32, i % 32);
    a.foreground = b.foreground = i % 2 ? NullMaterialId : EmptyMaterialId; a.background = b.background = i % 3 ? NullMaterialId : MaterialId(2);
    auto input = tileInput(); input.y = int(i % 32); legacyBaseTile(a, input, FrozenResolver{});
  }
  WorkerPool pool("Shared compact BaseTiles lookup context", 4); std::vector<WorkerPoolPromise<bool>> promises;
  for (unsigned part = 0; part < 4; ++part) promises.push_back(pool.addProducer<bool>([&, part] {
    for (unsigned i = part * 256; i < (part + 1) * 256; ++i) {
      auto input = tileInput(); input.y = int(i % 32);
      if (!context.apply((*actual)(i / 32, i % 32), input, lookupScope())) return false;
    }
    return true;
  }));
  for (auto& promise : promises) { EXPECT_TRUE(promise.get()); }
  pool.finish(); EXPECT_TRUE(compareWorldGenerationBaseTiles(*expected, *actual, baseTicket()).equal()); EXPECT_EQ(context.size(), 6u);
}

TEST(WorldGenerationBaseTileLookupContextTest, CatalogCoverageIsNotAnOriginalSnapshotGuardOrARetryPermit) {
  LookupContext context(lookupScope()); ASSERT_TRUE(context.add(lookupScope(), capturedTile())); ASSERT_TRUE(context.seal());
  Pair pair; missing(pair); pair.a.foreground = pair.b.foreground = 21; pair.a.foregroundHueShift = pair.b.foregroundHueShift = 88;
  legacyBaseTile(pair.a, tileInput(), FrozenResolver{}); EXPECT_TRUE(context.apply(pair.b, tileInput(), lookupScope()));
  EXPECT_TRUE(compareWorldGenerationBaseTile(pair.a, pair.b).equal()); EXPECT_EQ(pair.b.foregroundHueShift, 88u);
}

TEST(WorldGenerationBaseTileLookupContextTest, LiquidKeepPreservesFloatBitsWithoutGuardingAnUnrelatedEarlierSample) {
  LookupContext context(lookupScope()); ASSERT_TRUE(context.add(lookupScope(), capturedTile())); ASSERT_TRUE(context.seal()); Pair pair;
  pair.a.foreground = pair.b.foreground = EmptyMaterialId; pair.a.liquid.level = pair.b.liquid.level = -0.0f;
  pair.a.liquid.pressure = pair.b.liquid.pressure = bits(0x7fc00123u); auto input = tileInput(); input.oceanLiquid = EmptyLiquidId;
  legacyBaseTile(pair.a, input, FrozenResolver{}); EXPECT_TRUE(context.apply(pair.b, input, lookupScope()));
  EXPECT_TRUE(compareWorldGenerationBaseTile(pair.a, pair.b).equal());
}

// Isolated, explicit micro-cost experiment; no timing threshold in CTest.
TEST(WorldGenerationBaseTileLookupContextTest, DISABLED_SyntheticLookupContextCosts) {
  constexpr unsigned count = 8192, rounds = 9; std::vector<ServerTile> tiles(count);
  auto input = tileInput(); auto observation = capturedTile(); LookupContext context(lookupScope());
  ASSERT_TRUE(context.add(lookupScope(), observation)); ASSERT_TRUE(context.seal()); FrozenResolver resolver;
  std::array<std::array<double, rounds>, 4> timings{}; size_t checksum = 0;
  for (unsigned round = 0; round < rounds; ++round) {
    for (unsigned position = 0; position < 3; ++position) {
      unsigned mode = (round + position) % 3;
      for (auto& tile : tiles) { populate(tile); tile.foreground = tile.background = NullMaterialId; }
      auto begin = std::chrono::steady_clock::now();
      for (auto& tile : tiles) {
        if (mode == 0) applyWorldGenerationBaseTile(tile, input, resolver);
        else if (mode == 1) checksum += replayWorldGenerationBaseTile(tile, input, observation);
        else checksum += context.apply(tile, input, lookupScope());
      }
      timings[mode][round] = std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - begin).count() / count;
      for (auto const& tile : tiles) checksum += tile.foregroundHueShift + tile.backgroundModHueShift;
    }
    auto begin = std::chrono::steady_clock::now();
    for (unsigned i = 0; i < count; ++i) {
      LookupContext built(lookupScope()); checksum += built.add(lookupScope(), observation) && built.seal(); checksum += built.activeEntryBytes();
    }
    timings[3][round] = std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - begin).count() / count;
  }
  for (unsigned mode = 0; mode < 4; ++mode) {
    std::sort(timings[mode].begin(), timings[mode].end());
    std::printf("BaseTileLookupContextCost mode=%u count=%u rounds=%u medianNs=%.2f minNs=%.2f maxNs=%.2f contextBytes=%zu entries=%zu activeEntryBytes=%zu reservedEntryBytes=%zu\n",
        mode, count, rounds, timings[mode][rounds / 2], timings[mode].front(), timings[mode].back(), sizeof(LookupContext), context.size(), context.activeEntryBytes(), context.reservedEntryBytes());
  }
  EXPECT_EQ(checksum, size_t(rounds) * count * (3 * (19 + 27) + 2 + 49));
}
