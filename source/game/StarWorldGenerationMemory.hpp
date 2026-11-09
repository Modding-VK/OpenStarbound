#pragma once

#include <array>
#include <cstddef>
#include <limits>
#include <memory>
#include <utility>

namespace Star {

enum class WorldGenerationMemoryClass : size_t {
  BaseTilesArray,
  MicroDungeonHalo,
  TemplateJsonEstimate,
  SnapshotMetadataEstimate,
  MicroDungeonPlanEstimate,
  WorkerTemplateEstimate,
  MicroDungeonPlanBuffer,
  MicroDungeonCandidateBuffer,
  MicroDungeonDistributionBuffer,
  MicroDungeonDistributionEstimate,
  Count
};

constexpr bool worldGenerationBufferClass(WorldGenerationMemoryClass kind) {
  return kind == WorldGenerationMemoryClass::MicroDungeonPlanBuffer
      || kind == WorldGenerationMemoryClass::MicroDungeonCandidateBuffer
      || kind == WorldGenerationMemoryClass::MicroDungeonDistributionBuffer;
}

struct WorldGenerationMemorySnapshot {
  size_t reservedBytes = 0;
  size_t peakReservedBytes = 0;
  size_t resources = 0;
  size_t reservedArrayBytes = 0;
  size_t estimatedBytes = 0;
  size_t peakReservedArrayBytes = 0;
  size_t peakEstimatedBytes = 0;
  size_t reservedBufferBytes = 0;
  size_t peakReservedBufferBytes = 0;
  size_t sharedResources = 0;
  size_t sharedRegistryEntries = 0;
  size_t incompleteEstimateUpdates = 0;
  std::array<size_t, static_cast<size_t>(WorldGenerationMemoryClass::Count)> classBytes{};
  std::array<size_t, static_cast<size_t>(WorldGenerationMemoryClass::Count)> classResources{};
};

struct WorldGenerationMemoryState;

// Share this token only with references to the same physical resource/group.
// It owns the ledger state, not a world, task, worker or live-world callback.
class WorldGenerationMemoryReservation {
public:
  ~WorldGenerationMemoryReservation();
  WorldGenerationMemoryReservation(WorldGenerationMemoryReservation const&) = delete;
  WorldGenerationMemoryReservation& operator=(WorldGenerationMemoryReservation const&) = delete;
  bool resizeEstimate(size_t bytes);
  void noteIncompleteEstimate();

private:
  friend class WorldGenerationMemoryLedger;
  WorldGenerationMemoryReservation(std::shared_ptr<WorldGenerationMemoryState> state,
      WorldGenerationMemoryClass kind, size_t bytes, std::shared_ptr<void const> owner = {});

  std::shared_ptr<WorldGenerationMemoryState> m_state;
  WorldGenerationMemoryClass m_kind;
  size_t m_bytes;
  bool m_active = false;
  bool m_shared = false;
  std::weak_ptr<void const> m_owner;
};

class WorldGenerationMemoryLedger {
public:
  // The runtime singleton observes current usage without imposing a new cap.
  // A finite cap is available for isolated admission/lifecycle tests.
  explicit WorldGenerationMemoryLedger(size_t maximumBytes = std::numeric_limits<size_t>::max());
  std::shared_ptr<WorldGenerationMemoryReservation> reserve(WorldGenerationMemoryClass kind, size_t bytes);
  // Deduplicate immutable Json storage by ownership, not address/content.
  std::shared_ptr<WorldGenerationMemoryReservation> reserveSharedJson(
      std::shared_ptr<void const> const& owner, size_t bytes);
  WorldGenerationMemorySnapshot snapshot() const;

private:
  template <typename T> friend class WorldGenerationBufferAllocator;
  explicit WorldGenerationMemoryLedger(std::shared_ptr<WorldGenerationMemoryState> state)
    : m_state(std::move(state)) {}
  // Copies are views of the same ledger. Never replace a facade's state and
  // silently forget reservations still owned by outstanding resources.
  std::shared_ptr<WorldGenerationMemoryState> const m_state;
};

WorldGenerationMemoryLedger& worldGenerationMemoryLedger();

// Member order is intentional: destroy the payload before releasing its
// reservation. Copy shared_ptrs to this wrapper, never copy a payload while
// reusing its token. Mutable payloads must stay within their admitted bytes.
template <typename Payload>
class WorldGenerationTrackedResource {
private:
  std::shared_ptr<WorldGenerationMemoryReservation> m_memory;

public:
  explicit WorldGenerationTrackedResource(std::shared_ptr<WorldGenerationMemoryReservation> memory)
    : m_memory(std::move(memory)) {}
  WorldGenerationTrackedResource(WorldGenerationTrackedResource const&) = delete;
  WorldGenerationTrackedResource& operator=(WorldGenerationTrackedResource const&) = delete;

  Payload value{};
  bool resizeEstimate(size_t bytes) { return m_memory->resizeEstimate(bytes); }
};

// Acquire before allocating/default-constructing the payload. The local token
// also protects construction failures, including a throwing payload ctor.
template <typename Payload>
std::shared_ptr<WorldGenerationTrackedResource<Payload>> makeWorldGenerationTrackedResource(
    WorldGenerationMemoryLedger& ledger, WorldGenerationMemoryClass kind, size_t bytes) {
  auto memory = ledger.reserve(kind, bytes);
  if (!memory)
    return {};
  return std::make_shared<WorldGenerationTrackedResource<Payload>>(std::move(memory));
}

}
