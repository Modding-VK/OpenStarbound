#include "StarWorldRegionCoordinator.hpp"

#include <algorithm>
#include <limits>

#include "StarGameTypes.hpp"

namespace Star {
namespace {

bool regionLess(Vec2U const& a, Vec2U const& b) {
  return a[0] < b[0] || (a[0] == b[0] && a[1] < b[1]);
}

}

void WorldRegionObservation::reset() {
  m_revisions.clear();
}

Maybe<uint64_t> WorldRegionObservation::watch(Vec2U const& sector) {
  if (auto existing = m_revisions.ptr(sector))
    return *existing;
  if (m_revisions.size() >= MaxSectors || m_sequence == std::numeric_limits<uint64_t>::max())
    return {};
  auto stamp = ++m_sequence;
  m_revisions[sector] = stamp;
  return stamp;
}

void WorldRegionObservation::changed(Vec2U const& sector) {
  if (auto stamp = m_revisions.ptr(sector)) {
    if (m_sequence == std::numeric_limits<uint64_t>::max())
      m_revisions.remove(sector);
    else
      *stamp = ++m_sequence;
  }
}

Maybe<uint64_t> WorldRegionObservation::revision(Vec2U const& sector) const {
  if (auto stamp = m_revisions.ptr(sector))
    return *stamp;
  return {};
}

bool WorldRegionSectorState::operator==(WorldRegionSectorState const& rhs) const {
  return sector == rhs.sector && loaded == rhs.loaded && incarnation == rhs.incarnation
      && contentRevision == rhs.contentRevision && loadLevel == rhs.loadLevel
      && generationLevel == rhs.generationLevel;
}

WorldRegionTurn::WorldRegionTurn(uint64_t epoch, uint64_t sequence, unsigned lane, WorldRegionSnapshot snapshot)
  : worldEpoch(epoch), sequence(sequence), lane(lane), snapshot(std::move(snapshot)) {}

WorldRegionCoordinator::WorldRegionCoordinator(uint64_t epoch, WorldRegionPartition partition, size_t maxTurns)
  : m_epoch(epoch), m_partition(std::move(partition)),
    m_maxTurns(std::max<size_t>(1, std::min(maxTurns, MaxTurns))) {}

bool WorldRegionCoordinator::canonicalize(WorldRegionSnapshot& snapshot) const {
  auto bounds = m_partition.regionBounds(snapshot.region);
  if (!bounds || snapshot.sectors.size() > WorldRegionPartition::SectorsPerRegion
      * WorldRegionPartition::SectorsPerRegion)
    return false;

  unsigned xMin = bounds->xMin() / WorldSectorSize;
  unsigned yMin = bounds->yMin() / WorldSectorSize;
  unsigned xEnd = (bounds->xMax() - 1) / WorldSectorSize + 1;
  unsigned yEnd = (bounds->yMax() - 1) / WorldSectorSize + 1;
  if (snapshot.sectors.size() != (xEnd - xMin) * (yEnd - yMin))
    return false;

  std::sort(snapshot.sectors.begin(), snapshot.sectors.end(), [](auto const& a, auto const& b) {
    return regionLess(a.sector, b.sector);
  });
  size_t index = 0;
  for (unsigned x = xMin; x < xEnd; ++x) {
    for (unsigned y = yMin; y < yEnd; ++y) {
      if (snapshot.sectors[index++].sector != Vec2U(x, y))
        return false;
    }
  }
  return true;
}

List<shared_ptr<WorldRegionTurn const>> WorldRegionCoordinator::schedule(uint64_t step,
    List<WorldRegionSnapshot> snapshots) {
  List<shared_ptr<WorldRegionTurn const>> turns;
  if (m_lastStep && step <= *m_lastStep)
    return turns;
  m_lastStep = step;
  if (snapshots.size() > MaxRegions)
    return turns;

  for (auto& snapshot : snapshots) {
    if (snapshot.captureStep != step || !canonicalize(snapshot))
      return turns;
  }
  std::sort(snapshots.begin(), snapshots.end(), [](auto const& a, auto const& b) {
    return regionLess(a.region, b.region);
  });
  for (size_t i = 1; i < snapshots.size(); ++i) {
    if (snapshots[i - 1].region == snapshots[i].region)
      return turns;
  }
  if (snapshots.empty())
    return turns;

  size_t first = 0;
  if (m_lastRegion) {
    first = std::upper_bound(snapshots.begin(), snapshots.end(), *m_lastRegion,
        [](Vec2U const& region, auto const& snapshot) {
          return regionLess(region, snapshot.region);
        }) - snapshots.begin();
    if (first == snapshots.size())
      first = 0;
  }

  for (size_t offset = 0; offset < snapshots.size(); ++offset) {
    auto& snapshot = snapshots[(first + offset) % snapshots.size()];
    bool regionActive = false;
    size_t lane = m_maxTurns;
    for (size_t i = 0; i < m_maxTurns; ++i) {
      if (m_active[i])
        regionActive |= m_active[i]->snapshot.region == snapshot.region;
      else if (lane == m_maxTurns)
        lane = i;
    }
    if (lane == m_maxTurns || m_sequence == std::numeric_limits<uint64_t>::max())
      break;
    if (regionActive)
      continue;

    auto turn = make_shared<WorldRegionTurn const>(m_epoch, ++m_sequence,
        static_cast<unsigned>(lane), std::move(snapshot));
    m_active[lane] = turn;
    m_lastRegion = turn->snapshot.region;
    turns.append(std::move(turn));
  }
  return turns;
}

WorldRegionTurnOutcome WorldRegionCoordinator::finish(shared_ptr<WorldRegionTurn const> const& turn,
    WorldRegionSnapshot const& current) {
  if (!turn || turn->worldEpoch != m_epoch || turn->lane >= m_maxTurns
      || m_active[turn->lane] != turn || m_active[turn->lane]->sequence != turn->sequence)
    return WorldRegionTurnOutcome::Foreign;

  // Validate cardinality before copying any caller-supplied sector vector.
  bool matches = current.region == turn->snapshot.region
      && current.captureStep >= turn->snapshot.captureStep
      && current.sectors.size() == turn->snapshot.sectors.size();
  if (matches) {
    auto normalized = current;
    matches = canonicalize(normalized) && normalized.sectors == turn->snapshot.sectors;
  }
  m_active[turn->lane].reset();
  return matches ? WorldRegionTurnOutcome::Accepted : WorldRegionTurnOutcome::Stale;
}

void WorldRegionCoordinator::cancelAll() {
  for (auto& turn : m_active)
    turn.reset();
}

size_t WorldRegionCoordinator::activeCount() const {
  size_t count = 0;
  for (auto const& turn : m_active)
    count += static_cast<bool>(turn);
  return count;
}

}
