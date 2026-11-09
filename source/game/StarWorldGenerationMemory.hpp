#pragma once

#include "StarWorldGenerationMemoryPressure.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
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
  MicroDungeonNameBuffer,
  MicroDungeonNameEstimate,
  MicroDungeonCandidateEstimate,
  MicroDungeonTemporaryEstimate,
  Count
};

constexpr bool worldGenerationBufferClass(WorldGenerationMemoryClass kind) {
  return kind == WorldGenerationMemoryClass::MicroDungeonPlanBuffer
      || kind == WorldGenerationMemoryClass::MicroDungeonCandidateBuffer
      || kind == WorldGenerationMemoryClass::MicroDungeonDistributionBuffer
      || kind == WorldGenerationMemoryClass::MicroDungeonNameBuffer;
}

// A hint that declared capacity may have improved, not a permit or a wakeup.
// Compare only revisions from the same ledger. Saturation conservatively stops
// suppressing polls instead of wrapping/losing subsequent release events.
struct WorldGenerationMemoryCapacityRevision {
  static constexpr unsigned ModelVersion = 1;
  uint64_t sequence = 0;
  void advance() noexcept {
    if (sequence != std::numeric_limits<uint64_t>::max()) ++sequence;
  }
  bool canSuppress(WorldGenerationMemoryCapacityRevision previous) const noexcept {
    return sequence != std::numeric_limits<uint64_t>::max() && sequence == previous.sequence;
  }
};

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
  WorldGenerationMemoryCapacityRevision capacity;
  WorldGenerationMemoryAdmissionSnapshot admission;
  std::array<size_t, static_cast<size_t>(WorldGenerationMemoryClass::Count)> classBytes{};
  std::array<size_t, static_cast<size_t>(WorldGenerationMemoryClass::Count)> classResources{};
};

struct WorldGenerationMemoryState;

struct WorldGenerationMemoryRequest {
  WorldGenerationMemoryClass kind;
  size_t bytes;
};

enum class WorldGenerationMemoryBatchStatus { Admitted, Deferred, Impossible, Invalid };

class WorldGenerationMemoryReservation;

template <size_t N>
struct WorldGenerationMemoryBatchAdmission {
  WorldGenerationMemoryBatchStatus status = WorldGenerationMemoryBatchStatus::Invalid;
  size_t requestedBytes = 0;
  // Only Deferred carries a revision, sampled with that decision under lock.
  std::optional<WorldGenerationMemoryCapacityRevision> deferredCapacity;
  std::optional<std::array<std::shared_ptr<WorldGenerationMemoryReservation>, N>> memory;
};

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
      WorldGenerationMemoryClass kind, size_t bytes, WorldGenerationMemoryDemand demand,
      std::shared_ptr<void const> owner = {});

  std::shared_ptr<WorldGenerationMemoryState> m_state;
  WorldGenerationMemoryClass m_kind;
  size_t m_bytes;
  WorldGenerationMemoryDemand m_demand;
  bool m_active = false;
  bool m_shared = false;
  std::weak_ptr<void const> m_owner;
};

class WorldGenerationMemoryLedger {
public:
  // The runtime singleton observes current usage without imposing a new cap.
  // A finite cap is available for isolated admission/lifecycle tests.
  explicit WorldGenerationMemoryLedger(size_t maximumBytes = std::numeric_limits<size_t>::max());
  // Opt-in scoped admission, configured before publishing this ledger. The
  // runtime singleton keeps the existing unlimited/disabled constructor.
  WorldGenerationMemoryLedger(size_t maximumBytes, WorldGenerationMemoryPressureLimits limits);
  WorldGenerationMemoryLedger withDemand(WorldGenerationMemoryDemand demand) const;
  std::shared_ptr<WorldGenerationMemoryReservation> reserve(WorldGenerationMemoryClass kind, size_t bytes);
  // One check/charge for every part, or none. Parts describe distinct physical
  // resources (input reused as output is one part), not subsequent allocations
  // which would reserve again. Shared Json owners use reserveSharedJson instead.
  // The fixed-size result adds no intermediate vector allocation. Each token
  // can be moved to its physical owner and releases independently thereafter.
  template <size_t N>
  std::optional<std::array<std::shared_ptr<WorldGenerationMemoryReservation>, N>> reserveBatch(
      std::array<WorldGenerationMemoryRequest, N> const& requests) {
    return tryReserveBatch(requests).memory;
  }
  // Status and reservation come from the same admission, not a later snapshot.
  // Impossible means this request cannot fit even in an empty declared scope.
  template <size_t N>
  WorldGenerationMemoryBatchAdmission<N> tryReserveBatch(std::array<WorldGenerationMemoryRequest, N> const& requests) {
    WorldGenerationMemoryBatchAdmission<N> result;
    std::array<std::shared_ptr<WorldGenerationMemoryReservation>, N> reservations{};
    result.status = tryReserveBatch(requests.data(), reservations.data(), N, result.requestedBytes, result.deferredCapacity);
    if (result.status == WorldGenerationMemoryBatchStatus::Admitted) result.memory.emplace(std::move(reservations));
    return result;
  }
  // Deduplicate immutable Json storage by ownership, not address/content.
  std::shared_ptr<WorldGenerationMemoryReservation> reserveSharedJson(
      std::shared_ptr<void const> const& owner, size_t bytes);
  WorldGenerationMemorySnapshot snapshot() const;
  // Small owner-side query; no envelope conversion, callbacks or allocations.
  WorldGenerationMemoryCapacityRevision capacityRevision() const;

private:
  template <typename T> friend class WorldGenerationBufferAllocator;
  template <typename Source, typename Payload> friend class WorldGenerationWorkerCache;
  WorldGenerationMemoryBatchStatus tryReserveBatch(WorldGenerationMemoryRequest const* requests,
      std::shared_ptr<WorldGenerationMemoryReservation>* reservations, size_t count, size_t& requestedBytes,
      std::optional<WorldGenerationMemoryCapacityRevision>& deferredCapacity);
  bool matches(std::shared_ptr<WorldGenerationMemoryReservation> const& reservation,
      WorldGenerationMemoryClass kind, size_t bytes) const;
  bool owns(std::shared_ptr<WorldGenerationMemoryReservation> const& reservation,
      WorldGenerationMemoryClass kind) const noexcept;
  WorldGenerationMemoryLedger(std::shared_ptr<WorldGenerationMemoryState> state, WorldGenerationMemoryDemand demand)
    : m_state(std::move(state)), m_demand(demand) {}
  // Copies are views of the same ledger. Never replace a facade's state and
  // silently forget reservations still owned by outstanding resources.
  std::shared_ptr<WorldGenerationMemoryState> const m_state;
  WorldGenerationMemoryDemand const m_demand = WorldGenerationMemoryDemand::Visible;
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
