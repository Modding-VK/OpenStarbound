#include "StarWorldGenerationExecutivePriority.hpp"

#include <algorithm>

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

WorldGenerationExecutivePriorityScore WorldGenerationExecutivePriority::score(
    WorldGenerationDependencyNode const& node, WorldGenerationExecutiveDemand const& demand) {
  WorldGenerationExecutivePriorityScore out;
  out.node = node;
  out.consumer = demand.consumer;
  out.interest = demand.interest;
  out.ageTick = demand.ageTick;
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
  uint64_t ageScore = demand.ageTick > 999ull ? 999ull : demand.ageTick;
  // Interest dominates; closer then older; age never outranks a higher class.
  out.total = interestWeight * 1000000000ull + distanceScore * 1000ull + ageScore;
  return out;
}

Maybe<WorldGenerationExecutivePriorityScore> WorldGenerationExecutivePriority::selectFair(
    List<WorldGenerationDependencyNode> const& frontier,
    List<WorldGenerationExecutiveDemand> const& demands) {
  if (frontier.empty() || demands.empty())
    return {};

  Maybe<WorldGenerationExecutivePriorityScore> best;
  for (auto const& node : frontier) {
    Maybe<WorldGenerationExecutivePriorityScore> bestForNode;
    for (auto const& demand : demands) {
      auto candidate = score(node, demand);
      if (!bestForNode || candidate.total > bestForNode->total
          || (candidate.total == bestForNode->total && candidate.ageTick > bestForNode->ageTick)
          || (candidate.total == bestForNode->total && candidate.ageTick == bestForNode->ageTick
              && candidate.consumer < bestForNode->consumer)) {
        bestForNode = candidate;
      }
    }
    if (!bestForNode)
      continue;
    if (!best || bestForNode->total > best->total
        || (bestForNode->total == best->total && bestForNode->node < best->node)
        || (bestForNode->total == best->total && bestForNode->node == best->node
            && bestForNode->consumer < best->consumer)) {
      best = bestForNode;
    }
  }
  return best;
}

}
