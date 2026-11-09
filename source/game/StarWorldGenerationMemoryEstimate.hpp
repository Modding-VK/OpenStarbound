#pragma once

#include "StarJson.hpp"
#include "StarWorldGenerationMemory.hpp"

#include <vector>

namespace Star {

// Version 7 includes complete bucket-vector payloads and damage-factor values.
// It is neither allocator accounting nor an upper bound on retained heap.
struct WorldGenerationSizeEstimate {
  static constexpr unsigned ModelVersion = 7;
  size_t bytes = 0;
  bool complete = true;

  void add(size_t count, size_t elementBytes = 1);
  void stringCapacity(String const& string);
};

struct WorldGenerationBackingEstimate {
  WorldGenerationSizeEstimate storage;
  size_t opaqueItems = 0;

  void add(WorldGenerationBackingEstimate const& other) {
    storage.add(other.storage.bytes);
    storage.complete = storage.complete && other.storage.complete;
    if (other.opaqueItems > std::numeric_limits<size_t>::max() - opaqueItems)
      storage.complete = false;
    else opaqueItems += other.opaqueItems;
  }
};

struct WorldGenerationJsonSnapshot {
  // Destroy the Json before releasing the tokens for tracked backing nodes.
  std::vector<std::shared_ptr<WorldGenerationMemoryReservation>> accounting;
  Json value;
  bool measured = false;
  bool complete = false;
  size_t visitedValues = 0;
};

using WorldGenerationJsonSnapshotPtr = std::shared_ptr<WorldGenerationJsonSnapshot const>;

WorldGenerationJsonSnapshotPtr captureWorldGenerationJson(WorldGenerationMemoryLedger& ledger,
    Json value, bool measure, size_t maximumValues = 32768);

}
