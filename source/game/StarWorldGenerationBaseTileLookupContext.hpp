#pragma once

#include "StarWorldGenerationBaseTileObservation.hpp"

namespace Star {

// Explicit caller-owned generations, not inferred from pointers or sample
// values. The engine's reload/asset generation producer is NOT wired here yet.
struct WorldGenerationBaseTileLookupScope {
  static constexpr unsigned ModelVersion = 1;
  uint64_t worldEpoch = 0;
  uint64_t templateRevision = 0;
  uint64_t assetsGeneration = 0;
  uint64_t materialsGeneration = 0;
  unsigned modelVersion = ModelVersion;
  unsigned inputVersion = WorldGenerationBaseTileInput::ModelVersion;
  unsigned liquidVersion = WorldGenerationBaseTileLiquidDecision::ModelVersion;

  bool valid() const {
    return worldEpoch && assetsGeneration && materialsGeneration && modelVersion == ModelVersion
        && inputVersion == WorldGenerationBaseTileInput::ModelVersion
        && liquidVersion == WorldGenerationBaseTileLiquidDecision::ModelVersion;
  }
  bool operator==(WorldGenerationBaseTileLookupScope const& other) const {
    return worldEpoch == other.worldEpoch && templateRevision == other.templateRevision
        && assetsGeneration == other.assetsGeneration && materialsGeneration == other.materialsGeneration
        && modelVersion == other.modelVersion && inputVersion == other.inputVersion && liquidVersion == other.liquidVersion;
  }
};

// Bounded H0 prototype, NOT used by the normal producer. Capacity is explicit
// at the owning call site, not a hardware/default queue limit. Freeze once, then
// share by const reference with lifetime/synchronization supplied by the owner.
// No heap, tile copies, world callbacks, cursors or per-tile tapes are retained.
template <size_t Capacity>
class WorldGenerationBaseTileLookupContext {
public:
  static constexpr unsigned ModelVersion = 1;
  explicit WorldGenerationBaseTileLookupContext(WorldGenerationBaseTileLookupScope scope) : m_scope(scope) {}
  WorldGenerationBaseTileLookupContext(WorldGenerationBaseTileLookupContext const&) = delete;
  WorldGenerationBaseTileLookupContext& operator=(WorldGenerationBaseTileLookupContext const&) = delete;

  // Validate first, then insert at most six unique keys without allocation.
  // Invalid scope/sample/capacity leaves entries intact. Conflicting values for
  // one key poison the builder: samples do NOT prove an arbitrary resolver pure.
  bool add(WorldGenerationBaseTileLookupScope const& scope, WorldGenerationBaseTileObservation const& observation) {
    if (m_sealed || m_conflicted || !m_scope.valid() || !(scope == m_scope) || !observation.valid()) return false;
    std::array<uint64_t, 6> pending{};
    size_t added = 0;
    for (unsigned i = 0; i < observation.count; ++i) {
      auto const& value = observation.lookups[i];
      uint64_t encoded = (key(value.kind, value.biome, value.material, value.mod) << 8) | value.value;
      size_t position = lowerKey(encoded >> 8);
      if (position < m_size && (m_entries[position] >> 8) == (encoded >> 8)) {
        if (m_entries[position] != encoded) { m_conflicted = true; return false; }
        continue;
      }
      bool duplicate = false;
      for (size_t p = 0; p < added; ++p) if ((pending[p] >> 8) == (encoded >> 8)) {
        if (pending[p] != encoded) { m_conflicted = true; return false; }
        duplicate = true;
        break;
      }
      if (!duplicate) pending[added++] = encoded;
    }
    if (added > Capacity - m_size) return false;
    for (size_t i = 0; i < added; ++i) {
      size_t position = lowerKey(pending[i] >> 8);
      for (size_t p = m_size; p > position; --p) m_entries[p] = m_entries[p - 1];
      m_entries[position] = pending[i];
      ++m_size;
    }
    return true;
  }

