#pragma once

#include "StarWorldGenerationMemory.hpp"
#include "StarWorldGenerationMemoryPressure.hpp"

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

}
