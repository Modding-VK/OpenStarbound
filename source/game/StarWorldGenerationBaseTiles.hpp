#pragma once

#include "StarWorldGenerationTasks.hpp"
#include "StarWorldGenerationBaseTileLiquid.hpp"
#include "StarWorldTiles.hpp"

namespace Star {

// Per-admission identity, not a content/cache key or a complete pure template.
// Pointer bindings to the live array/template remain owner-side commit guards.
struct WorldGenerationBaseTilesIdentity {
  static constexpr unsigned ModelVersion = 1;
  static constexpr unsigned ProductVersion = 1;
  WorldGenerationTaskIdentity task{};
  uint64_t tileRevision = 0;
  uint64_t templateRevision = 0;
  Vec2U worldSize{};
  RectI tileRegion;
  unsigned productVersion = ProductVersion;
  unsigned liquidVersion = WorldGenerationBaseTileLiquidDecision::ModelVersion;

  bool valid() const;
  bool operator==(WorldGenerationBaseTilesIdentity const& other) const;
  bool operator!=(WorldGenerationBaseTilesIdentity const& other) const { return !(*this == other); }
  // Identity-only check, not permission to commit without live owner guards.
  bool matches(WorldGenerationBaseTilesIdentity const& result) const { return valid() && *this == result; }
};

enum class WorldGenerationBaseTileField : uint32_t {
  Foreground = 1u << 0, ForegroundHue = 1u << 1, ForegroundMod = 1u << 2,
  ForegroundModHue = 1u << 3, ForegroundColor = 1u << 4,
  Background = 1u << 5, BackgroundHue = 1u << 6, BackgroundMod = 1u << 7,
  BackgroundModHue = 1u << 8, BackgroundColor = 1u << 9,
  Collision = 1u << 10, ObjectCollision = 1u << 11, CollisionDirty = 1u << 12,
  CollisionCache = 1u << 13, BlockBiome = 1u << 14, EnvironmentBiome = 1u << 15,
  BiomeTransition = 1u << 16, ForegroundDamage = 1u << 17, BackgroundDamage = 1u << 18,
  Dungeon = 1u << 19, LiquidId = 1u << 20, LiquidLevel = 1u << 21,
  LiquidPressure = 1u << 22, LiquidSource = 1u << 23, RootSource = 1u << 24
};

enum class WorldGenerationBaseTileComparisonMode { Exact, IgnoreCollisionCache };

struct WorldGenerationBaseTilesComparison {
  static constexpr unsigned ModelVersion = 1;
  bool valid = false;
  size_t comparedTiles = 0;
  size_t differentTiles = 0;
  uint32_t differentFields = 0;
  Maybe<Vec2U> firstDifference;
  bool equal() const { return valid && differentTiles == 0; }
};

// Diagnostic only, on fully initialized fields/active cache entries. Never
// memcmp a tile/class/struct: padding, unused cache slots and capacity are not
// state. Floating fields compare bits, including signed zero/NaN payloads.
// IgnoreCollisionCache skips ONLY dirty flag/cache geometry, not collisions.
WorldGenerationBaseTilesComparison compareWorldGenerationBaseTile(ServerTile const& a, ServerTile const& b,
    WorldGenerationBaseTileComparisonMode mode = WorldGenerationBaseTileComparisonMode::Exact);
// Physical arrays include unused cells at clipped world edges. Read only the
// valid tileRegion from a validated identity, never uninitialized edge cells.
WorldGenerationBaseTilesComparison compareWorldGenerationBaseTiles(ServerTileSectorArray::Array const& a,
    ServerTileSectorArray::Array const& b, WorldGenerationBaseTilesIdentity const& identity,
    WorldGenerationBaseTileComparisonMode mode = WorldGenerationBaseTileComparisonMode::Exact);

}
