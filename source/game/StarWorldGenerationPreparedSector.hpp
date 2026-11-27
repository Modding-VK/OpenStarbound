#pragma once

#include "StarWorldGenerationBaseTiles.hpp"
#include "StarWorldGenerationProductRevisionCache.hpp"
#include "StarWorldGenerationTasks.hpp"

#include <cstdint>
#include <optional>

namespace Star {

// CPU-02 first slice: versioned product identity for a private prepared sector.
// Preparing never mutates the live world; consume requires an owner revalidation
// of ticket/revisions. Not a full pipeline product set (fields/candidates/empty
// deltas remain future slices).
enum class WorldGenerationPreparedProduct : uint8_t {
  PrivateBaseTiles = 0,
  TerrainField = 1,
  OrderedCandidates = 2,
  EmptyProof = 3,
  StageDelta = 4
};

struct WorldGenerationPreparedSectorIdentity {
  static constexpr unsigned ModelVersion = 1;
  static constexpr unsigned ProductVersion = 1;
  WorldGenerationPreparedProduct product = WorldGenerationPreparedProduct::PrivateBaseTiles;
  WorldGenerationTaskIdentity task{};
  uint64_t tileRevision = 0;
  uint64_t templateRevision = 0;
  Vec2U worldSize{};
  RectI tileRegion;
  unsigned productVersion = ProductVersion;

  bool valid() const {
    return productVersion == ProductVersion
        && worldSize[0] != 0 && worldSize[1] != 0
        && tileRegion.isNull() == false
        && tileRevision != 0;
  }

  bool operator==(WorldGenerationPreparedSectorIdentity const& other) const {
    return product == other.product
        && task == other.task
        && tileRevision == other.tileRevision
        && templateRevision == other.templateRevision
        && worldSize == other.worldSize
        && tileRegion == other.tileRegion
        && productVersion == other.productVersion;
  }

  bool matches(WorldGenerationPreparedSectorIdentity const& expectation) const {
    return valid() && expectation.valid() && *this == expectation;
  }

  WorldGenerationProductRevisionKey residentKey() const {
    return {
        product == WorldGenerationPreparedProduct::PrivateBaseTiles
            ? WorldGenerationResidentProductKind::PrivateBaseTiles
            : product == WorldGenerationPreparedProduct::TerrainField
                ? WorldGenerationResidentProductKind::PureField
                : product == WorldGenerationPreparedProduct::OrderedCandidates
                    ? WorldGenerationResidentProductKind::CandidatePlan
                    : WorldGenerationResidentProductKind::LiveSnapshot,
        task.sector,
        templateRevision,
        tileRevision,
        0};
  }

  static WorldGenerationPreparedSectorIdentity fromBaseTiles(WorldGenerationBaseTilesIdentity const& base) {
    WorldGenerationPreparedSectorIdentity out;
    out.product = WorldGenerationPreparedProduct::PrivateBaseTiles;
    out.task = base.task;
    out.tileRevision = base.tileRevision;
    out.templateRevision = base.templateRevision;
    out.worldSize = base.worldSize;
    out.tileRegion = base.tileRegion;
    return out;
  }
};

// Owner-local prepared handle: retains a resident cache entry by identity and
// never installs. Stale expectation → refuse consume without mutating live tiles.
class WorldGenerationPreparedSector {
public:
  static std::optional<WorldGenerationPreparedSector> tryPrepare(
      WorldGenerationProductRevisionCache& cache,
      WorldGenerationPreparedSectorIdentity identity,
      size_t retainedBytes,
      WorldGenerationMemoryDemand demand = WorldGenerationMemoryDemand::Dependency,
      uint32_t benefit = 1) {
    if (!identity.valid() || retainedBytes == 0)
      return std::nullopt;
    if (!cache.insert(identity.residentKey(), retainedBytes, demand, benefit))
      return std::nullopt;
    if (!cache.acquireConsumer(identity.residentKey())) {
      cache.erase(identity.residentKey());
      return std::nullopt;
    }
    return WorldGenerationPreparedSector(std::move(identity), &cache, retainedBytes);
  }

  WorldGenerationPreparedSector(WorldGenerationPreparedSector const&) = delete;
  WorldGenerationPreparedSector& operator=(WorldGenerationPreparedSector const&) = delete;

  WorldGenerationPreparedSector(WorldGenerationPreparedSector&& other) noexcept {
    moveFrom(std::move(other));
  }

  WorldGenerationPreparedSector& operator=(WorldGenerationPreparedSector&& other) noexcept {
    if (this != &other) {
      release();
      moveFrom(std::move(other));
    }
    return *this;
  }

  ~WorldGenerationPreparedSector() { release(); }

  WorldGenerationPreparedSectorIdentity const& identity() const { return m_identity; }
  size_t retainedBytes() const { return m_retainedBytes; }
  bool active() const { return m_cache != nullptr; }

  bool matches(WorldGenerationPreparedSectorIdentity const& expectation) const {
    return active() && m_identity.matches(expectation);
  }

  // Owner revalidation only. Success does not publish; it only confirms the
  // private product is still the expected revision for a later install path.
  bool tryConsume(WorldGenerationPreparedSectorIdentity const& liveExpectation) {
    if (!active() || !m_identity.matches(liveExpectation))
      return false;
    return true;
  }

  void discard() { release(); }

private:
  WorldGenerationPreparedSector(WorldGenerationPreparedSectorIdentity identity,
      WorldGenerationProductRevisionCache* cache, size_t retainedBytes)
    : m_identity(std::move(identity)), m_cache(cache), m_retainedBytes(retainedBytes) {}

  void moveFrom(WorldGenerationPreparedSector&& other) noexcept {
    m_identity = std::move(other.m_identity);
    m_cache = other.m_cache;
    m_retainedBytes = other.m_retainedBytes;
    other.m_cache = nullptr;
    other.m_retainedBytes = 0;
    other.m_identity = {};
  }

  void release() {
    if (!m_cache)
      return;
    m_cache->releaseConsumer(m_identity.residentKey());
    m_cache->erase(m_identity.residentKey());
    m_cache = nullptr;
    m_retainedBytes = 0;
    m_identity = {};
  }

  WorldGenerationPreparedSectorIdentity m_identity;
  WorldGenerationProductRevisionCache* m_cache = nullptr;
  size_t m_retainedBytes = 0;
};

}
