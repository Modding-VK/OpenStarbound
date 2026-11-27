#include "StarWorldRegionAuthoritativeOps.hpp"
#include "StarGameTypes.hpp"

#include "gtest/gtest.h"

using namespace Star;

namespace {

WorldRegionPartition partition() {
  return WorldRegionPartition(WorldGeometry(256, 128));
}

List<Vec2U> borderFootprint() {
  return {{1, 0}, {2, 0}};
}

WorldRegionEntityId entity() {
  return {7, 1};
}

void promote(WorldRegionOwnerRegistry& owners, WorldRegionAuthoritativeOwnerLedger& authority, Vec2U sector) {
  ASSERT_EQ(owners.admit(sector, WorldRegionMemberLevel::Tiles), WorldRegionOwnerAdmitResult::Created);
  auto identity = owners.identityForSector(sector);
  ASSERT_TRUE(identity);
  ASSERT_EQ(authority.promote(*identity), WorldRegionAuthorityPromoteResult::Promoted);
}

}

TEST(WorldRegionAuthoritativeOpsTest, HarnessAloneCannotProposeCrossOp) {
  WorldRegionOwnerRegistry owners(5, partition());
  WorldRegionAuthoritativeOwnerLedger authority(owners);
  WorldRegionAuthoritativeOps ops(5, partition(), owners, authority);

  ASSERT_EQ(owners.admit({0, 0}, WorldRegionMemberLevel::Tiles), WorldRegionOwnerAdmitResult::Created);
  ASSERT_EQ(owners.admit({2, 0}, WorldRegionMemberLevel::Tiles), WorldRegionOwnerAdmitResult::Created);
  EXPECT_EQ(WorldRegionAuthorityGrade::Harness, authority.grade({0, 0}));

  Maybe<WorldRegionCrossIntentId> id;
  EXPECT_EQ(ops.proposeCrossOp(1, {0, 0}, {{1, 0}}, borderFootprint(), WorldRegionCrossIntentKind::BorderPublish, id),
      WorldRegionAuthoritativeGateResult::SourceNotAuthoritative);
  EXPECT_FALSE(id);
}

TEST(WorldRegionAuthoritativeOpsTest, AuthoritativeSourceAndTargetAllowCrossOp) {
  WorldRegionOwnerRegistry owners(5, partition());
  WorldRegionAuthoritativeOwnerLedger authority(owners);
  WorldRegionAuthoritativeOps ops(5, partition(), owners, authority);

  promote(owners, authority, {0, 0});
  promote(owners, authority, {2, 0});
  EXPECT_TRUE(authority.isAuthoritative({0, 0}));
  EXPECT_TRUE(authority.isAuthoritative({1, 0}));

  Maybe<WorldRegionCrossIntentId> id;
  EXPECT_EQ(ops.proposeCrossOp(1, {0, 0}, {{1, 0}}, borderFootprint(), WorldRegionCrossIntentKind::BorderPublish, id),
      WorldRegionAuthoritativeGateResult::Allowed);
  ASSERT_TRUE(id);
  EXPECT_EQ(ops.crossOps().reserve(*id), WorldRegionCrossOpOutcome::Accepted);
}

TEST(WorldRegionAuthoritativeOpsTest, HandoffRequiresAuthoritativeSourceAndDestination) {
  WorldRegionOwnerRegistry owners(5, partition());
  WorldRegionAuthoritativeOwnerLedger authority(owners);
  WorldRegionAuthoritativeOps ops(5, partition(), owners, authority);

  promote(owners, authority, {0, 0});
  ASSERT_TRUE(ops.handoffs().seedOwner({entity(), {0, 0}, 1}));

  Maybe<WorldRegionEntityHandoffId> blocked;
  EXPECT_EQ(ops.prepareHandoff(1, entity(), {1, 0}, blocked),
      WorldRegionAuthoritativeGateResult::DestinationNotAuthoritative);
  EXPECT_FALSE(blocked);

  promote(owners, authority, {2, 0});
  Maybe<WorldRegionEntityHandoffId> id;
  EXPECT_EQ(ops.prepareHandoff(1, entity(), {1, 0}, id), WorldRegionAuthoritativeGateResult::Allowed);
  ASSERT_TRUE(id);
  EXPECT_EQ(ops.handoffs().commit(*id), WorldRegionEntityHandoffOutcome::Accepted);
}

TEST(WorldRegionAuthoritativeOpsTest, RecoveryNoteRequiresAuthoritativeRegion) {
  WorldRegionOwnerRegistry owners(5, partition());
  WorldRegionAuthoritativeOwnerLedger authority(owners);
  WorldRegionAuthoritativeOps ops(5, partition(), owners, authority);

  EXPECT_EQ(ops.noteUnconfirmed(1, {0, 0}, 9), WorldRegionAuthoritativeGateResult::RegionNotAuthoritative);
  promote(owners, authority, {0, 0});
  EXPECT_EQ(ops.noteUnconfirmed(1, {0, 0}, 9), WorldRegionAuthoritativeGateResult::Allowed);
  EXPECT_EQ(1u, ops.recovery().unconfirmedCount(1, {0, 0}));
}
