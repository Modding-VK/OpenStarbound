#include "StarWorldRegionThreadStub.hpp"

#include "StarFormat.hpp"

#include <limits>

namespace Star {

WorldRegionOwnerThreadStubLedger::WorldRegionOwnerThreadStubLedger(
    WorldRegionAuthoritativeOwnerLedger const& authority, bool creationAllowed)
  : m_authority(authority), m_creationAllowed(creationAllowed) {}

WorldRegionOwnerThreadStubLedger::~WorldRegionOwnerThreadStubLedger() {
  clear();
}

void WorldRegionOwnerThreadStubLedger::setCreationAllowed(bool allowed) {
  m_creationAllowed = allowed;
}

bool WorldRegionOwnerThreadStubLedger::creationAllowed() const {
  return m_creationAllowed;
}

size_t WorldRegionOwnerThreadStubLedger::runningCount() const {
  size_t count = 0;
  for (auto const& key : m_stubs.keys()) {
    auto const& stub = m_stubs.get(key);
    if (stub && (stub->record.phase == WorldRegionThreadStubPhase::Running
            || stub->record.phase == WorldRegionThreadStubPhase::Joining))
      ++count;
  }
  return count;
}

WorldRegionThreadStubPhase WorldRegionOwnerThreadStubLedger::phase(Vec2U const& region) const {
  if (auto stubPtr = m_stubs.ptr(region)) {
    if (*stubPtr)
      return (*stubPtr)->record.phase;
  }
  return WorldRegionThreadStubPhase::Absent;
}

Maybe<WorldRegionThreadStubRecord> WorldRegionOwnerThreadStubLedger::record(Vec2U const& region) const {
  if (auto stubPtr = m_stubs.ptr(region)) {
    if (*stubPtr) {
      auto copy = (*stubPtr)->record;
      copy.cpuTicks = (*stubPtr)->ticks->load();
      return copy;
    }
  }
  return {};
}

uint64_t WorldRegionOwnerThreadStubLedger::cpuTicks(Vec2U const& region) const {
  if (auto stubPtr = m_stubs.ptr(region)) {
    if (*stubPtr)
      return (*stubPtr)->ticks->load();
  }
  return 0;
}

void WorldRegionOwnerThreadStubLedger::joinStub(CpuStub& stub) {
  stub.stop->store(true);
  if (stub.worker)
    stub.worker.finish();
  stub.record.cpuTicks = stub.ticks->load();
  stub.record.phase = WorldRegionThreadStubPhase::Stopped;
}

WorldRegionThreadStubStartResult WorldRegionOwnerThreadStubLedger::start(WorldRegionOwnerIdentity const& owner) {
  auto authoritative = m_authority.authoritativeIdentity(owner.region);
  if (!authoritative)
    return WorldRegionThreadStubStartResult::NotAuthoritative;
  if (*authoritative != owner)
    return WorldRegionThreadStubStartResult::OwnerMismatch;
  if (!m_creationAllowed)
    return WorldRegionThreadStubStartResult::CreationFailed;
  if (m_nextStubThreadId == std::numeric_limits<uint64_t>::max())
    return WorldRegionThreadStubStartResult::CreationFailed;

  if (auto existing = m_stubs.ptr(owner.region)) {
    if (*existing
        && ((*existing)->record.phase == WorldRegionThreadStubPhase::Running
            || (*existing)->record.phase == WorldRegionThreadStubPhase::Joining))
      return WorldRegionThreadStubStartResult::AlreadyRunning;
    if (*existing)
      joinStub(**existing);
  }

  auto created = make_shared<CpuStub>();
  created->record.owner = owner;
  created->record.phase = WorldRegionThreadStubPhase::Running;
  created->record.stubThreadId = ++m_nextStubThreadId;
  created->stop->store(false);
  created->ticks->store(0);

  auto stop = created->stop;
  auto ticks = created->ticks;
  try {
    created->worker = Thread::invoke(strf("RegionCpuStub-{}-{}", owner.region[0], owner.region[1]), [stop, ticks]() {
      // Pure CPU shadow work: no WorldServer, storage, Lua, or network.
      while (!stop->load(std::memory_order_relaxed)) {
        volatile uint64_t sink = 0;
        for (uint64_t i = 0; i < 128; ++i)
          sink += i;
        ticks->fetch_add(1, std::memory_order_relaxed);
        Thread::yield();
      }
    });
  } catch (...) {
    return WorldRegionThreadStubStartResult::CreationFailed;
  }

  m_stubs[owner.region] = created;
  return WorldRegionThreadStubStartResult::Started;
}

WorldRegionThreadStubStopResult WorldRegionOwnerThreadStubLedger::beginJoin(Vec2U const& region) {
  auto stubPtr = m_stubs.ptr(region);
  if (!stubPtr || !*stubPtr)
    return WorldRegionThreadStubStopResult::RegionAbsent;
  auto& stub = **stubPtr;
  if (stub.record.phase == WorldRegionThreadStubPhase::Joining)
    return WorldRegionThreadStubStopResult::Joining;
  if (stub.record.phase == WorldRegionThreadStubPhase::Stopped)
    return WorldRegionThreadStubStopResult::Stopped;
  if (stub.record.phase != WorldRegionThreadStubPhase::Running)
    return WorldRegionThreadStubStopResult::NotRunning;
  stub.stop->store(true);
  stub.record.phase = WorldRegionThreadStubPhase::Joining;
  return WorldRegionThreadStubStopResult::Joining;
}

WorldRegionThreadStubStopResult WorldRegionOwnerThreadStubLedger::completeJoin(Vec2U const& region) {
  auto stubPtr = m_stubs.ptr(region);
  if (!stubPtr || !*stubPtr)
    return WorldRegionThreadStubStopResult::RegionAbsent;
  auto& stub = **stubPtr;
  if (stub.record.phase == WorldRegionThreadStubPhase::Stopped)
    return WorldRegionThreadStubStopResult::Stopped;
  if (stub.record.phase != WorldRegionThreadStubPhase::Joining
      && stub.record.phase != WorldRegionThreadStubPhase::Running)
    return WorldRegionThreadStubStopResult::NotRunning;
  joinStub(stub);
  return WorldRegionThreadStubStopResult::Stopped;
}

void WorldRegionOwnerThreadStubLedger::clear() {
  for (auto const& key : m_stubs.keys()) {
    auto& stub = m_stubs.get(key);
    if (stub)
      joinStub(*stub);
  }
  m_stubs.clear();
}

}
