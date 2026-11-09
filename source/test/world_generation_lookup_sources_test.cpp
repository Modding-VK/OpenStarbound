#include "StarWorldGenerationLookupSources.hpp"
#include "StarWorldGenerationBaseTiles.hpp"
#include "StarWorkerPool.hpp"

#include <algorithm>
#include <future>
#include <stdexcept>
#include <vector>
#include <cstdio>

#include "gtest/gtest.h"

using namespace Star;

TEST(WorldGenerationSourceVersionTest, FreshClocksHaveDistinctNonzeroProcessIdentities) {
  GenerationVersionClock a, b; EXPECT_NE(a.current(), 0u); EXPECT_NE(b.current(), 0u); EXPECT_NE(a.current(), b.current());
}

TEST(WorldGenerationSourceVersionTest, ChangeIsUnavailableUntilItsSuccessfulScopeEnds) {
  GenerationVersionClock clock; auto previous = clock.current();
  { auto change = clock.change(); EXPECT_EQ(clock.current(), 0u); change.complete(); EXPECT_EQ(clock.current(), 0u); }
  EXPECT_NE(clock.current(), 0u); EXPECT_NE(clock.current(), previous);
}

TEST(WorldGenerationSourceVersionTest, UncompletedOrThrowingChangesStayUnavailable) {
  GenerationVersionClock clock; { auto change = clock.change(); } EXPECT_EQ(clock.current(), 0u);
  EXPECT_THROW({ auto change = clock.change(); throw std::runtime_error("partial source mutation"); }, std::runtime_error);
  EXPECT_EQ(clock.current(), 0u);
}

TEST(WorldGenerationSourceVersionTest, ANewSuccessfulTransactionCanRecoverAfterFailure) {
  GenerationVersionClock clock; auto previous = clock.current(); { auto change = clock.change(); }
  { auto change = clock.change(); change.complete(); } EXPECT_NE(clock.current(), 0u); EXPECT_NE(clock.current(), previous);
}

TEST(WorldGenerationSourceVersionTest, NestedSuccessDoesNotPublishBeforeTheOuterChangeFinishes) {
  GenerationVersionClock clock;
  { auto outer = clock.change(); { auto inner = clock.change(); inner.complete(); } EXPECT_EQ(clock.current(), 0u); outer.complete(); }
  EXPECT_NE(clock.current(), 0u);
}

TEST(WorldGenerationSourceVersionTest, NestedFailureCannotBeHiddenByOuterSuccess) {
  GenerationVersionClock clock;
  { auto outer = clock.change(); { auto inner = clock.change(); } outer.complete(); } EXPECT_EQ(clock.current(), 0u);
}

TEST(WorldGenerationSourceVersionTest, RetiredClockCannotBeReactivatedByNewOrOutstandingChanges) {
  GenerationVersionClock clock; { auto change = clock.change(); clock.retire(); change.complete(); }
  EXPECT_EQ(clock.current(), 0u); { auto change = clock.change(); change.complete(); } EXPECT_EQ(clock.current(), 0u);
}

TEST(WorldGenerationSourceVersionTest, IdentityAllocationSaturatesWithoutWrappingOrReusingZero) {
  std::atomic<uint64_t> counter{std::numeric_limits<uint64_t>::max() - 1};
  EXPECT_EQ(GenerationVersionDetail::allocate(counter), std::numeric_limits<uint64_t>::max());
  EXPECT_EQ(GenerationVersionDetail::allocate(counter), 0u); EXPECT_EQ(GenerationVersionDetail::allocate(counter), 0u);
  EXPECT_EQ(counter.load(), std::numeric_limits<uint64_t>::max());
}

TEST(WorldGenerationSourceVersionTest, ConcurrentIdentityAllocationRemainsUnique) {
  WorkerPool pool("Source generation identities", 4); std::vector<WorkerPoolPromise<std::vector<uint64_t>>> promises;
  for (unsigned i = 0; i < 4; ++i) promises.push_back(pool.addProducer<std::vector<uint64_t>>([] {
    std::vector<uint64_t> values; for (unsigned n = 0; n < 128; ++n) values.push_back(nextGenerationVersion()); return values;
  }));
  std::vector<uint64_t> all;
  for (auto& promise : promises) { auto const& values = promise.get(); all.insert(all.end(), values.begin(), values.end()); }
  pool.finish(); std::sort(all.begin(), all.end()); EXPECT_NE(all.front(), 0u);
  EXPECT_EQ(std::adjacent_find(all.begin(), all.end()), all.end()); EXPECT_EQ(all.size(), 512u);
}

