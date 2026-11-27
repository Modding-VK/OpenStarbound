#pragma once

#include <cstdint>

#include "StarList.hpp"
#include "StarMap.hpp"
#include "StarMaybe.hpp"
#include "StarSet.hpp"
#include "StarWorldRegionPartition.hpp"

namespace Star {

// Shadow-only REG-05 model: a failure invalidates unconfirmed cross-region
// outputs for the affected logical step and blocks treating a partial publish
// as reverted. Checkpoint tokens gate a documented return-to-legacy path.
// No live threads, metrics collection, or world writes.

enum class WorldRegionRecoveryMode : uint8_t {
  RegionalShadow = 0,
  Legacy
};

enum class WorldRegionStepHealth : uint8_t {
  Open = 0,
  Failed,
  Checkpointed
};

struct WorldRegionCheckpointId {
  uint64_t worldEpoch = 0;
  uint64_t sequence = 0;

  bool operator==(WorldRegionCheckpointId const& rhs) const;
  bool operator<(WorldRegionCheckpointId const& rhs) const;
};

struct WorldRegionCheckpoint {
  WorldRegionCheckpointId id;
  uint64_t logicalStep = 0;
  Set<Vec2U> regions;
  bool consistent = true;
};

struct WorldRegionFailureRecord {
  uint64_t logicalStep = 0;
  Vec2U region;
  List<uint64_t> invalidatedIntentSequences;
};

class WorldRegionRecoveryShadow {
public:
  explicit WorldRegionRecoveryShadow(uint64_t worldEpoch, WorldRegionPartition partition);

  WorldRegionRecoveryShadow(WorldRegionRecoveryShadow const&) = delete;
  WorldRegionRecoveryShadow& operator=(WorldRegionRecoveryShadow const&) = delete;

  // Registers an unconfirmed intent sequence under a region for a step.
  bool noteUnconfirmed(uint64_t logicalStep, Vec2U const& region, uint64_t intentSequence);

  // Failure invalidates all unconfirmed intents for that step/region and marks
  // the step Failed. Partially published regions stay marked until a consistent
  // checkpoint or explicit legacy reload.
  Maybe<WorldRegionFailureRecord> fail(uint64_t logicalStep, Vec2U const& region);

  // A consistent checkpoint may only cover regions that are not partially
  // published without prior invalidation.
  Maybe<WorldRegionCheckpointId> checkpoint(uint64_t logicalStep, List<Vec2U> regions);

  // Return-to-legacy requires a consistent checkpoint for the current failed
  // step or an explicit full reload token.
  bool returnToLegacy(WorldRegionCheckpointId const& checkpoint);
  bool reloadLegacy();

  WorldRegionRecoveryMode mode() const;
  Maybe<WorldRegionStepHealth> stepHealth(uint64_t logicalStep) const;
  bool regionPartiallyPublished(Vec2U const& region) const;
  size_t unconfirmedCount(uint64_t logicalStep, Vec2U const& region) const;
  Maybe<WorldRegionCheckpoint> checkpointRecord(WorldRegionCheckpointId const& id) const;

private:
  uint64_t const m_worldEpoch;
  WorldRegionPartition m_partition;
  uint64_t m_checkpointSequence = 0;
  WorldRegionRecoveryMode m_mode = WorldRegionRecoveryMode::RegionalShadow;
  Map<uint64_t, WorldRegionStepHealth> m_steps;
  Map<Vec2U, Set<uint64_t>> m_unconfirmed; // region -> intent sequences (latest step tracked below)
  Map<Vec2U, uint64_t> m_unconfirmedStep;
  Set<Vec2U> m_partialPublish;
  Map<WorldRegionCheckpointId, WorldRegionCheckpoint> m_checkpoints;
};

}
