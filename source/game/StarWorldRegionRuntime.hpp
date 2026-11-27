#pragma once

#include "StarWorldRegionAuthority.hpp"
#include "StarWorldRegionBorder.hpp"
#include "StarWorldRegionCheckpoint.hpp"
#include "StarWorldRegionLocalState.hpp"
#include "StarWorldRegionOwner.hpp"
#include "StarWorldRegionThreadStub.hpp"

namespace Star {

enum class WorldRegionRuntimeInterestResult {
  Noted,
  IgnoredQueued,
  Rejected,
  CreationFailed,
  UnknownRegion
};

// WT-05 coordinator: routes interest to region owners, can promote exclusive
// authoritative write ownership (value identity only), and exposes confirmed
// borders/checkpoints. Does not start OS threads or publish live simulation.
class WorldRegionRuntimeCoordinator {
public:
  WorldRegionRuntimeCoordinator(uint64_t worldEpoch, WorldRegionPartition partition, bool creationAllowed = true);
  WorldRegionRuntimeCoordinator(WorldRegionRuntimeCoordinator const&) = delete;
  WorldRegionRuntimeCoordinator& operator=(WorldRegionRuntimeCoordinator const&) = delete;

  WorldRegionRuntimeInterestResult noteInterest(Vec2U const& sector, WorldRegionMemberLevel level);
  WorldRegionOwnerReleaseResult releaseInterest(Vec2U const& sector);
  WorldRegionOwnerReleaseResult completeDrain(Vec2U const& region);

  bool addPendingWork(Vec2U const& region, uint64_t units = 1);
  bool clearPendingWork(Vec2U const& region, uint64_t units = 1);

  WorldRegionAuthorityPromoteResult promoteAuthoritative(Vec2U const& region);
  WorldRegionAuthorityDemoteResult demoteAuthoritative(Vec2U const& region);
  bool isAuthoritative(Vec2U const& region) const;
  Maybe<WorldRegionOwnerIdentity> authoritativeOwner(Vec2U const& region) const;
  WorldRegionAuthorityGrade authorityGrade(Vec2U const& region) const;

  WorldRegionThreadStubStartResult startAuthoritativeThreadStub(Vec2U const& region);
  WorldRegionThreadStubStopResult beginAuthoritativeThreadJoin(Vec2U const& region);
  WorldRegionThreadStubStopResult completeAuthoritativeThreadJoin(Vec2U const& region);
  WorldRegionThreadStubPhase threadStubPhase(Vec2U const& region) const;
  uint64_t threadStubCpuTicks(Vec2U const& region) const;
  void setThreadStubCreationAllowed(bool allowed);

  WorldRegionBorderCaptureResult publishBorder(Vec2U const& region, List<WorldRegionBorderCell> cells);
  WorldRegionBorderCaptureResult publishAuthoritativeBorder(Vec2U const& region, List<WorldRegionBorderCell> cells);
  WorldRegionBorderCaptureResult publishPerimeterSkeleton(Vec2U const& region);
  Maybe<WorldRegionBorderSnapshot> border(Vec2U const& region) const;
  Maybe<WorldRegionOwnerIdentity> activeOwner(Vec2U const& region) const;
  size_t retireAllOwners();

  WorldRegionLocalDeclareResult declareLocalKinds(Vec2U const& region, uint8_t kinds);
  WorldRegionLocalDeclareResult declareAuthoritativeLocalKinds(Vec2U const& region, uint8_t kinds);
  Maybe<WorldRegionLocalInventorySnapshot> localInventory(Vec2U const& region) const;
  WorldRegionCheckpointNoteResult confirmCheckpoint(Vec2U const& region, uint64_t step);
  WorldRegionCheckpointNoteResult confirmAuthoritativeCheckpoint(Vec2U const& region, uint64_t step);
  WorldRegionCheckpointAdvanceResult advanceCheckpoint(uint64_t targetStep);
  uint64_t committedCheckpoint() const;

  uint64_t advanceClock();
  uint64_t clock() const;
  size_t ownerCount() const;
  size_t authoritativeOwnerCount() const;
  size_t runningThreadStubCount() const;
  WorldRegionOwnerRegistry const& owners() const;
  WorldRegionAuthoritativeOwnerLedger const& authority() const;

private:
  WorldRegionOwnerRegistry m_owners;
  WorldRegionBorderLedger m_borders;
  WorldRegionLocalStateLedger m_localState;
  WorldRegionCheckpointBarrier m_checkpoints;
  WorldRegionAuthoritativeOwnerLedger m_authority;
  WorldRegionOwnerThreadStubLedger m_threadStubs;
  uint64_t m_clock = 0;
};

}
