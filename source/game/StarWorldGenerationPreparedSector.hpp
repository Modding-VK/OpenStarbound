#pragma once

#include "StarWorldGenerationBaseTiles.hpp"
#include "StarWorldGenerationProductRevisionCache.hpp"
#include "StarWorldGenerationTasks.hpp"

#include <cstdint>
#include <optional>

namespace Star {

// CPU-02: versioned product identity for a private prepared sector (H2 resident
// cache handle) plus H1-shaped payload / WorldStorage path consumption APIs.
// Preparing never mutates the live world; probeHandoff never writes WorldStorage.
enum class WorldGenerationPreparedProduct : uint8_t {
  PrivateBaseTiles = 0,
  TerrainField = 1,
  OrderedCandidates = 2,
  EmptyProof = 3,
  StageDelta = 4
};

// H1-compatible payload kind tags for decode/promotion consumers.
enum class WorldGenerationPreparedProductKind : uint8_t {
  Empty = 0,
  OrderedCandidates,
  BaseTilesPrivate,
  TerrainFields,
  StepDelta
};

using WorldGenerationPreparedProductId = uint64_t;

// Compact product identity for payload/path views (H1 shape). Derived from the
// H2 sector identity; not a substitute for WorldGenerationPreparedSectorIdentity.
struct WorldGenerationPreparedProductIdentity {
  Vec2I sector;
  uint64_t templateRevision = 0;
  uint64_t distributionRevision = 0;
  uint64_t placementEpoch = 0;

  bool operator==(WorldGenerationPreparedProductIdentity const& other) const {
    return sector == other.sector && templateRevision == other.templateRevision
        && distributionRevision == other.distributionRevision
        && placementEpoch == other.placementEpoch;
  }
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
  // Opaque TerrainField / StageDelta payload metadata (H1 makeTerrainFields /
  // makeStepDelta). Ignored for BaseTiles/Empty/Candidates in this slice.
  uint64_t opaqueContentToken = 0;

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
        && productVersion == other.productVersion
        && opaqueContentToken == other.opaqueContentToken;
  }

  bool matches(WorldGenerationPreparedSectorIdentity const& expectation) const {
    return valid() && expectation.valid() && *this == expectation;
  }

  WorldGenerationProductRevisionKey residentKey() const {
    // assetGeneration stays 0 for BaseTiles/Empty/Candidates so existing H2
    // cache keys remain stable; opaque tokens distinguish Terrain/Delta only.
    uint64_t assetGeneration = 0;
    if (product == WorldGenerationPreparedProduct::TerrainField
        || product == WorldGenerationPreparedProduct::StageDelta)
      assetGeneration = opaqueContentToken;
    return {
        product == WorldGenerationPreparedProduct::PrivateBaseTiles
            ? WorldGenerationResidentProductKind::PrivateBaseTiles
            : product == WorldGenerationPreparedProduct::TerrainField
                ? WorldGenerationResidentProductKind::PureField
                : product == WorldGenerationPreparedProduct::OrderedCandidates
                    ? WorldGenerationResidentProductKind::CandidatePlan
                    : product == WorldGenerationPreparedProduct::EmptyProof
                        ? WorldGenerationResidentProductKind::StaticStructure
                        : WorldGenerationResidentProductKind::LiveSnapshot,
        task.sector,
        templateRevision,
        tileRevision,
        assetGeneration};
  }

  WorldGenerationPreparedProductKind payloadKind() const {
    switch (product) {
      case WorldGenerationPreparedProduct::PrivateBaseTiles:
        return WorldGenerationPreparedProductKind::BaseTilesPrivate;
      case WorldGenerationPreparedProduct::TerrainField:
        return WorldGenerationPreparedProductKind::TerrainFields;
      case WorldGenerationPreparedProduct::OrderedCandidates:
        return WorldGenerationPreparedProductKind::OrderedCandidates;
      case WorldGenerationPreparedProduct::EmptyProof:
        return WorldGenerationPreparedProductKind::Empty;
      case WorldGenerationPreparedProduct::StageDelta:
        return WorldGenerationPreparedProductKind::StepDelta;
    }
    return WorldGenerationPreparedProductKind::Empty;
  }

