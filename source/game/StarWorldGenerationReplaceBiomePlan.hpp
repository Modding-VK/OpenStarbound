#pragma once

#include "StarGameTypes.hpp"
#include "StarList.hpp"
#include "StarMap.hpp"
#include "StarMaterialTypes.hpp"
#include "StarWorldGenerationTasks.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <utility>

namespace Star {

// CPU-04 replaceBiomeBlocks slice: private biome-placeholder resolution into
// ordered material/mod/hue intents. Owner supplies biome main/subBlocks and
// hue shifts — no Root/MaterialDatabase, no tile mutation, no collision update.
enum class WorldGenerationReplaceBiomeOp : uint8_t {
  SetForeground = 0,
  SetBackground = 1,
  SetForegroundHue = 2,
  SetBackgroundHue = 3,
  ClearForeground = 4,
  ClearBackground = 5,
  ClearForegroundMod = 6,
  ClearBackgroundMod = 7,
  ClearForegroundModHue = 8,
  ClearBackgroundModHue = 9
};

struct WorldGenerationReplaceBiomeTileSample {
  MaterialId foreground = EmptyMaterialId;
  MaterialId background = EmptyMaterialId;
  ModId foregroundMod = NoModId;
  ModId backgroundMod = NoModId;
  MaterialHue foregroundHueShift = 0;
  MaterialHue backgroundHueShift = 0;
  MaterialHue foregroundModHueShift = 0;
  MaterialHue backgroundModHueShift = 0;
  uint32_t blockBiomeIndex = 0;
};

struct WorldGenerationReplaceBiomeBlockSample {
  MaterialId mainBlock = EmptyMaterialId;
  List<MaterialId> subBlocks;
  HashMap<MaterialId, MaterialHue> materialHueShifts;

  MaterialHue hueFor(MaterialId material) const {
    if (auto const* hue = materialHueShifts.ptr(material))
      return *hue;
    return 0;
  }
};

struct WorldGenerationReplaceBiomeDeltaEntry {
  Vec2I position;
  WorldGenerationReplaceBiomeOp op = WorldGenerationReplaceBiomeOp::SetForeground;
  MaterialId material = EmptyMaterialId;
  ModId mod = NoModId;
  MaterialHue hue = 0;

  bool operator==(WorldGenerationReplaceBiomeDeltaEntry const& other) const {
    return position == other.position && op == other.op && material == other.material
        && mod == other.mod && hue == other.hue;
  }
};

struct WorldGenerationReplaceBiomePlanIdentity {
  static constexpr unsigned ModelVersion = 1;
  static constexpr unsigned ProductVersion = 1;
  WorldGenerationTaskIdentity task{};
  uint64_t tileRevision = 0;
  uint64_t templateRevision = 0;
  Vec2U worldSize{};
  RectI sectorTiles;
  unsigned productVersion = ProductVersion;

  bool valid() const {
    return productVersion == ProductVersion
        && worldSize[0] != 0 && worldSize[1] != 0
        && sectorTiles.isNull() == false
        && tileRevision != 0
        && task.stage == WorldGenerationTaskStage::FinalizePlan;
  }

  bool operator==(WorldGenerationReplaceBiomePlanIdentity const& other) const {
    return task == other.task
        && tileRevision == other.tileRevision
        && templateRevision == other.templateRevision
        && worldSize == other.worldSize
        && sectorTiles == other.sectorTiles
        && productVersion == other.productVersion;
  }