TEST(WorldGenerationSourceVersionTest, OverlappingWritersCannotPublishAnIntermediateGeneration) {
  GenerationVersionClock clock; WorkerPool pool("Overlapping source changes", 1);
  std::promise<void> started, release; auto ready = started.get_future(); auto resume = release.get_future();
  {
    auto outer = clock.change();
    auto job = pool.addProducer<bool>([&] { auto inner = clock.change(); started.set_value(); resume.wait(); inner.complete(); return true; });
    ready.wait(); EXPECT_EQ(clock.current(), 0u); release.set_value(); EXPECT_TRUE(job.get());
    EXPECT_EQ(clock.current(), 0u); outer.complete();
  }
  pool.finish(); EXPECT_NE(clock.current(), 0u);
}

TEST(WorldGenerationSourceVersionTest, CompletedOneOfTwoWritersCannotClearAnotherFailure) {
  GenerationVersionClock clock;
  { auto a = clock.change(); { auto b = clock.change(); b.complete(); } }
  EXPECT_EQ(clock.current(), 0u);
}

namespace {
// Controlled sources exercise the real clock/binding algorithm, not a claim
// of executing Assets, MaterialDatabase, Root or their constructors in NoAssets.
struct AssetsSource {
  mutable GenerationVersionClock clock;
  mutable std::atomic<unsigned> reads{0};
  std::atomic<unsigned> invalidateAt{0};
  std::shared_ptr<GenerationVersionClock> rootToInvalidate;
  uint64_t generationRevision() const {
    if (reads.fetch_add(1) + 1 == invalidateAt.load()) {
      auto change = rootToInvalidate ? rootToInvalidate->change() : clock.change(); change.complete();
    }
    return clock.current();
  }
};
struct MaterialsSource {
  uint64_t version = nextGenerationVersion();
  uint64_t generationRevision() const { return version; }
};
using Sources = WorldGenerationLookupSources<AssetsSource, MaterialsSource>;
using Status = WorldGenerationLookupApplyStatus;
using Context = WorldGenerationBaseTileLookupContext<32>;
struct Fixture {
  std::shared_ptr<GenerationVersionClock> root = std::make_shared<GenerationVersionClock>();
  std::shared_ptr<AssetsSource> assets = std::make_shared<AssetsSource>();
  std::shared_ptr<MaterialsSource> materials = std::make_shared<MaterialsSource>();
  Sources capture() const { return Sources::capture(root, root->current(), assets, materials); }
};
WorldGenerationBaseTileInput input() { return {11, 77, true, {21, 31}, {22, 32}, 3, 10, true, 0}; }
void initialize(ServerTile& tile) {
  tile.biomeTransition = false; tile.blockBiomeIndex = 3; tile.environmentBiomeIndex = 4;
  tile.liquid = LiquidStore::filled(3, 0.25f, 4.5f);
}
WorldGenerationBaseTileObservation sample() {
  struct Resolver {
    MaterialHue materialHue(BiomeIndex, MaterialId) const { return 19; }
    bool supportsMod(MaterialId, ModId) const { return true; }
    MaterialHue modHue(BiomeIndex, ModId) const { return 27; }
  } resolver;
  ServerTile tile; initialize(tile); WorldGenerationBaseTileObservation result;
  observeWorldGenerationBaseTile(tile, input(), resolver, result); return result;
}
void expectRefused(Sources const& sources, Context const& context, uint64_t world = 1, uint64_t revision = 7) {
  ServerTile a, b; initialize(a); initialize(b);
  EXPECT_EQ(sources.apply(context, b, input(), world, revision), Status::Refused);
  EXPECT_TRUE(compareWorldGenerationBaseTile(a, b).equal());
}
}

TEST(WorldGenerationLookupSourcesTest, EmptyBindingHasNoValidScopeOrSources) {
  Sources sources; EXPECT_FALSE(sources.valid()); EXPECT_FALSE(sources.scope(1, 7)); EXPECT_FALSE(sources.assets()); EXPECT_FALSE(sources.materials());
}

TEST(WorldGenerationLookupSourcesTest, RealClockValuesPopulateAllSourceGenerationsAndRootEpoch) {
  Fixture fixture; auto sources = fixture.capture(); ASSERT_TRUE(sources.valid()); auto scope = sources.scope(1, 7); ASSERT_TRUE(scope);
  EXPECT_EQ(scope->sourceEpoch, fixture.root->current()); EXPECT_EQ(scope->assetsGeneration, fixture.assets->clock.current());
  EXPECT_EQ(scope->materialsGeneration, fixture.materials->version); EXPECT_EQ(scope->modelVersion, 2u); EXPECT_TRUE(scope->valid());
  EXPECT_EQ(sources.assets(), fixture.assets); EXPECT_EQ(sources.materials(), fixture.materials);
}

