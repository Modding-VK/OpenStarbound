#include "StarWorldGenerationShadowPacket.hpp"

#include <cstring>

#include "gtest/gtest.h"

using namespace Star;

namespace {
ByteArray bytes(char const* text) {
  return ByteArray(text, strlen(text));
}

WorldGenerationShadowDbKey tiles(Vec2U sector) {
  return {sector, String("Tiles")};
}

WorldGenerationShadowDbKey ents(Vec2U sector) {
  return {sector, String("Entities")};
}
}

TEST(WorldGenerationShadowPacketTest, ProvisionalCannotBecomeInitial) {
  WorldGenerationShadowPacketHarness harness("pk-a");
  EXPECT_EQ(WorldGenerationShadowPacketStatus::ProvisionalBlocked,
      harness.tryBuildFromProvisional({0, 0}, bytes("provisional-tiles")));
  uint64_t packetId = 0;
  EXPECT_EQ(WorldGenerationShadowPacketStatus::MissingConfirmed,
      harness.tryBuildInitial({0, 0}, packetId));
}

TEST(WorldGenerationShadowPacketTest, InitialBuiltOnlyFromConfirmedCheckpoint) {
  WorldGenerationShadowPacketHarness harness("pk-b");
  ASSERT_EQ(WorldGenerationShadowPacketStatus::Ok,
      harness.confirmProduct(tiles({1, 0}), 1, bytes("tiles-confirmed")));
  uint64_t packetId = 0;
  ASSERT_EQ(WorldGenerationShadowPacketStatus::Ok, harness.tryBuildInitial({1, 0}, packetId));
  auto view = harness.packet(packetId);
  ASSERT_TRUE(view);
  EXPECT_EQ(WorldGenerationShadowPacketKind::TileArrayInitial, view->kind);
  EXPECT_EQ(WorldGenerationShadowPacketPhase::Prepared, view->phase);
  EXPECT_TRUE(view->sectorFormat32);
  EXPECT_EQ(WorldGenerationShadowPacketSectorExtent, 32u);
  EXPECT_EQ(bytes("tiles-confirmed").size(), view->payloadBytes);
  EXPECT_FALSE(harness.sectorActive({1, 0}));
}

TEST(WorldGenerationShadowPacketTest, DeltaRequiresAppliedInitial) {
  WorldGenerationShadowPacketHarness harness("pk-c");
  ASSERT_EQ(WorldGenerationShadowPacketStatus::Ok,
      harness.confirmProduct(tiles({2, 0}), 1, bytes("tiles")));
  ASSERT_EQ(WorldGenerationShadowPacketStatus::Ok,
      harness.confirmProduct(ents({2, 0}), 1, bytes("ents")));
  uint64_t deltaId = 0;
  EXPECT_EQ(WorldGenerationShadowPacketStatus::MissingInitial,
      harness.tryBuildDelta({2, 0}, String("Entities"), deltaId));

  uint64_t initialId = 0;
  ASSERT_EQ(WorldGenerationShadowPacketStatus::Ok, harness.tryBuildInitial({2, 0}, initialId));
  ASSERT_EQ(WorldGenerationShadowPacketStatus::Ok, harness.enqueueAndSend(initialId));
  ASSERT_EQ(WorldGenerationShadowPacketStatus::Ok, harness.receiveAndApply(initialId));
  EXPECT_TRUE(harness.sectorActive({2, 0}));

  ASSERT_EQ(WorldGenerationShadowPacketStatus::Ok,
      harness.tryBuildDelta({2, 0}, String("Entities"), deltaId));
  auto delta = harness.packet(deltaId);
  ASSERT_TRUE(delta);
  EXPECT_EQ(WorldGenerationShadowPacketKind::SectorDelta, delta->kind);
}

TEST(WorldGenerationShadowPacketTest, ServerReadyDoesNotImplyClientVisible) {
  WorldGenerationShadowPacketHarness harness("pk-d");
  ASSERT_EQ(WorldGenerationShadowPacketStatus::Ok,
      harness.confirmProduct(tiles({3, 0}), 1, bytes("tiles")));
  uint64_t packetId = 0;
  ASSERT_EQ(WorldGenerationShadowPacketStatus::Ok, harness.tryBuildInitial({3, 0}, packetId));
  ASSERT_EQ(WorldGenerationShadowPacketStatus::Ok, harness.enqueueAndSend(packetId));
  auto view = harness.packet(packetId);
  ASSERT_TRUE(view);
  EXPECT_EQ(WorldGenerationShadowPacketPhase::Sent, view->phase);
  EXPECT_FALSE(harness.sectorActive({3, 0}));
  ASSERT_EQ(WorldGenerationShadowPacketStatus::Ok, harness.receiveAndApply(packetId));
  EXPECT_TRUE(harness.sectorActive({3, 0}));
  EXPECT_EQ(WorldGenerationShadowPacketPhase::Applied, harness.packet(packetId)->phase);
}

TEST(WorldGenerationShadowPacketTest, MilestoneOrderEnforced) {
  WorldGenerationShadowPacketHarness harness("pk-e");
  ASSERT_EQ(WorldGenerationShadowPacketStatus::Ok,
      harness.confirmProduct(tiles({4, 0}), 1, bytes("tiles")));
  uint64_t packetId = 0;
  ASSERT_EQ(WorldGenerationShadowPacketStatus::Ok, harness.tryBuildInitial({4, 0}, packetId));
  EXPECT_EQ(WorldGenerationShadowPacketStatus::OrderViolation,
      harness.advancePhase(packetId, WorldGenerationShadowPacketPhase::Enqueued));
  ASSERT_EQ(WorldGenerationShadowPacketStatus::Ok,
      harness.advancePhase(packetId, WorldGenerationShadowPacketPhase::Published));
  ASSERT_EQ(WorldGenerationShadowPacketStatus::Ok,
      harness.advancePhase(packetId, WorldGenerationShadowPacketPhase::Enqueued));
  ASSERT_EQ(WorldGenerationShadowPacketStatus::Ok,
      harness.advancePhase(packetId, WorldGenerationShadowPacketPhase::Sent));
  ASSERT_EQ(WorldGenerationShadowPacketStatus::Ok,
      harness.advancePhase(packetId, WorldGenerationShadowPacketPhase::Received));
  ASSERT_EQ(WorldGenerationShadowPacketStatus::Ok,
      harness.advancePhase(packetId, WorldGenerationShadowPacketPhase::Applied));
}

TEST(WorldGenerationShadowPacketTest, DistinctClocksRejectSubtraction) {
  WorldGenerationShadowPacketHarness harness("pk-f");
  ASSERT_EQ(WorldGenerationShadowPacketStatus::Ok,
      harness.confirmProduct(tiles({5, 0}), 1, bytes("tiles")));
  uint64_t packetId = 0;
  ASSERT_EQ(WorldGenerationShadowPacketStatus::Ok, harness.tryBuildInitial({5, 0}, packetId));
  ASSERT_EQ(WorldGenerationShadowPacketStatus::Ok, harness.enqueueAndSend(packetId));
  ASSERT_EQ(WorldGenerationShadowPacketStatus::Ok, harness.receiveAndApply(packetId));
  auto view = harness.packet(packetId);
  ASSERT_TRUE(view);
  EXPECT_NE(0u, view->identity.serverClock);
  EXPECT_NE(0u, view->clientClock);
  int64_t delta = 0;
  EXPECT_EQ(WorldGenerationShadowPacketStatus::ClockMismatch,
      harness.compareClocks(packetId, delta));
}
