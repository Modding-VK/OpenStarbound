#pragma once

#include "StarList.hpp"
#include "StarMap.hpp"
#include "StarMaybe.hpp"
#include "StarWorldRegionOwner.hpp"

namespace Star {

// REG-02 confirmed border observation. An absent region is unavailable, never
// empty terrain. External readers consume this immutable snapshot only.
struct WorldRegionBorderCell {
  Vec2I position{};
  bool present = false;
  uint64_t contentRevision = 0;
};

struct WorldRegionBorderSnapshot {
  WorldRegionOwnerIdentity owner;
  uint64_t captureStep = 0;
  uint64_t logicalAge = 0;
  List<WorldRegionBorderCell> cells;
};

enum class WorldRegionBorderCaptureResult {
  Captured,
  RegionAbsent,
  RegionDraining,
  EmptyCells,
  OwnerMismatch
};

// First REG-02 harness: capture a value-only border snapshot from an active
// owner. Does not grant write access to neighbors and does not copy live world
// tiles or entities.
class WorldRegionBorderLedger {
public:
  explicit WorldRegionBorderLedger(WorldRegionOwnerRegistry const& owners);

  Maybe<WorldRegionBorderSnapshot> latest(Vec2U const& region) const;
  WorldRegionBorderCaptureResult capture(Vec2U const& region, uint64_t captureStep, uint64_t logicalAge,
      List<WorldRegionBorderCell> cells);
  size_t snapshotCount() const;

private:
  WorldRegionOwnerRegistry const& m_owners;
  HashMap<Vec2U, WorldRegionBorderSnapshot> m_latest;
};

}
