#include "StarWorldRegionPartition.hpp"

#include <algorithm>
#include <limits>
#include <utility>

#include "StarGameTypes.hpp"

namespace Star {

WorldRegionPartition::WorldRegionPartition(WorldGeometry geometry)
  : m_geometry(std::move(geometry)) {}

bool WorldRegionPartition::validGeometry() const {
  return m_geometry.width() > 0 && m_geometry.height() > 0
      && m_geometry.width() <= static_cast<unsigned>(std::numeric_limits<int>::max())
      && m_geometry.height() <= static_cast<unsigned>(std::numeric_limits<int>::max());
}

Maybe<Vec2U> WorldRegionPartition::regionForPosition(Vec2I const& position) const {
  if (!validGeometry() || position[1] < 0 || static_cast<unsigned>(position[1]) >= m_geometry.height())
    return {};

  auto canonical = m_geometry.xwrap(position);
  return regionForSector(Vec2U(static_cast<unsigned>(canonical[0]) / WorldSectorSize,
      static_cast<unsigned>(canonical[1]) / WorldSectorSize));
}

Maybe<Vec2U> WorldRegionPartition::regionForSector(Vec2U const& sector) const {
  if (!validGeometry())
    return {};

  // Subtract before dividing to avoid overflow when rounding upwards.
  auto columns = (m_geometry.width() - 1) / WorldSectorSize + 1;
  auto rows = (m_geometry.height() - 1) / WorldSectorSize + 1;
  if (sector[0] >= columns || sector[1] >= rows)
    return {};

  return Vec2U(sector[0] / SectorsPerRegion, sector[1] / SectorsPerRegion);
}

Maybe<RectI> WorldRegionPartition::regionBounds(Vec2U const& region) const {
  if (!validGeometry())
    return {};

  uint64_t const regionSize = WorldSectorSize * SectorsPerRegion;
  uint64_t const xMin = static_cast<uint64_t>(region[0]) * regionSize;
  uint64_t const yMin = static_cast<uint64_t>(region[1]) * regionSize;
  if (xMin >= m_geometry.width() || yMin >= m_geometry.height())
    return {};

  auto xMax = std::min(xMin + regionSize, static_cast<uint64_t>(m_geometry.width()));
  auto yMax = std::min(yMin + regionSize, static_cast<uint64_t>(m_geometry.height()));
  return RectI(static_cast<int>(xMin), static_cast<int>(yMin), static_cast<int>(xMax), static_cast<int>(yMax));
}

}
