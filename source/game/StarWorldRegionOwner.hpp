#pragma once

#include "StarMap.hpp"
#include "StarMaybe.hpp"
#include "StarWorldRegionPartition.hpp"

namespace Star {

// REG-01 identity for a logical 2x2 region owner. World epoch, region
// generation and region coordinates identify the owner; object or thread
// addresses never do. This harness does not start OS threads or publish into
// a live WorldServer / WorldStorage.
struct WorldRegionOwnerIdentity {
  uint64_t worldEpoch = 0;
  uint64_t regionGeneration = 0;
  Vec2U region{};

  bool operator==(WorldRegionOwnerIdentity const& rhs) const;
  bool operator!=(WorldRegionOwnerIdentity const& rhs) const;
};

enum class WorldRegionMemberLevel : uint8_t {
  None = 0,
  Queued = 1,
  Tiles = 2,
  Entities = 3
};

enum class WorldRegionOwnerPhase : uint8_t {
  Absent = 0,
  Active = 1,
  Draining = 2
};

enum class WorldRegionOwnerAdmitResult {
  Unchanged,
  Created,
  MemberAdded,
  MemberRaised,
  IgnoredQueued,
  RejectedWhileDraining,
  CreationFailed,
  UnknownRegion
};

enum class WorldRegionOwnerReleaseResult {
  Unchanged,
  MemberRemoved,
  DrainStarted,
  OwnerRetired,
  Foreign,
  NotMember,
  UnknownRegion
};

// First REG-01 executable slice: create an owner identity on the first
// Tiles/Entities membership of a region, refuse Queued-only activation, drain
// after the last member leaves, and retire only after completeDrain. Prefetch
// or enqueue alone must not mint owners.
class WorldRegionOwnerRegistry {
public:
  WorldRegionOwnerRegistry(uint64_t worldEpoch, WorldRegionPartition partition, bool creationAllowed = true);

  WorldRegionOwnerAdmitResult admit(Vec2U const& sector, WorldRegionMemberLevel level);
  WorldRegionOwnerReleaseResult release(Vec2U const& sector);
  WorldRegionOwnerReleaseResult completeDrain(Vec2U const& region);

  Maybe<WorldRegionOwnerIdentity> identity(Vec2U const& region) const;
  WorldRegionOwnerPhase phase(Vec2U const& region) const;
  size_t memberCount(Vec2U const& region) const;
  size_t ownerCount() const;
  void setCreationAllowed(bool allowed);
  bool creationAllowed() const;

private:
  struct Owner {
    WorldRegionOwnerIdentity identity;
    WorldRegionOwnerPhase phase = WorldRegionOwnerPhase::Active;
    HashMap<Vec2U, WorldRegionMemberLevel> members;
  };

  uint64_t const m_worldEpoch;
  WorldRegionPartition m_partition;
  bool m_creationAllowed;
  uint64_t m_nextGeneration = 0;
  HashMap<Vec2U, Owner> m_owners;
};

}
