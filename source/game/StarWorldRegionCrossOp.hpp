#pragma once

#include <cstddef>
#include <cstdint>

#include "StarList.hpp"
#include "StarMap.hpp"
#include "StarMaybe.hpp"
#include "StarSet.hpp"
#include "StarWorldRegionPartition.hpp"

namespace Star {

// Shadow-only REG-03 model: versioned cross-region intentions under a common
// logical step. It reserves footprints in stable order, records
// commit/reject/timeout, and never writes live world state or runs Lua.

enum class WorldRegionCrossIntentKind : uint8_t {
  BorderPublish = 0,
  LiquidBridge,
  WireBridge,
  Generic
};

enum class WorldRegionCrossIntentState : uint8_t {
  Proposed = 0,
  Reserved,
  Committed,
  Rejected,
  TimedOut
};

enum class WorldRegionCrossOpOutcome : uint8_t {
  Accepted = 0,
  Conflict,
  StaleStep,
  Unknown,
  Invalid
};

struct WorldRegionCrossIntentId {
  uint64_t worldEpoch = 0;
  uint64_t sequence = 0;

  bool operator==(WorldRegionCrossIntentId const& rhs) const;
  bool operator!=(WorldRegionCrossIntentId const& rhs) const;
  bool operator<(WorldRegionCrossIntentId const& rhs) const;
};

struct WorldRegionCrossIntent {
  WorldRegionCrossIntentId id;
  uint64_t logicalStep = 0;
  Vec2U sourceRegion;
  List<Vec2U> targetRegions;
  List<Vec2U> footprintSectors;
  WorldRegionCrossIntentKind kind = WorldRegionCrossIntentKind::Generic;
  WorldRegionCrossIntentState state = WorldRegionCrossIntentState::Proposed;
};

// World-thread-owned shadow ledger for multiregion intentions. Outstanding
// reservations are exclusive by sector; completion order among reserved
// intents does not rewrite earlier conflict decisions.
class WorldRegionCrossOpShadow {
public:
  explicit WorldRegionCrossOpShadow(uint64_t worldEpoch, WorldRegionPartition partition);

  WorldRegionCrossOpShadow(WorldRegionCrossOpShadow const&) = delete;
  WorldRegionCrossOpShadow& operator=(WorldRegionCrossOpShadow const&) = delete;

  // Rejects empty footprints, unsorted duplicates, unknown regions, or a step
  // older than the ledger's current barrier. On success the intent is Proposed.
  Maybe<WorldRegionCrossIntentId> propose(uint64_t logicalStep, Vec2U const& sourceRegion,
      List<Vec2U> targetRegions, List<Vec2U> footprintSectors,
      WorldRegionCrossIntentKind kind = WorldRegionCrossIntentKind::Generic);

  // Reserves every footprint sector in lexicographic order. A busy sector
  // yields Conflict without partial reservation. Wall-clock finish order of
  // other intents never bypasses this check.
  WorldRegionCrossOpOutcome reserve(WorldRegionCrossIntentId const& id);

  WorldRegionCrossOpOutcome commit(WorldRegionCrossIntentId const& id);
  WorldRegionCrossOpOutcome reject(WorldRegionCrossIntentId const& id);
  WorldRegionCrossOpOutcome timeout(WorldRegionCrossIntentId const& id);

  Maybe<WorldRegionCrossIntent> intent(WorldRegionCrossIntentId const& id) const;
  size_t size() const;
  size_t reservedSectorCount() const;
  Maybe<uint64_t> currentStep() const;

private:
  bool canonicalize(List<Vec2U>& regions, List<Vec2U>& sectors) const;
  bool releaseReservation(WorldRegionCrossIntentId const& id);

  uint64_t const m_worldEpoch;
  WorldRegionPartition m_partition;
  uint64_t m_sequence = 0;
  Maybe<uint64_t> m_currentStep;
  Map<WorldRegionCrossIntentId, WorldRegionCrossIntent> m_intents;
  Map<Vec2U, WorldRegionCrossIntentId> m_reservedSectors;
};

}
