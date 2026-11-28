#include "StarWorldRegionAuthority.hpp"
#include "StarWorldRegionRuntime.hpp"
#include "StarGameTypes.hpp"

#include "gtest/gtest.h"

using namespace Star;

namespace {

WorldRegionPartition partition() {
  return WorldRegionPartition(WorldGeometry(256, 128));
}

}

TEST(WorldRegionAuthorityTest, HarnessOwnerIsNotAuthoritativeUntilPromoted) {
  WorldRegionOwnerRegistry owners(3, partition());
  WorldRegionAuthoritativeOwnerLedger authority(owners);
  ASSERT_EQ(owners.admit({0, 0}, WorldRegionMemberLevel::Tiles), WorldRegionOwnerAdmitResult::Created);
  EXPECT_EQ(WorldRegionAuthorityGrade::Harness, authority.grade({0, 0}));
  EXPECT_FALSE(authority.isAuthoritative({0, 0}));
  auto identity = *owners.identity({0, 0});
  EXPECT_EQ(authority.promote(identity), WorldRegionAuthorityPromoteResult::Promoted);
  EXPECT_TRUE(authority.isAuthoritative({0, 0}));
  EXPECT_EQ(1u, authority.authoritativeCount());
  auto record = authority.record({0, 0});
  ASSERT_TRUE(record);
  EXPECT_EQ(identity, record->owner);
  EXPECT_EQ(1u, record->authorityGeneration);
  EXPECT_EQ(authority.promote(identity), WorldRegionAuthorityPromoteResult::AlreadyAuthoritative);
}

TEST(WorldRegionAuthorityTest, ForeignIdentityCannotStealAuthority) {
  WorldRegionOwnerRegistry owners(3, partition());
  WorldRegionAuthoritativeOwnerLedger authority(owners);
  ASSERT_EQ(owners.admit({0, 0}, WorldRegionMemberLevel::Tiles), WorldRegionOwnerAdmitResult::Created);
  ASSERT_EQ(owners.admit({2, 0}, WorldRegionMemberLevel::Tiles), WorldRegionOwnerAdmitResult::Created);
  auto forged = *owners.identity({0, 0});
  forged.region = {1, 0};
  EXPECT_EQ(authority.promote(forged), WorldRegionAuthorityPromoteResult::OwnerMismatch);
  EXPECT_FALSE(authority.isAuthoritative({1, 0}));
}

TEST(WorldRegionAuthorityTest, DrainDemotesAuthoritativeOwner) {
  WorldRegionRuntimeCoordinator runtime(9, partition());
  ASSERT_EQ(runtime.noteInterest({0, 0}, WorldRegionMemberLevel::Entities), WorldRegionRuntimeInterestResult::Noted);
  EXPECT_EQ(runtime.promoteAuthoritative({0, 0}), WorldRegionAuthorityPromoteResult::Promoted);
  EXPECT_TRUE(runtime.isAuthoritative({0, 0}));
  EXPECT_EQ(runtime.releaseInterest({0, 0}), WorldRegionOwnerReleaseResult::DrainStarted);
  EXPECT_FALSE(runtime.isAuthoritative({0, 0}));
  EXPECT_EQ(0u, runtime.authoritativeOwnerCount());
}

TEST(WorldRegionAuthorityTest, AuthoritativeWritesRequirePromotion) {
  WorldRegionRuntimeCoordinator runtime(9, partition());
  ASSERT_EQ(runtime.noteInterest({0, 0}, WorldRegionMemberLevel::Tiles), WorldRegionRuntimeInterestResult::Noted);
  EXPECT_EQ(runtime.publishAuthoritativeBorder({0, 0}, {{{0, 0}, true, 1}}),
      WorldRegionBorderCaptureResult::NotAuthoritative);
  EXPECT_EQ(runtime.declareAuthoritativeLocalKinds({0, 0}, worldRegionLocalKindMask(WorldRegionLocalKind::Tiles)),
      WorldRegionLocalDeclareResult::NotAuthoritative);
  EXPECT_EQ(runtime.confirmAuthoritativeCheckpoint({0, 0}, 1), WorldRegionCheckpointNoteResult::NotAuthoritative);

  EXPECT_EQ(runtime.promoteAuthoritative({0, 0}), WorldRegionAuthorityPromoteResult::Promoted);
  EXPECT_EQ(runtime.publishAuthoritativeBorder({0, 0}, {{{0, 0}, true, 1}}), WorldRegionBorderCaptureResult::Captured);
  EXPECT_EQ(runtime.declareAuthoritativeLocalKinds({0, 0}, worldRegionLocalKindMask(WorldRegionLocalKind::Tiles)),
      WorldRegionLocalDeclareResult::Declared);
  EXPECT_EQ(runtime.confirmAuthoritativeCheckpoint({0, 0}, 1), WorldRegionCheckpointNoteResult::Noted);
  auto auth = runtime.authoritativeOwner({0, 0});
  ASSERT_TRUE(auth);
  EXPECT_EQ(Vec2U(0, 0), auth->region);
}
