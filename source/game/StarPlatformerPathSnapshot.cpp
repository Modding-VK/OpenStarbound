#include "StarPlatformerPathSnapshot.hpp"

namespace Star {
namespace PlatformerAStar {

PathSnapshot::PathSnapshot(WorldGeometry geometry, RectI region, uint64_t captureStep)
  : m_geometry(std::move(geometry)), m_region(region), m_captureStep(captureStep),
    m_cells((size_t)region.width() * (size_t)region.height()) {}

shared_ptr<PathSnapshot const> PathSnapshot::capture(World* world, RectI region, size_t maxTiles, size_t maxBlocks) {
  auto geometry = world->geometry();
  if (region.isEmpty() || region.width() <= 0 || region.height() <= 0
      || geometry.width() == 0 || region.width() >= (int)geometry.width()
      || (size_t)region.width() > maxTiles / (size_t)region.height())
    throw PathReadUnavailable(std::string("Path snapshot exceeds bounded coverage"), false);

  auto snapshot = shared_ptr<PathSnapshot>(new PathSnapshot(std::move(geometry), region, world->currentStep()));

  // Check coverage before refreshing collision caches. In an unexplored area
  // this avoids dirtying sectors for a snapshot that cannot be used.
  for (int x = region.xMin(); x < region.xMax(); ++x) {
    for (int y = region.yMin(); y < region.yMax(); ++y) {
      Vec2I position(x, y);
      auto& cell = snapshot->m_cells[snapshot->index(position)];
      cell.collision = world->tileCollisionKind(position);
      if (cell.collision == CollisionKind::Null) {
        ++snapshot->m_missingTiles;
      } else {
        cell.liquid = world->liquidLevel(position);
        cell.gravity = world->gravity(Vec2F(position));
      }
    }
  }
  if (snapshot->m_missingTiles != 0)
    throw PathReadUnavailable(std::string("Path snapshot contains unloaded tiles"), false);

  // Refresh and copy the same collision polygons that the live pathfinder
  // observes. Collision cache refresh may mutate world tiles, so capture only
  // on the owning world thread. Block coordinates can be canonical across wrap.
  world->forEachCollisionBlock(region, [&](CollisionBlock const& block) {
    if (block.kind == CollisionKind::Null)
      return;
    int x = region.xMin() + pmod<int>(block.space[0] - region.xMin(), (int)snapshot->m_geometry.width());
    Vec2I position(x, block.space[1]);
    if (x < region.xMax() && position[1] >= region.yMin() && position[1] < region.yMax()) {
      if (snapshot->m_capturedBlocks >= maxBlocks)
        throw PathReadUnavailable(std::string("Path snapshot collision block limit exceeded"), false);
      snapshot->m_cells[snapshot->index(position)].blocks.append(block);
      ++snapshot->m_capturedBlocks;
    }
  });

  return snapshot;
}

WorldGeometry PathSnapshot::geometry() const {
  return m_geometry;
}

void PathSnapshot::forEachCollisionBlock(RectI const& region, function<void(CollisionBlock const&)> const& iterator) const {
  requireRegion(region);
  for (int x = region.xMin(); x < region.xMax(); ++x)
    for (int y = region.yMin(); y < region.yMax(); ++y)
      for (auto const& block : cell(Vec2I(x, y)).blocks)
        iterator(block);
}

bool PathSnapshot::rectTileCollision(RectI const& region, CollisionSet const& collisionSet) const {
  requireRegion(region);
  for (int x = region.xMin(); x < region.xMax(); ++x)
    for (int y = region.yMin(); y < region.yMax(); ++y)
      if (isColliding(cell(Vec2I(x, y)).collision, collisionSet))
        return true;
  return false;
}

LiquidLevel PathSnapshot::liquidLevel(RectF const& region) const {
  if (region.isEmpty())
    return {};
  auto sampleRect = RectI::integral(region);
  requireRegion(sampleRect);
  float totalSpace = 0;
  Map<LiquidId, float> totals;
  for (int x = sampleRect.xMin(); x < sampleRect.xMax(); ++x) {
    for (int y = sampleRect.yMin(); y < sampleRect.yMax(); ++y) {
      Vec2I position(x, y);
      float incidence = RectF(x, y, x + 1, y + 1).overlap(region).volume();
      totalSpace += incidence;
      auto const& liquid = cell(position).liquid;
      if (liquid.liquid != EmptyLiquidId)
        totals[liquid.liquid] += min(1.0f, liquid.level) * incidence;
    }
  }

  float totalLiquid = 0;
  float largestLiquid = 0;
  LiquidId largestId = EmptyLiquidId;
  for (auto const& entry : totals) {
    totalLiquid += entry.second;
    if (entry.second > largestLiquid) {
      largestLiquid = entry.second;
      largestId = entry.first;
    }
  }
  return totalSpace > 0 ? LiquidLevel(largestId, totalLiquid / totalSpace) : LiquidLevel();
}

float PathSnapshot::gravity(Vec2F const& position) const {
  return cell(Vec2I::round(position)).gravity;
}

RectI PathSnapshot::region() const {
  return m_region;
}

uint64_t PathSnapshot::captureStep() const {
  return m_captureStep;
}

size_t PathSnapshot::missingTiles() const {
  return m_missingTiles;
}

size_t PathSnapshot::capturedBlocks() const {
  return m_capturedBlocks;
}

size_t PathSnapshot::index(Vec2I const& position) const {
  return (size_t)(position[0] - m_region.xMin()) * (size_t)m_region.height()
      + (size_t)(position[1] - m_region.yMin());
}

PathSnapshot::Cell const& PathSnapshot::cell(Vec2I const& position) const {
  if (position[0] < m_region.xMin() || position[0] >= m_region.xMax()
      || position[1] < m_region.yMin() || position[1] >= m_region.yMax())
    throw PathReadUnavailable(std::string("Path query outside snapshot coverage"), false);
  auto const& result = m_cells[index(position)];
  if (result.collision == CollisionKind::Null)
    throw PathReadUnavailable(std::string("Path query reached an unloaded tile"), false);
  return result;
}

void PathSnapshot::requireRegion(RectI const& region) const {
  for (int x = region.xMin(); x < region.xMax(); ++x)
    for (int y = region.yMin(); y < region.yMax(); ++y)
      cell(Vec2I(x, y));
}

}
}
