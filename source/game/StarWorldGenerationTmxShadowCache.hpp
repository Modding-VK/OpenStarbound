#pragma once

#include "StarHash.hpp"
#include "StarList.hpp"
#include "StarMap.hpp"
#include "StarWorldGenerationTmxShadowAssets.hpp"

#include <optional>
#include <utility>

namespace Star {

// MD-02 cache/reload shadow slice: keyed private TMX static products with a
// retained-byte budget. Keys include asset generation and format/product
// versions. Explicit reload clears entries so a mod reload cannot reuse stale
// products. No Root/Assets live path and no facade publish.
struct WorldGenerationTmxShadowCacheKey {
  String assetKey;
  uint64_t assetGeneration = 0;
  unsigned formatVersion = WorldGenerationTmxStaticIdentity::FormatVersion;
  unsigned productVersion = WorldGenerationTmxStaticIdentity::ModelVersion;

  bool valid() const {
    return !assetKey.empty()
        && assetGeneration != 0
        && formatVersion == WorldGenerationTmxStaticIdentity::FormatVersion
        && productVersion == WorldGenerationTmxStaticIdentity::ModelVersion;
  }

  bool operator==(WorldGenerationTmxShadowCacheKey const& other) const {
    return assetKey == other.assetKey
        && assetGeneration == other.assetGeneration
        && formatVersion == other.formatVersion
        && productVersion == other.productVersion;
  }

  static WorldGenerationTmxShadowCacheKey fromIdentity(WorldGenerationTmxStaticIdentity const& identity) {
    WorldGenerationTmxShadowCacheKey key;
    key.assetKey = identity.assetKey;
    key.assetGeneration = identity.assetGeneration;
    key.formatVersion = identity.formatVersion;
    key.productVersion = identity.productVersion;
    return key;
  }
};

template <>
struct hash<WorldGenerationTmxShadowCacheKey> {
  size_t operator()(WorldGenerationTmxShadowCacheKey const& key) const {
    size_t h = hash<String>()(key.assetKey);
    hashCombine(h, (size_t)key.assetGeneration);
    hashCombine(h, (size_t)key.formatVersion);
    hashCombine(h, (size_t)key.productVersion);
    return h;
  }
};

enum class WorldGenerationTmxShadowCacheStatus : uint8_t {
  Hit = 0,
  MissInserted = 1,
  MissEvictedInsert = 2,
  DecodeFailed = 3,
  InvalidKey = 4,
  OverBudget = 5,
  Missing = 6
};

struct WorldGenerationTmxShadowCacheStats {
  uint64_t hits = 0;
  uint64_t misses = 0;
  uint64_t inserts = 0;
  uint64_t evictions = 0;
  uint64_t invalidations = 0;
  uint64_t reloads = 0;
  size_t entryCount = 0;
  size_t retainedBytes = 0;
};

struct WorldGenerationTmxShadowCacheLookup {
  WorldGenerationTmxShadowCacheStatus status = WorldGenerationTmxShadowCacheStatus::Missing;
  WorldGenerationTmxStaticProduct const* product = nullptr;
  uint64_t reloadEpoch = 0;
};

class WorldGenerationTmxShadowCache {
public:
  explicit WorldGenerationTmxShadowCache(size_t budgetBytes)
    : m_budgetBytes(budgetBytes) {}

  size_t budgetBytes() const { return m_budgetBytes; }
  uint64_t reloadEpoch() const { return m_reloadEpoch; }
  WorldGenerationTmxShadowCacheStats stats() const {
    WorldGenerationTmxShadowCacheStats out = m_stats;
    out.entryCount = m_entries.size();
    out.retainedBytes = m_retainedBytes;
    return out;
  }
  size_t retainedBytes() const { return m_retainedBytes; }
  size_t size() const { return m_entries.size(); }

  WorldGenerationTmxShadowCacheLookup tryGet(WorldGenerationTmxShadowCacheKey const& key) {
    WorldGenerationTmxShadowCacheLookup out;
    out.reloadEpoch = m_reloadEpoch;
    if (!key.valid()) {
      out.status = WorldGenerationTmxShadowCacheStatus::InvalidKey;
      return out;
    }
    if (auto* entry = m_entries.ptr(key)) {
      ++m_stats.hits;
      touch(key);
      out.status = WorldGenerationTmxShadowCacheStatus::Hit;
      out.product = &entry->product;
      return out;
    }
    ++m_stats.misses;
    out.status = WorldGenerationTmxShadowCacheStatus::Missing;
    return out;
  }

