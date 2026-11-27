#pragma once

#include "StarMap.hpp"
#include "StarMaybe.hpp"
#include "StarWorldRegionAuthority.hpp"

namespace Star {

// REG-01 shadow OS-thread stub for an authoritative owner. Tracks lifecycle
// without pthread_create, Lua, or live WorldServer simulation. Thread ids are
// opaque counters — never used as owner identity.
enum class WorldRegionThreadStubPhase : uint8_t {
  Absent = 0,
  Running = 1,
  Joining = 2,
  Stopped = 3,
  Failed = 4
};

enum class WorldRegionThreadStubStartResult {
  Started,
  AlreadyRunning,
  NotAuthoritative,
  OwnerMismatch,
  CreationFailed,
  RegionAbsent
};

enum class WorldRegionThreadStubStopResult {
  Joining,
  Stopped,
  Unchanged,
  RegionAbsent,
  NotRunning
};

struct WorldRegionThreadStubRecord {
  WorldRegionOwnerIdentity owner;
  WorldRegionThreadStubPhase phase = WorldRegionThreadStubPhase::Absent;
  uint64_t stubThreadId = 0;
};

class WorldRegionOwnerThreadStubLedger {
public:
  WorldRegionOwnerThreadStubLedger(WorldRegionAuthoritativeOwnerLedger const& authority, bool creationAllowed = true);

  WorldRegionThreadStubStartResult start(WorldRegionOwnerIdentity const& owner);
  WorldRegionThreadStubStopResult beginJoin(Vec2U const& region);
  WorldRegionThreadStubStopResult completeJoin(Vec2U const& region);
  void clear();
  void setCreationAllowed(bool allowed);
  bool creationAllowed() const;

  WorldRegionThreadStubPhase phase(Vec2U const& region) const;
  Maybe<WorldRegionThreadStubRecord> record(Vec2U const& region) const;
  size_t runningCount() const;

private:
  WorldRegionAuthoritativeOwnerLedger const& m_authority;
  bool m_creationAllowed;
  uint64_t m_nextStubThreadId = 0;
  HashMap<Vec2U, WorldRegionThreadStubRecord> m_stubs;
};

}
