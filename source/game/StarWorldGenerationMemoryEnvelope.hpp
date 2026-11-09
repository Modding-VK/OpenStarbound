#pragma once

#include "StarWorldGenerationMemory.hpp"

namespace Star {

enum class WorldGenerationMemoryQuantity { ArrayPayload, AllocatorRequest, Estimate };
enum class WorldGenerationMemoryCollection { Always, RegionalProfiling };
enum class WorldGenerationMemoryFrontier { BeforeAllocation, ObservationOrPrediction, JobBoundary };

struct WorldGenerationMemoryClassInfo {
  WorldGenerationMemoryClass kind;
  WorldGenerationMemoryQuantity quantity;
  WorldGenerationMemoryCollection collection;
  WorldGenerationMemoryFrontier frontier;
};

WorldGenerationMemoryClassInfo const* worldGenerationMemoryClassInfo(WorldGenerationMemoryClass kind) noexcept;

// This envelope is the accounted private-generation subset, never RSS or a
// complete heap bound. Incoherent/overflowed snapshots cannot support advice.
struct WorldGenerationMemoryEnvelope {
  static constexpr unsigned ModelVersion = 1;
  static constexpr bool CompleteHeap = false;
  size_t arrayPayloadBytes = 0;
  size_t allocatorRequestBytes = 0;
  size_t estimatedBytes = 0;
  size_t alwaysEstimatedBytes = 0;
  size_t profilingEstimatedBytes = 0;
  size_t trackedBytes = 0;
  size_t resources = 0;
  // Historical diagnostic, not a count of currently incomplete owners.
  size_t historicalIncompleteUpdates = 0;
  bool coherent = true;
};

WorldGenerationMemoryEnvelope worldGenerationMemoryEnvelope(WorldGenerationMemorySnapshot const& usage) noexcept;

enum class WorldGenerationMemoryPressure { Unrestricted, Normal, Soft, Hard, Invalid };
enum class WorldGenerationMemoryDemand { Visible, Dependency, Speculative };
enum class WorldGenerationMemoryAdviceReason {
  Disabled, InvalidLimits, InvalidEnvelope, InvalidDemand, Reuse,
  AtHardLimit, ExceedsHardLimit, SpeculationPaused, ExceedsSoftLimit, Fits
};

struct WorldGenerationMemoryPressureLimits {
  bool enabled = false;
  size_t resumeBytes = 0;
  size_t softBytes = 0;
  size_t hardBytes = 0;
};

struct WorldGenerationMemoryAdvice {
  WorldGenerationMemoryPressure pressure;
  WorldGenerationMemoryAdviceReason reason;
  bool allowRequest;
  // Never stop consumption/release of existing results because of pressure.
  bool drainExisting = true;
};

// Owner-local advisory model for a declared accounted subset. No runtime
// admission uses this yet. A snapshot/advice is not an atomic reservation,
// eviction, cancellation, hardware query or permission to change world state.
class WorldGenerationMemoryPressurePolicy {
public:
  explicit WorldGenerationMemoryPressurePolicy(WorldGenerationMemoryPressureLimits limits = {}) noexcept
    : m_limits(limits) {}
  WorldGenerationMemoryAdvice evaluate(WorldGenerationMemoryEnvelope const& usage,
      WorldGenerationMemoryDemand demand, size_t additionalBytes) noexcept;

private:
  WorldGenerationMemoryPressureLimits m_limits;
  // Invalid input rejects this advice, but cannot erase valid recovery state.
  WorldGenerationMemoryPressure m_pressure = WorldGenerationMemoryPressure::Unrestricted;
};

}