  WorldGenerationTmxShadowCacheLookup tryInsert(WorldGenerationTmxStaticProduct product) {
    WorldGenerationTmxShadowCacheLookup out;
    out.reloadEpoch = m_reloadEpoch;
    if (!product.active()) {
      out.status = WorldGenerationTmxShadowCacheStatus::DecodeFailed;
      return out;
    }
    auto key = WorldGenerationTmxShadowCacheKey::fromIdentity(product.identity());
    if (!key.valid()) {
      out.status = WorldGenerationTmxShadowCacheStatus::InvalidKey;
      return out;
    }
    if (auto* existing = m_entries.ptr(key)) {
      // Same key: treat as hit / refresh without double-counting bytes.
      size_t oldBytes = existing->bytes;
      existing->product = std::move(product);
      existing->bytes = existing->product.retainedBytesEstimate();
      existing->insertEpoch = m_reloadEpoch;
      if (m_retainedBytes >= oldBytes)
        m_retainedBytes -= oldBytes;
      else
        m_retainedBytes = 0;
      m_retainedBytes += existing->bytes;
      touch(key);
      ++m_stats.hits;
      out.status = WorldGenerationTmxShadowCacheStatus::Hit;
      out.product = &existing->product;
      return out;
    }

    size_t bytes = product.retainedBytesEstimate();
    if (bytes > m_budgetBytes) {
      out.status = WorldGenerationTmxShadowCacheStatus::OverBudget;
      return out;
    }
    size_t evicted = 0;
    while (m_retainedBytes + bytes > m_budgetBytes && !m_lru.empty()) {
      if (!evictOldest())
        break;
      ++evicted;
    }
    if (m_retainedBytes + bytes > m_budgetBytes) {
      out.status = WorldGenerationTmxShadowCacheStatus::OverBudget;
      return out;
    }

    m_entries.add(key, Entry{std::move(product), bytes, m_reloadEpoch});
    m_lru.append(key);
    m_retainedBytes += bytes;
    ++m_stats.inserts;
    if (evicted)
      out.status = WorldGenerationTmxShadowCacheStatus::MissEvictedInsert;
    else
      out.status = WorldGenerationTmxShadowCacheStatus::MissInserted;
    out.product = &m_entries.get(key).product;
    return out;
  }

  WorldGenerationTmxShadowCacheLookup tryGetOrDecode(
      WorldGenerationTmxStaticDecodeRequest request, WorldGenerationTmxShadowAssetStore const& assets) {
    WorldGenerationTmxShadowCacheKey key;
    key.assetKey = request.assetKey;
    key.assetGeneration = request.assetGeneration;
    auto hit = tryGet(key);
    if (hit.status == WorldGenerationTmxShadowCacheStatus::Hit
        || hit.status == WorldGenerationTmxShadowCacheStatus::InvalidKey)
      return hit;

    auto product = tryDecodeWorldGenerationTmxStaticProductWithAssets(std::move(request), assets);
    if (!product)
      return {WorldGenerationTmxShadowCacheStatus::DecodeFailed, nullptr, m_reloadEpoch};
    return tryInsert(std::move(*product));
  }

  bool invalidate(WorldGenerationTmxShadowCacheKey const& key) {
    if (!m_entries.contains(key))
      return false;
    drop(key);
    ++m_stats.invalidations;
    return true;
  }

  size_t invalidateAll() {
    size_t count = m_entries.size();
    m_entries.clear();
    m_lru.clear();
    m_retainedBytes = 0;
    m_stats.invalidations += count;
    return count;
  }

  // Explicit mod-reload barrier: bump epoch and drop every retained product.
  uint64_t reload() {
    invalidateAll();
    ++m_reloadEpoch;
    if (m_reloadEpoch == 0)
      m_reloadEpoch = 1;
    ++m_stats.reloads;
    return m_reloadEpoch;
  }

private:
  struct Entry {
    Entry(WorldGenerationTmxStaticProduct productIn, size_t bytesIn, uint64_t epochIn)
      : product(std::move(productIn)), bytes(bytesIn), insertEpoch(epochIn) {}

    WorldGenerationTmxStaticProduct product;
    size_t bytes = 0;
    uint64_t insertEpoch = 0;
  };

  void touch(WorldGenerationTmxShadowCacheKey const& key) {
    for (size_t i = 0; i < m_lru.size(); ++i) {
      if (m_lru[i] == key) {
        m_lru.eraseAt(i);
        break;
      }
    }
    m_lru.append(key);
  }

  bool evictOldest() {
    if (m_lru.empty())
      return false;
    auto key = m_lru[0];
    drop(key);
    ++m_stats.evictions;
    return true;
  }

  void drop(WorldGenerationTmxShadowCacheKey const& key) {
    if (auto* entry = m_entries.ptr(key)) {
      if (m_retainedBytes >= entry->bytes)
        m_retainedBytes -= entry->bytes;
      else
        m_retainedBytes = 0;
      m_entries.remove(key);
    }
    for (size_t i = 0; i < m_lru.size(); ++i) {
      if (m_lru[i] == key) {
        m_lru.eraseAt(i);
        break;
      }
    }
  }

  size_t m_budgetBytes = 0;
  uint64_t m_reloadEpoch = 1;
  size_t m_retainedBytes = 0;
  HashMap<WorldGenerationTmxShadowCacheKey, Entry> m_entries;
  List<WorldGenerationTmxShadowCacheKey> m_lru;
  WorldGenerationTmxShadowCacheStats m_stats;
};

}
