#pragma once

#include <cstddef>
#include <cstdint>

#include "StarList.hpp"
#include "StarMaybe.hpp"
#include "StarWorldGenerationDependency.hpp"
#include "StarWorldGenerationExecutiveMailbox.hpp"
#include "StarWorldGenerationExecutivePriority.hpp"
#include "StarWorldGenerationExecutiveShadow.hpp"

namespace Star {

// Shadow CPU-03 / WT-01 slice: fair frontier pick, mark executing, complete via
// mailbox. Never writes the live world.

struct WorldGenerationExecutiveScheduleStep {
  WorldGenerationDependencyNode node;
  WorldGenerationExecutiveConsumerId consumer;
  uint64_t resultToken = 0;
};

class WorldGenerationExecutiveScheduler {
public:
  WorldGenerationExecutiveScheduler(WorldGenerationExecutiveShadowGraph& shadow,
      WorldGenerationExecutiveMailbox& mailbox);

  Maybe<WorldGenerationExecutiveScheduleStep> startNext(
      WorldGenerationDependencyGraph::CurrentLevel const& currentLevel,
      List<WorldGenerationExecutiveDemand> const& demands,
      uint64_t resultToken);

  bool complete(WorldGenerationExecutiveScheduleStep const& step, bool success,
      uint64_t workerFinishedTick, uint64_t observedTick = 0);

  size_t drainMailbox();

private:
  WorldGenerationExecutiveShadowGraph* m_shadow = nullptr;
  WorldGenerationExecutiveMailbox* m_mailbox = nullptr;
};

}
