#include "StarWorldRegionRuntime.hpp"

#include <limits>

namespace Star {

WorldRegionRuntimeCoordinator::WorldRegionRuntimeCoordinator(uint64_t worldEpoch, WorldRegionPartition partition,
    bool creationAllowed)
  : m_owners(worldEpoch, std::move(partition), creationAllowed), m_borders(m_owners), m_localState(m_owners),
    m_checkpoints(m_owners), m_authority(m_owners), m_threadStubs(m_authority, creationAllowed) {}

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
  auto region = m_owners.partition().regionForSector(sector);
  auto result = m_owners.release(sector);
  if (region && (result == WorldRegionOwnerReleaseResult::DrainStarted
          || result == WorldRegionOwnerReleaseResult::OwnerRetired)) {
    m_threadStubs.beginJoin(*region);
    m_threadStubs.completeJoin(*region);
    m_authority.demote(*region);
  }
  return result;
}

WorldRegionOwnerReleaseResult WorldRegionRuntimeCoordinator::completeDrain(Vec2U const& region) {
  auto result = m_owners.completeDrain(region);
  if (result == WorldRegionOwnerReleaseResult::OwnerRetired) {
    m_threadStubs.beginJoin(region);
    m_threadStubs.completeJoin(region);
    m_authority.demote(region);
  }
  return result;
}

bool WorldRegionRuntimeCoordinator::addPendingWork(Vec2U const& region, uint64_t units) {
  return m_owners.addPendingWork(region, units);
}

bool WorldRegionRuntimeCoordinator::clearPendingWork(Vec2U const& region, uint64_t units) {
  return m_owners.clearPendingWork(region, units);
}

WorldRegionAuthorityPromoteResult WorldRegionRuntimeCoordinator::promoteAuthoritative(Vec2U const& region) {
  auto owner = activeOwner(region);
  if (!owner)
    return WorldRegionAuthorityPromoteResult::RegionAbsent;
  return m_authority.promote(*owner);
}

WorldRegionAuthorityDemoteResult WorldRegionRuntimeCoordinator::demoteAuthoritative(Vec2U const& region) {
  m_threadStubs.beginJoin(region);
  m_threadStubs.completeJoin(region);
  return m_authority.demote(region);
}

WorldRegionThreadStubStartResult WorldRegionRuntimeCoordinator::startAuthoritativeThreadStub(Vec2U const& region) {
  auto owner = authoritativeOwner(region);
  if (!owner)
    return WorldRegionThreadStubStartResult::NotAuthoritative;
  return m_threadStubs.start(*owner);
}

WorldRegionThreadStubStopResult WorldRegionRuntimeCoordinator::beginAuthoritativeThreadJoin(Vec2U const& region) {
  return m_threadStubs.beginJoin(region);
}

WorldRegionThreadStubStopResult WorldRegionRuntimeCoordinator::completeAuthoritativeThreadJoin(Vec2U const& region) {
  return m_threadStubs.completeJoin(region);
}

WorldRegionThreadStubPhase WorldRegionRuntimeCoordinator::threadStubPhase(Vec2U const& region) const {
  return m_threadStubs.phase(region);
}

uint64_t WorldRegionRuntimeCoordinator::threadStubCpuTicks(Vec2U const& region) const {
  return m_threadStubs.cpuTicks(region);
}

void WorldRegionRuntimeCoordinator::setThreadStubCreationAllowed(bool allowed) {
  m_threadStubs.setCreationAllowed(allowed);
}

bool WorldRegionRuntimeCoordinator::isAuthoritative(Vec2U const& region) const {
  return m_authority.isAuthoritative(region);
}

Maybe<WorldRegionOwnerIdentity> WorldRegionRuntimeCoordinator::authoritativeOwner(Vec2U const& region) const {
  return m_authority.authoritativeIdentity(region);
}

WorldRegionAuthorityGrade WorldRegionRuntimeCoordinator::authorityGrade(Vec2U const& region) const {
  return m_authority.grade(region);
}

WorldRegionBorderCaptureResult WorldRegionRuntimeCoordinator::publishBorder(Vec2U const& region,
    List<WorldRegionBorderCell> cells) {
  return m_borders.capture(region, m_clock, m_clock, std::move(cells));
}

WorldRegionBorderCaptureResult WorldRegionRuntimeCoordinator::publishAuthoritativeBorder(Vec2U const& region,
    List<WorldRegionBorderCell> cells) {
  if (!m_authority.isAuthoritative(region))
    return WorldRegionBorderCaptureResult::NotAuthoritative;
  return publishBorder(region, std::move(cells));
}

WorldRegionBorderCaptureResult WorldRegionRuntimeCoordinator::publishPerimeterSkeleton(Vec2U const& region) {
  auto positions = WorldRegionBorderOutline::perimeterPositions(m_owners.partition(), region);
  if (!positions)
    return WorldRegionBorderCaptureResult::RegionAbsent;
  return publishBorder(region, WorldRegionBorderOutline::unavailableSkeleton(*positions));
}

Maybe<WorldRegionBorderSnapshot> WorldRegionRuntimeCoordinator::border(Vec2U const& region) const {
  return m_borders.latest(region);
}

size_t WorldRegionRuntimeCoordinator::retireAllOwners() {
  m_threadStubs.clear();
  m_authority.clear();
  return m_owners.retireAll();
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

WorldRegionLocalDeclareResult WorldRegionRuntimeCoordinator::declareAuthoritativeLocalKinds(Vec2U const& region,
    uint8_t kinds) {
  if (!m_authority.isAuthoritative(region))
    return WorldRegionLocalDeclareResult::NotAuthoritative;
  return declareLocalKinds(region, kinds);
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

WorldRegionCheckpointNoteResult WorldRegionRuntimeCoordinator::confirmAuthoritativeCheckpoint(Vec2U const& region,
    uint64_t step) {
  if (!m_authority.isAuthoritative(region))
    return WorldRegionCheckpointNoteResult::NotAuthoritative;
  return confirmCheckpoint(region, step);
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

size_t WorldRegionRuntimeCoordinator::authoritativeOwnerCount() const {
  return m_authority.authoritativeCount();
}

size_t WorldRegionRuntimeCoordinator::runningThreadStubCount() const {
  return m_threadStubs.runningCount();
}

WorldRegionOwnerRegistry const& WorldRegionRuntimeCoordinator::owners() const {
  return m_owners;
}

WorldRegionAuthoritativeOwnerLedger const& WorldRegionRuntimeCoordinator::authority() const {
  return m_authority;
}

}
