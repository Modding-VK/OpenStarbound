#include "StarWorldGenerationExecutiveScheduler.hpp"

namespace Star {

WorldGenerationExecutiveScheduler::WorldGenerationExecutiveScheduler(
    WorldGenerationExecutiveShadowGraph& shadow, WorldGenerationExecutiveMailbox& mailbox)
  : m_shadow(&shadow), m_mailbox(&mailbox) {}

Maybe<WorldGenerationExecutiveScheduleStep> WorldGenerationExecutiveScheduler::startNext(
    WorldGenerationDependencyGraph::CurrentLevel const& currentLevel,
    List<WorldGenerationExecutiveDemand> const& demands,
    uint64_t resultToken) {
  if (!m_shadow || !m_mailbox)
    return {};
  auto frontier = m_shadow->preparationFrontier(currentLevel);
  // Skip nodes already executing or without publish rights.
  eraseWhere(frontier, [&](WorldGenerationDependencyNode const& node) {
    auto record = m_shadow->record(node);
    return !record || record->executing || !record->publishAuthorized
        || record->preparationPhase == WorldGenerationExecutiveNodePhase::Published
        || record->preparationPhase == WorldGenerationExecutiveNodePhase::Discarded;
  });
  auto selected = WorldGenerationExecutivePriority::selectFair(frontier, demands);
  if (!selected)
    return {};
  if (!m_shadow->markExecuting(selected->node, true))
    return {};
  return WorldGenerationExecutiveScheduleStep{selected->node, selected->consumer, resultToken};
}

bool WorldGenerationExecutiveScheduler::complete(WorldGenerationExecutiveScheduleStep const& step, bool success,
    uint64_t workerFinishedTick, uint64_t observedTick) {
  if (!m_mailbox)
    return false;
  WorldGenerationExecutiveMailboxMessage message;
  message.node = step.node;
  message.consumer = step.consumer;
  message.resultToken = step.resultToken;
  message.workerFinishedTick = workerFinishedTick;
  message.firstObservedTick = observedTick;
  message.success = success;
  return m_mailbox->post(std::move(message));
}

size_t WorldGenerationExecutiveScheduler::drainMailbox() {
  if (!m_shadow || !m_mailbox)
    return 0;
  size_t applied = 0;
  while (auto message = m_mailbox->consume()) {
    if (m_mailbox->applyToShadow(*m_shadow, *message))
      ++applied;
  }
  return applied;
}

WorldGenerationExecutivePassResult WorldGenerationExecutiveScheduler::runPass(
    WorldGenerationDependencyGraph::CurrentLevel const& currentLevel,
    List<WorldGenerationExecutiveDemand> const& demands,
    WorldGenerationExecutivePassBudget const& budget,
    uint64_t nowTick,
    uint64_t firstResultToken) {
  WorldGenerationExecutivePassResult result;
  result.beganTick = nowTick;
  result.endedTick = nowTick;
  if (!m_shadow || !m_mailbox) {
    result.reason = WorldGenerationExecutivePassExitReason::Error;
    return result;
  }

  if (budget.deadlineTick != 0 && nowTick >= budget.deadlineTick) {
    result.reason = WorldGenerationExecutivePassExitReason::DeadlineReached;
    return result;
  }

  size_t drainLimit = budget.maxDrainMessages;
  while (drainLimit > 0) {
    auto message = m_mailbox->consume();
    if (!message)
      break;
    ++result.drained;
    --drainLimit;
    if (m_mailbox->applyToShadow(*m_shadow, *message))
      ++result.applied;
  }
  if (!m_mailbox->empty() && drainLimit == 0) {
    result.reason = WorldGenerationExecutivePassExitReason::DrainBudgetExhausted;
    result.endedTick = nowTick;
    return result;
  }

  uint64_t nextToken = firstResultToken;
  while (result.starts < budget.maxStarts) {
    if (budget.deadlineTick != 0 && nowTick >= budget.deadlineTick) {
      result.reason = WorldGenerationExecutivePassExitReason::DeadlineReached;
      result.endedTick = nowTick;
      return result;
    }
    auto step = startNext(currentLevel, demands, nextToken);
    if (!step) {
      if (result.starts == 0 && result.drained == 0)
        result.reason = WorldGenerationExecutivePassExitReason::EmptyMailboxAndFrontier;
      else
        result.reason = WorldGenerationExecutivePassExitReason::NoEligibleNode;
      result.endedTick = nowTick;
      return result;
    }
    ++result.starts;
    ++nextToken;
  }

  result.reason = WorldGenerationExecutivePassExitReason::StepBudgetExhausted;
  result.endedTick = nowTick;
  return result;
}

}
