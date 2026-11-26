#pragma once

#include "StarWorldGenerationMemory.hpp"
#include "StarWorldGenerationSelectorSnapshot.hpp"

#include <cmath>
#include <cstring>
#include <limits>
#include <optional>
#include <utility>

namespace Star {

// Per-admission identity for owner-side region selector capture. Not a content
// key, asset lease, BaseTiles ticket or permission to commit. Callers must set
// every field before capture: legacy TerrainSelectorParameters default still
// leaves baseHeight uninitialized until the owner assigns it.
struct WorldGenerationSelectorContextIdentity {
  static constexpr unsigned ModelVersion = 1;
  uint64_t worldEpoch = 0;
  uint64_t sequence = 0;
  uint64_t templateRevision = 0;
  unsigned worldWidth = 0;
  float baseHeight = 0;
  uint64_t regionSeed = 0;
  unsigned modelVersion = ModelVersion;

  bool valid() const {
    if (!worldEpoch || !sequence || !worldWidth || modelVersion != ModelVersion)
      return false;
    if (worldWidth > unsigned(std::numeric_limits<int>::max()))
      return false;
    return std::isfinite(baseHeight);
  }

  bool operator==(WorldGenerationSelectorContextIdentity const& other) const {
    if (worldEpoch != other.worldEpoch || sequence != other.sequence
        || templateRevision != other.templateRevision || worldWidth != other.worldWidth
        || regionSeed != other.regionSeed || modelVersion != other.modelVersion)
      return false;
    uint32_t a = 0, b = 0;
    std::memcpy(&a, &baseHeight, sizeof(a));
    std::memcpy(&b, &other.baseHeight, sizeof(b));
    return a == b;
  }

  bool operator!=(WorldGenerationSelectorContextIdentity const& other) const { return !(*this == other); }
  // Identity-only check, not a commit guard or read lease.
  bool matches(WorldGenerationSelectorContextIdentity const& result) const {
    return valid() && *this == result;
  }
};

// Move-only owner capture: admits retained selector-parameter bytes, then runs
// the existing region snapshot factory so the legacy global RNG draw stays in
// the same place. Not layout/biome/assets, not a job, not publication authority.
// Polygons, Json roots, Perlin caches and materials remain out of scope.
class WorldGenerationRegionSelectorContext {
public:
  WorldGenerationRegionSelectorContext(WorldGenerationRegionSelectorContext const&) = delete;
  WorldGenerationRegionSelectorContext& operator=(WorldGenerationRegionSelectorContext const&) = delete;
  WorldGenerationRegionSelectorContext(WorldGenerationRegionSelectorContext&&) = default;
  WorldGenerationRegionSelectorContext& operator=(WorldGenerationRegionSelectorContext&&) = default;

  static constexpr size_t retainedBytes() {
    return sizeof(WorldGenerationSelectorContextIdentity) + sizeof(WorldGenerationRegionSelectorSnapshots);
  }

  // Reserve first. Invalid identity or refused admission must not consume RNG.
  static std::optional<WorldGenerationRegionSelectorContext> tryCapture(
      WorldGenerationMemoryLedger& ledger, WorldGenerationSelectorContextIdentity identity) {
    if (!identity.valid())
      return std::nullopt;
    auto memory = ledger.reserve(WorldGenerationMemoryClass::SnapshotMetadataEstimate, retainedBytes());
    if (!memory)
      return std::nullopt;
    auto snapshots = WorldGenerationRegionSelectorSnapshots::capture(
        identity.worldWidth, identity.baseHeight, identity.regionSeed);
    return WorldGenerationRegionSelectorContext(std::move(identity), std::move(snapshots), std::move(memory));
  }

  WorldGenerationSelectorContextIdentity const& identity() const { return m_identity; }
  WorldGenerationRegionSelectorSnapshots const& snapshots() const { return m_snapshots; }
  bool matches(WorldGenerationSelectorContextIdentity const& expected) const {
    return expected.matches(m_identity);
  }

  TerrainSelectorParameters terrain() const { return m_snapshots.terrain.materialize(); }
  TerrainSelectorParameters foregroundCave() const { return m_snapshots.foregroundCave.materialize(); }
  TerrainSelectorParameters backgroundCave() const { return m_snapshots.backgroundCave.materialize(); }

private:
  WorldGenerationRegionSelectorContext(WorldGenerationSelectorContextIdentity identity,
      WorldGenerationRegionSelectorSnapshots snapshots,
      std::shared_ptr<WorldGenerationMemoryReservation> memory)
    : m_memory(std::move(memory)), m_identity(std::move(identity)), m_snapshots(std::move(snapshots)) {}

  // Destroy snapshots/identity before releasing the reservation token.
  std::shared_ptr<WorldGenerationMemoryReservation> m_memory;
  WorldGenerationSelectorContextIdentity m_identity;
  WorldGenerationRegionSelectorSnapshots m_snapshots;
};

}
