#pragma once

#include "StarHash.hpp"
#include "StarMap.hpp"
#include "StarVector.hpp"
#include "StarWorldGenerationMemoryPressure.hpp"
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

struct WorldGenerationProductRetentionLimits {
  bool enabled = false;
  size_t resumeBytes = 0;
  size_t softBytes = 0;
  size_t hardBytes = 0;
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
  uint64_t lastAccessSequence = 0;
  WorldGenerationMemoryDemand demand = WorldGenerationMemoryDemand::Speculative;
  uint32_t benefit = 0;
  uint32_t activeConsumers = 0;
};

// Opt-in private product/revision cache with retention (RAM-03) and consumer
// reuse (RAM-05). Owner-thread only unless the caller synchronizes externally.
// Hit requires exact key equality. Active consumers are never evicted. Soft/hard
// byte limits drain speculative/cheap residents first and use hysteresis so a
// soft drain does not immediately re-admit the same speculation.
class WorldGenerationProductRevisionCache {
public:
  explicit WorldGenerationProductRevisionCache(size_t maxEntries = 64,
      WorldGenerationProductRetentionLimits retention = {})
    : m_maxEntries(maxEntries ? maxEntries : 1), m_retention(retention) {
    if (m_retention.enabled) {
      if (m_retention.hardBytes < m_retention.softBytes
          || m_retention.softBytes < m_retention.resumeBytes
          || m_retention.hardBytes == 0)
        m_retention.enabled = false;
    }
  }

  size_t maxEntries() const { return m_maxEntries; }
  size_t size() const { return m_entries.size(); }
  size_t retainedBytes() const { return m_retainedBytes; }
  uint64_t hits() const { return m_hits; }
  uint64_t misses() const { return m_misses; }
  uint64_t evictions() const { return m_evictions; }
  uint64_t invalidations() const { return m_invalidations; }
  uint64_t retentionDrains() const { return m_retentionDrains; }
  uint64_t refusedInserts() const { return m_refusedInserts; }
  bool retentionEnabled() const { return m_retention.enabled; }
  bool softPressureLatched() const { return m_softPressureLatched; }

  bool contains(WorldGenerationProductRevisionKey const& key) const {
    return m_entries.contains(key);
  }

  std::optional<WorldGenerationProductCacheEntry> find(WorldGenerationProductRevisionKey const& key) {
    if (auto* entry = m_entries.ptr(key)) {
      entry->lastAccessSequence = ++m_sequence;
      ++m_hits;
      return *entry;
    }
    ++m_misses;
    return std::nullopt;
  }

  std::optional<WorldGenerationProductCacheEntry> find(WorldGenerationProductRevisionKey const& key) const {
    if (auto* entry = m_entries.ptr(key)) {
      ++m_hits;
      return *entry;
    }
    ++m_misses;
    return std::nullopt;
  }

  bool insert(WorldGenerationProductRevisionKey const& key, size_t retainedBytes,
      WorldGenerationMemoryDemand demand = WorldGenerationMemoryDemand::Speculative,
      uint32_t benefit = 0) {
    if (retainedBytes == 0)
      return false;
    if (auto* existing = m_entries.ptr(key)) {
      m_retainedBytes -= existing->retainedBytes;
      existing->retainedBytes = retainedBytes;
      existing->demand = demand;
      existing->benefit = benefit;
      existing->insertionSequence = ++m_sequence;
      existing->lastAccessSequence = existing->insertionSequence;
      m_retainedBytes += retainedBytes;
      drainToLimits();
      return true;
    }
    if (m_retention.enabled && m_softPressureLatched
        && demand == WorldGenerationMemoryDemand::Speculative
        && m_retainedBytes + retainedBytes > m_retention.resumeBytes) {
      ++m_refusedInserts;
      return false;
    }
    while (m_entries.size() >= m_maxEntries) {
      if (!evictVictim(false)) {
        ++m_refusedInserts;
        return false;
      }
    }
    if (m_retention.enabled && m_retainedBytes + retainedBytes > m_retention.hardBytes) {
      while (m_retainedBytes + retainedBytes > m_retention.hardBytes) {
        if (!evictVictim(false)) {
          ++m_refusedInserts;
          return false;
        }
      }
    }
    WorldGenerationProductCacheEntry entry;
    entry.retainedBytes = retainedBytes;
    entry.insertionSequence = ++m_sequence;
    entry.lastAccessSequence = entry.insertionSequence;
    entry.demand = demand;
    entry.benefit = benefit;
    m_entries.add(key, entry);
    m_retainedBytes += retainedBytes;
    drainToLimits();
    return true;
  }

