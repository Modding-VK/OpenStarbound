#include "StarWorldRegionRuntime.hpp"

#include <limits>

namespace Star {

WorldRegionRuntimeCoordinator::WorldRegionRuntimeCoordinator(uint64_t worldEpoch, WorldRegionPartition partition,
    bool creationAllowed)
  : m_owners(worldEpoch, std::move(partition), creationAllowed), m_borders(m_owners), m_localState(m_owners),
    m_checkpoints(m_owners) {}

WorldRegionRuntimeInterestResult WorldRegionRuntimeCoordinator::noteInterest(Vec2U const& sector,
    WorldRegionMemberLevel level) {
  switch (m_owners.admit(sector, level)) {
    case WorldRegionOwnerAdmitResult::Created:
    case WorldRegionOwnerAdmitResult::MemberAdded:
    case WorldRegionOwnerAdmitResult::MemberRaised:
    case WorldRegionOwnerAdmitResult::Unchanged:
      return WorldRegionRuntimeInterestResult::Noted;
    case WorldRegionOwnerAdmitResult::IgnoredQueued:
      return WorldRegionRuntimeInterestResult::IgnoredQueued;
    case WorldRegionOwnerAdmitResult::CreationFailed:
      return WorldRegionRuntimeInterestResult::CreationFailed;
    case WorldRegionOwnerAdmitResult::UnknownRegion:
      return WorldRegionRuntimeInterestResult::UnknownRegion;
    case WorldRegionOwnerAdmitResult::RejectedWhileDraining:
      return WorldRegionRuntimeInterestResult::Rejected;
  }
  return WorldRegionRuntimeInterestResult::Rejected;
}

WorldRegionOwnerReleaseResult WorldRegionRuntimeCoordinator::releaseInterest(Vec2U const& sector) {
  return m_owners.release(sector);
}

WorldRegionOwnerReleaseResult WorldRegionRuntimeCoordinator::completeDrain(Vec2U const& region) {
  return m_owners.completeDrain(region);
}

bool WorldRegionRuntimeCoordinator::addPendingWork(Vec2U const& region, uint64_t units) {
  return m_owners.addPendingWork(region, units);
}

bool WorldRegionRuntimeCoordinator::clearPendingWork(Vec2U const& region, uint64_t units) {
  return m_owners.clearPendingWork(region, units);
}

WorldRegionBorderCaptureResult WorldRegionRuntimeCoordinator::publishBorder(Vec2U const& region,
    List<WorldRegionBorderCell> cells) {
  return m_borders.capture(region, m_clock, m_clock, std::move(cells));
}

Maybe<WorldRegionBorderSnapshot> WorldRegionRuntimeCoordinator::border(Vec2U const& region) const {
  return m_borders.latest(region);
}

Maybe<WorldRegionOwnerIdentity> WorldRegionRuntimeCoordinator::activeOwner(Vec2U const& region) const {
  if (m_owners.phase(region) != WorldRegionOwnerPhase::Active)
    return {};
  return m_owners.identity(region);
}

WorldRegionLocalDeclareResult WorldRegionRuntimeCoordinator::declareLocalKinds(Vec2U const& region, uint8_t kinds) {
  auto owner = activeOwner(region);
  if (!owner)
    return WorldRegionLocalDeclareResult::RegionAbsent;
  return m_localState.declareKinds(*owner, kinds);
}

Maybe<WorldRegionLocalInventorySnapshot> WorldRegionRuntimeCoordinator::localInventory(Vec2U const& region) const {
  return m_localState.snapshot(region);
}

WorldRegionCheckpointNoteResult WorldRegionRuntimeCoordinator::confirmCheckpoint(Vec2U const& region, uint64_t step) {
  auto owner = activeOwner(region);
  if (!owner)
    return WorldRegionCheckpointNoteResult::RegionAbsent;
  return m_checkpoints.noteConfirmed(*owner, step);
}

WorldRegionCheckpointAdvanceResult WorldRegionRuntimeCoordinator::advanceCheckpoint(uint64_t targetStep) {
  return m_checkpoints.tryAdvance(targetStep);
}

uint64_t WorldRegionRuntimeCoordinator::committedCheckpoint() const {
  return m_checkpoints.committedStep();
}

uint64_t WorldRegionRuntimeCoordinator::advanceClock() {
  if (m_clock != std::numeric_limits<uint64_t>::max())
    ++m_clock;
  return m_clock;
}

uint64_t WorldRegionRuntimeCoordinator::clock() const {
  return m_clock;
}

size_t WorldRegionRuntimeCoordinator::ownerCount() const {
  return m_owners.ownerCount();
}

WorldRegionOwnerRegistry const& WorldRegionRuntimeCoordinator::owners() const {
  return m_owners;
}

}
