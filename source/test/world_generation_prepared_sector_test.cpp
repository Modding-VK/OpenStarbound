#include "StarWorldGenerationPreparedSector.hpp"

#include "gtest/gtest.h"

using namespace Star;

namespace {
WorldGenerationPreparedSectorIdentity makeIdentity(Vec2U sector = {0, 0}, uint64_t templateRevision = 1,
    uint64_t tileRevision = 2) {
  WorldGenerationPreparedSectorIdentity out;
  out.product = WorldGenerationPreparedProduct::PrivateBaseTiles;
  out.task = {1, 1, sector, {0, 0}, WorldGenerationTaskStage::BaseTiles};
  out.templateRevision = templateRevision;
  out.tileRevision = tileRevision;
  out.worldSize = {64, 64};
  out.tileRegion = RectI(0, 0, 32, 32);
  return out;
}
}

TEST(WorldGenerationPreparedSectorTest, PrepareRetainsResidentWithoutPublishing) {
  WorldGenerationProductRevisionCache cache(4);
  auto prepared = WorldGenerationPreparedSector::tryPrepare(cache, makeIdentity(), 128);
  ASSERT_TRUE(prepared.has_value());
  EXPECT_TRUE(prepared->active());
  EXPECT_EQ(128u, prepared->retainedBytes());
  EXPECT_TRUE(cache.contains(makeIdentity().residentKey()));
  EXPECT_TRUE(prepared->tryConsume(makeIdentity()));
}

TEST(WorldGenerationPreparedSectorTest, StaleExpectationRefusesConsume) {
  WorldGenerationProductRevisionCache cache(4);
  auto prepared = WorldGenerationPreparedSector::tryPrepare(cache, makeIdentity(), 64);
  ASSERT_TRUE(prepared.has_value());
  auto live = makeIdentity();
  live.tileRevision = 99;
  EXPECT_FALSE(prepared->tryConsume(live));
  EXPECT_TRUE(prepared->active());
}

TEST(WorldGenerationPreparedSectorTest, DiscardReleasesResidentEntry) {
  WorldGenerationProductRevisionCache cache(4);
  auto key = makeIdentity().residentKey();
  {
    auto prepared = WorldGenerationPreparedSector::tryPrepare(cache, makeIdentity(), 32);
    ASSERT_TRUE(prepared.has_value());
    EXPECT_TRUE(cache.contains(key));
  }
  EXPECT_FALSE(cache.contains(key));
}

TEST(WorldGenerationPreparedSectorTest, FromBaseTilesPreservesSectorAndRevisions) {
  WorldGenerationBaseTilesIdentity base;
  base.task = {4, 8, {1, 0}, {0, 0}, WorldGenerationTaskStage::BaseTiles};
  base.tileRevision = 7;
  base.templateRevision = 3;
  base.worldSize = {128, 128};
  base.tileRegion = RectI(32, 0, 64, 32);
  auto preparedId = WorldGenerationPreparedSectorIdentity::fromBaseTiles(base);
  EXPECT_TRUE(preparedId.valid());
  EXPECT_EQ(WorldGenerationPreparedProduct::PrivateBaseTiles, preparedId.product);
  EXPECT_EQ(Vec2U(1, 0), preparedId.task.sector);
  EXPECT_EQ(7u, preparedId.tileRevision);
  EXPECT_EQ(3u, preparedId.templateRevision);
}
