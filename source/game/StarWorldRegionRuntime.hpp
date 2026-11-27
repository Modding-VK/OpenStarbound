#pragma once

#include "StarWorldRegionBorder.hpp"
#include "StarWorldRegionCheckpoint.hpp"
#include "StarWorldRegionLocalState.hpp"
#include "StarWorldRegionOwner.hpp"

namespace Star {

enum class WorldRegionRuntimeInterestResult {
  Noted,
  IgnoredQueued,
  Rejected,
  CreationFailed,
  UnknownRegion
};

// WT-05 first harness: the central coordinator routes interest to region
// owners and exposes confirmed identities/borders. It does not publish tiles,
// entities, Lua, or network effects, and it does not run region simulations
// on a shared worker pool.
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

  WorldRegionBorderCaptureResult publishBorder(Vec2U const& region, List<WorldRegionBorderCell> cells);
  Maybe<WorldRegionBorderSnapshot> border(Vec2U const& region) const;
  Maybe<WorldRegionOwnerIdentity> activeOwner(Vec2U const& region) const;

  WorldRegionLocalDeclareResult declareLocalKinds(Vec2U const& region, uint8_t kinds);
  Maybe<WorldRegionLocalInventorySnapshot> localInventory(Vec2U const& region) const;
  WorldRegionCheckpointNoteResult confirmCheckpoint(Vec2U const& region, uint64_t step);
  WorldRegionCheckpointAdvanceResult advanceCheckpoint(uint64_t targetStep);
  uint64_t committedCheckpoint() const;

  uint64_t advanceClock();
  uint64_t clock() const;
  size_t ownerCount() const;
  WorldRegionOwnerRegistry const& owners() const;

private:
  WorldRegionOwnerRegistry m_owners;
  WorldRegionBorderLedger m_borders;
  WorldRegionLocalStateLedger m_localState;
  WorldRegionCheckpointBarrier m_checkpoints;
  uint64_t m_clock = 0;
};

}
