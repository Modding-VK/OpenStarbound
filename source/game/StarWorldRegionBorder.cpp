#include "StarWorldRegionBorder.hpp"

#include "StarSet.hpp"

namespace Star {

Maybe<List<Vec2I>> WorldRegionBorderOutline::perimeterPositions(WorldRegionPartition const& partition,
    Vec2U const& region) {
  auto bounds = partition.regionBounds(region);
  if (!bounds)
    return {};

  auto const xMin = (*bounds).xMin();
  auto const yMin = (*bounds).yMin();
  auto const xMax = (*bounds).xMax();
  auto const yMax = (*bounds).yMax();
  if (xMin >= xMax || yMin >= yMax)
    return {};

  List<Vec2I> positions;
  HashSet<Vec2I> seen;

  auto appendUnique = [&](Vec2I const& position) {
    if (seen.add(position))
      positions.append(position);
  };

  for (int x = xMin; x < xMax; ++x) {
    appendUnique({x, yMin});
    if (yMax - 1 != yMin)
      appendUnique({x, yMax - 1});
  }
  for (int y = yMin + 1; y < yMax - 1; ++y) {
    appendUnique({xMin, y});
    if (xMax - 1 != xMin)
      appendUnique({xMax - 1, y});
  }

  return positions;
}

List<WorldRegionBorderCell> WorldRegionBorderOutline::unavailableSkeleton(List<Vec2I> const& positions) {
  List<WorldRegionBorderCell> cells;
  for (auto const& position : positions)
    cells.append({position, false, 0});
  return cells;
}

WorldRegionBorderLedger::WorldRegionBorderLedger(WorldRegionOwnerRegistry const& owners)
  : m_owners(owners) {}

size_t WorldRegionBorderLedger::snapshotCount() const {
  return m_latest.size();
}

Maybe<WorldRegionBorderSnapshot> WorldRegionBorderLedger::latest(Vec2U const& region) const {
  if (auto snapshot = m_latest.ptr(region))
    return *snapshot;
  return {};
}

WorldRegionBorderCaptureResult WorldRegionBorderLedger::capture(Vec2U const& region, uint64_t captureStep,
    uint64_t logicalAge, List<WorldRegionBorderCell> cells) {
  auto identity = m_owners.identity(region);
  if (!identity)
    return WorldRegionBorderCaptureResult::RegionAbsent;

  auto phase = m_owners.phase(region);
  if (phase == WorldRegionOwnerPhase::Absent)
    return WorldRegionBorderCaptureResult::RegionAbsent;
  if (phase == WorldRegionOwnerPhase::Draining)
    return WorldRegionBorderCaptureResult::RegionDraining;
  if (cells.empty())
    return WorldRegionBorderCaptureResult::EmptyCells;

  auto const& partition = m_owners.partition();
  for (auto const& cell : cells) {
    auto cellRegion = partition.regionForPosition(cell.position);
    if (!cellRegion || *cellRegion != region)
      return WorldRegionBorderCaptureResult::ForeignCell;
  }

  WorldRegionBorderSnapshot snapshot;
  snapshot.owner = *identity;
  snapshot.captureStep = captureStep;
  snapshot.logicalAge = logicalAge;
  snapshot.cells = std::move(cells);
  m_latest[region] = std::move(snapshot);
  return WorldRegionBorderCaptureResult::Captured;
}

}
