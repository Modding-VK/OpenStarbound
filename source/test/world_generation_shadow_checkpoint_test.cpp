#include "StarWorldGenerationShadowCheckpoint.hpp"

#include <cstring>

#include "gtest/gtest.h"

using namespace Star;

namespace {
WorldGenerationShadowDbKey key(Vec2U sector, char const* kind) {
  return {sector, String(kind)};
}

ByteArray bytes(char const* text) {
  return ByteArray(text, strlen(text));
}
}

TEST(WorldGenerationShadowCheckpointTest, PartialUnitCannotSealOrCommit) {
  WorldGenerationShadowCheckpointHarness harness("cp-a");
  List<WorldGenerationShadowDbKey> required = {
      key({0, 0}, "Tiles"), key({0, 0}, "Entities")};
  ASSERT_EQ(WorldGenerationShadowCheckpointStatus::Ok, harness.beginIntention(1, required));
  ASSERT_EQ(WorldGenerationShadowCheckpointStatus::Ok,
      harness.put(key({0, 0}, "Tiles"), 1, bytes("tiles")));
  EXPECT_EQ(WorldGenerationShadowCheckpointStatus::IncompleteUnit, harness.seal());
  EXPECT_EQ(WorldGenerationShadowCheckpointStatus::NotSealed, harness.commitCheckpoint());
  EXPECT_EQ(WorldGenerationShadowDbStatus::MissingKey,
      harness.db().readCheckpoint(key({0, 0}, "Tiles")).status);
}

TEST(WorldGenerationShadowCheckpointTest, CompleteUnitCommitsAndSyncsCheckpoint) {
  WorldGenerationShadowCheckpointHarness harness("cp-b");
  List<WorldGenerationShadowDbKey> required = {
      key({1, 0}, "Tiles"), key({1, 0}, "Entities")};
  ASSERT_EQ(WorldGenerationShadowCheckpointStatus::Ok, harness.beginIntention(1, required));
  ASSERT_EQ(WorldGenerationShadowCheckpointStatus::Ok,
      harness.put(key({1, 0}, "Tiles"), 1, bytes("tiles-1")));
  ASSERT_EQ(WorldGenerationShadowCheckpointStatus::Ok,
      harness.put(key({1, 0}, "Entities"), 1, bytes("ents-1")));
  ASSERT_EQ(WorldGenerationShadowCheckpointStatus::Ok, harness.seal());
  ASSERT_EQ(WorldGenerationShadowCheckpointStatus::Ok, harness.commitCheckpoint());
  EXPECT_EQ(1u, harness.confirmedStepEpoch());
  EXPECT_FALSE(harness.hasIntention());
  auto tiles = harness.db().readCheckpoint(key({1, 0}, "Tiles"));
  auto ents = harness.db().readCheckpoint(key({1, 0}, "Entities"));
  ASSERT_EQ(WorldGenerationShadowDbStatus::Ok, tiles.status);
  ASSERT_EQ(WorldGenerationShadowDbStatus::Ok, ents.status);
  EXPECT_EQ(bytes("tiles-1"), tiles.entry->bytes);
  EXPECT_EQ(bytes("ents-1"), ents.entry->bytes);
}

TEST(WorldGenerationShadowCheckpointTest, IncompatibleStepBlockedUntilPriorConfirmed) {
  WorldGenerationShadowCheckpointHarness harness("cp-c");
  List<WorldGenerationShadowDbKey> required = {key({2, 0}, "Tiles")};
  EXPECT_EQ(WorldGenerationShadowCheckpointStatus::IncompatibleStep,
      harness.beginIntention(2, required));
  ASSERT_EQ(WorldGenerationShadowCheckpointStatus::Ok, harness.beginIntention(1, required));
  ASSERT_EQ(WorldGenerationShadowCheckpointStatus::Ok,
      harness.put(key({2, 0}, "Tiles"), 1, bytes("t")));
  ASSERT_EQ(WorldGenerationShadowCheckpointStatus::Ok, harness.seal());
  ASSERT_EQ(WorldGenerationShadowCheckpointStatus::Ok, harness.commitCheckpoint());
  EXPECT_EQ(WorldGenerationShadowCheckpointStatus::Ok, harness.beginIntention(2, required));
}

