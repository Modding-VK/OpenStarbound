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
