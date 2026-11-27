#pragma once

#include "StarList.hpp"
#include "StarMap.hpp"
#include "StarMaybe.hpp"
#include "StarWorldRegionOwner.hpp"

namespace Star {

// IO-03 regional first harness: a common logical step barrier. One owner cannot
// advance the shared checkpoint while another active owner still lags on an
// earlier step. Does not write .world storage.
struct WorldRegionCheckpointConfirmation {
  WorldRegionOwnerIdentity owner;
  uint64_t step = 0;
};

enum class WorldRegionCheckpointNoteResult {
  Noted,
  RegionAbsent,
  RegionDraining,
  OwnerMismatch,
  StepRegression
};

enum class WorldRegionCheckpointAdvanceResult {
  Advanced,
  WaitingOnOwners,
  NoActiveOwners,
  StepRegression
};

class WorldRegionCheckpointBarrier {
public:
  explicit WorldRegionCheckpointBarrier(WorldRegionOwnerRegistry const& owners);

  WorldRegionCheckpointNoteResult noteConfirmed(WorldRegionOwnerIdentity const& owner, uint64_t step);
  WorldRegionCheckpointAdvanceResult tryAdvance(uint64_t targetStep);
  uint64_t committedStep() const;
  List<Vec2U> laggingOwners(uint64_t targetStep) const;

private:
  WorldRegionOwnerRegistry const& m_owners;
  HashMap<Vec2U, WorldRegionCheckpointConfirmation> m_confirmations;
  uint64_t m_committedStep = 0;
};

}
