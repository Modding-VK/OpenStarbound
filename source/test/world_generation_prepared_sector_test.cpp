#include "gtest/gtest.h"

#include "StarWorldGenerationPreparedSector.hpp"

using namespace Star;

TEST(WorldGenerationPreparedSectorTest, EmptyProductMatchesIdentityWithoutPersistedMutation) {
  WorldGenerationPreparedProductIdentity identity{{3, 4}, 10, 20, 30};
  auto empty = WorldGenerationPreparedSectorStore::makeEmpty(identity);
  EXPECT_EQ(WorldGenerationPreparedProductKind::Empty, empty.kind);
  EXPECT_TRUE(empty.candidates.empty());
  EXPECT_TRUE(empty.matches(identity));
  EXPECT_FALSE(empty.mutatesPersistedLevel());

  WorldGenerationPreparedProductIdentity bumped = identity;
  bumped.distributionRevision = 21;
  EXPECT_FALSE(empty.matches(bumped));
}

TEST(WorldGenerationPreparedSectorTest, OrderedCandidatesCarryPlacementIdsWithoutHalo) {
  WorldGenerationPreparedProductIdentity identity{{1, 2}, 1, 2, 3};
  List<WorldGenerationMicroDungeonCandidate> candidates{
      {7, String("apexapartments"), {8, 9}, 42},
      {8, String("humanbunker"), {10, 11}, 43}};
  auto product = WorldGenerationPreparedSectorStore::makeOrderedCandidates(identity, candidates);
  EXPECT_EQ(WorldGenerationPreparedProductKind::OrderedCandidates, product.kind);
  ASSERT_EQ(2u, product.candidates.size());
  EXPECT_EQ(7u, product.candidates[0].placementId);
  EXPECT_EQ(String("apexapartments"), product.candidates[0].dungeonName);
  // Selection product holds IDs/names/seeds only — no tile halo payload field exists.
  EXPECT_FALSE(product.mutatesPersistedLevel());
}

TEST(WorldGenerationPreparedSectorTest, StoreSharesMatchingIdentityAndInvalidatesStale) {
  WorldGenerationPreparedSectorStore store;
  WorldGenerationPreparedProductIdentity identity{{0, 0}, 5, 6, 7};
  auto first = store.shareOrPut(WorldGenerationPreparedSectorStore::makeEmpty(identity));
  auto second = store.shareOrPut(WorldGenerationPreparedSectorStore::makeEmpty(identity));
  EXPECT_EQ(first, second);
  EXPECT_EQ(1u, store.size());

  WorldGenerationPreparedProductIdentity live = identity;
  live.templateRevision = 99;
  EXPECT_EQ(1u, store.invalidateStale(live));
  EXPECT_FALSE(store.contains(first));
  EXPECT_EQ(0u, store.size());
}

TEST(WorldGenerationPreparedSectorTest, BaseTilesPrivateHoldsTicketWithoutInstall) {
  WorldGenerationPreparedProductIdentity identity{{0, 0}, 7, 0, 0};
  WorldGenerationBaseTilesIdentity ticket{
      {1, 1, {0, 0}, {0, 0}, WorldGenerationTaskStage::BaseTiles},
      5,
      7,
      {128, 128},
      RectI(0, 0, 32, 32)};
  ASSERT_TRUE(ticket.valid());
  auto product = WorldGenerationPreparedSectorStore::makeBaseTilesPrivate(
      identity, ticket, sizeof(ServerTileSectorArray::Array));
  EXPECT_EQ(WorldGenerationPreparedProductKind::BaseTilesPrivate, product.kind);
  ASSERT_TRUE(product.baseTiles);
  EXPECT_TRUE(product.baseTiles->ticket.matches(ticket));
  EXPECT_EQ(sizeof(ServerTileSectorArray::Array), product.baseTiles->retainedBytes);
  EXPECT_FALSE(product.baseTiles->installAuthorized);
  EXPECT_FALSE(product.canInstall());
  EXPECT_FALSE(product.mutatesPersistedLevel());
}

