#pragma once

#include "StarWorldGenerationStructureBatchStaging.hpp"

#include <cstdint>
#include <optional>
#include <utility>

namespace Star {

// WT-04 first slice: shadowed publication pass budget over invisible structure
// staging. Advances private batches within a pass budget, defers dirty marks
// until a confirmed logical frontier, and never installs into the live world.
enum class WorldGenerationPublicationPassExit : uint8_t {
  Idle = 0,
  BudgetExhausted = 1,
  Complete = 2,
  StaleDiscarded = 3,
  BlockedIncomplete = 4
};

struct WorldGenerationPublicationPassBudget {
  uint32_t maxBatches = 1;
  uint32_t maxCommands = 32;

  bool valid() const { return maxBatches > 0 && maxCommands > 0; }
};

struct WorldGenerationPublicationDeferredDirty {
  Set<Vec2I> collisionDirty;
  Set<Vec2I> notificationCells;

  bool empty() const {
    return collisionDirty.empty() && notificationCells.empty();
  }

  void clear() {
    collisionDirty.clear();
    notificationCells.clear();
  }
};

struct WorldGenerationPublicationPassStats {
  uint32_t batchesApplied = 0;
  uint32_t commandsApplied = 0;
  uint32_t deferredDirtyCells = 0;
  WorldGenerationPublicationPassExit lastExit = WorldGenerationPublicationPassExit::Idle;
};

class WorldGenerationPublicationPass {
public:
  static std::optional<WorldGenerationPublicationPass> tryOpen(
      WorldGenerationStructureBatchStaging staging,
      WorldGenerationPublicationPassBudget budget) {
    if (!staging.active() || staging.committed() || !budget.valid())
      return std::nullopt;
    return WorldGenerationPublicationPass(std::move(staging), budget);
  }

  WorldGenerationPublicationPassBudget const& budget() const { return m_budget; }
  WorldGenerationPublicationPassStats const& stats() const { return m_stats; }
  WorldGenerationPublicationDeferredDirty const& deferredDirty() const {
    return m_deferredDirty;
  }
  WorldGenerationStructureBatchStaging const& staging() const { return m_staging; }
  bool active() const { return m_active; }
  bool frontierConfirmed() const { return m_frontierConfirmed; }

  // One shadowed pass: apply batches until budget or completion, then optionally
  // confirm the logical frontier. Never publishes live tiles/entities.
  WorldGenerationPublicationPassExit run(
      WorldGenerationStructureCommandPlanIdentity const& liveExpectation,
      HashMap<Vec2U, uint64_t> const& liveRevisions,
      WorldGenerationStructureCommandPlan const& plan) {
    if (!m_active)
      return recordExit(WorldGenerationPublicationPassExit::Idle);

    if (!plan.active() || !plan.matches(liveExpectation)
        || !m_staging.identity().matches(liveExpectation)) {
      discardStale();
      return recordExit(WorldGenerationPublicationPassExit::StaleDiscarded);
    }

    uint32_t batchesLeft = m_budget.maxBatches;
    uint32_t commandsLeft = m_budget.maxCommands;
    while (batchesLeft > 0 && commandsLeft > 0 && !m_staging.complete()) {
      auto const nextIndex = m_staging.nextBatchIndex();
      if (nextIndex >= m_staging.batches().size())
        break;
      auto const& batch = m_staging.batches()[(size_t)nextIndex];
      if (batch.commands.size() > commandsLeft)
        return recordExit(WorldGenerationPublicationPassExit::BudgetExhausted);
      if (!m_staging.applyNextBatch())
        break;
      ++m_stats.batchesApplied;
      m_stats.commandsApplied += (uint32_t)batch.commands.size();
      --batchesLeft;
      commandsLeft -= (uint32_t)batch.commands.size();
    }

    if (!m_staging.complete())
      return recordExit(WorldGenerationPublicationPassExit::BudgetExhausted);

    if (!m_staging.tryCommit(liveExpectation, liveRevisions, plan)) {
      // Completed staging but reservations/identity failed: drop invisible work.
      discardStale();
      return recordExit(WorldGenerationPublicationPassExit::StaleDiscarded);
    }

    confirmFrontier(plan);
    return recordExit(WorldGenerationPublicationPassExit::Complete);
  }

  // Explicit stale path before any deferred dirty becomes confirmed.
  void discardStale() {
    m_staging.discard();
    m_deferredDirty.clear();
    m_frontierConfirmed = false;
    m_active = false;
  }

  void discard() { discardStale(); }

private:
  WorldGenerationPublicationPass(WorldGenerationStructureBatchStaging staging,
      WorldGenerationPublicationPassBudget budget)
    : m_staging(std::move(staging)), m_budget(budget), m_active(true) {}

  WorldGenerationPublicationPassExit recordExit(WorldGenerationPublicationPassExit exit) {
    m_stats.lastExit = exit;
    return exit;
  }

  void confirmFrontier(WorldGenerationStructureCommandPlan const& plan) {
    // Only after logical commit: record deferred dirty/notification cells from
    // the footprint write set. Still invisible to live world observers.
    for (auto const& cell : plan.footprint().writeSet) {
      m_deferredDirty.collisionDirty.add(cell);
      m_deferredDirty.notificationCells.add(cell);
    }
    for (auto const& cell : plan.footprint().entityReach)
      m_deferredDirty.notificationCells.add(cell);
    m_stats.deferredDirtyCells = (uint32_t)(m_deferredDirty.collisionDirty.size()
        + m_deferredDirty.notificationCells.size());
    m_frontierConfirmed = true;
  }

  WorldGenerationStructureBatchStaging m_staging;
  WorldGenerationPublicationPassBudget m_budget;
  WorldGenerationPublicationDeferredDirty m_deferredDirty;
  WorldGenerationPublicationPassStats m_stats;
  bool m_active = false;
  bool m_frontierConfirmed = false;
};

}
