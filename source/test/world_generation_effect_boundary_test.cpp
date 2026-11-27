#include "StarWorldGenerationEffectBoundary.hpp"

#include <set>
#include <string>

#include "gtest/gtest.h"

using namespace Star;

TEST(WorldGenerationEffectBoundaryTest, CatalogSatisfiesH0Invariants) {
  EXPECT_TRUE(worldGenerationEffectBoundaryCatalogValid());
  EXPECT_GE(worldGenerationEffectBoundaryCount(), 30u);
}

TEST(WorldGenerationEffectBoundaryTest, IdsAreUniqueAndFindable) {
  std::set<std::string> seen;
  for (size_t i = 0; i != worldGenerationEffectBoundaryCount(); ++i) {
    auto const* entry = worldGenerationEffectBoundaryAt(i);
    ASSERT_TRUE(entry);
    ASSERT_TRUE(entry->id);
    EXPECT_TRUE(seen.insert(entry->id).second) << entry->id;
    auto found = worldGenerationEffectBoundaryFind(entry->id);
    ASSERT_TRUE(found);
    EXPECT_STREQ(entry->id, found->id);
    EXPECT_EQ(entry->kind, found->kind);
  }
  EXPECT_TRUE(worldGenerationEffectBoundaryFind("missing-operation").isNothing());
  EXPECT_TRUE(worldGenerationEffectBoundaryFind(nullptr).isNothing());
}

TEST(WorldGenerationEffectBoundaryTest, PureKernelsNeverTouchLiveWorldOrScripts) {
  size_t pure = 0;
  for (size_t i = 0; i != worldGenerationEffectBoundaryCount(); ++i) {
    auto const* entry = worldGenerationEffectBoundaryAt(i);
    ASSERT_TRUE(entry);
    if (entry->kind != WorldGenerationEffectKind::PureKernel)
      continue;
    ++pure;
    EXPECT_FALSE(entry->mayInvokeScript) << entry->id;
    EXPECT_FALSE(entry->mutatesLiveWorld) << entry->id;
    EXPECT_FALSE(entry->requiresOwnerThread) << entry->id;
    EXPECT_FALSE(entry->readsUninitializedDefault) << entry->id;
  }
  EXPECT_GE(pure, 4u);
}

TEST(WorldGenerationEffectBoundaryTest, AuditedUninitializedDefaultsRemainClassified) {
  auto biome = worldGenerationEffectBoundaryFind("world-tile-assign-biome-transition");
  ASSERT_TRUE(biome);
  EXPECT_TRUE(biome->readsUninitializedDefault);
  EXPECT_EQ(biome->kind, WorldGenerationEffectKind::PreparedDependency);

  auto sky = worldGenerationEffectBoundaryFind("sky-parameters-horizon-clouds");
  ASSERT_TRUE(sky);
  EXPECT_TRUE(sky->readsUninitializedDefault);

  auto baseHeight = worldGenerationEffectBoundaryFind("terrain-selector-base-height");
  ASSERT_TRUE(baseHeight);
  EXPECT_TRUE(baseHeight->readsUninitializedDefault);

  auto perlin = worldGenerationEffectBoundaryFind("perlin-uninitialized-copy");
  ASSERT_TRUE(perlin);
  EXPECT_TRUE(perlin->readsUninitializedDefault);
  EXPECT_EQ(perlin->kind, WorldGenerationEffectKind::PreparedDependency);
}

TEST(WorldGenerationEffectBoundaryTest, AuthoritativeCallbacksStayOnOwnerThread) {
  size_t callbacks = 0;
  for (size_t i = 0; i != worldGenerationEffectBoundaryCount(); ++i) {
    auto const* entry = worldGenerationEffectBoundaryAt(i);
    ASSERT_TRUE(entry);
    if (entry->kind != WorldGenerationEffectKind::AuthoritativeCallback)
      continue;
    ++callbacks;
    EXPECT_TRUE(entry->requiresOwnerThread) << entry->id;
    EXPECT_FALSE(entry->mutatesLiveWorld) << entry->id; // H0: no parallel live publish
  }
  EXPECT_GE(callbacks, 2u);
}

TEST(WorldGenerationEffectBoundaryTest, NoMultiRegionEffectInH0Catalog) {
  for (size_t i = 0; i != worldGenerationEffectBoundaryCount(); ++i) {
    auto const* entry = worldGenerationEffectBoundaryAt(i);
    ASSERT_TRUE(entry);
    EXPECT_NE(entry->kind, WorldGenerationEffectKind::MultiRegionEffect) << entry->id;
  }
}

TEST(WorldGenerationEffectBoundaryTest, ExpandedSurfacesHaveCoverage) {
  char const* surfaces[] = {"BaseTiles", "SectorShadow", "Memory", "Distribution", "Planning", "Json",
      "Payload", "Lookup", "Tasks", "SelectorContext", "WorldRegion", "Perlin", "VAL-02"};
  for (auto surface : surfaces) {
    EXPECT_GE(worldGenerationEffectBoundaryCountForSurface(surface), 1u) << surface;
  }
  EXPECT_EQ(0u, worldGenerationEffectBoundaryCountForSurface(nullptr));
  EXPECT_EQ(0u, worldGenerationEffectBoundaryCountForSurface("MissingSurface"));
}

TEST(WorldGenerationEffectBoundaryTest, MemoryAndLookupRefuseWithoutLiveMutation) {
  auto admit = worldGenerationEffectBoundaryFind("memory-ledger-admit");
  ASSERT_TRUE(admit);
  EXPECT_FALSE(admit->mutatesLiveWorld);
  EXPECT_TRUE(admit->requiresOwnerThread);

  auto refuse = worldGenerationEffectBoundaryFind("memory-ledger-refuse");
  ASSERT_TRUE(refuse);
  EXPECT_FALSE(refuse->mutatesLiveWorld);

  auto binding = worldGenerationEffectBoundaryFind("lookup-source-binding");
  ASSERT_TRUE(binding);
  EXPECT_EQ(binding->kind, WorldGenerationEffectKind::PreparedDependency);
  EXPECT_TRUE(binding->requiresOwnerThread);

  auto stale = worldGenerationEffectBoundaryFind("lookup-source-stale");
  ASSERT_TRUE(stale);
  EXPECT_FALSE(stale->mutatesLiveWorld);

  auto regionLiquids = worldGenerationEffectBoundaryFind("world-region-liquids-default");
  ASSERT_TRUE(regionLiquids);
  EXPECT_TRUE(regionLiquids->readsUninitializedDefault);
}

TEST(WorldGenerationEffectBoundaryTest, TaskLifecycleBoundariesStayPrivate) {
  for (auto id : {"task-preparation-abandon", "task-producer-failure", "task-invalidate"}) {
    auto entry = worldGenerationEffectBoundaryFind(id);
    ASSERT_TRUE(entry) << id;
    EXPECT_FALSE(entry->mutatesLiveWorld) << id;
    EXPECT_TRUE(entry->requiresOwnerThread) << id;
  }
  auto invalidate = worldGenerationEffectBoundaryFind("task-invalidate");
  ASSERT_TRUE(invalidate);
  EXPECT_EQ(invalidate->kind, WorldGenerationEffectKind::AuthoritativeCallback);
}
