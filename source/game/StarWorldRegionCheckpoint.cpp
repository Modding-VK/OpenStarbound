#include "StarWorldRegionCheckpoint.hpp"

namespace Star {

WorldRegionCheckpointBarrier::WorldRegionCheckpointBarrier(WorldRegionOwnerRegistry const& owners)
  : m_owners(owners) {}

uint64_t WorldRegionCheckpointBarrier::committedStep() const {
  return m_committedStep;
}

WorldRegionCheckpointNoteResult WorldRegionCheckpointBarrier::noteConfirmed(
    WorldRegionOwnerIdentity const& owner, uint64_t step) {
  auto current = m_owners.identity(owner.region);
  if (!current)
    return WorldRegionCheckpointNoteResult::RegionAbsent;
  if (*current != owner)
    return WorldRegionCheckpointNoteResult::OwnerMismatch;
  if (m_owners.phase(owner.region) != WorldRegionOwnerPhase::Active)
    return WorldRegionCheckpointNoteResult::RegionDraining;
  if (step < m_committedStep)
    return WorldRegionCheckpointNoteResult::StepRegression;

  if (auto existing = m_confirmations.ptr(owner.region)) {
    if (step < existing->step)
      return WorldRegionCheckpointNoteResult::StepRegression;
    existing->owner = owner;
    existing->step = step;
    return WorldRegionCheckpointNoteResult::Noted;
  }

  m_confirmations[owner.region] = {owner, step};
  return WorldRegionCheckpointNoteResult::Noted;
}

List<Vec2U> WorldRegionCheckpointBarrier::laggingOwners(uint64_t targetStep) const {
  List<Vec2U> lagging;
  for (auto const& region : m_owners.activeRegions()) {
    auto confirmation = m_confirmations.ptr(region);
    if (!confirmation || confirmation->step < targetStep)
      lagging.append(region);
  }
  return lagging;
}

WorldRegionCheckpointAdvanceResult WorldRegionCheckpointBarrier::tryAdvance(uint64_t targetStep) {
  if (targetStep < m_committedStep)
    return WorldRegionCheckpointAdvanceResult::StepRegression;
  if (targetStep == m_committedStep)
    return WorldRegionCheckpointAdvanceResult::Advanced;

  auto active = m_owners.activeRegions();
  if (active.empty())
    return WorldRegionCheckpointAdvanceResult::NoActiveOwners;

  for (auto const& region : active) {
    auto confirmation = m_confirmations.ptr(region);
    if (!confirmation)
      return WorldRegionCheckpointAdvanceResult::WaitingOnOwners;
    auto identity = m_owners.identity(region);
    if (!identity || *identity != confirmation->owner)
      return WorldRegionCheckpointAdvanceResult::WaitingOnOwners;
    if (confirmation->step < targetStep)
      return WorldRegionCheckpointAdvanceResult::WaitingOnOwners;
  }

  m_committedStep = targetStep;
  return WorldRegionCheckpointAdvanceResult::Advanced;
}

}
