#include "StarWorldRegionAuthoritativeOps.hpp"

namespace Star {

WorldRegionAuthoritativeOps::WorldRegionAuthoritativeOps(uint64_t worldEpoch, WorldRegionPartition partition,
    WorldRegionOwnerRegistry& /*owners*/, WorldRegionAuthoritativeOwnerLedger& authority)
  : m_authority(authority),
    m_crossOps(worldEpoch, partition),
    m_handoffs(worldEpoch, partition),
    m_recovery(worldEpoch, std::move(partition)) {}

bool WorldRegionAuthoritativeOps::isAuthoritative(Vec2U const& region) const {
  return m_authority.isAuthoritative(region);
}

WorldRegionAuthoritativeGateResult WorldRegionAuthoritativeOps::proposeCrossOp(uint64_t logicalStep,
    Vec2U const& sourceRegion, List<Vec2U> targetRegions, List<Vec2U> footprintSectors,
    WorldRegionCrossIntentKind kind, Maybe<WorldRegionCrossIntentId>& outId) {
  outId = {};
  if (!isAuthoritative(sourceRegion))
    return WorldRegionAuthoritativeGateResult::SourceNotAuthoritative;
  for (auto const& target : targetRegions) {
    if (!isAuthoritative(target))
      return WorldRegionAuthoritativeGateResult::TargetNotAuthoritative;
  }
  outId = m_crossOps.propose(logicalStep, sourceRegion, std::move(targetRegions), std::move(footprintSectors), kind);
  return WorldRegionAuthoritativeGateResult::Allowed;
}

WorldRegionAuthoritativeGateResult WorldRegionAuthoritativeOps::prepareHandoff(uint64_t logicalStep,
    WorldRegionEntityId const& entity, Vec2U const& destinationRegion, Maybe<WorldRegionEntityHandoffId>& outId) {
  outId = {};
  auto owner = m_handoffs.owner(entity);
  if (!owner || !isAuthoritative(owner->region))
    return WorldRegionAuthoritativeGateResult::SourceNotAuthoritative;
  if (!isAuthoritative(destinationRegion))
    return WorldRegionAuthoritativeGateResult::DestinationNotAuthoritative;
  outId = m_handoffs.prepare(logicalStep, entity, destinationRegion);
  return WorldRegionAuthoritativeGateResult::Allowed;
}

WorldRegionAuthoritativeGateResult WorldRegionAuthoritativeOps::noteUnconfirmed(uint64_t logicalStep,
    Vec2U const& region, uint64_t intentSequence) {
  if (!isAuthoritative(region))
    return WorldRegionAuthoritativeGateResult::RegionNotAuthoritative;
  m_recovery.noteUnconfirmed(logicalStep, region, intentSequence);
  return WorldRegionAuthoritativeGateResult::Allowed;
}

WorldRegionCrossOpShadow& WorldRegionAuthoritativeOps::crossOps() {
  return m_crossOps;
}

WorldRegionEntityHandoffShadow& WorldRegionAuthoritativeOps::handoffs() {
  return m_handoffs;
}

WorldRegionRecoveryShadow& WorldRegionAuthoritativeOps::recovery() {
  return m_recovery;
}

WorldRegionAuthoritativeOwnerLedger const& WorldRegionAuthoritativeOps::authority() const {
  return m_authority;
}

}
