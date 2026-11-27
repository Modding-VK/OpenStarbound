#include "StarWorldGenerationExecutiveMailbox.hpp"

namespace Star {

WorldGenerationExecutiveMailbox::WorldGenerationExecutiveMailbox(size_t capacity)
  : m_capacity(capacity) {}

bool WorldGenerationExecutiveMailbox::post(WorldGenerationExecutiveMailboxMessage message) {
  if (!m_capacity || m_messages.size() >= m_capacity)
    return false;
  if (!message.firstObservedTick)
    message.firstObservedTick = message.workerFinishedTick;
  m_messages.append(std::move(message));
  return true;
}

Maybe<WorldGenerationExecutiveMailboxMessage> WorldGenerationExecutiveMailbox::consume() {
  if (m_messages.empty())
    return {};
  return m_messages.takeAt(0);
}

size_t WorldGenerationExecutiveMailbox::size() const {
  return m_messages.size();
}

size_t WorldGenerationExecutiveMailbox::capacity() const {
  return m_capacity;
}

bool WorldGenerationExecutiveMailbox::empty() const {
  return m_messages.empty();
}

bool WorldGenerationExecutiveMailbox::applyToShadow(WorldGenerationExecutiveShadowGraph& shadow,
    WorldGenerationExecutiveMailboxMessage const& message) {
  if (!shadow.contains(message.node))
    return false;
  shadow.markExecuting(message.node, false);
  if (!message.success)
    return shadow.invalidate(message.node);
  if (!shadow.markPrepared(message.node))
    return false;
  auto record = shadow.record(message.node);
  if (!record || !record->publishAuthorized)
    return false;
  return shadow.markPublished(message.node);
}

}
