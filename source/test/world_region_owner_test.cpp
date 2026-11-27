#include "StarWorldRegionOwner.hpp"
#include "StarGameTypes.hpp"

#include "gtest/gtest.h"

using namespace Star;

namespace {

WorldRegionPartition partition() {
  return WorldRegionPartition(WorldGeometry(256, 128));
}

}

TEST(WorldRegionOwnerTest, QueuedMembershipDoesNotCreateOwner) {
  WorldRegionOwnerRegistry registry(9, partition());
  EXPECT_EQ(registry.admit({0, 0}, WorldRegionMemberLevel::Queued), WorldRegionOwnerAdmitResult::IgnoredQueued);
  EXPECT_EQ(0u, registry.ownerCount());
  EXPECT_FALSE(registry.identity({0, 0}));
  EXPECT_EQ(WorldRegionOwnerPhase::Absent, registry.phase({0, 0}));
}

TEST(WorldRegionOwnerTest, FirstTilesMembershipCreatesStableIdentity) {
  WorldRegionOwnerRegistry registry(9, partition());
  EXPECT_EQ(registry.admit({0, 0}, WorldRegionMemberLevel::Tiles), WorldRegionOwnerAdmitResult::Created);
  auto identity = registry.identity({0, 0});
  ASSERT_TRUE(identity);
  EXPECT_EQ(9u, identity->worldEpoch);
  EXPECT_EQ(1u, identity->regionGeneration);
  EXPECT_EQ(Vec2U(0, 0), identity->region);
  EXPECT_EQ(WorldRegionOwnerPhase::Active, registry.phase({0, 0}));
  EXPECT_EQ(1u, registry.memberCount({0, 0}));

  WorldRegionOwnerIdentity copy = *identity;
  EXPECT_EQ(copy, *identity);
  auto bySector = registry.identityForSector({0, 0});
  ASSERT_TRUE(bySector);
  EXPECT_EQ(*identity, *bySector);
  ASSERT_TRUE(registry.memberLevel({0, 0}));
  EXPECT_EQ(WorldRegionMemberLevel::Tiles, *registry.memberLevel({0, 0}));
}

TEST(WorldRegionOwnerTest, SameRegionSectorsShareOneOwnerGeneration) {
  WorldRegionOwnerRegistry registry(3, partition());
  EXPECT_EQ(registry.admit({0, 0}, WorldRegionMemberLevel::Tiles), WorldRegionOwnerAdmitResult::Created);
  EXPECT_EQ(registry.admit({1, 0}, WorldRegionMemberLevel::Entities), WorldRegionOwnerAdmitResult::MemberAdded);
  EXPECT_EQ(1u, registry.ownerCount());
  EXPECT_EQ(2u, registry.memberCount({0, 0}));
  auto identity = registry.identity({0, 0});
  ASSERT_TRUE(identity);
  EXPECT_EQ(1u, identity->regionGeneration);
  EXPECT_EQ(registry.admit({0, 0}, WorldRegionMemberLevel::Entities), WorldRegionOwnerAdmitResult::MemberRaised);
  EXPECT_EQ(registry.admit({0, 0}, WorldRegionMemberLevel::Entities), WorldRegionOwnerAdmitResult::Unchanged);
  EXPECT_EQ(2u, registry.members({0, 0}).size());
}

