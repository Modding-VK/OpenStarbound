#pragma once

#include "StarMap.hpp"
#include "StarMaybe.hpp"
#include "StarWorldRegionOwner.hpp"

namespace Star {

// REG-02 inventory of local authority kinds. Declaring a kind does not copy
// live world state; it records which categories the owner claims exclusively.
enum class WorldRegionLocalKind : uint8_t {
  Tiles = 1 << 0,
  Liquids = 1 << 1,
  Collision = 1 << 2,
  AnchoredEntities = 1 << 3,
  Wires = 1 << 4,
  Damage = 1 << 5,
  LocalLua = 1 << 6
};

inline uint8_t worldRegionLocalKindMask(WorldRegionLocalKind kind) {
  return static_cast<uint8_t>(kind);
}

struct WorldRegionLocalInventorySnapshot {
  WorldRegionOwnerIdentity owner;
  uint8_t kinds = 0;
  uint64_t stateRevision = 0;
};

enum class WorldRegionLocalDeclareResult {
  Declared,
  Raised,
  Unchanged,
  RegionAbsent,
  RegionDraining,
  OwnerMismatch,
  EmptyKinds
};

// First REG-02 local-state harness: exclusive kind inventory per active owner.
// Neighbor regions cannot mutate another owner's inventory.
class WorldRegionLocalStateLedger {
public:
  explicit WorldRegionLocalStateLedger(WorldRegionOwnerRegistry const& owners);

  WorldRegionLocalDeclareResult declareKinds(WorldRegionOwnerIdentity const& owner, uint8_t kinds);
  Maybe<WorldRegionLocalInventorySnapshot> snapshot(Vec2U const& region) const;
  bool ownsKind(Vec2U const& region, WorldRegionLocalKind kind) const;
  size_t inventoryCount() const;

private:
  WorldRegionOwnerRegistry const& m_owners;
  HashMap<Vec2U, WorldRegionLocalInventorySnapshot> m_inventories;
};

}
