#include "StarWorldRegionLocalState.hpp"

namespace Star {

WorldRegionLocalStateLedger::WorldRegionLocalStateLedger(WorldRegionOwnerRegistry const& owners)
  : m_owners(owners) {}

size_t WorldRegionLocalStateLedger::inventoryCount() const {
  return m_inventories.size();
}

Maybe<WorldRegionLocalInventorySnapshot> WorldRegionLocalStateLedger::snapshot(Vec2U const& region) const {
  if (auto inventory = m_inventories.ptr(region))
    return *inventory;
  return {};
}

bool WorldRegionLocalStateLedger::ownsKind(Vec2U const& region, WorldRegionLocalKind kind) const {
  if (auto inventory = m_inventories.ptr(region))
    return (inventory->kinds & worldRegionLocalKindMask(kind)) != 0;
  return false;
}

WorldRegionLocalDeclareResult WorldRegionLocalStateLedger::declareKinds(WorldRegionOwnerIdentity const& owner,
    uint8_t kinds) {
  if (kinds == 0)
    return WorldRegionLocalDeclareResult::EmptyKinds;

  auto current = m_owners.identity(owner.region);
  if (!current)
    return WorldRegionLocalDeclareResult::RegionAbsent;
  if (*current != owner)
    return WorldRegionLocalDeclareResult::OwnerMismatch;
  if (m_owners.phase(owner.region) == WorldRegionOwnerPhase::Draining)
    return WorldRegionLocalDeclareResult::RegionDraining;

  if (auto inventory = m_inventories.ptr(owner.region)) {
    if (inventory->owner != owner)
      return WorldRegionLocalDeclareResult::OwnerMismatch;
    auto merged = static_cast<uint8_t>(inventory->kinds | kinds);
    if (merged == inventory->kinds)
      return WorldRegionLocalDeclareResult::Unchanged;
    inventory->kinds = merged;
    ++inventory->stateRevision;
    return WorldRegionLocalDeclareResult::Raised;
  }

  WorldRegionLocalInventorySnapshot created;
  created.owner = owner;
  created.kinds = kinds;
  created.stateRevision = 1;
  m_inventories[owner.region] = created;
  return WorldRegionLocalDeclareResult::Declared;
}

}