TEST(WorldGenerationShadowCheckpointTest, CrashBeforeSealRejectsPartialIntention) {
  WorldGenerationShadowCheckpointHarness harness("cp-d");
  List<WorldGenerationShadowDbKey> required = {
      key({3, 0}, "Tiles"), key({3, 0}, "Meta")};
  ASSERT_EQ(WorldGenerationShadowCheckpointStatus::Ok, harness.beginIntention(1, required));
  ASSERT_EQ(WorldGenerationShadowCheckpointStatus::Ok,
      harness.put(key({3, 0}, "Tiles"), 1, bytes("partial")));
  EXPECT_EQ(WorldGenerationShadowCheckpointStatus::Rejected, harness.recover());
  EXPECT_FALSE(harness.hasIntention());
  EXPECT_EQ(0u, harness.confirmedStepEpoch());
  EXPECT_EQ(WorldGenerationShadowDbStatus::MissingKey,
      harness.db().readCheckpoint(key({3, 0}, "Tiles")).status);
}

TEST(WorldGenerationShadowCheckpointTest, CrashAfterSealRecoversIdempotently) {
  WorldGenerationShadowCheckpointHarness harness("cp-e");
  List<WorldGenerationShadowDbKey> required = {
      key({4, 0}, "Tiles"), key({4, 0}, "Entities")};
  ASSERT_EQ(WorldGenerationShadowCheckpointStatus::Ok, harness.beginIntention(1, required));
  ASSERT_EQ(WorldGenerationShadowCheckpointStatus::Ok,
      harness.put(key({4, 0}, "Tiles"), 1, bytes("tiles")));
  ASSERT_EQ(WorldGenerationShadowCheckpointStatus::Ok,
      harness.put(key({4, 0}, "Entities"), 1, bytes("ents")));
  ASSERT_EQ(WorldGenerationShadowCheckpointStatus::Ok, harness.seal());
  ASSERT_EQ(WorldGenerationShadowCheckpointStatus::Ok, harness.simulateCrashAfterSeal());
  EXPECT_TRUE(harness.hasIntention());
  EXPECT_EQ(0u, harness.confirmedStepEpoch());

  EXPECT_EQ(WorldGenerationShadowCheckpointStatus::Recovered, harness.recover());
  EXPECT_EQ(1u, harness.confirmedStepEpoch());
  EXPECT_FALSE(harness.hasIntention());
  auto tiles = harness.db().readCheckpoint(key({4, 0}, "Tiles"));
  ASSERT_EQ(WorldGenerationShadowDbStatus::Ok, tiles.status);
  EXPECT_EQ(bytes("tiles"), tiles.entry->bytes);

  // Second recover with no intention is a missing journal, not a duplicate write.
  EXPECT_EQ(WorldGenerationShadowCheckpointStatus::MissingIntention, harness.recover());
  EXPECT_EQ(1u, harness.db().checkpointEpoch());
}

TEST(WorldGenerationShadowCheckpointTest, RejectClearsIntentionWithoutVisibility) {
  WorldGenerationShadowCheckpointHarness harness("cp-f");
  List<WorldGenerationShadowDbKey> required = {key({5, 0}, "Frontier")};
  ASSERT_EQ(WorldGenerationShadowCheckpointStatus::Ok, harness.beginIntention(1, required));
  ASSERT_EQ(WorldGenerationShadowCheckpointStatus::Ok,
      harness.put(key({5, 0}, "Frontier"), 1,
          WorldGenerationShadowDbService::encodeFrontierBlob(1, 9, 2)));
  EXPECT_EQ(WorldGenerationShadowCheckpointStatus::Rejected, harness.reject());
  EXPECT_EQ(WorldGenerationShadowDbStatus::MissingKey,
      harness.db().readCheckpoint(key({5, 0}, "Frontier")).status);
}
