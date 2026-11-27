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
  EXPECT_EQ(runtime.beginAuthoritativeThreadJoin({0, 0}), WorldRegionThreadStubStopResult::Joining);
  EXPECT_EQ(runtime.completeAuthoritativeThreadJoin({0, 0}), WorldRegionThreadStubStopResult::Stopped);
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

TEST(WorldRegionThreadStubTest, RealOsThreadBurnsCpuWithoutWorldServer) {
  WorldRegionRuntimeCoordinator runtime(4, partition());
  ASSERT_EQ(runtime.noteInterest({0, 0}, WorldRegionMemberLevel::Tiles), WorldRegionRuntimeInterestResult::Noted);
  ASSERT_EQ(runtime.promoteAuthoritative({0, 0}), WorldRegionAuthorityPromoteResult::Promoted);
  ASSERT_EQ(runtime.startAuthoritativeThreadStub({0, 0}), WorldRegionThreadStubStartResult::Started);

  // Wait until the OS worker has executed at least one CPU tick loop.
  // Prefer short sleeps over pure yield: Windows CI under load may not
  // schedule the stub before 2000 yields complete.
  for (int i = 0; i < 500 && runtime.threadStubCpuTicks({0, 0}) == 0; ++i)
    Thread::sleep(1);
  EXPECT_GT(runtime.threadStubCpuTicks({0, 0}), 0u);

  EXPECT_EQ(runtime.beginAuthoritativeThreadJoin({0, 0}), WorldRegionThreadStubStopResult::Joining);
  EXPECT_EQ(runtime.completeAuthoritativeThreadJoin({0, 0}), WorldRegionThreadStubStopResult::Stopped);
  EXPECT_EQ(WorldRegionThreadStubPhase::Stopped, runtime.threadStubPhase({0, 0}));
  EXPECT_EQ(0u, runtime.runningThreadStubCount());
}

TEST(WorldRegionThreadStubTest, DrainJoinsRunningCpuStub) {
  WorldRegionRuntimeCoordinator runtime(4, partition());
  ASSERT_EQ(runtime.noteInterest({0, 0}, WorldRegionMemberLevel::Tiles), WorldRegionRuntimeInterestResult::Noted);
  ASSERT_EQ(runtime.promoteAuthoritative({0, 0}), WorldRegionAuthorityPromoteResult::Promoted);
  ASSERT_EQ(runtime.startAuthoritativeThreadStub({0, 0}), WorldRegionThreadStubStartResult::Started);
  for (int i = 0; i < 500 && runtime.threadStubCpuTicks({0, 0}) == 0; ++i)
    Thread::sleep(1);
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
  for (int i = 0; i < 500 && stubs.cpuTicks({0, 0}) == 0; ++i)
    Thread::sleep(1);
  auto stub = stubs.record({0, 0});
  ASSERT_TRUE(stub);
  EXPECT_EQ(identity, stub->owner);
  EXPECT_NE(0u, stub->stubThreadId);
  EXPECT_GT(stub->cpuTicks, 0u);
  EXPECT_EQ(stubs.beginJoin({0, 0}), WorldRegionThreadStubStopResult::Joining);
  EXPECT_EQ(stubs.completeJoin({0, 0}), WorldRegionThreadStubStopResult::Stopped);
}
