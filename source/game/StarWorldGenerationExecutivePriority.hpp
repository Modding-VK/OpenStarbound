#pragma once

#include <cstddef>
#include <cstdint>

#include "StarList.hpp"
#include "StarMaybe.hpp"
#include "StarWorldGenerationDependency.hpp"
#include "StarWorldGenerationExecutiveShadow.hpp"

namespace Star {

// First DAG-03 slice: shadow-only ranking for executive frontier selection.
// Does not mutate WorldStorage or authorize publication.

enum class WorldGenerationInterestClass : uint8_t {
  Background = 0,
  Nearby,
  Visible,
  Immediate
};

struct WorldGenerationExecutiveDemand {
  WorldGenerationExecutiveConsumerId consumer;
  WorldGenerationInterestClass interest = WorldGenerationInterestClass::Nearby;
  Vec2U focusSector;
  uint64_t ageTick = 0;
  uint32_t worldWidthSectors = 0; // 0 disables horizontal wrap
};

struct WorldGenerationExecutivePriorityScore {
  WorldGenerationDependencyNode node;
  WorldGenerationExecutiveConsumerId consumer;
  WorldGenerationInterestClass interest = WorldGenerationInterestClass::Nearby;
  uint64_t ageTick = 0;
  uint32_t distance = 0;
  uint64_t total = 0;
};

class WorldGenerationExecutivePriority {
public:
  static uint32_t sectorDistance(Vec2U const& a, Vec2U const& b, uint32_t worldWidthSectors);

  // Higher total is more urgent. Interest class dominates, then lower distance,
  // then older age, then stable node/consumer order.
  static WorldGenerationExecutivePriorityScore score(WorldGenerationDependencyNode const& node,
      WorldGenerationExecutiveDemand const& demand);

  // Pick one frontier node. Deduplication keeps the most urgent consumer age.
  // Dense new demand cannot erase an older Visible/Immediate opportunity when
  // both remain on the frontier.
  static Maybe<WorldGenerationExecutivePriorityScore> selectFair(
      List<WorldGenerationDependencyNode> const& frontier,
      List<WorldGenerationExecutiveDemand> const& demands);
};

}
