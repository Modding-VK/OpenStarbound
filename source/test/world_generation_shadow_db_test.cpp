#include "StarWorldGenerationShadowDb.hpp"

#include <cstring>

#include "gtest/gtest.h"

using namespace Star;

namespace {
WorldGenerationShadowDbKey key(Vec2U sector = {1, 0}, String kind = "StructureFrontier") {
  return {sector, std::move(kind)};
}

ByteArray bytes(char const* text) {
  return ByteArray(text, strlen(text));
}
}

TEST(WorldGenerationShadowDbTest, UncommittedWritesInvisibleToCheckpointReaders) {
  WorldGenerationShadowDbService db("shadow-world-a");
  ASSERT_EQ(WorldGenerationShadowDbStatus::Ok, db.beginWrite(10));
  ASSERT_EQ(WorldGenerationShadowDbStatus::Ok, db.put(key(), 1, bytes("frontier-v1")));
  auto read = db.readCheckpoint(key());
  EXPECT_EQ(WorldGenerationShadowDbStatus::MissingKey, read.status);
  EXPECT_FALSE(read.entry.isValid());
}

TEST(WorldGenerationShadowDbTest, CommitMakesBytesVisibleAtCheckpoint) {
  WorldGenerationShadowDbService db("shadow-world-b");
  ASSERT_EQ(WorldGenerationShadowDbStatus::Ok, db.beginWrite(11));
  ASSERT_EQ(WorldGenerationShadowDbStatus::Ok, db.put(key(), 1, bytes("frontier-v1")));
  ASSERT_EQ(WorldGenerationShadowDbStatus::Ok, db.commit());
  EXPECT_EQ(1u, db.checkpointEpoch());
  auto read = db.readCheckpoint(key());
  ASSERT_EQ(WorldGenerationShadowDbStatus::Ok, read.status);
  ASSERT_TRUE(read.entry.isValid());
  EXPECT_EQ(bytes("frontier-v1"), read.entry->bytes);
  EXPECT_EQ(1u, read.entry->contentVersion);
  EXPECT_EQ(11u, read.entry->writeTicket);
}

TEST(WorldGenerationShadowDbTest, RollbackDropsScratchWithoutAdvancingCheckpoint) {
  WorldGenerationShadowDbService db("shadow-world-c");
  ASSERT_EQ(WorldGenerationShadowDbStatus::Ok, db.beginWrite(12));
  ASSERT_EQ(WorldGenerationShadowDbStatus::Ok, db.put(key(), 1, bytes("tmp")));
  ASSERT_EQ(WorldGenerationShadowDbStatus::Ok, db.rollback());
  EXPECT_EQ(0u, db.checkpointEpoch());
  EXPECT_EQ(WorldGenerationShadowDbStatus::MissingKey, db.readCheckpoint(key()).status);
}

TEST(WorldGenerationShadowDbTest, ConcurrentStoryReaderSeesOnlyPriorCheckpoint) {
  WorldGenerationShadowDbService db("shadow-world-d");
  auto k = key({0, 0});
  ASSERT_EQ(WorldGenerationShadowDbStatus::Ok, db.beginWrite(1));
  ASSERT_EQ(WorldGenerationShadowDbStatus::Ok, db.put(k, 1, bytes("old")));
  ASSERT_EQ(WorldGenerationShadowDbStatus::Ok, db.commit());

  ASSERT_EQ(WorldGenerationShadowDbStatus::Ok, db.beginWrite(2));
  ASSERT_EQ(WorldGenerationShadowDbStatus::Ok, db.put(k, 2, bytes("new")));
  // Independent checkpoint reader mid-transaction still sees committed "old".
  auto mid = db.readCheckpoint(k);
  ASSERT_EQ(WorldGenerationShadowDbStatus::Ok, mid.status);
  EXPECT_EQ(bytes("old"), mid.entry->bytes);
  EXPECT_EQ(1u, mid.entry->contentVersion);

  ASSERT_EQ(WorldGenerationShadowDbStatus::Ok, db.commit());
  auto after = db.readCheckpoint(k);
  ASSERT_EQ(WorldGenerationShadowDbStatus::Ok, after.status);
  EXPECT_EQ(bytes("new"), after.entry->bytes);
  EXPECT_EQ(2u, after.entry->contentVersion);
}

TEST(WorldGenerationShadowDbTest, StaleContentVersionRejected) {
  WorldGenerationShadowDbService db("shadow-world-e");
  auto k = key({2, 0});
  ASSERT_EQ(WorldGenerationShadowDbStatus::Ok, db.beginWrite(3));
  ASSERT_EQ(WorldGenerationShadowDbStatus::Ok, db.put(k, 5, bytes("v5")));
  ASSERT_EQ(WorldGenerationShadowDbStatus::Ok, db.commit());
  ASSERT_EQ(WorldGenerationShadowDbStatus::Ok, db.beginWrite(4));
  EXPECT_EQ(WorldGenerationShadowDbStatus::StaleContent, db.put(k, 4, bytes("older")));
}

TEST(WorldGenerationShadowDbTest, SyncFailsWhileTransactionOpenAndCloseBlocksOps) {
  WorldGenerationShadowDbService db("shadow-world-f");
  ASSERT_EQ(WorldGenerationShadowDbStatus::Ok, db.beginWrite(5));
  EXPECT_EQ(WorldGenerationShadowDbStatus::BusyTransaction, db.sync());
  ASSERT_EQ(WorldGenerationShadowDbStatus::Ok, db.rollback());
  EXPECT_EQ(WorldGenerationShadowDbStatus::Ok, db.sync());
  EXPECT_EQ(WorldGenerationShadowDbStatus::Ok, db.close());
  EXPECT_EQ(WorldGenerationShadowDbStatus::Closed, db.beginWrite(6));
  EXPECT_EQ(WorldGenerationShadowDbStatus::Closed, db.readCheckpoint(key()).status);
}

TEST(WorldGenerationShadowDbTest, FrontierBlobRoundTripCoherentWithPublicationProducts) {
  auto blob = WorldGenerationShadowDbService::encodeFrontierBlob(8, 42, 3);
  uint64_t epoch = 0;
  uint64_t seed = 0;
  uint32_t dirty = 0;
  ASSERT_TRUE(WorldGenerationShadowDbService::decodeFrontierBlob(blob, epoch, seed, dirty));
  EXPECT_EQ(8u, epoch);
  EXPECT_EQ(42u, seed);
  EXPECT_EQ(3u, dirty);

  WorldGenerationShadowDbService db("shadow-world-g");
  WorldGenerationShadowDbKey frontierKey{{3, 1}, "PublicationFrontier"};
  ASSERT_EQ(WorldGenerationShadowDbStatus::Ok, db.beginWrite(20));
  ASSERT_EQ(WorldGenerationShadowDbStatus::Ok, db.put(frontierKey, 9, blob));
  ASSERT_EQ(WorldGenerationShadowDbStatus::Ok, db.commit());
  auto read = db.readCheckpoint(frontierKey);
  ASSERT_EQ(WorldGenerationShadowDbStatus::Ok, read.status);
  EXPECT_EQ(blob, read.entry->bytes);
  EXPECT_EQ(9u, read.entry->contentVersion);
}