  bool acquireConsumer(WorldGenerationProductRevisionKey const& key) {
    if (auto* existing = m_entries.ptr(key)) {
      ++existing->activeConsumers;
      existing->lastAccessSequence = ++m_sequence;
      return true;
    }
    return false;
  }

  // Dropping a consumer never discards a still-referenced resident (RAM-05).
  bool releaseConsumer(WorldGenerationProductRevisionKey const& key) {
    if (auto* existing = m_entries.ptr(key)) {
      if (existing->activeConsumers == 0)
        return false;
      --existing->activeConsumers;
      return true;
    }
    return false;
  }

  bool erase(WorldGenerationProductRevisionKey const& key) {
    if (auto* existing = m_entries.ptr(key)) {
      if (existing->activeConsumers != 0)
        return false;
      m_retainedBytes -= existing->retainedBytes;
      m_entries.remove(key);
      ++m_invalidations;
      updateSoftLatch();
      return true;
    }
    return false;
  }

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
      if (drop && pair.second.activeConsumers == 0)
        doomed.append(pair.first);
    }
    for (auto const& key : doomed) {
      if (erase(key))
        ++removed;
    }
    return removed;
  }

  void clear() {
    List<WorldGenerationProductRevisionKey> doomed;
    for (auto const& pair : m_entries) {
      if (pair.second.activeConsumers == 0)
        doomed.append(pair.first);
    }
    for (auto const& key : doomed)
      erase(key);
    if (m_entries.empty())
      ++m_invalidations;
    updateSoftLatch();
  }

private:
  static int demandRank(WorldGenerationMemoryDemand demand) {
    switch (demand) {
      case WorldGenerationMemoryDemand::Visible: return 3;
      case WorldGenerationMemoryDemand::Dependency: return 2;
      case WorldGenerationMemoryDemand::Speculative: return 1;
      default: return 0;
    }
  }

  bool betterVictim(WorldGenerationProductCacheEntry const& candidate,
      WorldGenerationProductCacheEntry const& incumbent) const {
    if (demandRank(candidate.demand) != demandRank(incumbent.demand))
      return demandRank(candidate.demand) < demandRank(incumbent.demand);
    if (candidate.benefit != incumbent.benefit)
      return candidate.benefit < incumbent.benefit;
    return candidate.lastAccessSequence < incumbent.lastAccessSequence;
  }

  // softOnly: drain speculation first so visible/dependency residents are not
  // oscillated by soft pressure (RAM-03 hysteresis contract).
  bool evictVictim(bool softOnly) {
    if (m_entries.empty())
      return false;
    WorldGenerationProductRevisionKey victimKey;
    WorldGenerationProductCacheEntry const* victim = nullptr;
    for (auto const& pair : m_entries) {
      if (pair.second.activeConsumers != 0)
        continue;
      if (softOnly && pair.second.demand != WorldGenerationMemoryDemand::Speculative)
        continue;
      if (!victim || betterVictim(pair.second, *victim)) {
        victimKey = pair.first;
        victim = &pair.second;
      }
    }
    if (!victim)
      return false;
    m_retainedBytes -= victim->retainedBytes;
    m_entries.remove(victimKey);
    ++m_evictions;
    updateSoftLatch();
    return true;
  }

  void drainToLimits() {
    if (!m_retention.enabled)
      return;
    if (m_retainedBytes > m_retention.softBytes) {
      m_softPressureLatched = true;
      while (m_retainedBytes > m_retention.softBytes) {
        if (!evictVictim(true))
          break;
        ++m_retentionDrains;
      }
    }
    updateSoftLatch();
  }

  void updateSoftLatch() {
    if (!m_retention.enabled) {
      m_softPressureLatched = false;
      return;
    }
    if (m_retainedBytes <= m_retention.resumeBytes)
      m_softPressureLatched = false;
    else if (m_retainedBytes > m_retention.softBytes)
      m_softPressureLatched = true;
  }

  size_t m_maxEntries = 1;
  size_t m_retainedBytes = 0;
  WorldGenerationProductRetentionLimits m_retention;
  bool m_softPressureLatched = false;
  mutable uint64_t m_sequence = 0;
  mutable uint64_t m_hits = 0;
  mutable uint64_t m_misses = 0;
  uint64_t m_evictions = 0;
  uint64_t m_invalidations = 0;
  uint64_t m_retentionDrains = 0;
  uint64_t m_refusedInserts = 0;
  HashMap<WorldGenerationProductRevisionKey, WorldGenerationProductCacheEntry> m_entries;
};

}
