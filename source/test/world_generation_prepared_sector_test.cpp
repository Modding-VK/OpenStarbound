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

TEST(WorldGenerationPreparedSectorTest, PayloadViewAndWorldStoragePathNeverWrite) {
  WorldGenerationProductRevisionCache cache(4);
  auto identity = makeIdentity({2, 3}, 11, 12);
  identity.product = WorldGenerationPreparedProduct::EmptyProof;
  auto prepared = WorldGenerationPreparedSector::tryPrepare(cache, identity, 0);
  ASSERT_TRUE(prepared.has_value());

  auto view = prepared->payloadView();
  EXPECT_EQ(prepared->productId(), view.productId);
  EXPECT_EQ(WorldGenerationPreparedProductKind::Empty, view.kind);
  EXPECT_EQ(0u, view.payloadBytes);
  EXPECT_NE(0u, view.contentToken);
  EXPECT_FALSE(view.installAuthorized);

  auto path = prepared->worldStoragePath(9);
  EXPECT_EQ(9u, path.worldEpoch);
  EXPECT_EQ(Vec2I(2, 3), path.sector);
  EXPECT_EQ(prepared->productId(), path.productId);
  EXPECT_EQ(WorldGenerationPreparedProductKind::Empty, path.kind);

  auto handoff = prepared->probeHandoff(9, identity);
  EXPECT_EQ(WorldGenerationPreparedHandoffOutcome::Ready, handoff.outcome);
  EXPECT_FALSE(handoff.writesWorldStorage);
  EXPECT_EQ(path, handoff.path);
}

TEST(WorldGenerationPreparedSectorTest, HandoffReportsStaleAndBaseTilesNotAuthorized) {
  WorldGenerationProductRevisionCache cache(4);
  auto identity = makeIdentity({0, 0}, 7, 2);
  auto prepared = WorldGenerationPreparedSector::tryPrepare(cache, identity, 1024);
  ASSERT_TRUE(prepared.has_value());

  auto stale = identity;
  stale.tileRevision = 99;
  auto staleHandoff = prepared->probeHandoff(1, stale);
  EXPECT_EQ(WorldGenerationPreparedHandoffOutcome::StaleIdentity, staleHandoff.outcome);
  EXPECT_FALSE(staleHandoff.writesWorldStorage);

  auto denied = prepared->probeHandoff(1, identity);
  EXPECT_EQ(WorldGenerationPreparedHandoffOutcome::NotAuthorized, denied.outcome);
  EXPECT_FALSE(denied.writesWorldStorage);
  EXPECT_EQ(1024u, prepared->payloadView().payloadBytes);
}

TEST(WorldGenerationPreparedSectorTest, TerrainFieldsAndStepDeltaExposeOpaquePayload) {
  WorldGenerationProductRevisionCache cache(4);
  auto terrainId = makeIdentity({4, 5}, 1, 2);
  terrainId.product = WorldGenerationPreparedProduct::TerrainField;
  terrainId.opaqueContentToken = 0xabc;
  auto terrain = WorldGenerationPreparedSector::tryPrepare(cache, terrainId, 64);
  ASSERT_TRUE(terrain.has_value());
  auto terrainView = terrain->payloadView();
  EXPECT_EQ(WorldGenerationPreparedProductKind::TerrainFields, terrainView.kind);
  EXPECT_EQ(64u, terrainView.payloadBytes);
  EXPECT_EQ(0xabcu, terrainView.contentToken);

  auto deltaId = makeIdentity({4, 5}, 1, 3);
  deltaId.product = WorldGenerationPreparedProduct::StageDelta;
  deltaId.opaqueContentToken = 0xdef;
  auto delta = WorldGenerationPreparedSector::tryPrepare(cache, deltaId, 128);
  ASSERT_TRUE(delta.has_value());
  auto deltaHandoff = delta->probeHandoff(3, deltaId);
  EXPECT_EQ(WorldGenerationPreparedHandoffOutcome::Ready, deltaHandoff.outcome);
  EXPECT_EQ(128u, deltaHandoff.payload.payloadBytes);
  EXPECT_EQ(0xdefu, deltaHandoff.payload.contentToken);
  EXPECT_FALSE(deltaHandoff.writesWorldStorage);
}
