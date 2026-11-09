#include "StarWorldGenerationBaseTiles.hpp"
#include "StarWorldGenerationMemory.hpp"
#include "StarWorkerPool.hpp"

#include <array>
#include <cstring>
#include <functional>
#include <limits>

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
