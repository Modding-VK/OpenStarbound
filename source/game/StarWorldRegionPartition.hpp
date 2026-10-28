#pragma once

#include "StarMaybe.hpp"
#include "StarWorldGeometry.hpp"

namespace Star {

// Logical regions group physical sectors without changing their storage or
// generation size. This value-only mapping does not assign simulation owners.
class WorldRegionPartition {
public:
  static constexpr unsigned SectorsPerRegion = 2;

  explicit WorldRegionPartition(WorldGeometry geometry);

  // Horizontal tile positions wrap using the exact world width; vertical
  // positions outside the world are absent, never clamped into a region.
  Maybe<Vec2U> regionForPosition(Vec2I const& position) const;
  // Sector indices must already be canonical. Invalid indices do not wrap.
  Maybe<Vec2U> regionForSector(Vec2U const& sector) const;
  // Half-open tile bounds clipped to the exact size of a partial edge region.
  Maybe<RectI> regionBounds(Vec2U const& region) const;

private:
  // Mapping requires finite, positive dimensions representable by RectI.
  bool validGeometry() const;

  WorldGeometry m_geometry;
};

}