TEST(WorldGenerationPreparedSectorTest, PayloadViewAndWorldStoragePathNeverWrite) {
  WorldGenerationPreparedSectorStore store;
  WorldGenerationPreparedProductIdentity identity{{2, 3}, 11, 12, 13};
  auto id = store.put(WorldGenerationPreparedSectorStore::makeEmpty(identity));

  auto view = store.payloadView(id);
  ASSERT_TRUE(view);
  EXPECT_EQ(id, view->productId);
  EXPECT_EQ(WorldGenerationPreparedProductKind::Empty, view->kind);
  EXPECT_EQ(0u, view->payloadBytes);
  EXPECT_NE(0u, view->contentToken);
  EXPECT_FALSE(view->installAuthorized);

  auto path = store.worldStoragePath(9, id);
  EXPECT_EQ(9u, path.worldEpoch);
  EXPECT_EQ((Vec2I{2, 3}), path.sector);
  EXPECT_EQ(id, path.productId);
  EXPECT_EQ(WorldGenerationPreparedProductKind::Empty, path.kind);

  auto handoff = store.probeHandoff(9, id, identity);
  EXPECT_EQ(WorldGenerationPreparedHandoffOutcome::Ready, handoff.outcome);
  EXPECT_FALSE(handoff.writesWorldStorage);
  EXPECT_EQ(path, handoff.path);
}

TEST(WorldGenerationPreparedSectorTest, HandoffReportsStaleAndBaseTilesNotAuthorized) {
  WorldGenerationPreparedSectorStore store;
  WorldGenerationPreparedProductIdentity identity{{0, 0}, 7, 0, 0};
  WorldGenerationBaseTilesIdentity ticket{
      {1, 1, {0, 0}, {0, 0}, WorldGenerationTaskStage::BaseTiles},
      5,
      7,
      {128, 128},
      RectI(0, 0, 32, 32)};
  auto id = store.put(WorldGenerationPreparedSectorStore::makeBaseTilesPrivate(
      identity, ticket, 1024));

  WorldGenerationPreparedProductIdentity stale = identity;
  stale.templateRevision = 99;
  auto staleHandoff = store.probeHandoff(1, id, stale);
  EXPECT_EQ(WorldGenerationPreparedHandoffOutcome::StaleIdentity, staleHandoff.outcome);
  EXPECT_FALSE(staleHandoff.writesWorldStorage);

  auto denied = store.probeHandoff(1, id, identity);
  EXPECT_EQ(WorldGenerationPreparedHandoffOutcome::NotAuthorized, denied.outcome);
  EXPECT_FALSE(denied.writesWorldStorage);
  ASSERT_TRUE(store.payloadView(id));
  EXPECT_EQ(1024u, store.payloadView(id)->payloadBytes);
}

TEST(WorldGenerationPreparedSectorTest, TerrainFieldsAndStepDeltaExposeOpaquePayload) {
  WorldGenerationPreparedSectorStore store;
  WorldGenerationPreparedProductIdentity identity{{4, 5}, 1, 2, 3};
  auto terrainId = store.put(
      WorldGenerationPreparedSectorStore::makeTerrainFields(identity, 0xabc, 64));
  auto deltaId = store.put(
      WorldGenerationPreparedSectorStore::makeStepDelta(identity, 0xdef, 128));

  auto terrain = store.payloadView(terrainId);
  ASSERT_TRUE(terrain);
  EXPECT_EQ(WorldGenerationPreparedProductKind::TerrainFields, terrain->kind);
  EXPECT_EQ(64u, terrain->payloadBytes);
  EXPECT_EQ(0xabcu, terrain->contentToken);

  auto deltaHandoff = store.probeHandoff(3, deltaId, identity);
  EXPECT_EQ(WorldGenerationPreparedHandoffOutcome::Ready, deltaHandoff.outcome);
  EXPECT_EQ(128u, deltaHandoff.payload.payloadBytes);
  EXPECT_FALSE(deltaHandoff.writesWorldStorage);
}