TEST(WorldRegionOwnerTest, LastUnloadDrainsThenRetiresWithNewGenerationOnReload) {
  WorldRegionOwnerRegistry registry(11, partition());
  EXPECT_EQ(registry.admit({0, 0}, WorldRegionMemberLevel::Tiles), WorldRegionOwnerAdmitResult::Created);
  EXPECT_EQ(registry.admit({1, 0}, WorldRegionMemberLevel::Tiles), WorldRegionOwnerAdmitResult::MemberAdded);
  auto first = registry.identity({0, 0});
  ASSERT_TRUE(first);

  EXPECT_EQ(registry.release({0, 0}), WorldRegionOwnerReleaseResult::MemberRemoved);
  EXPECT_EQ(WorldRegionOwnerPhase::Active, registry.phase({0, 0}));
  EXPECT_EQ(registry.release({1, 0}), WorldRegionOwnerReleaseResult::DrainStarted);
  EXPECT_EQ(WorldRegionOwnerPhase::Draining, registry.phase({0, 0}));
  EXPECT_EQ(registry.admit({0, 0}, WorldRegionMemberLevel::Tiles),
      WorldRegionOwnerAdmitResult::RejectedWhileDraining);

  EXPECT_EQ(registry.completeDrain({0, 0}), WorldRegionOwnerReleaseResult::OwnerRetired);
  EXPECT_EQ(WorldRegionOwnerPhase::Absent, registry.phase({0, 0}));
  EXPECT_EQ(0u, registry.ownerCount());

  EXPECT_EQ(registry.admit({0, 0}, WorldRegionMemberLevel::Entities), WorldRegionOwnerAdmitResult::Created);
  auto second = registry.identity({0, 0});
  ASSERT_TRUE(second);
  EXPECT_EQ(first->worldEpoch, second->worldEpoch);
  EXPECT_EQ(first->region, second->region);
  EXPECT_NE(first->regionGeneration, second->regionGeneration);
  EXPECT_EQ(2u, second->regionGeneration);
}

TEST(WorldRegionOwnerTest, PendingWorkBlocksDrainRetirement) {
  WorldRegionOwnerRegistry registry(4, partition());
  EXPECT_EQ(registry.admit({0, 0}, WorldRegionMemberLevel::Tiles), WorldRegionOwnerAdmitResult::Created);
  EXPECT_TRUE(registry.addPendingWork({0, 0}, 2));
  EXPECT_EQ(2u, registry.pendingWork({0, 0}));
  EXPECT_EQ(registry.release({0, 0}), WorldRegionOwnerReleaseResult::DrainStarted);
  EXPECT_EQ(registry.completeDrain({0, 0}), WorldRegionOwnerReleaseResult::PendingWork);
  EXPECT_EQ(WorldRegionOwnerPhase::Draining, registry.phase({0, 0}));
  EXPECT_TRUE(registry.clearPendingWork({0, 0}, 2));
  EXPECT_EQ(registry.completeDrain({0, 0}), WorldRegionOwnerReleaseResult::OwnerRetired);
  EXPECT_EQ(0u, registry.ownerCount());
}

TEST(WorldRegionOwnerTest, CreationFailureBlocksRegionalActivation) {
  WorldRegionOwnerRegistry registry(5, partition(), false);
  EXPECT_FALSE(registry.creationAllowed());
  EXPECT_EQ(registry.admit({2, 0}, WorldRegionMemberLevel::Tiles), WorldRegionOwnerAdmitResult::CreationFailed);
  EXPECT_EQ(0u, registry.ownerCount());
  registry.setCreationAllowed(true);
  EXPECT_EQ(registry.admit({2, 0}, WorldRegionMemberLevel::Tiles), WorldRegionOwnerAdmitResult::Created);
  EXPECT_EQ(Vec2U(1, 0), registry.identity({1, 0})->region);
}

TEST(WorldRegionOwnerTest, UnknownSectorAndForeignReleaseStayRejected) {
  WorldRegionOwnerRegistry registry(1, WorldRegionPartition(WorldGeometry(64, 64)));
  EXPECT_EQ(registry.admit({99, 99}, WorldRegionMemberLevel::Tiles), WorldRegionOwnerAdmitResult::UnknownRegion);
  EXPECT_EQ(registry.release({0, 0}), WorldRegionOwnerReleaseResult::Foreign);
  EXPECT_EQ(registry.completeDrain({0, 0}), WorldRegionOwnerReleaseResult::Foreign);
  EXPECT_EQ(registry.admit({0, 0}, WorldRegionMemberLevel::Tiles), WorldRegionOwnerAdmitResult::Created);
  EXPECT_EQ(registry.release({1, 0}), WorldRegionOwnerReleaseResult::NotMember);
}
