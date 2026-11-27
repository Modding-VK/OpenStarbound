#include "StarWorldRegionEntityHandoff.hpp"

namespace Star {

bool WorldRegionEntityId::operator==(WorldRegionEntityId const& rhs) const {
  return entityId == rhs.entityId && incarnation == rhs.incarnation;
}

bool WorldRegionEntityId::operator!=(WorldRegionEntityId const& rhs) const {
  return !(*this == rhs);
}

bool WorldRegionEntityId::operator<(WorldRegionEntityId const& rhs) const {
  if (entityId != rhs.entityId)
    return entityId < rhs.entityId;
  return incarnation < rhs.incarnation;
}

bool WorldRegionEntityHandoffId::operator==(WorldRegionEntityHandoffId const& rhs) const {
  return worldEpoch == rhs.worldEpoch && sequence == rhs.sequence;
}

bool WorldRegionEntityHandoffId::operator<(WorldRegionEntityHandoffId const& rhs) const {
  if (worldEpoch != rhs.worldEpoch)
    return worldEpoch < rhs.worldEpoch;
  return sequence < rhs.sequence;
}

WorldRegionEntityHandoffShadow::WorldRegionEntityHandoffShadow(uint64_t worldEpoch, WorldRegionPartition partition)
  : m_worldEpoch(worldEpoch), m_partition(std::move(partition)) {}

bool WorldRegionEntityHandoffShadow::seedOwner(WorldRegionEntityOwner owner) {
  if (!owner.entity.entityId || !owner.entity.incarnation)
    return false;
  if (!m_partition.regionBounds(owner.region))
    return false;
  if (m_owners.contains(owner.entity))
    return false;
  if (!owner.ownerGeneration)
    owner.ownerGeneration = 1;
  m_owners[owner.entity] = owner;
  return true;
}

Maybe<WorldRegionEntityHandoffId> WorldRegionEntityHandoffShadow::prepare(uint64_t logicalStep,
    WorldRegionEntityId const& entity, Vec2U const& destinationRegion,
    Maybe<WorldRegionCrossIntentId> crossIntent) {
  if (!logicalStep || !entity.entityId || !entity.incarnation)
    return {};
  if (!m_partition.regionBounds(destinationRegion))
    return {};

  auto ownerIt = m_owners.find(entity);
  if (ownerIt == m_owners.end())
    return {};
  if (ownerIt->second.region == destinationRegion)
    return {};

  auto committed = m_lastCommittedStep.find(entity);
  if (committed != m_lastCommittedStep.end() && committed->second == logicalStep)
    return {};

  for (auto const& entry : m_handoffs) {
    if (entry.second.entity == entity && entry.second.state == WorldRegionEntityHandoffState::Prepared)
      return {};
  }

  if (m_sequence + 1 < m_sequence)
    return {};

  WorldRegionEntityHandoff handoff;
  handoff.id = {m_worldEpoch, ++m_sequence};
  handoff.logicalStep = logicalStep;
  handoff.entity = entity;
  handoff.sourceRegion = ownerIt->second.region;
  handoff.destinationRegion = destinationRegion;
  handoff.expectedOwnerGeneration = ownerIt->second.ownerGeneration;
  handoff.state = WorldRegionEntityHandoffState::Prepared;
  handoff.crossIntent = std::move(crossIntent);
  m_handoffs[handoff.id] = handoff;
  return handoff.id;
}

WorldRegionEntityHandoffOutcome WorldRegionEntityHandoffShadow::commit(WorldRegionEntityHandoffId const& id) {
  auto it = m_handoffs.find(id);
  if (it == m_handoffs.end())
    return WorldRegionEntityHandoffOutcome::Unknown;
  if (it->second.state != WorldRegionEntityHandoffState::Prepared)
    return WorldRegionEntityHandoffOutcome::Invalid;

  auto ownerIt = m_owners.find(it->second.entity);
  if (ownerIt == m_owners.end())
    return WorldRegionEntityHandoffOutcome::Stale;
  if (ownerIt->second.region != it->second.sourceRegion
      || ownerIt->second.ownerGeneration != it->second.expectedOwnerGeneration) {
    it->second.state = WorldRegionEntityHandoffState::Stale;
    return WorldRegionEntityHandoffOutcome::Stale;
  }

  auto committed = m_lastCommittedStep.find(it->second.entity);
  if (committed != m_lastCommittedStep.end() && committed->second == it->second.logicalStep) {
    it->second.state = WorldRegionEntityHandoffState::Stale;
    return WorldRegionEntityHandoffOutcome::Duplicate;
  }

  ownerIt->second.region = it->second.destinationRegion;
  ++ownerIt->second.ownerGeneration;
  m_lastCommittedStep[it->second.entity] = it->second.logicalStep;
  it->second.state = WorldRegionEntityHandoffState::Committed;
  return WorldRegionEntityHandoffOutcome::Accepted;
}

WorldRegionEntityHandoffOutcome WorldRegionEntityHandoffShadow::reject(WorldRegionEntityHandoffId const& id) {
  auto it = m_handoffs.find(id);
  if (it == m_handoffs.end())
    return WorldRegionEntityHandoffOutcome::Unknown;
  if (it->second.state != WorldRegionEntityHandoffState::Prepared)
    return WorldRegionEntityHandoffOutcome::Invalid;
  it->second.state = WorldRegionEntityHandoffState::Rejected;
  return WorldRegionEntityHandoffOutcome::Accepted;
}

Maybe<WorldRegionEntityOwner> WorldRegionEntityHandoffShadow::owner(WorldRegionEntityId const& entity) const {
  auto it = m_owners.find(entity);
  if (it == m_owners.end())
    return {};
  return it->second;
}

Maybe<WorldRegionEntityHandoff> WorldRegionEntityHandoffShadow::handoff(WorldRegionEntityHandoffId const& id) const {
  auto it = m_handoffs.find(id);
  if (it == m_handoffs.end())
    return {};
  return it->second;
}

size_t WorldRegionEntityHandoffShadow::size() const {
  return m_handoffs.size();
}

bool WorldRegionEntityHandoffShadow::bumpOwnerGeneration(WorldRegionEntityId const& entity) {
  auto it = m_owners.find(entity);
  if (it == m_owners.end())
    return false;
  ++it->second.ownerGeneration;
  return true;
}

}