  bool matches(WorldGenerationReplaceBiomePlanIdentity const& expectation) const {
    return valid() && expectation.valid() && *this == expectation;
  }
};

using WorldGenerationReplaceBiomeTileSamples = HashMap<Vec2I, WorldGenerationReplaceBiomeTileSample>;
using WorldGenerationReplaceBiomeBlocksByIndex =
    HashMap<uint32_t, WorldGenerationReplaceBiomeBlockSample>;

inline MaterialId resolveWorldGenerationBiomeMaterial(MaterialId placeholder,
    WorldGenerationReplaceBiomeBlockSample const& biome) {
  if (placeholder == BiomeMaterialId)
    return biome.mainBlock;
  if (placeholder >= Biome1MaterialId && placeholder <= Biome5MaterialId) {
    if (!biome.subBlocks.empty())
      return biome.subBlocks[(placeholder - Biome1MaterialId) % biome.subBlocks.size()];
    return biome.mainBlock;
  }
  return placeholder;
}

inline List<WorldGenerationReplaceBiomeDeltaEntry> planWorldGenerationReplaceBiomeDelta(
    WorldGenerationReplaceBiomePlanIdentity const& identity,
    WorldGenerationReplaceBiomeTileSamples const& tiles,
    WorldGenerationReplaceBiomeBlocksByIndex const& biomes) {
  List<WorldGenerationReplaceBiomeDeltaEntry> delta;
  if (!identity.valid())
    return delta;

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
    if (!tile)
      continue;

    if (auto const* biome = biomes.ptr(tile->blockBiomeIndex)) {
      if (isBiomeMaterial(tile->foreground)) {
        MaterialId resolved = resolveWorldGenerationBiomeMaterial(tile->foreground, *biome);
        delta.append({position, WorldGenerationReplaceBiomeOp::SetForeground, resolved, NoModId, 0});
        delta.append({position, WorldGenerationReplaceBiomeOp::SetForegroundHue, EmptyMaterialId,
            NoModId, biome->hueFor(resolved)});
      }
      if (isBiomeMaterial(tile->background)) {
        MaterialId resolved = resolveWorldGenerationBiomeMaterial(tile->background, *biome);
        delta.append({position, WorldGenerationReplaceBiomeOp::SetBackground, resolved, NoModId, 0});
        delta.append({position, WorldGenerationReplaceBiomeOp::SetBackgroundHue, EmptyMaterialId,
            NoModId, biome->hueFor(resolved)});
      }
    } else {
      // Missing biome: clear biome placeholders / biome mods (no collision write).
      if (isBiomeMaterial(tile->foreground)) {
        delta.append({position, WorldGenerationReplaceBiomeOp::ClearForeground, EmptyMaterialId,
            NoModId, 0});
        delta.append({position, WorldGenerationReplaceBiomeOp::SetForegroundHue, EmptyMaterialId,
            NoModId, 0});
      }
      if (isBiomeMod(tile->foregroundMod)) {
        delta.append({position, WorldGenerationReplaceBiomeOp::ClearForegroundMod, EmptyMaterialId,
            NoModId, 0});
        delta.append({position, WorldGenerationReplaceBiomeOp::ClearForegroundModHue,
            EmptyMaterialId, NoModId, 0});
      }
      if (isBiomeMaterial(tile->background)) {
        delta.append({position, WorldGenerationReplaceBiomeOp::ClearBackground, EmptyMaterialId,
            NoModId, 0});
        delta.append({position, WorldGenerationReplaceBiomeOp::SetBackgroundHue, EmptyMaterialId,
            NoModId, 0});
      }
      if (isBiomeMod(tile->backgroundMod)) {
        delta.append({position, WorldGenerationReplaceBiomeOp::ClearBackgroundMod, EmptyMaterialId,
            NoModId, 0});
        delta.append({position, WorldGenerationReplaceBiomeOp::ClearBackgroundModHue,
            EmptyMaterialId, NoModId, 0});
      }
    }
  }
  return delta;
}

class WorldGenerationReplaceBiomePlan {
public:
  static std::optional<WorldGenerationReplaceBiomePlan> tryPlan(
      WorldGenerationReplaceBiomePlanIdentity identity,
      WorldGenerationReplaceBiomeTileSamples const& tiles,
      WorldGenerationReplaceBiomeBlocksByIndex const& biomes) {
    if (!identity.valid())
      return std::nullopt;
    auto delta = planWorldGenerationReplaceBiomeDelta(identity, tiles, biomes);
    return WorldGenerationReplaceBiomePlan(std::move(identity), std::move(delta));
  }

  WorldGenerationReplaceBiomePlanIdentity const& identity() const { return m_identity; }
  List<WorldGenerationReplaceBiomeDeltaEntry> const& delta() const { return m_delta; }
  bool active() const { return m_active; }

  bool matches(WorldGenerationReplaceBiomePlanIdentity const& expectation) const {
    return active() && m_identity.matches(expectation);
  }

  // Owner revalidation only. Success does not mutate tiles or collision.
  bool tryConsume(WorldGenerationReplaceBiomePlanIdentity const& liveExpectation) {
    if (!active() || !m_identity.matches(liveExpectation))
      return false;
    return true;
  }

  void discard() {
    m_active = false;
    m_delta.clear();
    m_identity = {};
  }

private:
  WorldGenerationReplaceBiomePlan(WorldGenerationReplaceBiomePlanIdentity identity,
      List<WorldGenerationReplaceBiomeDeltaEntry> delta)
    : m_identity(std::move(identity)), m_delta(std::move(delta)), m_active(true) {}

  WorldGenerationReplaceBiomePlanIdentity m_identity;
  List<WorldGenerationReplaceBiomeDeltaEntry> m_delta;
  bool m_active = false;
};

}
