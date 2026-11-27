#pragma once

#include "StarWorldRegionAuthority.hpp"
#include "StarWorldRegionCrossOp.hpp"
#include "StarWorldRegionEntityHandoff.hpp"
#include "StarWorldRegionOwner.hpp"
#include "StarWorldRegionRecovery.hpp"

namespace Star {

// R2 beyond-shadow gate: REG-03/04/05 ledgers may only act when the named
// regions hold WorldRegionAuthorityGrade::Authoritative (R1 contract
// WorldRegionAuthoritativeOwner). Harness Active alone is refused.

enum class WorldRegionAuthoritativeGateResult : uint8_t {
  Allowed = 0,
  SourceNotAuthoritative,
  TargetNotAuthoritative,
  DestinationNotAuthoritative,
  RegionNotAuthoritative
};

class WorldRegionAuthoritativeOps {
public:
  WorldRegionAuthoritativeOps(uint64_t worldEpoch, WorldRegionPartition partition,
      WorldRegionOwnerRegistry& owners, WorldRegionAuthoritativeOwnerLedger& authority);

  WorldRegionAuthoritativeOps(WorldRegionAuthoritativeOps const&) = delete;
  WorldRegionAuthoritativeOps& operator=(WorldRegionAuthoritativeOps const&) = delete;

  WorldRegionAuthoritativeGateResult proposeCrossOp(uint64_t logicalStep, Vec2U const& sourceRegion,
      List<Vec2U> targetRegions, List<Vec2U> footprintSectors,
      WorldRegionCrossIntentKind kind, Maybe<WorldRegionCrossIntentId>& outId);

  WorldRegionAuthoritativeGateResult prepareHandoff(uint64_t logicalStep, WorldRegionEntityId const& entity,
      Vec2U const& destinationRegion, Maybe<WorldRegionEntityHandoffId>& outId);

  WorldRegionAuthoritativeGateResult noteUnconfirmed(uint64_t logicalStep, Vec2U const& region,
      uint64_t intentSequence);

  WorldRegionCrossOpShadow& crossOps();
  WorldRegionEntityHandoffShadow& handoffs();
  WorldRegionRecoveryShadow& recovery();
  WorldRegionAuthoritativeOwnerLedger const& authority() const;

private:
  bool isAuthoritative(Vec2U const& region) const;

  WorldRegionAuthoritativeOwnerLedger& m_authority;
  WorldRegionCrossOpShadow m_crossOps;
  WorldRegionEntityHandoffShadow m_handoffs;
  WorldRegionRecoveryShadow m_recovery;
};

}
