#pragma once

#include "StarMap.hpp"
#include "StarMaybe.hpp"
#include "StarThread.hpp"
#include "StarWorldRegionAuthority.hpp"

#include <atomic>
#include <memory>

namespace Star {

// REG-01 CPU OS-thread stub for an authoritative owner. Starts a real OS
// thread via Star::Thread that burns CPU cycles until join. Does not touch
// WorldServerThread, WorldStorage, Lua, or live simulation. Opaque stub ids
// are never owner identity.
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
  uint64_t cpuTicks = 0;
};

class WorldRegionOwnerThreadStubLedger {
public:
  WorldRegionOwnerThreadStubLedger(WorldRegionAuthoritativeOwnerLedger const& authority, bool creationAllowed = true);
  ~WorldRegionOwnerThreadStubLedger();

  WorldRegionOwnerThreadStubLedger(WorldRegionOwnerThreadStubLedger const&) = delete;
  WorldRegionOwnerThreadStubLedger& operator=(WorldRegionOwnerThreadStubLedger const&) = delete;

  WorldRegionThreadStubStartResult start(WorldRegionOwnerIdentity const& owner);
  WorldRegionThreadStubStopResult beginJoin(Vec2U const& region);
  WorldRegionThreadStubStopResult completeJoin(Vec2U const& region);
  void clear();
  void setCreationAllowed(bool allowed);
  bool creationAllowed() const;

  WorldRegionThreadStubPhase phase(Vec2U const& region) const;
  Maybe<WorldRegionThreadStubRecord> record(Vec2U const& region) const;
  uint64_t cpuTicks(Vec2U const& region) const;
  size_t runningCount() const;

private:
  struct CpuStub {
    WorldRegionThreadStubRecord record;
    std::shared_ptr<std::atomic<bool>> stop = std::make_shared<std::atomic<bool>>(false);
    std::shared_ptr<std::atomic<uint64_t>> ticks = std::make_shared<std::atomic<uint64_t>>(0);
    ThreadFunction<void> worker;
  };

  void joinStub(CpuStub& stub);

  WorldRegionAuthoritativeOwnerLedger const& m_authority;
  bool m_creationAllowed;
  uint64_t m_nextStubThreadId = 0;
  HashMap<Vec2U, shared_ptr<CpuStub>> m_stubs;
};

}
