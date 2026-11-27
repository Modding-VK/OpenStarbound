#pragma once

#include "StarList.hpp"
#include "StarMap.hpp"
#include "StarMathCommon.hpp"
#include "StarSet.hpp"
#include "StarWorldGenerationTmxBindingPlan.hpp"
#include "StarWorldRegionPartition.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <utility>

namespace Star {

// MD-04 first slice: private structure command + footprint plan. Describes
// reads/writes and ordered multi-region reservations without facade install.
enum class WorldGenerationStructureCommandKind : uint8_t {
  SetForegroundMaterial = 0,
  SetBackgroundMaterial = 1,
  SetForegroundMod = 2,
  SetBackgroundMod = 3,
  SetLiquid = 4,
  SetDungeonId = 5,
  SetTileProtection = 6,
  SetPlayerStart = 7,
  ClearTileEntities = 8,
  PlaceObject = 9,
  PlaceFlora = 10,
  SpawnNpc = 11,
  PlaceVehicle = 12,
  SpawnStagehand = 13,
  AddDrop = 14,
  ConnectWire = 15,
  MarkTerrain = 16,
  MarkSpace = 17
};

struct WorldGenerationStructureCommand {
  WorldGenerationStructureCommandKind kind = WorldGenerationStructureCommandKind::SetForegroundMaterial;
  Vec2I position;
  uint64_t sequence = 0;
  uint64_t value = 0;
  uint64_t auxiliary = 0;
  String label;

  bool operator==(WorldGenerationStructureCommand const& other) const {
    return kind == other.kind
        && position == other.position
        && sequence == other.sequence
        && value == other.value
        && auxiliary == other.auxiliary
        && label == other.label;
  }
};

struct WorldGenerationStructureFootprint {
  Set<Vec2I> readSet;
  Set<Vec2I> writeSet;
  Set<Vec2I> entityReach;
  // Truncated coverage is diagnostic only and never authorizes commit.
  bool truncated = false;

  bool empty() const {
    return readSet.empty() && writeSet.empty() && entityReach.empty();
  }

  size_t coveredCells() const {
    return readSet.size() + writeSet.size() + entityReach.size();
  }
};

struct WorldGenerationStructureRegionReservation {
  Vec2U region;
  uint64_t expectedRevision = 0;
  uint64_t sequence = 0;

  bool operator==(WorldGenerationStructureRegionReservation const& other) const {
    return region == other.region
        && expectedRevision == other.expectedRevision
        && sequence == other.sequence;
  }
};

struct WorldGenerationStructureCommandPlanIdentity {
  static constexpr unsigned ModelVersion = 1;
  static constexpr unsigned ProductVersion = 1;
  WorldGenerationTmxBindingPlanIdentity bindingIdentity;
  uint64_t structureEpoch = 0;
  unsigned productVersion = ProductVersion;

  bool valid() const {
    return productVersion == ProductVersion
        && bindingIdentity.valid()
        && structureEpoch != 0;
  }

  bool operator==(WorldGenerationStructureCommandPlanIdentity const& other) const {
    return bindingIdentity == other.bindingIdentity
        && structureEpoch == other.structureEpoch
        && productVersion == other.productVersion;
  }

