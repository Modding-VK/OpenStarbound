#include "StarWorldRegionAuthority.hpp"

#include <limits>

namespace Star {

WorldRegionAuthoritativeOwnerLedger::WorldRegionAuthoritativeOwnerLedger(WorldRegionOwnerRegistry const& owners)
  : m_owners(owners) {}

size_t WorldRegionAuthoritativeOwnerLedger::authoritativeCount() const {
  size_t count = 0;
  for (auto const& entry : m_records.pairs()) {
    if (entry.second.grade == WorldRegionAuthorityGrade::Authoritative)
      ++count;
  }
  return count;
}

WorldRegionAuthorityGrade WorldRegionAuthoritativeOwnerLedger::grade(Vec2U const& region) const {
  if (auto record = m_records.ptr(region))
    return record->grade;
  if (m_owners.phase(region) == WorldRegionOwnerPhase::Active)
    return WorldRegionAuthorityGrade::Harness;
  return WorldRegionAuthorityGrade::None;
}

bool WorldRegionAuthoritativeOwnerLedger::isAuthoritative(Vec2U const& region) const {
  return grade(region) == WorldRegionAuthorityGrade::Authoritative;
}

Maybe<WorldRegionAuthorityRecord> WorldRegionAuthoritativeOwnerLedger::record(Vec2U const& region) const {
  if (auto record = m_records.ptr(region))
    return *record;
  return {};
}

Maybe<WorldRegionOwnerIdentity> WorldRegionAuthoritativeOwnerLedger::authoritativeIdentity(Vec2U const& region) const {
  if (auto record = m_records.ptr(region)) {
    if (record->grade == WorldRegionAuthorityGrade::Authoritative)
      return record->owner;
  }
  return {};
}

WorldRegionAuthorityPromoteResult WorldRegionAuthoritativeOwnerLedger::promote(WorldRegionOwnerIdentity const& owner) {
  auto current = m_owners.identity(owner.region);
  if (!current)
    return WorldRegionAuthorityPromoteResult::RegionAbsent;
  if (*current != owner)
    return WorldRegionAuthorityPromoteResult::OwnerMismatch;

  auto phase = m_owners.phase(owner.region);
  if (phase == WorldRegionOwnerPhase::Draining)
    return WorldRegionAuthorityPromoteResult::RegionDraining;
  if (phase != WorldRegionOwnerPhase::Active)
    return WorldRegionAuthorityPromoteResult::NotActive;

  if (auto existing = m_records.ptr(owner.region)) {
    if (existing->grade == WorldRegionAuthorityGrade::Authoritative && existing->owner == owner)
      return WorldRegionAuthorityPromoteResult::AlreadyAuthoritative;
  }

  if (m_nextAuthorityGeneration == std::numeric_limits<uint64_t>::max())
    return WorldRegionAuthorityPromoteResult::NotActive;

  WorldRegionAuthorityRecord created;
  created.owner = owner;
  created.grade = WorldRegionAuthorityGrade::Authoritative;
  created.authorityGeneration = ++m_nextAuthorityGeneration;
  m_records[owner.region] = created;
  return WorldRegionAuthorityPromoteResult::Promoted;
}

WorldRegionAuthorityDemoteResult WorldRegionAuthoritativeOwnerLedger::demote(Vec2U const& region) {
  auto existing = m_records.ptr(region);
  if (!existing)
    return m_owners.phase(region) == WorldRegionOwnerPhase::Absent ? WorldRegionAuthorityDemoteResult::RegionAbsent
                                                                    : WorldRegionAuthorityDemoteResult::Unchanged;
  if (existing->grade != WorldRegionAuthorityGrade::Authoritative)
    return WorldRegionAuthorityDemoteResult::Unchanged;
  m_records.remove(region);
  return WorldRegionAuthorityDemoteResult::Demoted;
}

void WorldRegionAuthoritativeOwnerLedger::clear() {
  m_records.clear();
}

}
