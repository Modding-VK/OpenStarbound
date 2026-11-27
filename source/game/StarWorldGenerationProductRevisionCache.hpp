#pragma once

#include "StarHash.hpp"
#include "StarMap.hpp"
#include "StarVector.hpp"
#include "StarWorldGenerationTasks.hpp"

#include <cstdint>
#include <optional>
#include <utility>

namespace Star {

// RAM-02 layer tags. Conceptual only: this cache never widens a product's
// validity and never substitutes a modified saved sector or live incarnation.
enum class WorldGenerationResidentProductKind : uint8_t {
  PrivateBaseTiles = 0,
  PureField = 1,
  CandidatePlan = 2,
  DecodeStaging = 3,
  StaticStructure = 4,
  LiveSnapshot = 5
};

enum class WorldGenerationProductCacheInvalidation : uint8_t {
  TemplateRevision = 0,
  TileRevision = 1,
  AssetGeneration = 2,
  All = 3
};

// Minimum resident key: product kind + sector + revisions that gate reuse.
// Not a content hash, PreparedSector payload, or publish ticket.
struct WorldGenerationProductRevisionKey {
  WorldGenerationResidentProductKind kind = WorldGenerationResidentProductKind::PrivateBaseTiles;
  Vec2U sector{};
  uint64_t templateRevision = 0;
  uint64_t tileRevision = 0;
  uint64_t assetGeneration = 0;

  bool operator==(WorldGenerationProductRevisionKey const& other) const {
    return kind == other.kind
        && sector == other.sector
        && templateRevision == other.templateRevision
        && tileRevision == other.tileRevision
        && assetGeneration == other.assetGeneration;
  }
  bool operator!=(WorldGenerationProductRevisionKey const& other) const { return !(*this == other); }
};

template <>
struct hash<WorldGenerationProductRevisionKey> {
  size_t operator()(WorldGenerationProductRevisionKey const& key) const {
    size_t h = hash<WorldGenerationResidentProductKind>()(key.kind);
    hashCombine(h, hash<Vec2U>()(key.sector));
    hashCombine(h, hash<uint64_t>()(key.templateRevision));
    hashCombine(h, hash<uint64_t>()(key.tileRevision));
    hashCombine(h, hash<uint64_t>()(key.assetGeneration));
    return h;
  }
};

struct WorldGenerationProductCacheEntry {
  size_t retainedBytes = 0;
  uint64_t insertionSequence = 0;
};

// Opt-in private product/revision cache. Owner-thread only unless the caller
// provides external synchronization. Hit requires exact key equality; a
// revision bump cannot revive a stale entry. Capacity eviction drops the
// oldest insertion and never claims saved-sector authority.
class WorldGenerationProductRevisionCache {
public:
  explicit WorldGenerationProductRevisionCache(size_t maxEntries = 64) : m_maxEntries(maxEntries ? maxEntries : 1) {}

  size_t maxEntries() const { return m_maxEntries; }
  size_t size() const { return m_entries.size(); }
  size_t retainedBytes() const { return m_retainedBytes; }
  uint64_t hits() const { return m_hits; }
  uint64_t misses() const { return m_misses; }
  uint64_t evictions() const { return m_evictions; }
  uint64_t invalidations() const { return m_invalidations; }

  bool contains(WorldGenerationProductRevisionKey const& key) const {
    return m_entries.contains(key);
  }

  std::optional<WorldGenerationProductCacheEntry> find(WorldGenerationProductRevisionKey const& key) const {
    if (auto* entry = m_entries.ptr(key)) {
      ++m_hits;
      return *entry;
    }
    ++m_misses;
    return std::nullopt;
  }

  // Insert or replace an exact key. Replacing the same key does not evict
  // another resident. Returns false only when retainedBytes is zero (no
  // empty placeholder that could look like a free saved sector).
  bool insert(WorldGenerationProductRevisionKey const& key, size_t retainedBytes) {
    if (retainedBytes == 0)
      return false;
    if (auto* existing = m_entries.ptr(key)) {
      m_retainedBytes -= existing->retainedBytes;
      existing->retainedBytes = retainedBytes;
      existing->insertionSequence = ++m_sequence;
      m_retainedBytes += retainedBytes;
      return true;
    }
    while (m_entries.size() >= m_maxEntries)
      evictOldest();
    WorldGenerationProductCacheEntry entry{retainedBytes, ++m_sequence};
    m_entries.add(key, entry);
    m_retainedBytes += retainedBytes;
    return true;
  }

  bool erase(WorldGenerationProductRevisionKey const& key) {
    if (auto* existing = m_entries.ptr(key)) {
      m_retainedBytes -= existing->retainedBytes;
      m_entries.remove(key);
      ++m_invalidations;
      return true;
    }
    return false;
  }

  // Drop entries whose matching revision field differs from the live value.
  // Exact matches stay; this never republishes or widens validity.
  size_t invalidate(WorldGenerationProductCacheInvalidation kind, uint64_t liveValue) {
    size_t removed = 0;
    List<WorldGenerationProductRevisionKey> doomed;
    for (auto const& pair : m_entries) {
      bool drop = false;
      switch (kind) {
        case WorldGenerationProductCacheInvalidation::TemplateRevision:
          drop = pair.first.templateRevision != liveValue;
          break;
        case WorldGenerationProductCacheInvalidation::TileRevision:
          drop = pair.first.tileRevision != liveValue;
          break;
        case WorldGenerationProductCacheInvalidation::AssetGeneration:
          drop = pair.first.assetGeneration != liveValue;
          break;
        case WorldGenerationProductCacheInvalidation::All:
          drop = true;
          break;
      }
      if (drop)
        doomed.append(pair.first);
    }
    for (auto const& key : doomed) {
      if (erase(key))
        ++removed;
    }
    return removed;
  }

  void clear() {
    m_entries.clear();
    m_retainedBytes = 0;
    ++m_invalidations;
  }

private:
  void evictOldest() {
    if (m_entries.empty())
      return;
    WorldGenerationProductRevisionKey oldestKey;
    uint64_t oldestSequence = (uint64_t)-1;
    bool found = false;
    for (auto const& pair : m_entries) {
      if (!found || pair.second.insertionSequence < oldestSequence) {
        oldestKey = pair.first;
        oldestSequence = pair.second.insertionSequence;
        found = true;
      }
    }
    if (!found)
      return;
    if (auto* existing = m_entries.ptr(oldestKey)) {
      m_retainedBytes -= existing->retainedBytes;
      m_entries.remove(oldestKey);
      ++m_evictions;
    }
  }

  size_t m_maxEntries = 1;
  size_t m_retainedBytes = 0;
  uint64_t m_sequence = 0;
  mutable uint64_t m_hits = 0;
  mutable uint64_t m_misses = 0;
  uint64_t m_evictions = 0;
  uint64_t m_invalidations = 0;
  HashMap<WorldGenerationProductRevisionKey, WorldGenerationProductCacheEntry> m_entries;
};

}
