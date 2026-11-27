#include "StarWorldRegionBorder.hpp"

namespace Star {

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

  WorldRegionBorderSnapshot snapshot;
  snapshot.owner = *identity;
  snapshot.captureStep = captureStep;
  snapshot.logicalAge = logicalAge;
  snapshot.cells = std::move(cells);
  m_latest[region] = std::move(snapshot);
  return WorldRegionBorderCaptureResult::Captured;
}

}