  bool matches(WorldGenerationStructureCommandPlanIdentity const& expectation) const {
    return valid() && expectation.valid() && *this == expectation;
  }
};

struct WorldGenerationStructureCommandDraft {
  WorldGenerationStructureCommandKind kind = WorldGenerationStructureCommandKind::SetForegroundMaterial;
  Vec2I position;
  // Optional extra cells touched beyond the primary position (halo/callback).
  List<Vec2I> extraRead;
  List<Vec2I> extraWrite;
  List<Vec2I> entityReach;
  uint64_t value = 0;
  uint64_t auxiliary = 0;
  String label;
};

inline Vec2I worldGenerationStructureWrap(Vec2I position, Vec2U worldSize) {
  if (worldSize[0] == 0)
    return position;
  return Vec2I{pmod<int>(position[0], (int)worldSize[0]), position[1]};
}

inline WorldGenerationStructureFootprint planWorldGenerationStructureFootprint(
    List<WorldGenerationStructureCommandDraft> const& drafts, Vec2U worldSize) {
  WorldGenerationStructureFootprint footprint;
  auto addWrapped = [&](Set<Vec2I>& target, Vec2I position) {
    target.add(worldGenerationStructureWrap(position, worldSize));
  };
  for (auto const& draft : drafts) {
    addWrapped(footprint.writeSet, draft.position);
    addWrapped(footprint.readSet, draft.position);
    for (auto const& cell : draft.extraRead)
      addWrapped(footprint.readSet, cell);
    for (auto const& cell : draft.extraWrite)
      addWrapped(footprint.writeSet, cell);
    for (auto const& cell : draft.entityReach)
      addWrapped(footprint.entityReach, cell);
  }
  return footprint;
}

inline List<WorldGenerationStructureCommand> planWorldGenerationStructureCommands(
    List<WorldGenerationStructureCommandDraft> const& drafts) {
  List<WorldGenerationStructureCommand> commands;
  uint64_t sequence = 0;
  for (auto const& draft : drafts) {
    ++sequence;
    commands.append({draft.kind, draft.position, sequence, draft.value, draft.auxiliary, draft.label});
  }
  return commands;
}

inline List<WorldGenerationStructureRegionReservation> planWorldGenerationStructureReservations(
    WorldGenerationStructureFootprint const& footprint,
    WorldRegionPartition const& partition,
    HashMap<Vec2U, uint64_t> const& regionRevisions) {
  Set<Vec2U> regions;
  auto note = [&](Vec2I const& cell) {
    if (auto region = partition.regionForPosition(cell))
      regions.add(*region);
  };
  for (auto const& cell : footprint.writeSet)
    note(cell);
  for (auto const& cell : footprint.entityReach)
    note(cell);
  List<Vec2U> ordered(regions.begin(), regions.end());
  sort(ordered.begin(), ordered.end(), [](Vec2U const& a, Vec2U const& b) {
    return a[1] != b[1] ? a[1] < b[1] : a[0] < b[0];
  });
  List<WorldGenerationStructureRegionReservation> reservations;
  uint64_t sequence = 0;
  for (auto const& region : ordered) {
    ++sequence;
    uint64_t revision = 0;
    if (auto const* found = regionRevisions.ptr(region))
      revision = *found;
    reservations.append({region, revision, sequence});
  }
  return reservations;
}

// Conflict if any reservation's expected revision differs from live map.
inline bool worldGenerationStructureReservationsConflict(
    List<WorldGenerationStructureRegionReservation> const& reservations,
    HashMap<Vec2U, uint64_t> const& liveRevisions) {
  for (auto const& reservation : reservations) {
    auto const* live = liveRevisions.ptr(reservation.region);
    if (!live || *live != reservation.expectedRevision)
      return true;
  }
  return false;
}

// Private command plan. tryConsume never installs facade effects.
class WorldGenerationStructureCommandPlan {
public:
  static std::optional<WorldGenerationStructureCommandPlan> tryPlan(
      WorldGenerationStructureCommandPlanIdentity identity,
      WorldGenerationTmxBindingPlan const& binding,
      List<WorldGenerationStructureCommandDraft> const& drafts,
      Vec2U worldSize,
      WorldRegionPartition const& partition,
      HashMap<Vec2U, uint64_t> const& regionRevisions,
      bool markTruncated = false) {
    if (!identity.valid() || !binding.active())
      return std::nullopt;
    if (!binding.matches(identity.bindingIdentity))
      return std::nullopt;
    if (drafts.empty() || worldSize[0] == 0 || worldSize[1] == 0)
      return std::nullopt;

    auto footprint = planWorldGenerationStructureFootprint(drafts, worldSize);
    footprint.truncated = markTruncated;
    // Truncated footprints remain diagnostic: plan may exist, but consume fails.
    auto commands = planWorldGenerationStructureCommands(drafts);
    auto reservations = planWorldGenerationStructureReservations(footprint, partition, regionRevisions);
    if (reservations.empty())
      return std::nullopt;
    return WorldGenerationStructureCommandPlan(std::move(identity), std::move(commands),
        std::move(footprint), std::move(reservations));
  }

  WorldGenerationStructureCommandPlanIdentity const& identity() const { return m_identity; }
  List<WorldGenerationStructureCommand> const& commands() const { return m_commands; }
  WorldGenerationStructureFootprint const& footprint() const { return m_footprint; }
  List<WorldGenerationStructureRegionReservation> const& reservations() const {
    return m_reservations;
  }
  bool active() const { return m_active; }

  bool matches(WorldGenerationStructureCommandPlanIdentity const& expectation) const {
    return active() && m_identity.matches(expectation);
  }

  // Revalidate identity + live region revisions. Truncated footprints never
  // authorize consume. Success does not publish structure effects.
  bool tryConsume(WorldGenerationStructureCommandPlanIdentity const& liveExpectation,
      HashMap<Vec2U, uint64_t> const& liveRevisions) const {
    if (!active() || !m_identity.matches(liveExpectation))
      return false;
    if (m_footprint.truncated)
      return false;
    if (worldGenerationStructureReservationsConflict(m_reservations, liveRevisions))
      return false;
    return true;
  }

  void discard() {
    m_active = false;
    m_commands.clear();
    m_footprint = {};
    m_reservations.clear();
    m_identity = {};
  }

private:
  WorldGenerationStructureCommandPlan(WorldGenerationStructureCommandPlanIdentity identity,
      List<WorldGenerationStructureCommand> commands,
      WorldGenerationStructureFootprint footprint,
      List<WorldGenerationStructureRegionReservation> reservations)
    : m_identity(std::move(identity))
    , m_commands(std::move(commands))
    , m_footprint(std::move(footprint))
    , m_reservations(std::move(reservations))
    , m_active(true) {}

  WorldGenerationStructureCommandPlanIdentity m_identity;
  List<WorldGenerationStructureCommand> m_commands;
  WorldGenerationStructureFootprint m_footprint;
  List<WorldGenerationStructureRegionReservation> m_reservations;
  bool m_active = false;
};

}
