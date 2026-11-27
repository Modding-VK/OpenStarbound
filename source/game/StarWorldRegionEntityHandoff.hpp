#pragma once

#include <cstdint>

#include "StarList.hpp"
#include "StarMap.hpp"
#include "StarMaybe.hpp"
#include "StarWorldRegionCrossOp.hpp"
#include "StarWorldRegionPartition.hpp"

namespace Star {

// Shadow-only REG-04 model: at most one authoritative owner per entity
// identity within a logical step. Prepare → validate → commit updates owner
// jointly; stale or duplicate messages are rejected. No NPC externalization
// and no live-world mutation.

struct WorldRegionEntityId {
  uint64_t entityId = 0;
  uint64_t incarnation = 0;

  bool operator==(WorldRegionEntityId const& rhs) const;
  bool operator!=(WorldRegionEntityId const& rhs) const;
  bool operator<(WorldRegionEntityId const& rhs) const;
};

struct WorldRegionEntityOwner {
  WorldRegionEntityId entity;
  Vec2U region;
  uint64_t ownerGeneration = 0;
};

enum class WorldRegionEntityHandoffState : uint8_t {
  Prepared = 0,
  Committed,
  Rejected,
  Stale
};

struct WorldRegionEntityHandoffId {
  uint64_t worldEpoch = 0;
  uint64_t sequence = 0;

  bool operator==(WorldRegionEntityHandoffId const& rhs) const;
  bool operator<(WorldRegionEntityHandoffId const& rhs) const;
};

struct WorldRegionEntityHandoff {
  WorldRegionEntityHandoffId id;
  uint64_t logicalStep = 0;
  WorldRegionEntityId entity;
  Vec2U sourceRegion;
  Vec2U destinationRegion;
  uint64_t expectedOwnerGeneration = 0;
  WorldRegionEntityHandoffState state = WorldRegionEntityHandoffState::Prepared;
  Maybe<WorldRegionCrossIntentId> crossIntent;
};

enum class WorldRegionEntityHandoffOutcome : uint8_t {
  Accepted = 0,
  Conflict,
  Stale,
  Duplicate,
  Unknown,
  Invalid
};

class WorldRegionEntityHandoffShadow {
public:
  explicit WorldRegionEntityHandoffShadow(uint64_t worldEpoch, WorldRegionPartition partition);

  WorldRegionEntityHandoffShadow(WorldRegionEntityHandoffShadow const&) = delete;
  WorldRegionEntityHandoffShadow& operator=(WorldRegionEntityHandoffShadow const&) = delete;

  // Installs the initial exclusive owner for an entity incarnation.
  bool seedOwner(WorldRegionEntityOwner owner);

  // Prepares a transfer. Requires current owner match, distinct regions, and
  // optional cross-intent reserved/committed for the same step.
  Maybe<WorldRegionEntityHandoffId> prepare(uint64_t logicalStep, WorldRegionEntityId const& entity,
      Vec2U const& destinationRegion, Maybe<WorldRegionCrossIntentId> crossIntent = {});

  WorldRegionEntityHandoffOutcome commit(WorldRegionEntityHandoffId const& id);
  WorldRegionEntityHandoffOutcome reject(WorldRegionEntityHandoffId const& id);

  Maybe<WorldRegionEntityOwner> owner(WorldRegionEntityId const& entity) const;
  Maybe<WorldRegionEntityHandoff> handoff(WorldRegionEntityHandoffId const& id) const;
  size_t size() const;

  // Shadow-only: simulate an authoritative owner revision that invalidates a
  // prepared handoff without transferring the entity.
  bool bumpOwnerGeneration(WorldRegionEntityId const& entity);

private:
  uint64_t const m_worldEpoch;
  WorldRegionPartition m_partition;
  uint64_t m_sequence = 0;
  Map<WorldRegionEntityId, WorldRegionEntityOwner> m_owners;
  Map<WorldRegionEntityHandoffId, WorldRegionEntityHandoff> m_handoffs;
  // Idempotency: a completed transfer key cannot re-apply the same effect.
  Map<WorldRegionEntityId, uint64_t> m_lastCommittedStep;
};

}
