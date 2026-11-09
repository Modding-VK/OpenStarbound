#pragma once

#include "StarWorldGenerationBaseTileInput.hpp"

#include <array>
#include <cstring>
#include <limits>

namespace Star {

enum class WorldGenerationBaseTileLookup : uint8_t { Invalid, MaterialHue, SupportsMod, ModHue };

struct WorldGenerationBaseTileLookupValue {
  WorldGenerationBaseTileLookup kind = WorldGenerationBaseTileLookup::Invalid;
  BiomeIndex biome = NullBiomeIndex;
  MaterialId material = NullMaterialId;
  ModId mod = NoModId;
  unsigned value = 0;
};

// Diagnostic capture of successful lookup observations, NOT an asset version,
// content key, commit permission, or a snapshot of the whole terrain producer.
// Callers own initialized tiles; no WorldTile copy (which normalizes cache and
// omits biomeTransition), Root, pointers, heap, callbacks or exception is retained.
struct WorldGenerationBaseTileObservation {
  static constexpr unsigned ModelVersion = 1;
  unsigned modelVersion = ModelVersion;
  unsigned inputVersion = WorldGenerationBaseTileInput::ModelVersion;
  unsigned liquidVersion = WorldGenerationBaseTileLiquidDecision::ModelVersion;
  WorldGenerationBaseTileInput input;
  // Ten layer fields + four liquid fields. Other fields pass through unchanged;
  // old biomeTransition is deliberately NOT read, since application overwrites it.
  std::array<uint32_t, 14> source{};
  std::array<WorldGenerationBaseTileLookupValue, 6> lookups{};
  unsigned count = 0;
  bool complete = false;

  static std::array<uint32_t, 14> sourceState(ServerTile const& tile) {
    auto bits = [](float const& value) {
      uint32_t result;
      static_assert(sizeof(result) == sizeof(value));
      std::memcpy(&result, &value, sizeof(result));
      return result;
    };
    return {tile.foreground, tile.foregroundHueShift, tile.foregroundMod, tile.foregroundModHueShift, tile.foregroundColorVariant,
        tile.background, tile.backgroundHueShift, tile.backgroundMod, tile.backgroundModHueShift, tile.backgroundColorVariant,
        tile.liquid.liquid, bits(tile.liquid.level), bits(tile.liquid.pressure), unsigned(tile.liquid.source)};
  }

  bool valid() const {
    if (!complete || modelVersion != ModelVersion || inputVersion != WorldGenerationBaseTileInput::ModelVersion
        || liquidVersion != WorldGenerationBaseTileLiquidDecision::ModelVersion || count > lookups.size()) return false;
    unsigned cursor = 0;
    auto consume = [&](WorldGenerationBaseTileLookup kind, BiomeIndex biome, MaterialId material, ModId mod, unsigned maximum) {
      if (cursor >= count) return false;
      auto const& value = lookups[cursor];
      if (value.kind != kind || value.biome != biome || value.material != material || value.mod != mod || value.value > maximum) return false;
      ++cursor;
      return true;
    };
    auto layer = [&](uint32_t original, WorldGenerationBaseTileLayerInput const& proposed) {
      if (original != NullMaterialId) return true;
      if (!consume(WorldGenerationBaseTileLookup::MaterialHue, input.blockBiome, proposed.material, NoModId,
              std::numeric_limits<MaterialHue>::max())) return false;
      if (!consume(WorldGenerationBaseTileLookup::SupportsMod, NullBiomeIndex, proposed.material, proposed.mod, 1)) return false;
      if (lookups[cursor - 1].value && !consume(WorldGenerationBaseTileLookup::ModHue, input.blockBiome, NullMaterialId, proposed.mod,
              std::numeric_limits<MaterialHue>::max())) return false;
      return true;
    };
    return layer(source[0], input.foreground) && layer(source[5], input.background) && cursor == count;
  }

  bool matches(ServerTile const& tile, WorldGenerationBaseTileInput const& expected) const {
    return valid() && sameInput(input, expected) && source == sourceState(tile);
  }

private:
  static bool sameInput(WorldGenerationBaseTileInput const& a, WorldGenerationBaseTileInput const& b) {
    return a.blockBiome == b.blockBiome && a.environmentBiome == b.environmentBiome && a.biomeTransition == b.biomeTransition
        && a.foreground.material == b.foreground.material && a.foreground.mod == b.foreground.mod
        && a.background.material == b.background.material && a.background.mod == b.background.mod
        && a.oceanLiquid == b.oceanLiquid && a.oceanLiquidLevel == b.oceanLiquidLevel && a.encloseLiquids == b.encloseLiquids && a.y == b.y;
  }
};

// Explicit diagnostic call, not enabled by the normal producer. Record returned
// values at the existing query positions, never by prefetching. Reset before work
// so a thrown lookup cannot leave an earlier complete capture replayable. Input
// is passed by value to support callers passing observation.input itself.
template <typename Resolver>
void observeWorldGenerationBaseTile(ServerTile& tile, WorldGenerationBaseTileInput input,
    Resolver const& resolver, WorldGenerationBaseTileObservation& observation) {
  observation = {};
  observation.input = input;
  observation.source = WorldGenerationBaseTileObservation::sourceState(tile);
  struct Recorder {
    Resolver const& resolver;
    WorldGenerationBaseTileObservation& observation;
    mutable bool overflow = false;
    void record(WorldGenerationBaseTileLookup kind, BiomeIndex biome, MaterialId material, ModId mod, unsigned value) const {
      if (observation.count < observation.lookups.size()) observation.lookups[observation.count++] = {kind, biome, material, mod, value};
      else overflow = true;
    }
    MaterialHue materialHue(BiomeIndex biome, MaterialId material) const {
      auto value = resolver.materialHue(biome, material);
      record(WorldGenerationBaseTileLookup::MaterialHue, biome, material, NoModId, value);
      return value;
    }
    bool supportsMod(MaterialId material, ModId mod) const {
      bool value = resolver.supportsMod(material, mod);
      record(WorldGenerationBaseTileLookup::SupportsMod, NullBiomeIndex, material, mod, unsigned(value));
      return value;
    }
    MaterialHue modHue(BiomeIndex biome, ModId mod) const {
      auto value = resolver.modHue(biome, mod);
      record(WorldGenerationBaseTileLookup::ModHue, biome, NullMaterialId, mod, value);
      return value;
    }
  } recorder{resolver, observation};
  applyWorldGenerationBaseTile(tile, input, recorder);
  observation.complete = !recorder.overflow;
}

// Preflight the WHOLE trace and relevant original state before the first write.
// Each replay owns its cursor; an immutable observation can serve private tiles
// concurrently. No live resolver, callback, allocation or exception replay.
inline bool replayWorldGenerationBaseTile(ServerTile& tile, WorldGenerationBaseTileInput const& input,
    WorldGenerationBaseTileObservation const& observation) {
  if (!observation.matches(tile, input)) return false;
  struct Resolver {
    WorldGenerationBaseTileObservation const& observation;
    mutable unsigned cursor = 0;
    MaterialHue materialHue(BiomeIndex, MaterialId) const { return MaterialHue(observation.lookups[cursor++].value); }
    bool supportsMod(MaterialId, ModId) const { return observation.lookups[cursor++].value != 0; }
    MaterialHue modHue(BiomeIndex, ModId) const { return MaterialHue(observation.lookups[cursor++].value); }
  } resolver{observation};
  applyWorldGenerationBaseTile(tile, input, resolver);
  return true;
}

}
