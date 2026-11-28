#include "StarWorldRegionRecovery.hpp"

#include <algorithm>

namespace Star {

bool WorldRegionCheckpointId::operator==(WorldRegionCheckpointId const& rhs) const {
  return worldEpoch == rhs.worldEpoch && sequence == rhs.sequence;
}

bool WorldRegionCheckpointId::operator<(WorldRegionCheckpointId const& rhs) const {
  if (worldEpoch != rhs.worldEpoch)
    return worldEpoch < rhs.worldEpoch;
  return sequence < rhs.sequence;
}

WorldRegionRecoveryShadow::WorldRegionRecoveryShadow(uint64_t worldEpoch, WorldRegionPartition partition)
  : m_worldEpoch(worldEpoch), m_partition(std::move(partition)) {}

bool WorldRegionRecoveryShadow::noteUnconfirmed(uint64_t logicalStep, Vec2U const& region, uint64_t intentSequence) {
  if (!logicalStep || !intentSequence)
    return false;
  if (!m_partition.regionBounds(region))
    return false;
  auto health = m_steps.find(logicalStep);
  if (health != m_steps.end() && health->second == WorldRegionStepHealth::Failed)
    return false;

  auto prior = m_unconfirmedStep.find(region);
  if (prior != m_unconfirmedStep.end() && prior->second != logicalStep)
    m_unconfirmed[region].clear();

  m_unconfirmed[region].add(intentSequence);
  m_unconfirmedStep[region] = logicalStep;
  m_partialPublish.add(region);
  if (m_steps.find(logicalStep) == m_steps.end())
    m_steps[logicalStep] = WorldRegionStepHealth::Open;
  return true;
}

Maybe<WorldRegionFailureRecord> WorldRegionRecoveryShadow::fail(uint64_t logicalStep, Vec2U const& region) {
  if (!logicalStep || !m_partition.regionBounds(region))
    return {};

  WorldRegionFailureRecord record;
  record.logicalStep = logicalStep;
  record.region = region;

  auto stepIt = m_unconfirmedStep.find(region);
  if (stepIt != m_unconfirmedStep.end() && stepIt->second == logicalStep) {
    auto& intents = m_unconfirmed[region];
    for (auto sequence : intents)
      record.invalidatedIntentSequences.append(sequence);
    intents.clear();
  }

  m_steps[logicalStep] = WorldRegionStepHealth::Failed;
  // Partial publish mark remains until checkpoint/reload — must not look reverted.
  m_partialPublish.add(region);
  std::sort(record.invalidatedIntentSequences.begin(), record.invalidatedIntentSequences.end());
  return record;
}

Maybe<WorldRegionCheckpointId> WorldRegionRecoveryShadow::checkpoint(uint64_t logicalStep, List<Vec2U> regions) {
  if (!logicalStep || regions.empty())
    return {};
  std::sort(regions.begin(), regions.end());
  regions.erase(std::unique(regions.begin(), regions.end()), regions.end());

  for (auto const& region : regions) {
    if (!m_partition.regionBounds(region))
      return {};
    // A region still carrying unconfirmed intents for this step cannot enter a
    // consistent checkpoint.
    auto stepIt = m_unconfirmedStep.find(region);
    if (stepIt != m_unconfirmedStep.end() && stepIt->second == logicalStep) {
      auto intents = m_unconfirmed.find(region);
      if (intents != m_unconfirmed.end() && !intents->second.empty())
        return {};
    }
  }

  if (m_checkpointSequence + 1 < m_checkpointSequence)
    return {};

  WorldRegionCheckpoint checkpoint;
  checkpoint.id = {m_worldEpoch, ++m_checkpointSequence};
  checkpoint.logicalStep = logicalStep;
  for (auto const& region : regions) {
    checkpoint.regions.add(region);
    m_partialPublish.remove(region);
  }
  checkpoint.consistent = true;
  m_checkpoints[checkpoint.id] = checkpoint;
  m_steps[logicalStep] = WorldRegionStepHealth::Checkpointed;
  return checkpoint.id;
}

bool WorldRegionRecoveryShadow::returnToLegacy(WorldRegionCheckpointId const& checkpoint) {
  auto it = m_checkpoints.find(checkpoint);
  if (it == m_checkpoints.end() || !it->second.consistent)
    return false;
  m_mode = WorldRegionRecoveryMode::Legacy;
  m_unconfirmed.clear();
  m_unconfirmedStep.clear();
  m_partialPublish.clear();
  return true;
}

bool WorldRegionRecoveryShadow::reloadLegacy() {
  m_mode = WorldRegionRecoveryMode::Legacy;
  m_unconfirmed.clear();
  m_unconfirmedStep.clear();
  m_partialPublish.clear();
  m_steps.clear();
  return true;
}

WorldRegionRecoveryMode WorldRegionRecoveryShadow::mode() const {
  return m_mode;
}

Maybe<WorldRegionStepHealth> WorldRegionRecoveryShadow::stepHealth(uint64_t logicalStep) const {
  auto it = m_steps.find(logicalStep);
  if (it == m_steps.end())
    return {};
  return it->second;
}

bool WorldRegionRecoveryShadow::regionPartiallyPublished(Vec2U const& region) const {
  return m_partialPublish.contains(region);
}

size_t WorldRegionRecoveryShadow::unconfirmedCount(uint64_t logicalStep, Vec2U const& region) const {
  auto stepIt = m_unconfirmedStep.find(region);
  if (stepIt == m_unconfirmedStep.end() || stepIt->second != logicalStep)
    return 0;
  auto intents = m_unconfirmed.find(region);
  if (intents == m_unconfirmed.end())
    return 0;
  return intents->second.size();
}

Maybe<WorldRegionCheckpoint> WorldRegionRecoveryShadow::checkpointRecord(WorldRegionCheckpointId const& id) const {
  auto it = m_checkpoints.find(id);
  if (it == m_checkpoints.end())
    return {};
  return it->second;
}

}
