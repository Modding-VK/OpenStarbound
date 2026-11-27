#include "StarWorldGenerationEffectBoundary.hpp"

#include <set>
#include <string>

#include "gtest/gtest.h"

using namespace Star;

TEST(WorldGenerationEffectBoundaryTest, CatalogSatisfiesH0Invariants) {
  EXPECT_TRUE(worldGenerationEffectBoundaryCatalogValid());
  EXPECT_GE(worldGenerationEffectBoundaryCount(), 10u);
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
