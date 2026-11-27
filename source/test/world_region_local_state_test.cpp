#include "StarWorldRegionLocalState.hpp"
#include "StarGameTypes.hpp"

#include "gtest/gtest.h"

using namespace Star;

namespace {

WorldRegionPartition partition() {
  return WorldRegionPartition(WorldGeometry(256, 128));
}

uint8_t tilesAndLiquids() {
  return worldRegionLocalKindMask(WorldRegionLocalKind::Tiles)
      | worldRegionLocalKindMask(WorldRegionLocalKind::Liquids);
}

}

TEST(WorldRegionLocalStateTest, AbsentOwnerCannotDeclareInventory) {
  WorldRegionOwnerRegistry owners(8, partition());
  WorldRegionLocalStateLedger ledger(owners);
  WorldRegionOwnerIdentity fake{8, 1, {0, 0}};
  EXPECT_EQ(ledger.declareKinds(fake, tilesAndLiquids()), WorldRegionLocalDeclareResult::RegionAbsent);
}

TEST(WorldRegionLocalStateTest, ActiveOwnerDeclaresExclusiveKinds) {
  WorldRegionOwnerRegistry owners(8, partition());
  ASSERT_EQ(owners.admit({0, 0}, WorldRegionMemberLevel::Tiles), WorldRegionOwnerAdmitResult::Created);
  auto identity = *owners.identity({0, 0});
  WorldRegionLocalStateLedger ledger(owners);
  EXPECT_EQ(ledger.declareKinds(identity, tilesAndLiquids()), WorldRegionLocalDeclareResult::Declared);
  auto snapshot = ledger.snapshot({0, 0});
  ASSERT_TRUE(snapshot);
  EXPECT_EQ(identity, snapshot->owner);
  EXPECT_TRUE(ledger.ownsKind({0, 0}, WorldRegionLocalKind::Tiles));
  EXPECT_TRUE(ledger.ownsKind({0, 0}, WorldRegionLocalKind::Liquids));
  EXPECT_FALSE(ledger.ownsKind({0, 0}, WorldRegionLocalKind::LocalLua));
  EXPECT_EQ(ledger.declareKinds(identity, worldRegionLocalKindMask(WorldRegionLocalKind::LocalLua)),
      WorldRegionLocalDeclareResult::Raised);
  EXPECT_TRUE(ledger.ownsKind({0, 0}, WorldRegionLocalKind::LocalLua));
}

TEST(WorldRegionLocalStateTest, ForeignIdentityCannotMutateNeighborInventory) {
  WorldRegionOwnerRegistry owners(8, partition());
  ASSERT_EQ(owners.admit({0, 0}, WorldRegionMemberLevel::Tiles), WorldRegionOwnerAdmitResult::Created);
  ASSERT_EQ(owners.admit({2, 0}, WorldRegionMemberLevel::Tiles), WorldRegionOwnerAdmitResult::Created);
  auto origin = *owners.identity({0, 0});
  auto neighbor = *owners.identity({1, 0});
  WorldRegionLocalStateLedger ledger(owners);
  ASSERT_EQ(ledger.declareKinds(origin, tilesAndLiquids()), WorldRegionLocalDeclareResult::Declared);
  EXPECT_EQ(ledger.declareKinds(neighbor, tilesAndLiquids()), WorldRegionLocalDeclareResult::Declared);
  WorldRegionOwnerIdentity forged = origin;
  forged.region = {1, 0};
  EXPECT_EQ(ledger.declareKinds(forged, worldRegionLocalKindMask(WorldRegionLocalKind::Wires)),
      WorldRegionLocalDeclareResult::OwnerMismatch);
  EXPECT_FALSE(ledger.ownsKind({1, 0}, WorldRegionLocalKind::Wires));
}
