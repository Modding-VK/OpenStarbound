#pragma once

#include "StarPlatformerAStar.hpp"
#include <vector>

namespace Star {
namespace PlatformerAStar {

STAR_EXCEPTION(PathReadUnavailable, StarException);

// Captured on the owning world thread. After capture, all queries use copied
// values and may be made by a worker. A missing tile is an error, never air.
class PathSnapshot final : public PathReadSource {
public:
  static shared_ptr<PathSnapshot const> capture(World* world, RectI region,
      size_t maxTiles = 16384, size_t maxBlocks = 32768);

  WorldGeometry geometry() const override;
  void forEachCollisionBlock(RectI const& region, function<void(CollisionBlock const&)> const& iterator) const override;
  bool rectTileCollision(RectI const& region, CollisionSet const& collisionSet) const override;
  LiquidLevel liquidLevel(RectF const& region) const override;
  float gravity(Vec2F const& position) const override;

  RectI region() const;
  uint64_t captureStep() const;
  size_t missingTiles() const;
  size_t capturedBlocks() const;

private:
  struct Cell {
    CollisionKind collision = CollisionKind::Null;
    LiquidLevel liquid;
    float gravity = 0;
    List<CollisionBlock> blocks;
  };

  PathSnapshot(WorldGeometry geometry, RectI region, uint64_t captureStep);
  size_t index(Vec2I const& position) const;
  Cell const& cell(Vec2I const& position) const;
  void requireRegion(RectI const& region) const;

  WorldGeometry m_geometry;
  RectI m_region;
  uint64_t m_captureStep;
  size_t m_missingTiles = 0;
  size_t m_capturedBlocks = 0;
  std::vector<Cell> m_cells;
};

}
}
