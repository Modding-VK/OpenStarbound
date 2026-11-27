#pragma once

#include <cstddef>
#include <cstdint>

#include "StarWorldGenerationExecutiveScheduler.hpp"

namespace Star {

// CPU-05 shadow slice: first capacity policy from executable queue / cost.
// Does not resize live WorkerPools or change simulation dt.

enum class WorldGenerationResourceProfile : uint8_t {
  Balanced = 0,
  HighThroughput
};

struct WorldGenerationResourceObservation {
  size_t executableFrontier = 0;
  size_t mailboxDepth = 0;
  size_t retainedProducts = 0;
  uint64_t lastPassTicksSpent = 0;
  // 0 disables overrun-based shrinking.
  uint64_t targetPassTicks = 0;
};

class WorldGenerationResourceGovernor {
public:
  explicit WorldGenerationResourceGovernor(WorldGenerationResourceProfile profile =
      WorldGenerationResourceProfile::Balanced);

  WorldGenerationResourceProfile profile() const;
  void setProfile(WorldGenerationResourceProfile profile);

  // Suggest pass budgets from observed demand and recent cost.
  WorldGenerationExecutivePassBudget suggest(WorldGenerationResourceObservation const& observation) const;

private:
  WorldGenerationResourceProfile m_profile = WorldGenerationResourceProfile::Balanced;
};

}
