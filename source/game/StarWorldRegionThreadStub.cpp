#include "StarWorldRegionThreadStub.hpp"

#include <limits>

namespace Star {

WorldRegionOwnerThreadStubLedger::WorldRegionOwnerThreadStubLedger(
    WorldRegionAuthoritativeOwnerLedger const& authority, bool creationAllowed)
  : m_authority(authority), m_creationAllowed(creationAllowed) {}

void WorldRegionOwnerThreadStubLedger::setCreationAllowed(bool allowed) {
  m_creationAllowed = allowed;
}

bool WorldRegionOwnerThreadStubLedger::creationAllowed() const {
  return m_creationAllowed;
}

size_t WorldRegionOwnerThreadStubLedger::runningCount() const {
  size_t count = 0;
  for (auto const& entry : m_stubs.pairs()) {
    if (entry.second.phase == WorldRegionThreadStubPhase::Running
        || entry.second.phase == WorldRegionThreadStubPhase::Joining)
      ++count;
  }
  return count;
}

WorldRegionThreadStubPhase WorldRegionOwnerThreadStubLedger::phase(Vec2U const& region) const {
  if (auto stub = m_stubs.ptr(region))
    return stub->phase;
  return WorldRegionThreadStubPhase::Absent;
}

Maybe<WorldRegionThreadStubRecord> WorldRegionOwnerThreadStubLedger::record(Vec2U const& region) const {
  if (auto stub = m_stubs.ptr(region))
    return *stub;
  return {};
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
    if (existing->phase == WorldRegionThreadStubPhase::Running
        || existing->phase == WorldRegionThreadStubPhase::Joining)
      return WorldRegionThreadStubStartResult::AlreadyRunning;
  }

  WorldRegionThreadStubRecord created;
  created.owner = owner;
  created.phase = WorldRegionThreadStubPhase::Running;
  created.stubThreadId = ++m_nextStubThreadId;
  m_stubs[owner.region] = created;
  return WorldRegionThreadStubStartResult::Started;
}

WorldRegionThreadStubStopResult WorldRegionOwnerThreadStubLedger::beginJoin(Vec2U const& region) {
  auto stub = m_stubs.ptr(region);
  if (!stub)
    return WorldRegionThreadStubStopResult::RegionAbsent;
  if (stub->phase == WorldRegionThreadStubPhase::Joining)
    return WorldRegionThreadStubStopResult::Joining;
  if (stub->phase == WorldRegionThreadStubPhase::Stopped)
    return WorldRegionThreadStubStopResult::Stopped;
  if (stub->phase != WorldRegionThreadStubPhase::Running)
    return WorldRegionThreadStubStopResult::NotRunning;
  stub->phase = WorldRegionThreadStubPhase::Joining;
  return WorldRegionThreadStubStopResult::Joining;
}

WorldRegionThreadStubStopResult WorldRegionOwnerThreadStubLedger::completeJoin(Vec2U const& region) {
  auto stub = m_stubs.ptr(region);
  if (!stub)
    return WorldRegionThreadStubStopResult::RegionAbsent;
  if (stub->phase == WorldRegionThreadStubPhase::Stopped)
    return WorldRegionThreadStubStopResult::Stopped;
  if (stub->phase != WorldRegionThreadStubPhase::Joining)
    return WorldRegionThreadStubStopResult::NotRunning;
  stub->phase = WorldRegionThreadStubPhase::Stopped;
  return WorldRegionThreadStubStopResult::Stopped;
}

void WorldRegionOwnerThreadStubLedger::clear() {
  m_stubs.clear();
}

}