  bool seal() {
    if (m_sealed) return valid();
    if (!m_scope.valid() || m_conflicted) return false;
    m_sealed = true;
    return true;
  }
  bool valid() const { return m_sealed && !m_conflicted && m_scope.valid(); }
  bool conflicted() const { return m_conflicted; }
  size_t size() const { return m_size; }
  static constexpr size_t capacity() { return Capacity; }
  static constexpr size_t reservedEntryBytes() { return Capacity * sizeof(uint64_t); }
  size_t activeEntryBytes() const { return m_size * sizeof(uint64_t); }

  // Apply to the CURRENT exclusive private tile input, not an automatic retry
  // or proof it is an original snapshot. Unlike observation replay, this catalog
  // has no per-tile source guard. Task ownership/revisions remain separate gates.
  bool apply(ServerTile& tile, WorldGenerationBaseTileInput const& input, WorldGenerationBaseTileLookupScope const& scope) const {
    if (!valid() || !(scope == m_scope)) return false;
    std::array<uint8_t, 6> values{};
    size_t count = 0;
    auto take = [&](WorldGenerationBaseTileLookup kind, BiomeIndex biome, MaterialId material, ModId mod) {
      uint64_t wanted = key(kind, biome, material, mod);
      size_t position = lowerKey(wanted);
      if (position == m_size || (m_entries[position] >> 8) != wanted) return false;
      values[count++] = uint8_t(m_entries[position] & 255u);
      return true;
    };
    auto layer = [&](MaterialId original, WorldGenerationBaseTileLayerInput const& proposed) {
      if (original != NullMaterialId) return true;
      if (!take(WorldGenerationBaseTileLookup::MaterialHue, input.blockBiome, proposed.material, NoModId)
          || !take(WorldGenerationBaseTileLookup::SupportsMod, NullBiomeIndex, proposed.material, proposed.mod)) return false;
      if (values[count - 1] && !take(WorldGenerationBaseTileLookup::ModHue, input.blockBiome, NullMaterialId, proposed.mod)) return false;
      return true;
    };
    // Resolve all required keys ONCE before any metadata/layer/liquid write.
    if (!layer(tile.foreground, input.foreground) || !layer(tile.background, input.background)) return false;
    struct Resolver {
      std::array<uint8_t, 6> const& values;
      mutable unsigned cursor = 0;
      MaterialHue materialHue(BiomeIndex, MaterialId) const { return values[cursor++]; }
      bool supportsMod(MaterialId, ModId) const { return values[cursor++] != 0; }
      MaterialHue modHue(BiomeIndex, ModId) const { return values[cursor++]; }
    } resolver{values};
    applyWorldGenerationBaseTile(tile, input, resolver);
    return true;
  }

private:
  // Injective key, not a hash: kind(2) + biome(8) + material(16) + mod(16).
  // Low eight bits of each stored word hold the value, total at most 50 bits.
  static uint64_t key(WorldGenerationBaseTileLookup kind, BiomeIndex biome, MaterialId material, ModId mod) {
    static_assert(std::numeric_limits<BiomeIndex>::digits <= 8);
    static_assert(std::numeric_limits<MaterialId>::digits <= 16 && std::numeric_limits<ModId>::digits <= 16);
    static_assert(std::numeric_limits<MaterialHue>::digits <= 8);
    return (uint64_t(kind) << 40) | (uint64_t(biome) << 32) | (uint64_t(material) << 16) | uint64_t(mod);
  }
  size_t lowerKey(uint64_t wanted) const {
    size_t low = 0, high = m_size;
    while (low < high) {
      size_t middle = low + (high - low) / 2;
      if ((m_entries[middle] >> 8) < wanted) low = middle + 1;
      else high = middle;
    }
    return low;
  }
  WorldGenerationBaseTileLookupScope const m_scope;
  std::array<uint64_t, Capacity> m_entries{};
  size_t m_size = 0;
  bool m_sealed = false;
  bool m_conflicted = false;
};

}
