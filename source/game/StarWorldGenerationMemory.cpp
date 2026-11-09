#include "StarWorldGenerationMemory.hpp"

#include <algorithm>
#include <mutex>

namespace Star {

struct WorldGenerationMemoryState {
  explicit WorldGenerationMemoryState(size_t maximum) : maximumBytes(maximum) {}
  std::mutex mutex;
  size_t const maximumBytes;
  WorldGenerationMemorySnapshot usage;
};

WorldGenerationMemoryReservation::WorldGenerationMemoryReservation(
    std::shared_ptr<WorldGenerationMemoryState> state, WorldGenerationMemoryClass kind, size_t bytes)
  : m_state(std::move(state)), m_kind(kind), m_bytes(bytes) {}

WorldGenerationMemoryReservation::~WorldGenerationMemoryReservation() {
  if (!m_active)
    return;
  std::lock_guard<std::mutex> lock(m_state->mutex);
  auto index = static_cast<size_t>(m_kind);
  auto& usage = m_state->usage;
  usage.reservedBytes -= m_bytes;
  usage.classBytes[index] -= m_bytes;
  --usage.resources;
  --usage.classResources[index];
}

WorldGenerationMemoryLedger::WorldGenerationMemoryLedger(size_t maximumBytes)
  : m_state(std::make_shared<WorldGenerationMemoryState>(maximumBytes)) {}

std::shared_ptr<WorldGenerationMemoryReservation> WorldGenerationMemoryLedger::reserve(
    WorldGenerationMemoryClass kind, size_t bytes) {
  auto index = static_cast<size_t>(kind);
  if (index >= static_cast<size_t>(WorldGenerationMemoryClass::Count))
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
    usage.reservedBytes += bytes;
    usage.classBytes[index] += bytes;
    ++usage.resources;
    ++usage.classResources[index];
    usage.peakReservedBytes = std::max(usage.peakReservedBytes, usage.reservedBytes);
    reservation->m_active = true;
  }
  return reservation;
}

WorldGenerationMemorySnapshot WorldGenerationMemoryLedger::snapshot() const {
  std::lock_guard<std::mutex> lock(m_state->mutex);
  return m_state->usage;
}

WorldGenerationMemoryLedger& worldGenerationMemoryLedger() {
  static WorldGenerationMemoryLedger ledger;
  return ledger;
}

}
