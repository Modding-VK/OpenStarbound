#pragma once

#include "StarGameTypes.hpp"
#include "StarList.hpp"
#include "StarMap.hpp"
#include "StarMaterialTypes.hpp"
#include "StarStaticRandom.hpp"
#include "StarWorldGenerationFinalizePlan.hpp"
#include "StarWorldGenerationTasks.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <utility>

namespace Star {

// CPU-04 flora/spawn slice: private grass-mod selection and biome spawn
// intents. Separates placeBiomeGrass-style selection and potential item
// ordering from tile mutation / entity construction. No Root, PlantDatabase,
// ObjectDatabase, or live WorldStorage writes.
struct WorldGenerationFloraTileSample {
  MaterialId foreground = EmptyMaterialId;
  MaterialId background = EmptyMaterialId;
  ModId foregroundMod = NoModId;
  ModId backgroundMod = NoModId;
  DungeonId dungeonId = NoDungeonId;
  uint32_t blockBiomeIndex = 0;
  // Neighbor emptiness captured by the owner — no live tileArray reads here.
  bool aboveForegroundEmpty = true;
  bool aboveBackgroundEmpty = true;
  bool belowForegroundEmpty = true;
  bool belowBackgroundEmpty = true;
};

struct WorldGenerationFloraPlaceablesSample {
  ModId grassMod = NoModId;
  ModId ceilingGrassMod = NoModId;
  float grassModDensity = 0.0f;
  float ceilingGrassModDensity = 0.0f;
};

struct WorldGenerationFloraGrassIntent {
  Vec2I position;
  TileLayer modLayer = TileLayer::Foreground;
  ModId grassModId = NoModId;

  bool operator==(WorldGenerationFloraGrassIntent const& other) const {
    return position == other.position && modLayer == other.modLayer
        && grassModId == other.grassModId;
  }
};

struct WorldGenerationFloraSpawnCandidate {
  Vec2I position;
  WorldGenerationFinalizePlacementKind kind = WorldGenerationFinalizePlacementKind::Grass;
  uint64_t seed = 0;
  int priority = 0;
  String itemKey;
};

struct WorldGenerationFloraSpawnIntent {
  Vec2I position;
  WorldGenerationFinalizePlacementKind kind = WorldGenerationFinalizePlacementKind::Grass;
  uint64_t seed = 0;
  String itemKey;

  bool operator==(WorldGenerationFloraSpawnIntent const& other) const {
    return position == other.position && kind == other.kind && seed == other.seed
        && itemKey == other.itemKey;
  }
};

struct WorldGenerationFloraSpawnPlanIdentity {
  static constexpr unsigned ModelVersion = 1;
  static constexpr unsigned ProductVersion = 1;
  WorldGenerationTaskIdentity task{};
  uint64_t tileRevision = 0;
  uint64_t templateRevision = 0;
  uint64_t worldSeed = 0;
  Vec2U worldSize{};
  RectI sectorTiles;
  unsigned productVersion = ProductVersion;

  bool valid() const {
    return productVersion == ProductVersion
        && worldSize[0] != 0 && worldSize[1] != 0
        && sectorTiles.isNull() == false
        && tileRevision != 0
        && worldSeed != 0
        && task.stage == WorldGenerationTaskStage::FinalizePlan;
  }

  bool operator==(WorldGenerationFloraSpawnPlanIdentity const& other) const {
    return task == other.task
        && tileRevision == other.tileRevision
        && templateRevision == other.templateRevision
        && worldSeed == other.worldSeed
        && worldSize == other.worldSize
        && sectorTiles == other.sectorTiles
        && productVersion == other.productVersion;
  }

