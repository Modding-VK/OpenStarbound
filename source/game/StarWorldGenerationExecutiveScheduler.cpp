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

}
