#include "StarWorldGenerationProductRevisionCache.hpp"

#include "gtest/gtest.h"

using namespace Star;

namespace {
WorldGenerationProductRevisionKey key(WorldGenerationResidentProductKind kind, Vec2U sector,
    uint64_t templateRevision, uint64_t tileRevision = 1, uint64_t assetGeneration = 1) {
  return {kind, sector, templateRevision, tileRevision, assetGeneration};
}
}

TEST(WorldGenerationProductRevisionCacheTest, RejectsZeroBytePlaceholders) {
  WorldGenerationProductRevisionCache cache(4);
  EXPECT_FALSE(cache.insert(key(WorldGenerationResidentProductKind::PrivateBaseTiles, {0, 0}, 1), 0));
  EXPECT_EQ(0u, cache.size());
  EXPECT_EQ(0u, cache.retainedBytes());
}

TEST(WorldGenerationProductRevisionCacheTest, HitRequiresExactProductAndRevision) {
  WorldGenerationProductRevisionCache cache(4);
  auto resident = key(WorldGenerationResidentProductKind::PrivateBaseTiles, {1, 0}, 7, 3, 2);
  ASSERT_TRUE(cache.insert(resident, 128));
  auto hit = cache.find(resident);
  ASSERT_TRUE(hit.has_value());
  EXPECT_EQ(128u, hit->retainedBytes);
  EXPECT_EQ(1u, cache.hits());

  EXPECT_FALSE(cache.find(key(WorldGenerationResidentProductKind::PrivateBaseTiles, {1, 0}, 8, 3, 2)).has_value());
  EXPECT_FALSE(cache.find(key(WorldGenerationResidentProductKind::PureField, {1, 0}, 7, 3, 2)).has_value());
  EXPECT_FALSE(cache.find(key(WorldGenerationResidentProductKind::PrivateBaseTiles, {0, 0}, 7, 3, 2)).has_value());
  EXPECT_EQ(3u, cache.misses());
  EXPECT_TRUE(cache.contains(resident));
}

TEST(WorldGenerationProductRevisionCacheTest, TemplateRevisionInvalidationDropsStaleOnly) {
  WorldGenerationProductRevisionCache cache(8);
  ASSERT_TRUE(cache.insert(key(WorldGenerationResidentProductKind::PrivateBaseTiles, {0, 0}, 1), 32));
  ASSERT_TRUE(cache.insert(key(WorldGenerationResidentProductKind::PrivateBaseTiles, {1, 0}, 2), 64));
  ASSERT_TRUE(cache.insert(key(WorldGenerationResidentProductKind::CandidatePlan, {0, 0}, 2), 16));
  EXPECT_EQ(112u, cache.retainedBytes());

  EXPECT_EQ(1u, cache.invalidate(WorldGenerationProductCacheInvalidation::TemplateRevision, 2));
  EXPECT_FALSE(cache.contains(key(WorldGenerationResidentProductKind::PrivateBaseTiles, {0, 0}, 1)));
  EXPECT_TRUE(cache.contains(key(WorldGenerationResidentProductKind::PrivateBaseTiles, {1, 0}, 2)));
  EXPECT_TRUE(cache.contains(key(WorldGenerationResidentProductKind::CandidatePlan, {0, 0}, 2)));
  EXPECT_EQ(80u, cache.retainedBytes());
}

TEST(WorldGenerationProductRevisionCacheTest, AssetGenerationInvalidationDoesNotTouchMatchingTemplate) {
  WorldGenerationProductRevisionCache cache(4);
  ASSERT_TRUE(cache.insert(key(WorldGenerationResidentProductKind::StaticStructure, {0, 0}, 9, 1, 4), 40));
  ASSERT_TRUE(cache.insert(key(WorldGenerationResidentProductKind::StaticStructure, {0, 1}, 9, 1, 5), 40));
  EXPECT_EQ(1u, cache.invalidate(WorldGenerationProductCacheInvalidation::AssetGeneration, 5));
  EXPECT_FALSE(cache.contains(key(WorldGenerationResidentProductKind::StaticStructure, {0, 0}, 9, 1, 4)));
  EXPECT_TRUE(cache.contains(key(WorldGenerationResidentProductKind::StaticStructure, {0, 1}, 9, 1, 5)));
}

TEST(WorldGenerationProductRevisionCacheTest, CapacityEvictsOldestInsertionNotNewest) {
  WorldGenerationProductRevisionCache cache(2);
  ASSERT_TRUE(cache.insert(key(WorldGenerationResidentProductKind::PureField, {0, 0}, 1), 10));
  ASSERT_TRUE(cache.insert(key(WorldGenerationResidentProductKind::PureField, {1, 0}, 1), 20));
  ASSERT_TRUE(cache.insert(key(WorldGenerationResidentProductKind::PureField, {2, 0}, 1), 30));
  EXPECT_EQ(1u, cache.evictions());
  EXPECT_EQ(2u, cache.size());
  EXPECT_FALSE(cache.contains(key(WorldGenerationResidentProductKind::PureField, {0, 0}, 1)));
  EXPECT_TRUE(cache.contains(key(WorldGenerationResidentProductKind::PureField, {1, 0}, 1)));
  EXPECT_TRUE(cache.contains(key(WorldGenerationResidentProductKind::PureField, {2, 0}, 1)));
  EXPECT_EQ(50u, cache.retainedBytes());
}

