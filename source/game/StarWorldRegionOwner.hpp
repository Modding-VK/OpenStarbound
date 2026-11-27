#pragma once

#include "StarList.hpp"
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

struct WorldRegionOwnerMember {
  Vec2U sector{};
  WorldRegionMemberLevel level = WorldRegionMemberLevel::None;
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
  PendingWork,
  Foreign,
  NotMember,
  UnknownRegion
};

// REG-01 harness: identity + membership lifecycle for logical 2x2 regions.
// Prefetch/enqueue alone must not mint owners. Drain refuses retirement while
// pending local work remains.
class WorldRegionOwnerRegistry {
public:
  WorldRegionOwnerRegistry(uint64_t worldEpoch, WorldRegionPartition partition, bool creationAllowed = true);

  WorldRegionOwnerAdmitResult admit(Vec2U const& sector, WorldRegionMemberLevel level);
  WorldRegionOwnerReleaseResult release(Vec2U const& sector);
  WorldRegionOwnerReleaseResult completeDrain(Vec2U const& region);

  bool addPendingWork(Vec2U const& region, uint64_t units = 1);
  bool clearPendingWork(Vec2U const& region, uint64_t units = 1);
  uint64_t pendingWork(Vec2U const& region) const;

  Maybe<WorldRegionOwnerIdentity> identity(Vec2U const& region) const;
  Maybe<WorldRegionOwnerIdentity> identityForSector(Vec2U const& sector) const;
  Maybe<WorldRegionMemberLevel> memberLevel(Vec2U const& sector) const;
  List<WorldRegionOwnerMember> members(Vec2U const& region) const;
  List<Vec2U> activeRegions() const;
  WorldRegionOwnerPhase phase(Vec2U const& region) const;
  size_t memberCount(Vec2U const& region) const;
  size_t ownerCount() const;
  WorldRegionPartition const& partition() const;
  uint64_t worldEpoch() const;
  void setCreationAllowed(bool allowed);
  bool creationAllowed() const;

private:
  struct Owner {
    WorldRegionOwnerIdentity identity;
    WorldRegionOwnerPhase phase = WorldRegionOwnerPhase::Active;
    HashMap<Vec2U, WorldRegionMemberLevel> members;
    uint64_t pendingWork = 0;
  };

  uint64_t const m_worldEpoch;
  WorldRegionPartition m_partition;
  bool m_creationAllowed;
  uint64_t m_nextGeneration = 0;
  HashMap<Vec2U, Owner> m_owners;
};

}
