#include "StarWorldRegionOwner.hpp"

#include <limits>

namespace Star {

bool WorldRegionOwnerIdentity::operator==(WorldRegionOwnerIdentity const& rhs) const {
  return worldEpoch == rhs.worldEpoch && regionGeneration == rhs.regionGeneration && region == rhs.region;
}

bool WorldRegionOwnerIdentity::operator!=(WorldRegionOwnerIdentity const& rhs) const {
  return !(*this == rhs);
}

WorldRegionOwnerRegistry::WorldRegionOwnerRegistry(uint64_t worldEpoch, WorldRegionPartition partition,
    bool creationAllowed)
  : m_worldEpoch(worldEpoch), m_partition(std::move(partition)), m_creationAllowed(creationAllowed) {}

void WorldRegionOwnerRegistry::setCreationAllowed(bool allowed) {
  m_creationAllowed = allowed;
}

bool WorldRegionOwnerRegistry::creationAllowed() const {
  return m_creationAllowed;
}

size_t WorldRegionOwnerRegistry::ownerCount() const {
  return m_owners.size();
}

WorldRegionOwnerPhase WorldRegionOwnerRegistry::phase(Vec2U const& region) const {
  if (auto owner = m_owners.ptr(region))
    return owner->phase;
  return WorldRegionOwnerPhase::Absent;
}

size_t WorldRegionOwnerRegistry::memberCount(Vec2U const& region) const {
  if (auto owner = m_owners.ptr(region))
    return owner->members.size();
  return 0;
}

Maybe<WorldRegionOwnerIdentity> WorldRegionOwnerRegistry::identity(Vec2U const& region) const {
  if (auto owner = m_owners.ptr(region))
    return owner->identity;
  return {};
}

WorldRegionOwnerAdmitResult WorldRegionOwnerRegistry::admit(Vec2U const& sector, WorldRegionMemberLevel level) {
  auto region = m_partition.regionForSector(sector);
  if (!region)
    return WorldRegionOwnerAdmitResult::UnknownRegion;

  if (level == WorldRegionMemberLevel::None)
    return WorldRegionOwnerAdmitResult::Unchanged;

  // Queued preparation is not an authoritative load. REG-01 forbids minting a
  // region owner from enqueue/prefetch alone.
  if (level == WorldRegionMemberLevel::Queued)
    return WorldRegionOwnerAdmitResult::IgnoredQueued;

  if (auto owner = m_owners.ptr(*region)) {
    if (owner->phase == WorldRegionOwnerPhase::Draining)
      return WorldRegionOwnerAdmitResult::RejectedWhileDraining;
    if (auto existing = owner->members.ptr(sector)) {
      if (*existing == level)
        return WorldRegionOwnerAdmitResult::Unchanged;
      if (static_cast<uint8_t>(level) > static_cast<uint8_t>(*existing)) {
        *existing = level;
        return WorldRegionOwnerAdmitResult::MemberRaised;
      }
      return WorldRegionOwnerAdmitResult::Unchanged;
    }
    owner->members[sector] = level;
    return WorldRegionOwnerAdmitResult::MemberAdded;
  }

  if (!m_creationAllowed)
    return WorldRegionOwnerAdmitResult::CreationFailed;
  if (m_nextGeneration == std::numeric_limits<uint64_t>::max())
    return WorldRegionOwnerAdmitResult::CreationFailed;

  Owner created;
  created.identity.worldEpoch = m_worldEpoch;
  created.identity.regionGeneration = ++m_nextGeneration;
  created.identity.region = *region;
  created.phase = WorldRegionOwnerPhase::Active;
  created.members[sector] = level;
  m_owners[*region] = std::move(created);
  return WorldRegionOwnerAdmitResult::Created;
}

WorldRegionOwnerReleaseResult WorldRegionOwnerRegistry::release(Vec2U const& sector) {
  auto region = m_partition.regionForSector(sector);
  if (!region)
    return WorldRegionOwnerReleaseResult::UnknownRegion;

  auto owner = m_owners.ptr(*region);
  if (!owner)
    return WorldRegionOwnerReleaseResult::Foreign;
  if (!owner->members.remove(sector))
    return WorldRegionOwnerReleaseResult::NotMember;
  if (!owner->members.empty())
    return WorldRegionOwnerReleaseResult::MemberRemoved;

  owner->phase = WorldRegionOwnerPhase::Draining;
  return WorldRegionOwnerReleaseResult::DrainStarted;
}

WorldRegionOwnerReleaseResult WorldRegionOwnerRegistry::completeDrain(Vec2U const& region) {
  auto owner = m_owners.ptr(region);
  if (!owner)
    return WorldRegionOwnerReleaseResult::Foreign;
  if (owner->phase != WorldRegionOwnerPhase::Draining)
    return WorldRegionOwnerReleaseResult::Unchanged;
  if (!owner->members.empty())
    return WorldRegionOwnerReleaseResult::Unchanged;

  m_owners.remove(region);
  return WorldRegionOwnerReleaseResult::OwnerRetired;
}

}