TEST(WorldGenerationProductRevisionCacheTest, ReplaceSameKeyUpdatesBytesWithoutEviction) {
  WorldGenerationProductRevisionCache cache(1);
  auto resident = key(WorldGenerationResidentProductKind::DecodeStaging, {3, 3}, 4);
  ASSERT_TRUE(cache.insert(resident, 100));
  ASSERT_TRUE(cache.insert(resident, 250));
  EXPECT_EQ(0u, cache.evictions());
  EXPECT_EQ(1u, cache.size());
  EXPECT_EQ(250u, cache.retainedBytes());
  auto hit = cache.find(resident);
  ASSERT_TRUE(hit.has_value());
  EXPECT_EQ(250u, hit->retainedBytes);
}

TEST(WorldGenerationProductRevisionCacheTest, ClearAndAllInvalidationEmptyTheResidentSet) {
  WorldGenerationProductRevisionCache cache(4);
  ASSERT_TRUE(cache.insert(key(WorldGenerationResidentProductKind::LiveSnapshot, {0, 0}, 1), 8));
  ASSERT_TRUE(cache.insert(key(WorldGenerationResidentProductKind::LiveSnapshot, {0, 1}, 1), 8));
  EXPECT_EQ(2u, cache.invalidate(WorldGenerationProductCacheInvalidation::All, 0));
  EXPECT_EQ(0u, cache.size());
  EXPECT_EQ(0u, cache.retainedBytes());
  ASSERT_TRUE(cache.insert(key(WorldGenerationResidentProductKind::LiveSnapshot, {0, 0}, 2), 8));
  cache.clear();
  EXPECT_EQ(0u, cache.size());
  EXPECT_GE(cache.invalidations(), 1u);
}

TEST(WorldGenerationProductRevisionCacheTest, RetentionPrefersEvictingSpeculativeOverVisible) {
  WorldGenerationProductRetentionLimits limits{true, 40, 50, 80};
  WorldGenerationProductRevisionCache cache(8, limits);
  auto speculative = key(WorldGenerationResidentProductKind::PureField, {0, 0}, 1);
  auto visible = key(WorldGenerationResidentProductKind::PrivateBaseTiles, {1, 0}, 1);
  ASSERT_TRUE(cache.insert(speculative, 40, WorldGenerationMemoryDemand::Speculative, 1));
  ASSERT_TRUE(cache.insert(visible, 20, WorldGenerationMemoryDemand::Visible, 10));
  ASSERT_TRUE(cache.insert(key(WorldGenerationResidentProductKind::CandidatePlan, {2, 0}, 1), 20,
      WorldGenerationMemoryDemand::Speculative, 0));
  EXPECT_FALSE(cache.contains(speculative));
  EXPECT_TRUE(cache.contains(visible));
  EXPECT_GT(cache.retentionDrains() + cache.evictions(), 0u);
}

TEST(WorldGenerationProductRevisionCacheTest, SoftHysteresisRefusesSpeculationUntilResume) {
  WorldGenerationProductRetentionLimits limits{true, 30, 40, 100};
  WorldGenerationProductRevisionCache cache(8, limits);
  ASSERT_TRUE(cache.insert(key(WorldGenerationResidentProductKind::PrivateBaseTiles, {0, 0}, 1), 45,
      WorldGenerationMemoryDemand::Visible, 5));
  EXPECT_TRUE(cache.softPressureLatched());
  EXPECT_FALSE(cache.insert(key(WorldGenerationResidentProductKind::PureField, {1, 0}, 1), 10,
      WorldGenerationMemoryDemand::Speculative, 0));
  EXPECT_GT(cache.refusedInserts(), 0u);
  ASSERT_TRUE(cache.erase(key(WorldGenerationResidentProductKind::PrivateBaseTiles, {0, 0}, 1)));
  EXPECT_FALSE(cache.softPressureLatched());
  EXPECT_TRUE(cache.insert(key(WorldGenerationResidentProductKind::PureField, {1, 0}, 1), 10,
      WorldGenerationMemoryDemand::Speculative, 0));
}

TEST(WorldGenerationProductRevisionCacheTest, ActiveConsumersSurviveEvictionAndInvalidation) {
  WorldGenerationProductRevisionCache cache(1);
  auto pinned = key(WorldGenerationResidentProductKind::PrivateBaseTiles, {0, 0}, 1);
  auto other = key(WorldGenerationResidentProductKind::PrivateBaseTiles, {1, 0}, 1);
  ASSERT_TRUE(cache.insert(pinned, 16, WorldGenerationMemoryDemand::Dependency, 1));
  ASSERT_TRUE(cache.acquireConsumer(pinned));
  EXPECT_FALSE(cache.insert(other, 16));
  EXPECT_TRUE(cache.contains(pinned));
  EXPECT_FALSE(cache.erase(pinned));
  EXPECT_EQ(0u, cache.invalidate(WorldGenerationProductCacheInvalidation::All, 0));
  EXPECT_TRUE(cache.contains(pinned));
  ASSERT_TRUE(cache.releaseConsumer(pinned));
  EXPECT_TRUE(cache.erase(pinned));
}
