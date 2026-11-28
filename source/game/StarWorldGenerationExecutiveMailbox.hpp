#pragma once

#include <cstddef>
#include <cstdint>

#include "StarList.hpp"
#include "StarMaybe.hpp"
#include "StarWorldGenerationDependency.hpp"
#include "StarWorldGenerationExecutiveShadow.hpp"

namespace Star {

// DAG-04 shadow mailbox: carries identity and an immutable result token from
// worker completion to the owner. Does not mutate WorldStorage.

struct WorldGenerationExecutiveMailboxMessage {
  WorldGenerationDependencyNode node;
  WorldGenerationExecutiveConsumerId consumer;
  uint64_t resultToken = 0;
  uint64_t workerFinishedTick = 0;
  uint64_t firstObservedTick = 0;
  bool success = true;
};

class WorldGenerationExecutiveMailbox {
public:
  explicit WorldGenerationExecutiveMailbox(size_t capacity = 64);

  bool post(WorldGenerationExecutiveMailboxMessage message);
  Maybe<WorldGenerationExecutiveMailboxMessage> consume();
  size_t size() const;
  size_t capacity() const;
  bool empty() const;

  // Apply a consumed success to the shadow graph: mark prepared/published only
  // when still authorized. Failures invalidate without proving producer stop.
  bool applyToShadow(WorldGenerationExecutiveShadowGraph& shadow,
      WorldGenerationExecutiveMailboxMessage const& message);

private:
  size_t m_capacity = 0;
  List<WorldGenerationExecutiveMailboxMessage> m_messages;
};

}
