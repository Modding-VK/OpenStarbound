#include "StarWorldRegionRuntime.hpp"
#include "StarGameTypes.hpp"

#include "gtest/gtest.h"

using namespace Star;

namespace {

WorldRegionPartition partition() {
  return WorldRegionPartition(WorldGeometry(256, 128));
}

}

TEST(WorldRegionThreadStubTest, StubRequiresAuthoritativeOwner) {
  WorldRegionRuntimeCoordinator runtime(4, partition());
  ASSERT_EQ(runtime.noteInterest({0, 0}, WorldRegionMemberLevel::Tiles), WorldRegionRuntimeInterestResult::Noted);
  EXPECT_EQ(runtime.startAuthoritativeThreadStub({0, 0}), WorldRegionThreadStubStartResult::NotAuthoritative);
  EXPECT_EQ(runtime.promoteAuthoritative({0, 0}), WorldRegionAuthorityPromoteResult::Promoted);
  EXPECT_EQ(runtime.startAuthoritativeThreadStub({0, 0}), WorldRegionThreadStubStartResult::Started);
  EXPECT_EQ(WorldRegionThreadStubPhase::Running, runtime.threadStubPhase({0, 0}));
  EXPECT_EQ(1u, runtime.runningThreadStubCount());
  EXPECT_EQ(runtime.startAuthoritativeThreadStub({0, 0}), WorldRegionThreadStubStartResult::AlreadyRunning);
}

TEST(WorldRegionThreadStubTest, CreationFailureBlocksStubWithoutAffectingAuthority) {
  WorldRegionRuntimeCoordinator runtime(4, partition());
  ASSERT_EQ(runtime.noteInterest({0, 0}, WorldRegionMemberLevel::Entities), WorldRegionRuntimeInterestResult::Noted);
  ASSERT_EQ(runtime.promoteAuthoritative({0, 0}), WorldRegionAuthorityPromoteResult::Promoted);
  runtime.setThreadStubCreationAllowed(false);
  EXPECT_EQ(runtime.startAuthoritativeThreadStub({0, 0}), WorldRegionThreadStubStartResult::CreationFailed);
  EXPECT_TRUE(runtime.isAuthoritative({0, 0}));
  EXPECT_EQ(WorldRegionThreadStubPhase::Absent, runtime.threadStubPhase({0, 0}));
}

TEST(WorldRegionThreadStubTest, JoinStopsStubAndDrainClearsIt) {
  WorldRegionRuntimeCoordinator runtime(4, partition());
  ASSERT_EQ(runtime.noteInterest({0, 0}, WorldRegionMemberLevel::Tiles), WorldRegionRuntimeInterestResult::Noted);
  ASSERT_EQ(runtime.promoteAuthoritative({0, 0}), WorldRegionAuthorityPromoteResult::Promoted);
  ASSERT_EQ(runtime.startAuthoritativeThreadStub({0, 0}), WorldRegionThreadStubStartResult::Started);
  EXPECT_EQ(runtime.beginAuthoritativeThreadJoin({0, 0}), WorldRegionThreadStubStopResult::Joining);
  EXPECT_EQ(WorldRegionThreadStubPhase::Joining, runtime.threadStubPhase({0, 0}));
  EXPECT_EQ(runtime.completeAuthoritativeThreadJoin({0, 0}), WorldRegionThreadStubStopResult::Stopped);
  EXPECT_EQ(WorldRegionThreadStubPhase::Stopped, runtime.threadStubPhase({0, 0}));

  ASSERT_EQ(runtime.startAuthoritativeThreadStub({0, 0}), WorldRegionThreadStubStartResult::Started);
  EXPECT_EQ(runtime.releaseInterest({0, 0}), WorldRegionOwnerReleaseResult::DrainStarted);
  EXPECT_FALSE(runtime.isAuthoritative({0, 0}));
  EXPECT_EQ(WorldRegionThreadStubPhase::Stopped, runtime.threadStubPhase({0, 0}));
  EXPECT_EQ(0u, runtime.runningThreadStubCount());
}

TEST(WorldRegionThreadStubTest, StubLifecycleDoesNotReplaceValueIdentity) {
  WorldRegionOwnerRegistry owners(4, partition());
  WorldRegionAuthoritativeOwnerLedger authority(owners);
  WorldRegionOwnerThreadStubLedger stubs(authority);
  ASSERT_EQ(owners.admit({0, 0}, WorldRegionMemberLevel::Tiles), WorldRegionOwnerAdmitResult::Created);
  auto identity = *owners.identity({0, 0});
  ASSERT_EQ(authority.promote(identity), WorldRegionAuthorityPromoteResult::Promoted);
  ASSERT_EQ(stubs.start(identity), WorldRegionThreadStubStartResult::Started);
  auto stub = stubs.record({0, 0});
  ASSERT_TRUE(stub);
  EXPECT_EQ(identity, stub->owner);
  EXPECT_NE(0u, stub->stubThreadId);
  // Address/thread id is not identity: same value identity after opaque id assigned.
  EXPECT_EQ(identity.worldEpoch, stub->owner.worldEpoch);
  EXPECT_EQ(identity.regionGeneration, stub->owner.regionGeneration);
}
