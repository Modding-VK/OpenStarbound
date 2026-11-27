#pragma once

#include "StarGameTypes.hpp"
#include "StarLiquidTypes.hpp"
#include "StarList.hpp"
#include "StarMap.hpp"
#include "StarMaterialTypes.hpp"
#include "StarWorldGenerationTasks.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <utility>

namespace Star {

// CPU-04 second slice: private Finalize normalization plan. Captures ocean
// liquid source/pressure and non-real color-variant resets as an ordered delta.
// Does not place flora/objects/treasure and never mutates live tiles.
enum class WorldGenerationFinalizeOp : uint8_t {
  ClearLiquidSource = 0,
  SetLiquidPressure = 1,
  ResetForegroundColorVariant = 2,
  ResetBackgroundColorVariant = 3,
  MarkCollisionCacheDirty = 4
};

struct WorldGenerationFinalizeSample {
  MaterialId foreground = EmptyMaterialId;
  MaterialId background = EmptyMaterialId;
  DungeonId dungeonId = NoDungeonId;
  bool liquidSource = false;
  LiquidId oceanLiquid = EmptyLiquidId;
  int oceanLiquidLevel = 0;
};

struct WorldGenerationFinalizePlanIdentity {
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

  bool operator==(WorldGenerationFinalizePlanIdentity const& other) const {
    return task == other.task
        && tileRevision == other.tileRevision
        && templateRevision == other.templateRevision
        && worldSize == other.worldSize
        && sectorTiles == other.sectorTiles
        && productVersion == other.productVersion;
  }

  bool matches(WorldGenerationFinalizePlanIdentity const& expectation) const {
    return valid() && expectation.valid() && *this == expectation;
  }
};

struct WorldGenerationFinalizeDeltaEntry {
  Vec2I position;
  WorldGenerationFinalizeOp op = WorldGenerationFinalizeOp::MarkCollisionCacheDirty;
  float pressure = 0.0f;

  bool operator==(WorldGenerationFinalizeDeltaEntry const& other) const {
    return position == other.position && op == other.op && pressure == other.pressure;
  }
};

using WorldGenerationFinalizeSamples = HashMap<Vec2I, WorldGenerationFinalizeSample>;

inline List<WorldGenerationFinalizeDeltaEntry> planWorldGenerationFinalizeNormalizeDelta(
    WorldGenerationFinalizePlanIdentity const& identity,
    WorldGenerationFinalizeSamples const& samples) {
  List<WorldGenerationFinalizeDeltaEntry> delta;
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
    auto const* cell = samples.ptr(position);
    if (!cell)
      continue;

    if (cell->liquidSource) {
      if (!isRealDungeon(cell->dungeonId) && cell->background != EmptyMaterialId) {
        delta.append({position, WorldGenerationFinalizeOp::ClearLiquidSource, 0.0f});
      }
      if (cell->oceanLiquid != EmptyLiquidId && position[1] < cell->oceanLiquidLevel) {
        float pressure = (float)(cell->oceanLiquidLevel - position[1]);
        delta.append({position, WorldGenerationFinalizeOp::SetLiquidPressure, pressure});
      }
    }

    if (!isRealMaterial(cell->foreground))
      delta.append({position, WorldGenerationFinalizeOp::ResetForegroundColorVariant, 0.0f});
    if (!isRealMaterial(cell->background))
      delta.append({position, WorldGenerationFinalizeOp::ResetBackgroundColorVariant, 0.0f});

    delta.append({position, WorldGenerationFinalizeOp::MarkCollisionCacheDirty, 0.0f});
  }
  return delta;
}

class WorldGenerationFinalizePlan {
public:
  static std::optional<WorldGenerationFinalizePlan> tryPlan(
      WorldGenerationFinalizePlanIdentity identity,
      WorldGenerationFinalizeSamples const& samples) {
    if (!identity.valid())
      return std::nullopt;
    auto delta = planWorldGenerationFinalizeNormalizeDelta(identity, samples);
    return WorldGenerationFinalizePlan(std::move(identity), std::move(delta));
  }

  WorldGenerationFinalizePlanIdentity const& identity() const { return m_identity; }
  List<WorldGenerationFinalizeDeltaEntry> const& delta() const { return m_delta; }
  bool active() const { return m_active; }

  bool matches(WorldGenerationFinalizePlanIdentity const& expectation) const {
    return active() && m_identity.matches(expectation);
  }

  // Owner revalidation only. Success does not publish tiles or spawn entities.
  bool tryConsume(WorldGenerationFinalizePlanIdentity const& liveExpectation) {
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
  WorldGenerationFinalizePlan(WorldGenerationFinalizePlanIdentity identity,
      List<WorldGenerationFinalizeDeltaEntry> delta)
    : m_identity(std::move(identity)), m_delta(std::move(delta)), m_active(true) {}

  WorldGenerationFinalizePlanIdentity m_identity;
  List<WorldGenerationFinalizeDeltaEntry> m_delta;
  bool m_active = false;
};

}