  WorldGenerationPreparedProductIdentity productIdentity() const {
    return {Vec2I((int)task.sector[0], (int)task.sector[1]), templateRevision, 0,
        task.worldEpoch};
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

// Opaque immutable payload descriptor for H2 decode/promotion. Not a live
// pointer into WorldStorage or a worker buffer.
struct WorldGenerationPreparedPayloadView {
  WorldGenerationPreparedProductId productId = 0;
  WorldGenerationPreparedProductKind kind = WorldGenerationPreparedProductKind::Empty;
  WorldGenerationPreparedProductIdentity identity;
  size_t payloadBytes = 0;
  uint64_t contentToken = 0;
  bool installAuthorized = false;
};

// Logical WorldStorage path key. Sector + epoch + product id; no filesystem
// path and no mutation of WorldStorage.
struct WorldGenerationPreparedWorldStoragePath {
  uint64_t worldEpoch = 0;
  Vec2I sector;
  WorldGenerationPreparedProductId productId = 0;
  WorldGenerationPreparedProductKind kind = WorldGenerationPreparedProductKind::Empty;

  bool operator==(WorldGenerationPreparedWorldStoragePath const& other) const {
    return worldEpoch == other.worldEpoch && sector == other.sector && productId == other.productId
        && kind == other.kind;
  }
};

enum class WorldGenerationPreparedHandoffOutcome : uint8_t {
  Ready = 0,
  MissingProduct,
  StaleIdentity,
  NotAuthorized,
  EmptyPayload
};

struct WorldGenerationPreparedHandoff {
  WorldGenerationPreparedHandoffOutcome outcome = WorldGenerationPreparedHandoffOutcome::MissingProduct;
  WorldGenerationPreparedWorldStoragePath path;
  WorldGenerationPreparedPayloadView payload;
  // True only when Ready; still does not write WorldStorage in this slice.
  bool writesWorldStorage = false;
};

// Owner-local prepared handle: retains a resident cache entry by identity and
// never installs. Consumes H1 payload/WorldStorage path shapes without a live
// store write. Stale expectation → refuse consume / StaleIdentity handoff.
class WorldGenerationPreparedSector {
public:
  static std::optional<WorldGenerationPreparedSector> tryPrepare(
      WorldGenerationProductRevisionCache& cache,
      WorldGenerationPreparedSectorIdentity identity,
      size_t retainedBytes,
      WorldGenerationMemoryDemand demand = WorldGenerationMemoryDemand::Dependency,
      uint32_t benefit = 1) {
    if (!identity.valid())
      return std::nullopt;
    // EmptyProof payload bytes are zero; retain a sentinel budget so the
    // resident entry exists for path/view without implying WorldStorage writes.
    if (identity.product == WorldGenerationPreparedProduct::EmptyProof && retainedBytes == 0)
      retainedBytes = 1;
    if (retainedBytes == 0)
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

  // Stable id for path/view (hash of resident key). Not a store allocation id.
  WorldGenerationPreparedProductId productId() const {
    if (!active())
      return 0;
    return hashResidentKey(m_identity.residentKey());
  }

  // H1 payloadView / worldStoragePath / probeHandoff consumption. Never writes.
  WorldGenerationPreparedPayloadView payloadView() const {
    WorldGenerationPreparedPayloadView view;
    if (!active())
      return view;
    view.productId = productId();
    view.kind = m_identity.payloadKind();
    view.identity = m_identity.productIdentity();
    view.installAuthorized = false;
    switch (m_identity.product) {
      case WorldGenerationPreparedProduct::EmptyProof:
        view.payloadBytes = 0;
        view.contentToken = hashResidentKey(m_identity.residentKey());
        break;
      case WorldGenerationPreparedProduct::OrderedCandidates:
        view.payloadBytes = m_retainedBytes;
        view.contentToken = hashResidentKey(m_identity.residentKey()) ^ (uint64_t)m_retainedBytes;
        break;
      case WorldGenerationPreparedProduct::PrivateBaseTiles:
        view.payloadBytes = m_retainedBytes;
        view.contentToken = hashResidentKey(m_identity.residentKey()) ^ m_identity.tileRevision
            ^ (m_identity.templateRevision << 1);
        break;
      case WorldGenerationPreparedProduct::TerrainField:
      case WorldGenerationPreparedProduct::StageDelta:
        view.payloadBytes = m_retainedBytes;
        view.contentToken = m_identity.opaqueContentToken
            ? m_identity.opaqueContentToken
            : hashResidentKey(m_identity.residentKey());
        break;
    }
    return view;
  }

  WorldGenerationPreparedWorldStoragePath worldStoragePath(uint64_t worldEpoch) const {
    WorldGenerationPreparedWorldStoragePath path;
    path.worldEpoch = worldEpoch;
    if (!active())
      return path;
    path.productId = productId();
    path.sector = m_identity.productIdentity().sector;
    path.kind = m_identity.payloadKind();
    return path;
  }

  WorldGenerationPreparedHandoff probeHandoff(uint64_t worldEpoch,
      WorldGenerationPreparedSectorIdentity const& liveExpectation) const {
    WorldGenerationPreparedHandoff handoff;
    handoff.path = worldStoragePath(worldEpoch);
    if (!active()) {
      handoff.outcome = WorldGenerationPreparedHandoffOutcome::MissingProduct;
      return handoff;
    }
    handoff.payload = payloadView();
    if (!m_identity.matches(liveExpectation)) {
      handoff.outcome = WorldGenerationPreparedHandoffOutcome::StaleIdentity;
      return handoff;
    }
    // Shadow BaseTiles never authorize install in this H2 slice.
    if (m_identity.product == WorldGenerationPreparedProduct::PrivateBaseTiles) {
      handoff.outcome = WorldGenerationPreparedHandoffOutcome::NotAuthorized;
      handoff.writesWorldStorage = false;
      return handoff;
    }
    if (m_identity.product != WorldGenerationPreparedProduct::EmptyProof
        && m_identity.product != WorldGenerationPreparedProduct::OrderedCandidates
        && handoff.payload.payloadBytes == 0) {
      handoff.outcome = WorldGenerationPreparedHandoffOutcome::EmptyPayload;
      return handoff;
    }
    handoff.outcome = WorldGenerationPreparedHandoffOutcome::Ready;
    handoff.writesWorldStorage = false;
    return handoff;
  }

private:
  WorldGenerationPreparedSector(WorldGenerationPreparedSectorIdentity identity,
      WorldGenerationProductRevisionCache* cache, size_t retainedBytes)
    : m_identity(std::move(identity)), m_cache(cache), m_retainedBytes(retainedBytes) {}

  static uint64_t hashResidentKey(WorldGenerationProductRevisionKey const& key) {
    uint64_t h = 14695981039346656037ull;
    auto mix = [&](uint64_t v) {
      h ^= v;
      h *= 1099511628211ull;
    };
    mix((uint64_t)key.kind);
    mix((uint64_t)key.sector[0]);
    mix((uint64_t)key.sector[1]);
    mix(key.templateRevision);
    mix(key.tileRevision);
    mix(key.assetGeneration);
    return h ? h : 1;
  }

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
