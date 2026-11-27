#include "StarWorldGenerationShadowUnload.hpp"

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

TEST(WorldGenerationShadowUnloadTest, UnloadRevokesPublishAuthority) {
  WorldGenerationShadowUnloadHarness harness("ul-a");
  auto tiles = key({0, 0}, "Tiles");
  ASSERT_EQ(WorldGenerationShadowUnloadStatus::Ok, harness.beginProduce(tiles));
  ASSERT_EQ(WorldGenerationShadowUnloadStatus::Ok,
      harness.completePrivate(tiles, 1, bytes("tiles")));
  ASSERT_EQ(WorldGenerationShadowUnloadStatus::Ok, harness.requestUnload({0, 0}));
  EXPECT_EQ(WorldGenerationShadowUnloadStatus::PublishRevoked, harness.tryPublish(tiles));
  auto lease = harness.lease(tiles);
  ASSERT_TRUE(lease);
  EXPECT_EQ(WorldGenerationShadowLeaseState::Unloading, lease->state);
  EXPECT_FALSE(lease->publishAuthorized);
  EXPECT_TRUE(lease->privateBufferHeld);
}

TEST(WorldGenerationShadowUnloadTest, DrainWaitPreservesReservationWithoutInstall) {
  WorldGenerationShadowUnloadHarness harness("ul-b");
  auto tiles = key({1, 0}, "Tiles");
  ASSERT_EQ(WorldGenerationShadowUnloadStatus::Ok, harness.beginProduce(tiles));
  ASSERT_EQ(WorldGenerationShadowUnloadStatus::Ok, harness.preserveReservation(tiles));
  ASSERT_EQ(WorldGenerationShadowUnloadStatus::Ok,
      harness.completePrivate(tiles, 1, bytes("tiles")));
  ASSERT_EQ(WorldGenerationShadowUnloadStatus::Ok, harness.requestUnload({1, 0}));
  ASSERT_EQ(WorldGenerationShadowUnloadStatus::Ok, harness.drainWait(tiles));
  auto lease = harness.lease(tiles);
  ASSERT_TRUE(lease);
  EXPECT_EQ(WorldGenerationShadowLeaseState::Unloaded, lease->state);
  EXPECT_TRUE(lease->reservationPreserved);
  EXPECT_TRUE(lease->privateBufferHeld);
  EXPECT_EQ(WorldGenerationShadowDbStatus::MissingKey, harness.db().readCheckpoint(tiles).status);
  EXPECT_EQ(WorldGenerationShadowUnloadStatus::Ok, harness.releasePrivateBuffers(tiles));
  auto after = harness.lease(tiles);
  ASSERT_TRUE(after);
  EXPECT_FALSE(after->privateBufferHeld);
}

TEST(WorldGenerationShadowUnloadTest, DrainDiscardDropsConclusionButKeepAliveProtected) {
  WorldGenerationShadowUnloadHarness harness("ul-c");
  auto tiles = key({2, 0}, "Tiles");
  auto ents = key({2, 0}, "Entities");
  ASSERT_EQ(WorldGenerationShadowUnloadStatus::Ok, harness.beginProduce(tiles));
  ASSERT_EQ(WorldGenerationShadowUnloadStatus::Ok, harness.beginProduce(ents));
  ASSERT_EQ(WorldGenerationShadowUnloadStatus::Ok,
      harness.completePrivate(tiles, 1, bytes("tiles")));
  ASSERT_EQ(WorldGenerationShadowUnloadStatus::Ok,
      harness.completePrivate(ents, 1, bytes("ents")));
  ASSERT_EQ(WorldGenerationShadowUnloadStatus::Ok, harness.markKeepAlive(ents));
  ASSERT_EQ(WorldGenerationShadowUnloadStatus::Ok, harness.requestUnload({2, 0}));
  EXPECT_EQ(WorldGenerationShadowUnloadStatus::Ok, harness.drainDiscard(tiles));
  EXPECT_EQ(WorldGenerationShadowUnloadStatus::KeepAliveProtected, harness.drainDiscard(ents));
  EXPECT_EQ(WorldGenerationShadowLeaseState::Discarded, harness.lease(tiles)->state);
  EXPECT_EQ(WorldGenerationShadowLeaseState::KeepAlive, harness.lease(ents)->state);
  EXPECT_FALSE(harness.lease(ents)->publishAuthorized);
}

TEST(WorldGenerationShadowUnloadTest, PartialHandoffToCentralBlocked) {
  WorldGenerationShadowUnloadHarness harness("ul-d");
  auto tiles = key({3, 0}, "Tiles");
  ASSERT_EQ(WorldGenerationShadowUnloadStatus::Ok, harness.beginProduce(tiles));
  ASSERT_EQ(WorldGenerationShadowUnloadStatus::Ok,
      harness.completePrivate(tiles, 1, bytes("partial")));
  ASSERT_EQ(WorldGenerationShadowUnloadStatus::Ok, harness.requestUnload({3, 0}));
  EXPECT_EQ(WorldGenerationShadowUnloadStatus::PartialBlocked,
      harness.tryHandoffPartialToCentral({3, 0}));
}

TEST(WorldGenerationShadowUnloadTest, ModeSwitchRequiresConfirmedCheckpoint) {
  WorldGenerationShadowUnloadHarness harness("ul-e");
  auto tiles = key({4, 0}, "Tiles");
  ASSERT_EQ(WorldGenerationShadowUnloadStatus::Ok, harness.beginProduce(tiles));
  ASSERT_EQ(WorldGenerationShadowUnloadStatus::Ok,
      harness.completePrivate(tiles, 1, bytes("tiles")));
  EXPECT_EQ(WorldGenerationShadowUnloadStatus::CheckpointRequired, harness.switchToLegacyMode());

  ASSERT_EQ(WorldGenerationShadowUnloadStatus::Ok, harness.tryPublish(tiles));
  EXPECT_EQ(1u, harness.confirmedStepEpoch());
  EXPECT_EQ(WorldGenerationShadowUnloadStatus::Ok, harness.switchToLegacyMode());
  EXPECT_TRUE(harness.modeIsLegacy());
}

TEST(WorldGenerationShadowUnloadTest, PrivateBuffersNotReleasedWhileReady) {
  WorldGenerationShadowUnloadHarness harness("ul-f");
  auto tiles = key({5, 0}, "Tiles");
  ASSERT_EQ(WorldGenerationShadowUnloadStatus::Ok, harness.beginProduce(tiles));
  ASSERT_EQ(WorldGenerationShadowUnloadStatus::Ok,
      harness.completePrivate(tiles, 1, bytes("tiles")));
  EXPECT_EQ(WorldGenerationShadowUnloadStatus::BufferHeld, harness.releasePrivateBuffers(tiles));
  ASSERT_EQ(WorldGenerationShadowUnloadStatus::Ok, harness.requestUnload({5, 0}));
  ASSERT_EQ(WorldGenerationShadowUnloadStatus::Ok, harness.drainWait(tiles));
  EXPECT_EQ(WorldGenerationShadowUnloadStatus::Ok, harness.releasePrivateBuffers(tiles));
}
