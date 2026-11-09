#pragma once

#include "StarJson.hpp"
#include "StarWorldGenerationMemory.hpp"

#include <vector>

namespace Star {

// Version 9 adds shared Json roots retained by queued distribution bundles.
// It is neither allocator accounting nor an upper bound on retained heap.
struct WorldGenerationSizeEstimate {
  static constexpr unsigned ModelVersion = 9;
  static constexpr unsigned JsonModelVersion = 2;
  static constexpr unsigned DistributionJsonModelVersion = 1;
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
  // Single-value captures use value; forest captures retain only root handles,
  // not a synthetic JsonArray or copies of the containers holding these roots.
  Json value;
  std::vector<Json> roots;
  bool measured = false;
  bool complete = false;
  size_t visitedValues = 0;
  size_t sizedObjectKeys = 0;
  // No partial byte model is registered for an object whose keys do not fit
  // the traversal allowance. Another capture can measure that owner fully.
  size_t omittedObjects = 0;
  bool rootsComplete = true;
};

using WorldGenerationJsonSnapshotPtr = std::shared_ptr<WorldGenerationJsonSnapshot const>;

// Bounded heap-root collection. Scalar roots have no backing and are skipped.
// A false append result stops enumeration, without filtering generation inputs.
class WorldGenerationJsonRoots {
public:
  explicit WorldGenerationJsonRoots(size_t maximumValues = 32768) : m_maximumValues(maximumValues) {}
  bool append(Json const& value);
  bool empty() const noexcept { return m_values.empty(); }
  bool complete() const noexcept { return m_complete; }
private:
  friend WorldGenerationJsonSnapshotPtr captureWorldGenerationJsonRoots(
      WorldGenerationMemoryLedger&, WorldGenerationJsonRoots);
  std::vector<Json> m_values;
  size_t m_maximumValues;
  bool m_complete = true;
};

WorldGenerationJsonSnapshotPtr captureWorldGenerationJsonRoots(
    WorldGenerationMemoryLedger& ledger, WorldGenerationJsonRoots roots);

WorldGenerationJsonSnapshotPtr captureWorldGenerationJson(WorldGenerationMemoryLedger& ledger,
    Json value, bool measure, size_t maximumValues = 32768);

}
