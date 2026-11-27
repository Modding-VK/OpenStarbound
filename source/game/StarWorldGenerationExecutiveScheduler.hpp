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

// Shadow CPU-03 / WT-01 / WT-02 slice: fair frontier pick, mark executing,
// complete via mailbox, and pass budgets with explicit exit reasons.
// Never writes the live world.

struct WorldGenerationExecutiveScheduleStep {
  WorldGenerationDependencyNode node;
  WorldGenerationExecutiveConsumerId consumer;
  uint64_t resultToken = 0;
};

enum class WorldGenerationExecutivePassExitReason : uint8_t {
  EmptyMailboxAndFrontier = 0,
  NoEligibleNode,
  StepBudgetExhausted,
  DrainBudgetExhausted,
  DeadlineReached,
  Error
};

struct WorldGenerationExecutivePassBudget {
  size_t maxStarts = 1;
  size_t maxDrainMessages = 64;
  // 0 disables the deadline check.
  uint64_t deadlineTick = 0;
};

struct WorldGenerationExecutivePassResult {
  WorldGenerationExecutivePassExitReason reason = WorldGenerationExecutivePassExitReason::EmptyMailboxAndFrontier;
  size_t starts = 0;
  size_t drained = 0;
  size_t applied = 0;
  uint64_t beganTick = 0;
  uint64_t endedTick = 0;
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

  // WT-02: drain then start within budgets; record comparable exit cause.
  WorldGenerationExecutivePassResult runPass(
      WorldGenerationDependencyGraph::CurrentLevel const& currentLevel,
      List<WorldGenerationExecutiveDemand> const& demands,
      WorldGenerationExecutivePassBudget const& budget,
      uint64_t nowTick,
      uint64_t firstResultToken);

private:
  WorldGenerationExecutiveShadowGraph* m_shadow = nullptr;
  WorldGenerationExecutiveMailbox* m_mailbox = nullptr;
};

}
