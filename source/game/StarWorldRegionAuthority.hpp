#pragma once

#include "StarMap.hpp"
#include "StarMaybe.hpp"
#include "StarWorldRegionOwner.hpp"

namespace Star {

// REG-01 authoritative ownership grade. Distinct from harness Active: exclusive
// write authority bound to WorldRegionOwnerIdentity. Does not start an OS
// thread and does not publish into live WorldServer / WorldStorage.
enum class WorldRegionAuthorityGrade : uint8_t {
  None = 0,
  Harness = 1,
  Authoritative = 2
};

enum class WorldRegionAuthorityPromoteResult {
  Promoted,
  AlreadyAuthoritative,
  RegionAbsent,
  RegionDraining,
  OwnerMismatch,
  NotActive
};

enum class WorldRegionAuthorityDemoteResult {
  Demoted,
  Unchanged,
  RegionAbsent
};

struct WorldRegionAuthorityRecord {
  WorldRegionOwnerIdentity owner;
  WorldRegionAuthorityGrade grade = WorldRegionAuthorityGrade::None;
  uint64_t authorityGeneration = 0;
};

// Authoritative owner contract for dependents (R2+): promote an Active harness
// owner to exclusive write authority identified only by value identity.
class WorldRegionAuthoritativeOwnerLedger {
public:
  explicit WorldRegionAuthoritativeOwnerLedger(WorldRegionOwnerRegistry const& owners);

  WorldRegionAuthorityPromoteResult promote(WorldRegionOwnerIdentity const& owner);
  WorldRegionAuthorityDemoteResult demote(Vec2U const& region);
  void clear();

  WorldRegionAuthorityGrade grade(Vec2U const& region) const;
  bool isAuthoritative(Vec2U const& region) const;
  Maybe<WorldRegionAuthorityRecord> record(Vec2U const& region) const;
  Maybe<WorldRegionOwnerIdentity> authoritativeIdentity(Vec2U const& region) const;
  size_t authoritativeCount() const;

private:
  WorldRegionOwnerRegistry const& m_owners;
  HashMap<Vec2U, WorldRegionAuthorityRecord> m_records;
  uint64_t m_nextAuthorityGeneration = 0;
};

}