TEST(WorldGenerationLookupSourcesTest, NullSourcesAndZeroVersionsFailClosed) {
  Fixture fixture;
  EXPECT_FALSE(Sources::capture({}, 1, fixture.assets, fixture.materials).valid());
  EXPECT_FALSE(Sources::capture(fixture.root, 0, fixture.assets, fixture.materials).valid());
  EXPECT_FALSE(Sources::capture(fixture.root, fixture.root->current(), {}, fixture.materials).valid());
  EXPECT_FALSE(Sources::capture(fixture.root, fixture.root->current(), fixture.assets, {}).valid());
  fixture.materials->version = 0; EXPECT_FALSE(fixture.capture().valid());
}

TEST(WorldGenerationLookupSourcesTest, ZeroWorldEpochIsRefusedButZeroTemplateRevisionIsExplicit) {
  Fixture fixture; auto sources = fixture.capture(); EXPECT_FALSE(sources.scope(0, 7)); EXPECT_TRUE(sources.scope(1, 0));
}

TEST(WorldGenerationLookupSourcesTest, CaptureWhileRootOrAssetMutationIsActiveIsUnavailable) {
  Fixture fixture;
  { auto change = fixture.root->change(); EXPECT_FALSE(fixture.capture().valid()); change.complete(); }
  { auto change = fixture.assets->clock.change(); EXPECT_FALSE(fixture.capture().valid()); change.complete(); }
  EXPECT_TRUE(fixture.capture().valid());
}

TEST(WorldGenerationLookupSourcesTest, CaptureRejectsAssetOrRootChangesBetweenItsReads) {
  Fixture fixture; fixture.assets->invalidateAt.store(2); EXPECT_FALSE(fixture.capture().valid());
  Fixture other; other.assets->rootToInvalidate = other.root; other.assets->invalidateAt.store(1);
  EXPECT_FALSE(other.capture().valid());
}

TEST(WorldGenerationLookupSourcesTest, StrongHandlesKeepSelectedSourcesAliveWithoutRootOwnership) {
  Fixture fixture; auto sources = fixture.capture(); std::weak_ptr<AssetsSource> assets = fixture.assets;
  std::weak_ptr<MaterialsSource> materials = fixture.materials; fixture.assets.reset(); fixture.materials.reset();
  EXPECT_FALSE(assets.expired()); EXPECT_FALSE(materials.expired()); EXPECT_TRUE(sources.valid());
  fixture.root->retire(); fixture.root.reset(); EXPECT_FALSE(sources.valid());
  sources = {}; EXPECT_TRUE(assets.expired()); EXPECT_TRUE(materials.expired());
}

TEST(WorldGenerationLookupSourcesTest, RootChangeInvalidatesOldBindingsAndFreshScopesDifferEvenForSameHandles) {
  Fixture fixture; auto old = fixture.capture(); auto before = old.scope(1, 7); ASSERT_TRUE(before);
  { auto change = fixture.root->change(); EXPECT_FALSE(old.valid()); change.complete(); }
  EXPECT_FALSE(old.valid()); auto fresh = fixture.capture(); auto after = fresh.scope(1, 7); ASSERT_TRUE(after);
  EXPECT_NE(before->sourceEpoch, after->sourceEpoch); EXPECT_EQ(before->assetsGeneration, after->assetsGeneration);
  Context context(*before); ASSERT_TRUE(context.add(*before, sample())); ASSERT_TRUE(context.seal()); expectRefused(fresh, context);
}

TEST(WorldGenerationLookupSourcesTest, DirectAssetsHotReloadInvalidatesBindingsWithoutARootChange) {
  Fixture fixture; auto sources = fixture.capture(); auto scope = sources.scope(1, 7); ASSERT_TRUE(scope); auto root = fixture.root->current();
  { auto change = fixture.assets->clock.change(); change.complete(); }
  EXPECT_EQ(fixture.root->current(), root); EXPECT_FALSE(sources.valid());
  Context context(*scope); ASSERT_TRUE(context.add(*scope, sample())); ASSERT_TRUE(context.seal()); expectRefused(fixture.capture(), context);
}

TEST(WorldGenerationLookupSourcesTest, FreshMaterialDefinitionsCannotUseAnOlderCatalog) {
  Fixture fixture; auto old = fixture.capture(); auto scope = old.scope(1, 7); ASSERT_TRUE(scope);
  Context context(*scope); ASSERT_TRUE(context.add(*scope, sample())); ASSERT_TRUE(context.seal());
  fixture.materials = std::make_shared<MaterialsSource>(); auto fresh = fixture.capture(); ASSERT_TRUE(fresh.valid());
  EXPECT_NE(fresh.scope(1, 7)->materialsGeneration, scope->materialsGeneration); expectRefused(fresh, context);
}

