#include "StarWorldGenerationMemory.hpp"

#include <algorithm>
#include <map>
#include <mutex>

namespace Star {
namespace {

using SharedOwner = std::weak_ptr<void const>;
using SharedKey = std::pair<WorldGenerationMemoryClass, SharedOwner>;
struct SharedKeyLess {
  bool operator()(SharedKey const& a, SharedKey const& b) const {
    if (a.first != b.first) return a.first < b.first;
    return std::owner_less<SharedOwner>()(a.second, b.second);
  }
};
struct SharedEntry {
  std::weak_ptr<WorldGenerationMemoryReservation> reservation;
  WorldGenerationMemoryReservation const* identity;
  size_t bytes;
};
bool arrayClass(WorldGenerationMemoryClass kind) {
  return kind == WorldGenerationMemoryClass::BaseTilesArray
      || kind == WorldGenerationMemoryClass::MicroDungeonHalo;
}
void chargeBytes(WorldGenerationMemorySnapshot& usage, WorldGenerationMemoryClass kind, size_t bytes) {
  usage.reservedBytes += bytes;
  usage.classBytes[static_cast<size_t>(kind)] += bytes;
  if (arrayClass(kind)) usage.reservedArrayBytes += bytes;
  else if (worldGenerationBufferClass(kind)) usage.reservedBufferBytes += bytes;
  else usage.estimatedBytes += bytes;
  usage.peakReservedBytes = std::max(usage.peakReservedBytes, usage.reservedBytes);
  usage.peakReservedArrayBytes = std::max(usage.peakReservedArrayBytes, usage.reservedArrayBytes);
  usage.peakEstimatedBytes = std::max(usage.peakEstimatedBytes, usage.estimatedBytes);
  usage.peakReservedBufferBytes = std::max(usage.peakReservedBufferBytes, usage.reservedBufferBytes);
}
void releaseBytes(WorldGenerationMemorySnapshot& usage, WorldGenerationMemoryClass kind, size_t bytes) {
  usage.reservedBytes -= bytes;
  usage.classBytes[static_cast<size_t>(kind)] -= bytes;
  if (arrayClass(kind)) usage.reservedArrayBytes -= bytes;
  else if (worldGenerationBufferClass(kind)) usage.reservedBufferBytes -= bytes;
  else usage.estimatedBytes -= bytes;
}
void releaseResource(WorldGenerationMemorySnapshot& usage, WorldGenerationMemoryClass kind, size_t bytes, bool shared) {
  releaseBytes(usage, kind, bytes);
  --usage.resources;
  --usage.classResources[static_cast<size_t>(kind)];
  if (shared) --usage.sharedResources;
}

}

struct WorldGenerationMemoryState {
  explicit WorldGenerationMemoryState(size_t maximum) : maximumBytes(maximum) {}
  std::mutex mutex;
  size_t const maximumBytes;
  WorldGenerationMemorySnapshot usage;
  std::map<SharedKey, SharedEntry, SharedKeyLess> shared;
};

WorldGenerationMemoryReservation::WorldGenerationMemoryReservation(
    std::shared_ptr<WorldGenerationMemoryState> state, WorldGenerationMemoryClass kind, size_t bytes,
    std::shared_ptr<void const> owner)
  : m_state(std::move(state)), m_kind(kind), m_bytes(bytes), m_shared(bool(owner)), m_owner(owner) {}

WorldGenerationMemoryReservation::~WorldGenerationMemoryReservation() {
  if (!m_active)
    return;
  std::lock_guard<std::mutex> lock(m_state->mutex);
  if (m_shared) {
    auto entry = m_state->shared.find(SharedKey{m_kind, m_owner});
    // An acquire can replace an expired token before its destructor gets
    // this lock. That acquire has already retired the previous charge.
    if (entry == m_state->shared.end() || entry->second.identity != this) return;
    m_state->shared.erase(entry);
  }
  releaseResource(m_state->usage, m_kind, m_bytes, m_shared);
}

bool WorldGenerationMemoryReservation::resizeEstimate(size_t bytes) {
  if (!m_active || m_shared || arrayClass(m_kind) || worldGenerationBufferClass(m_kind)) return false;
  std::lock_guard<std::mutex> lock(m_state->mutex);
  auto& usage = m_state->usage;
  if (bytes > m_bytes && bytes - m_bytes > m_state->maximumBytes - usage.reservedBytes) return false;
  if (bytes >= m_bytes) chargeBytes(usage, m_kind, bytes - m_bytes);
  else releaseBytes(usage, m_kind, m_bytes - bytes);
  m_bytes = bytes;
  return true;
}

void WorldGenerationMemoryReservation::noteIncompleteEstimate() {
  std::lock_guard<std::mutex> lock(m_state->mutex);
  auto& count = m_state->usage.incompleteEstimateUpdates;
  if (count != std::numeric_limits<size_t>::max()) ++count;
}

WorldGenerationMemoryLedger::WorldGenerationMemoryLedger(size_t maximumBytes)
  : m_state(std::make_shared<WorldGenerationMemoryState>(maximumBytes)) {}

std::shared_ptr<WorldGenerationMemoryReservation> WorldGenerationMemoryLedger::reserve(
    WorldGenerationMemoryClass kind, size_t bytes) {
  auto index = static_cast<size_t>(kind);
  if (index >= static_cast<size_t>(WorldGenerationMemoryClass::Count)
      || kind == WorldGenerationMemoryClass::TemplateJsonEstimate)
    return {};
  // Allocate an inactive token before locking. An allocation failure cannot
  // charge the ledger, and a refusal cannot deadlock in the token destructor.
  auto reservation = std::shared_ptr<WorldGenerationMemoryReservation>(
      new WorldGenerationMemoryReservation(m_state, kind, bytes));
  {
    std::lock_guard<std::mutex> lock(m_state->mutex);
    auto& usage = m_state->usage;
    if (bytes > m_state->maximumBytes - usage.reservedBytes
        || usage.resources == std::numeric_limits<size_t>::max())
      return {};
    chargeBytes(usage, kind, bytes);
    ++usage.resources;
    ++usage.classResources[index];
    reservation->m_active = true;
  }
  return reservation;
}

std::shared_ptr<WorldGenerationMemoryReservation> WorldGenerationMemoryLedger::reserveSharedJson(
    std::shared_ptr<void const> const& owner, size_t bytes) {
  if (!owner) return {};
  auto kind = WorldGenerationMemoryClass::TemplateJsonEstimate;
  // Reuse existing nodes without allocating a throwaway token. Keep a live
  // reference outside the lock scope: refusing a conflicting model may drop
  // its last reference, whose destructor locks this same state.
  std::shared_ptr<WorldGenerationMemoryReservation> existing;
  {
    std::lock_guard<std::mutex> lock(m_state->mutex);
    auto entry = m_state->shared.find(SharedKey{kind, owner});
    if (entry != m_state->shared.end()) existing = entry->second.reservation.lock();
  }
  if (existing) return existing->m_bytes == bytes ? existing : nullptr;
  auto reservation = std::shared_ptr<WorldGenerationMemoryReservation>(
      new WorldGenerationMemoryReservation(m_state, kind, bytes, owner));
  {
    std::lock_guard<std::mutex> lock(m_state->mutex);
    SharedKey key{kind, owner};
    auto entry = m_state->shared.find(key);
    if (entry != m_state->shared.end()) {
      existing = entry->second.reservation.lock();
      if (existing) return existing->m_bytes == bytes ? existing : nullptr;
      releaseResource(m_state->usage, kind, entry->second.bytes, true);
      m_state->shared.erase(entry);
    }
    auto& usage = m_state->usage;
    if (bytes > m_state->maximumBytes - usage.reservedBytes
        || usage.resources == std::numeric_limits<size_t>::max()) return {};
    // Insertion can allocate; charge only after it succeeds.
    m_state->shared.emplace(std::move(key), SharedEntry{reservation, reservation.get(), bytes});
    chargeBytes(usage, kind, bytes);
    ++usage.resources;
    ++usage.sharedResources;
    ++usage.classResources[static_cast<size_t>(kind)];
    reservation->m_active = true;
  }
  return reservation;
}

WorldGenerationMemorySnapshot WorldGenerationMemoryLedger::snapshot() const {
  std::lock_guard<std::mutex> lock(m_state->mutex);
  auto result = m_state->usage;
  result.sharedRegistryEntries = m_state->shared.size();
  return result;
}

WorldGenerationMemoryLedger& worldGenerationMemoryLedger() {
  static WorldGenerationMemoryLedger ledger;
  return ledger;
}

}
