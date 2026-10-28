#pragma once

#include <array>

#include "StarList.hpp"
#include "StarMap.hpp"
#include "StarWorldRegionPartition.hpp"

namespace Star {

// Tracks only sectors in the current bounded observation batch. Changes to
// absent sectors remain observable even if they load and unload between two
// snapshots. Stamps are never reused across reset; no live world is retained.
class WorldRegionObservation {
public:
  static constexpr size_t MaxSectors = 1024;

  void reset();
  Maybe<uint64_t> watch(Vec2U const& sector);
  void changed(Vec2U const& sector);
  Maybe<uint64_t> revision(Vec2U const& sector) const;

private:
  uint64_t m_sequence = 0;
  HashMap<Vec2U, uint64_t> m_revisions;
};

// Value-only observation of one physical sector. An absent sector is explicit;
// it must not be interpreted as empty terrain. Incarnation distinguishes a
// reload from the previous occupant even if its content revision is identical.
struct WorldRegionSectorState {
  Vec2U sector;
  bool loaded = false;
  uint64_t incarnation = 0;
  uint64_t contentRevision = 0;
  uint8_t loadLevel = 0;
  uint8_t generationLevel = 0;

  bool operator==(WorldRegionSectorState const& rhs) const;
};

struct WorldRegionSnapshot {
  Vec2U region;
  uint64_t captureStep = 0;
  List<WorldRegionSectorState> sectors;
};

struct WorldRegionTurn {
  WorldRegionTurn(uint64_t epoch, uint64_t sequence, unsigned lane, WorldRegionSnapshot snapshot);

  uint64_t const worldEpoch;
  uint64_t const sequence;
  unsigned const lane;
  WorldRegionSnapshot const snapshot;
};

enum class WorldRegionTurnOutcome { Accepted, Stale, Foreign };

// World-thread-owned shadow scheduler. Lanes model exclusive regional turns;
// they are not worker threads and do not establish independence of live world
// state. Only immutable metadata leaves the coordinator. No callbacks run here.
class WorldRegionCoordinator {
public:
  static constexpr size_t MaxRegions = 256;
  static constexpr size_t MaxTurns = 8;

  WorldRegionCoordinator(uint64_t epoch, WorldRegionPartition partition, size_t maxTurns);
  WorldRegionCoordinator(WorldRegionCoordinator const&) = delete;
  WorldRegionCoordinator& operator=(WorldRegionCoordinator const&) = delete;

  // At most once per increasing logical step. Reject an entire malformed or
  // oversized frame; accepted frames rotate lexicographically after the last
  // selected region. Each region and lane may have only one outstanding turn.
  List<shared_ptr<WorldRegionTurn const>> schedule(uint64_t step, List<WorldRegionSnapshot> snapshots);
  // A correct ticket is retired even when its new snapshot is stale. A foreign
  // or previously retired ticket cannot release another outstanding turn.
  WorldRegionTurnOutcome finish(shared_ptr<WorldRegionTurn const> const& turn,
      WorldRegionSnapshot const& current);
  // Shadow work has no executing producers. Old immutable tickets survive as
  // observations but no longer authorize a finish after this cancellation.
  void cancelAll();
  size_t activeCount() const;

private:
  bool canonicalize(WorldRegionSnapshot& snapshot) const;

  uint64_t const m_epoch;
  WorldRegionPartition m_partition;
  size_t const m_maxTurns;
  uint64_t m_sequence = 0;
  Maybe<uint64_t> m_lastStep;
  Maybe<Vec2U> m_lastRegion;
  std::array<shared_ptr<WorldRegionTurn const>, MaxTurns> m_active{};
};

}
