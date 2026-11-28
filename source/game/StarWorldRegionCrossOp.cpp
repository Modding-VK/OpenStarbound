#include "StarWorldRegionCrossOp.hpp"

#include <algorithm>

namespace Star {

bool WorldRegionCrossIntentId::operator==(WorldRegionCrossIntentId const& rhs) const {
  return worldEpoch == rhs.worldEpoch && sequence == rhs.sequence;
}

bool WorldRegionCrossIntentId::operator!=(WorldRegionCrossIntentId const& rhs) const {
  return !(*this == rhs);
}

bool WorldRegionCrossIntentId::operator<(WorldRegionCrossIntentId const& rhs) const {
  if (worldEpoch != rhs.worldEpoch)
    return worldEpoch < rhs.worldEpoch;
  return sequence < rhs.sequence;
}

WorldRegionCrossOpShadow::WorldRegionCrossOpShadow(uint64_t worldEpoch, WorldRegionPartition partition)
  : m_worldEpoch(worldEpoch), m_partition(std::move(partition)) {}

bool WorldRegionCrossOpShadow::canonicalize(List<Vec2U>& regions, List<Vec2U>& sectors) const {
  if (sectors.empty())
    return false;

  std::sort(regions.begin(), regions.end());
  regions.erase(std::unique(regions.begin(), regions.end()), regions.end());
  std::sort(sectors.begin(), sectors.end());
  for (size_t i = 1; i < sectors.size(); ++i) {
    if (sectors[i] == sectors[i - 1])
      return false;
  }

  for (auto const& region : regions) {
    if (!m_partition.regionBounds(region))
      return false;
  }
  for (auto const& sector : sectors) {
    if (!m_partition.regionForSector(sector))
      return false;
  }
  return true;
}

Maybe<WorldRegionCrossIntentId> WorldRegionCrossOpShadow::propose(uint64_t logicalStep, Vec2U const& sourceRegion,
    List<Vec2U> targetRegions, List<Vec2U> footprintSectors, WorldRegionCrossIntentKind kind) {
  if (!logicalStep)
    return {};
  if (m_currentStep && logicalStep < *m_currentStep)
    return {};
  if (!m_partition.regionBounds(sourceRegion))
    return {};

  if (!canonicalize(targetRegions, footprintSectors))
    return {};
  if (std::find(targetRegions.begin(), targetRegions.end(), sourceRegion) != targetRegions.end())
    return {};

  // Footprint must touch the source region and every declared target.
  Set<Vec2U> touched;
  for (auto const& sector : footprintSectors) {
    if (auto region = m_partition.regionForSector(sector))
      touched.add(*region);
  }
  if (!touched.contains(sourceRegion))
    return {};
  for (auto const& target : targetRegions) {
    if (!touched.contains(target))
      return {};
  }

  if (m_sequence + 1 < m_sequence)
    return {};

  WorldRegionCrossIntent intent;
  intent.id = {m_worldEpoch, ++m_sequence};
  intent.logicalStep = logicalStep;
  intent.sourceRegion = sourceRegion;
  intent.targetRegions = std::move(targetRegions);
  intent.footprintSectors = std::move(footprintSectors);
  intent.kind = kind;
  intent.state = WorldRegionCrossIntentState::Proposed;
  m_intents[intent.id] = intent;
  m_currentStep = logicalStep;
  return intent.id;
}

WorldRegionCrossOpOutcome WorldRegionCrossOpShadow::reserve(WorldRegionCrossIntentId const& id) {
  auto it = m_intents.find(id);
  if (it == m_intents.end())
    return WorldRegionCrossOpOutcome::Unknown;
  if (it->second.state != WorldRegionCrossIntentState::Proposed)
    return WorldRegionCrossOpOutcome::Invalid;
  if (m_currentStep && it->second.logicalStep < *m_currentStep)
    return WorldRegionCrossOpOutcome::StaleStep;

  for (auto const& sector : it->second.footprintSectors) {
    if (m_reservedSectors.contains(sector))
      return WorldRegionCrossOpOutcome::Conflict;
  }
  for (auto const& sector : it->second.footprintSectors)
    m_reservedSectors[sector] = id;
  it->second.state = WorldRegionCrossIntentState::Reserved;
  return WorldRegionCrossOpOutcome::Accepted;
}

bool WorldRegionCrossOpShadow::releaseReservation(WorldRegionCrossIntentId const& id) {
  auto it = m_intents.find(id);
  if (it == m_intents.end() || it->second.state != WorldRegionCrossIntentState::Reserved)
    return false;
  for (auto const& sector : it->second.footprintSectors) {
    auto reserved = m_reservedSectors.find(sector);
    if (reserved != m_reservedSectors.end() && reserved->second == id)
      m_reservedSectors.erase(reserved);
  }
  return true;
}

WorldRegionCrossOpOutcome WorldRegionCrossOpShadow::commit(WorldRegionCrossIntentId const& id) {
  auto it = m_intents.find(id);
  if (it == m_intents.end())
    return WorldRegionCrossOpOutcome::Unknown;
  if (it->second.state != WorldRegionCrossIntentState::Reserved)
    return WorldRegionCrossOpOutcome::Invalid;
  releaseReservation(id);
  it->second.state = WorldRegionCrossIntentState::Committed;
  return WorldRegionCrossOpOutcome::Accepted;
}

WorldRegionCrossOpOutcome WorldRegionCrossOpShadow::reject(WorldRegionCrossIntentId const& id) {
  auto it = m_intents.find(id);
  if (it == m_intents.end())
    return WorldRegionCrossOpOutcome::Unknown;
  if (it->second.state == WorldRegionCrossIntentState::Proposed) {
    it->second.state = WorldRegionCrossIntentState::Rejected;
    return WorldRegionCrossOpOutcome::Accepted;
  }
  if (it->second.state != WorldRegionCrossIntentState::Reserved)
    return WorldRegionCrossOpOutcome::Invalid;
  releaseReservation(id);
  it->second.state = WorldRegionCrossIntentState::Rejected;
  return WorldRegionCrossOpOutcome::Accepted;
}

WorldRegionCrossOpOutcome WorldRegionCrossOpShadow::timeout(WorldRegionCrossIntentId const& id) {
  auto it = m_intents.find(id);
  if (it == m_intents.end())
    return WorldRegionCrossOpOutcome::Unknown;
  if (it->second.state == WorldRegionCrossIntentState::Proposed) {
    it->second.state = WorldRegionCrossIntentState::TimedOut;
    return WorldRegionCrossOpOutcome::Accepted;
  }
  if (it->second.state != WorldRegionCrossIntentState::Reserved)
    return WorldRegionCrossOpOutcome::Invalid;
  releaseReservation(id);
  it->second.state = WorldRegionCrossIntentState::TimedOut;
  return WorldRegionCrossOpOutcome::Accepted;
}

Maybe<WorldRegionCrossIntent> WorldRegionCrossOpShadow::intent(WorldRegionCrossIntentId const& id) const {
  auto it = m_intents.find(id);
  if (it == m_intents.end())
    return {};
  return it->second;
}

size_t WorldRegionCrossOpShadow::size() const {
  return m_intents.size();
}

size_t WorldRegionCrossOpShadow::reservedSectorCount() const {
  return m_reservedSectors.size();
}

Maybe<uint64_t> WorldRegionCrossOpShadow::currentStep() const {
  return m_currentStep;
}

}
