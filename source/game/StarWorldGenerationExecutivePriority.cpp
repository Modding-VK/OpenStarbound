#include "StarWorldGenerationExecutivePriority.hpp"

namespace Star {

uint32_t WorldGenerationExecutivePriority::sectorDistance(Vec2U const& a, Vec2U const& b,
    uint32_t worldWidthSectors) {
  uint32_t dy = a[1] > b[1] ? a[1] - b[1] : b[1] - a[1];
  uint32_t dx = a[0] > b[0] ? a[0] - b[0] : b[0] - a[0];
  if (worldWidthSectors > 0) {
    uint32_t wrap = worldWidthSectors - dx;
    if (wrap < dx)
      dx = wrap;
  }
  return dx + dy;
}

bool WorldGenerationExecutivePriority::demandActive(WorldGenerationExecutiveDemand const& demand,
    WorldGenerationExecutivePriorityPolicy const& policy) {
  if (policy.ttlTicks == 0)
    return true;
  if (policy.nowTick < demand.ageTick)
    return true;
  return (policy.nowTick - demand.ageTick) <= policy.ttlTicks;
}

WorldGenerationExecutivePriorityScore WorldGenerationExecutivePriority::score(
    WorldGenerationDependencyNode const& node, WorldGenerationExecutiveDemand const& demand,
    size_t unlockBenefit) {
  WorldGenerationExecutivePriorityScore out;
  out.node = node;
  out.consumer = demand.consumer;
  out.interest = demand.interest;
  out.ageTick = demand.ageTick;
  out.etaTick = demand.etaTick;
  out.unlockBenefit = unlockBenefit;
  out.distance = sectorDistance(node.sector, demand.focusSector, demand.worldWidthSectors);

  uint64_t interestWeight = 1;
  switch (demand.interest) {
    case WorldGenerationInterestClass::Background:
      interestWeight = 1;
      break;
    case WorldGenerationInterestClass::Nearby:
      interestWeight = 10;
      break;
    case WorldGenerationInterestClass::Visible:
      interestWeight = 100;
      break;
    case WorldGenerationInterestClass::Immediate:
      interestWeight = 1000;
      break;
  }

  uint64_t cappedDistance = out.distance > 999999u ? 999999u : out.distance;
  uint64_t distanceScore = 1000000ull - cappedDistance;
  uint64_t etaScore = 0;
  if (demand.etaTick != 0) {
    uint64_t cappedEta = demand.etaTick > 999999ull ? 999999ull : demand.etaTick;
    etaScore = 1000000ull - cappedEta;
  }
  uint64_t ageScore = demand.ageTick > 999ull ? 999ull : demand.ageTick;
  uint64_t unlockScore = unlockBenefit > 999ull ? 999ull : unlockBenefit;
  // Interest dominates; closer then earlier eta then older age; unlock is a
  // stable tie-break within the same class/distance/eta/age band.
  out.total = interestWeight * 1000000000ull + distanceScore * 1000ull + etaScore
      + ageScore * 10ull + unlockScore;
  return out;
}

Maybe<WorldGenerationExecutivePriorityScore> WorldGenerationExecutivePriority::selectFair(
    List<WorldGenerationDependencyNode> const& frontier,
    List<WorldGenerationExecutiveDemand> const& demands) {
  WorldGenerationExecutivePriorityPolicy policy;
  auto trace = selectFairTraced(frontier, demands, policy);
  return trace.chosen;
}

WorldGenerationExecutiveSelectionTrace WorldGenerationExecutivePriority::selectFairTraced(
    List<WorldGenerationDependencyNode> const& frontier,
    List<WorldGenerationExecutiveDemand> const& demands,
    WorldGenerationExecutivePriorityPolicy const& policy,
    Map<WorldGenerationDependencyNode, size_t> const& unlockBenefits) {
  WorldGenerationExecutiveSelectionTrace trace;
  if (frontier.empty()) {
    WorldGenerationExecutiveBlocker blocker;
    blocker.kind = WorldGenerationExecutiveBlockerKind::EmptyFrontier;
    if (trace.blockers.size() < policy.maxBlockers)
      trace.blockers.append(blocker);
    return trace;
  }

  List<WorldGenerationExecutiveDemand> active;
  for (auto const& demand : demands) {
    if (demandActive(demand, policy)) {
      active.append(demand);
      ++trace.activeDemands;
    } else {
      ++trace.expiredDemands;
      if (trace.blockers.size() < policy.maxBlockers) {
        WorldGenerationExecutiveBlocker blocker;
        blocker.kind = WorldGenerationExecutiveBlockerKind::TtlExpired;
        blocker.consumer = demand.consumer;
        trace.blockers.append(blocker);
      }
    }
  }
  if (active.empty()) {
    if (trace.blockers.size() < policy.maxBlockers) {
      WorldGenerationExecutiveBlocker blocker;
      blocker.kind = WorldGenerationExecutiveBlockerKind::NoActiveDemand;
      trace.blockers.append(blocker);
    }
    return trace;
  }

  Maybe<WorldGenerationExecutivePriorityScore> best;
  size_t backgroundSeen = 0;
  for (auto const& node : frontier) {
    size_t unlock = 0;
    if (auto it = unlockBenefits.find(node); it != unlockBenefits.end())
      unlock = it->second;

    Maybe<WorldGenerationExecutivePriorityScore> bestForNode;
    for (auto const& demand : active) {
      auto candidate = score(node, demand, unlock);
      if (!bestForNode || candidate.total > bestForNode->total
          || (candidate.total == bestForNode->total && candidate.ageTick > bestForNode->ageTick)
          || (candidate.total == bestForNode->total && candidate.ageTick == bestForNode->ageTick
              && candidate.consumer < bestForNode->consumer)) {
        bestForNode = candidate;
      }
    }
    if (!bestForNode)
      continue;

    if (bestForNode->interest == WorldGenerationInterestClass::Background
        && backgroundSeen >= policy.maxBackgroundSelections) {
      if (trace.blockers.size() < policy.maxBlockers) {
        WorldGenerationExecutiveBlocker blocker;
        blocker.kind = WorldGenerationExecutiveBlockerKind::LoadCapBackground;
        blocker.consumer = bestForNode->consumer;
        blocker.node = bestForNode->node;
        trace.blockers.append(blocker);
      }
      continue;
    }

    if (!best || bestForNode->total > best->total
        || (bestForNode->total == best->total && bestForNode->node < best->node)
        || (bestForNode->total == best->total && bestForNode->node == best->node
            && bestForNode->consumer < best->consumer)) {
      best = bestForNode;
    }
    if (bestForNode->interest == WorldGenerationInterestClass::Background)
      ++backgroundSeen;
  }

  if (!best)
    return trace;

  trace.chosen = best;
  if (best->unlockBenefit > 0
      && best->interest != WorldGenerationInterestClass::Background)
    trace.reason = WorldGenerationExecutiveSelectionReason::UnlockBenefit;
  else if (best->interest == WorldGenerationInterestClass::Visible
      || best->interest == WorldGenerationInterestClass::Immediate)
    trace.reason = WorldGenerationExecutiveSelectionReason::FairAgeRetention;
  else if (frontier.size() == 1)
    trace.reason = WorldGenerationExecutiveSelectionReason::OnlyEligible;
  else
    trace.reason = WorldGenerationExecutiveSelectionReason::HighestInterest;
  return trace;
}

}