TEST(WorldGenerationLookupSourcesTest, ReplacementRootCannotReuseCatalogEvenWhenSourceHandlesSurvive) {
  Fixture fixture; auto old = fixture.capture(); auto scope = old.scope(1, 7); ASSERT_TRUE(scope);
  Context context(*scope); ASSERT_TRUE(context.add(*scope, sample())); ASSERT_TRUE(context.seal());
  fixture.root->retire(); fixture.root = std::make_shared<GenerationVersionClock>();
  EXPECT_FALSE(old.valid()); expectRefused(fixture.capture(), context);
}

TEST(WorldGenerationLookupSourcesTest, FailedRootChangeKeepsBindingsUnavailableUntilSuccessfulRecovery) {
  Fixture fixture; auto old = fixture.capture(); { auto change = fixture.root->change(); }
  EXPECT_FALSE(old.valid()); EXPECT_FALSE(fixture.capture().valid());
  { auto change = fixture.root->change(); change.complete(); } EXPECT_TRUE(fixture.capture().valid()); EXPECT_FALSE(old.valid());
}

TEST(WorldGenerationLookupSourcesTest, CoveredPrivateApplicationReportsAppliedButDoesNotAuthorizeCommit) {
  Fixture fixture; auto sources = fixture.capture(); auto scope = sources.scope(1, 7); ASSERT_TRUE(scope);
  Context context(*scope); ASSERT_TRUE(context.add(*scope, sample())); ASSERT_TRUE(context.seal()); ServerTile tile; initialize(tile);
  EXPECT_EQ(sources.apply(context, tile, input(), 1, 7), Status::Applied);
  EXPECT_EQ(tile.foregroundHueShift, 19u); EXPECT_EQ(tile.backgroundModHueShift, 27u); EXPECT_TRUE(tile.biomeTransition);
  { auto change = fixture.root->change(); change.complete(); } EXPECT_FALSE(sources.valid());
}

TEST(WorldGenerationLookupSourcesTest, StaleBindingWrongTaskRevisionAndMissingCoverageRefuseWithoutWrites) {
  Fixture fixture; auto sources = fixture.capture(); auto scope = sources.scope(1, 7); ASSERT_TRUE(scope);
  Context context(*scope); ASSERT_TRUE(context.add(*scope, sample())); ASSERT_TRUE(context.seal());
  expectRefused(sources, context, 2, 7); expectRefused(sources, context, 1, 8); expectRefused(sources, context, 0, 7);
  Context empty(*scope); ASSERT_TRUE(empty.seal()); expectRefused(sources, empty);
  { auto change = fixture.root->change(); change.complete(); } expectRefused(sources, context);
}

TEST(WorldGenerationLookupSourcesTest, InvalidationAfterWritingHasDistinctStaleStatusAndMustNotFallback) {
  Fixture fixture; auto sources = fixture.capture(); auto scope = sources.scope(1, 7); ASSERT_TRUE(scope);
  Context context(*scope); ASSERT_TRUE(context.add(*scope, sample())); ASSERT_TRUE(context.seal()); ServerTile tile; initialize(tile);
  fixture.assets->invalidateAt.store(fixture.assets->reads.load() + 2);
  EXPECT_EQ(sources.apply(context, tile, input(), 1, 7), Status::Stale);
  EXPECT_EQ(tile.foreground, 21u); EXPECT_EQ(tile.foregroundHueShift, 19u); EXPECT_FALSE(sources.valid());
}

TEST(WorldGenerationLookupSourcesTest, RetiredRootRefusesWithoutDereferencingDestroyedRootObject) {
  Fixture fixture; auto sources = fixture.capture(); auto scope = sources.scope(1, 7); ASSERT_TRUE(scope);
  Context context(*scope); ASSERT_TRUE(context.add(*scope, sample())); ASSERT_TRUE(context.seal()); fixture.root->retire(); fixture.root.reset();
  EXPECT_FALSE(sources.scope(1, 7)); expectRefused(sources, context);
}

TEST(WorldGenerationLookupSourcesTest, LayoutMeasuresMetadataWithoutCountingItAsPayloadOrPerformanceGain) {
  EXPECT_EQ(Context::reservedEntryBytes(), 256u); EXPECT_GE(sizeof(Context), Context::reservedEntryBytes());
  std::printf("GenerationLookupSourcesLayout clockBytes=%zu bindingBytes=%zu context32Bytes=%zu scopeBytes=%zu\n",
      sizeof(GenerationVersionClock), sizeof(Sources), sizeof(Context), sizeof(WorldGenerationBaseTileLookupScope));
}