  bool matches(WorldGenerationFloraSpawnPlanIdentity const& expectation) const {
    return valid() && expectation.valid() && *this == expectation;
  }
};

using WorldGenerationFloraTileSamples = HashMap<Vec2I, WorldGenerationFloraTileSample>;
using WorldGenerationFloraPlaceablesByBiome = HashMap<uint32_t, WorldGenerationFloraPlaceablesSample>;

inline bool worldGenerationFloraModEligible(ModId mod) {
  return mod == BiomeModId || mod == UndergroundBiomeModId || mod == NoModId;
}

inline List<WorldGenerationFloraGrassIntent> planWorldGenerationFloraGrassIntents(
    WorldGenerationFloraSpawnPlanIdentity const& identity,
    WorldGenerationFloraTileSamples const& tiles,
    WorldGenerationFloraPlaceablesByBiome const& placeablesByBiome) {
  List<WorldGenerationFloraGrassIntent> intents;
  if (!identity.valid())
    return intents;

  List<Vec2I> positions;
  for (int y = identity.sectorTiles.yMin(); y < identity.sectorTiles.yMax(); ++y) {
    for (int x = identity.sectorTiles.xMin(); x < identity.sectorTiles.xMax(); ++x)
      positions.append(Vec2I(x, y));
  }
  sort(positions.begin(), positions.end(), [](Vec2I const& a, Vec2I const& b) {
    return a[1] != b[1] ? a[1] < b[1] : a[0] < b[0];
  });

  for (auto const& position : positions) {
    auto const* tile = tiles.ptr(position);
    if (!tile || tile->blockBiomeIndex == 0)
      continue;
    auto const* placeables = placeablesByBiome.ptr(tile->blockBiomeIndex);
    if (!placeables)
      continue;

    TileLayer modLayer = tile->foreground != EmptyMaterialId ? TileLayer::Foreground
                                                             : TileLayer::Background;
    ModId currentMod = modLayer == TileLayer::Foreground ? tile->foregroundMod
                                                         : tile->backgroundMod;
    if (!worldGenerationFloraModEligible(currentMod))
      continue;
    // Dungeon tiles only accept explicit biome/underground biome mods.
    if (tile->dungeonId != NoDungeonId
        && !(currentMod == BiomeModId || currentMod == UndergroundBiomeModId))
      continue;

    bool isFloor = (tile->foreground != EmptyMaterialId && tile->aboveForegroundEmpty)
        || (tile->background != EmptyMaterialId && tile->aboveBackgroundEmpty);
    bool isCeiling = !isFloor
        && ((tile->foreground != EmptyMaterialId && tile->belowForegroundEmpty)
            || (tile->background != EmptyMaterialId && tile->belowBackgroundEmpty));
    if (!isFloor && !isCeiling)
      continue;

    // Surface vs underground placeables are owner-supplied samples keyed by
    // biome index. Floor/ceiling choose which density/mod pair applies.
    ModId selected = NoModId;
    float density = 0.0f;
    if (isFloor) {
      selected = placeables->grassMod;
      density = placeables->grassModDensity;
    } else {
      selected = placeables->ceilingGrassMod;
      density = placeables->ceilingGrassModDensity;
    }

    if (!isRealMod(selected))
      continue;
    float chance = staticRandomFloat(identity.worldSeed, position[0], position[1]);
    if (chance > density)
      continue;

    intents.append({position, modLayer, selected});
  }
  return intents;
}

inline List<WorldGenerationFloraSpawnIntent> planWorldGenerationFloraSpawnIntents(
    RectI sectorTiles, WorldGenerationFloraTileSamples const& tiles,
    List<WorldGenerationFloraSpawnCandidate> const& candidates) {
  List<WorldGenerationFloraSpawnCandidate> selected;
  for (auto const& candidate : candidates) {
    if (!sectorTiles.contains(candidate.position))
      continue;
    if (candidate.itemKey.empty() || candidate.seed == 0)
      continue;
    auto const* tile = tiles.ptr(candidate.position);
    // Match live prepareSector: dungeon cells do not enqueue biome placements.
    if (tile && tile->dungeonId != NoDungeonId)
      continue;
    selected.append(candidate);
  }
  sort(selected.begin(), selected.end(),
      [](WorldGenerationFloraSpawnCandidate const& a, WorldGenerationFloraSpawnCandidate const& b) {
        if (a.priority != b.priority)
          return a.priority < b.priority;
        if (a.position[1] != b.position[1])
          return a.position[1] < b.position[1];
        if (a.position[0] != b.position[0])
          return a.position[0] < b.position[0];
        if ((uint8_t)a.kind != (uint8_t)b.kind)
          return (uint8_t)a.kind < (uint8_t)b.kind;
        return a.itemKey < b.itemKey;
      });
  List<WorldGenerationFloraSpawnIntent> intents;
  intents.reserve(selected.size());
  for (auto const& candidate : selected)
    intents.append({candidate.position, candidate.kind, candidate.seed, candidate.itemKey});
  return intents;
}

class WorldGenerationFloraSpawnPlan {
public:
  static std::optional<WorldGenerationFloraSpawnPlan> tryPlan(
      WorldGenerationFloraSpawnPlanIdentity identity,
      WorldGenerationFloraTileSamples const& tiles,
      WorldGenerationFloraPlaceablesByBiome const& placeablesByBiome,
      List<WorldGenerationFloraSpawnCandidate> const& spawnCandidates = {}) {
    if (!identity.valid())
      return std::nullopt;
    auto grass = planWorldGenerationFloraGrassIntents(identity, tiles, placeablesByBiome);
    auto spawns = planWorldGenerationFloraSpawnIntents(identity.sectorTiles, tiles, spawnCandidates);
    return WorldGenerationFloraSpawnPlan(std::move(identity), std::move(grass), std::move(spawns));
  }

  WorldGenerationFloraSpawnPlanIdentity const& identity() const { return m_identity; }
  List<WorldGenerationFloraGrassIntent> const& grassIntents() const { return m_grassIntents; }
  List<WorldGenerationFloraSpawnIntent> const& spawnIntents() const { return m_spawnIntents; }
  bool active() const { return m_active; }

  bool matches(WorldGenerationFloraSpawnPlanIdentity const& expectation) const {
    return active() && m_identity.matches(expectation);
  }

  // Owner revalidation only. Success does not write mods or spawn entities.
  bool tryConsume(WorldGenerationFloraSpawnPlanIdentity const& liveExpectation) {
    if (!active() || !m_identity.matches(liveExpectation))
      return false;
    return true;
  }

  void discard() {
    m_active = false;
    m_grassIntents.clear();
    m_spawnIntents.clear();
    m_identity = {};
  }

private:
  WorldGenerationFloraSpawnPlan(WorldGenerationFloraSpawnPlanIdentity identity,
      List<WorldGenerationFloraGrassIntent> grassIntents,
      List<WorldGenerationFloraSpawnIntent> spawnIntents)
    : m_identity(std::move(identity))
    , m_grassIntents(std::move(grassIntents))
    , m_spawnIntents(std::move(spawnIntents))
    , m_active(true) {}

  WorldGenerationFloraSpawnPlanIdentity m_identity;
  List<WorldGenerationFloraGrassIntent> m_grassIntents;
  List<WorldGenerationFloraSpawnIntent> m_spawnIntents;
  bool m_active = false;
};

}
