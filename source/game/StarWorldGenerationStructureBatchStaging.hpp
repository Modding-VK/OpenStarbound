#pragma once

#include "StarWorldGenerationStructureCommandPlan.hpp"

#include <cstdint>
#include <optional>
#include <utility>

namespace Star {

// MD-05 first slice: invisible batch staging over a structure command plan.
// Batches preserve command sequence; partial staging is never "committed" and
// discard drops all private effects. tryCommit does not publish to a live facade.
enum class WorldGenerationStructureBatchPhase : uint8_t {
  Materials = 0,
  Entities = 1,
  Marks = 2,
  Mixed = 3
};

struct WorldGenerationStructureBatch {
  uint64_t index = 0;
  WorldGenerationStructureBatchPhase phase = WorldGenerationStructureBatchPhase::Mixed;
  List<WorldGenerationStructureCommand> commands;
};

struct WorldGenerationStructureResumeToken {
  WorldGenerationStructureCommandPlanIdentity planIdentity;
  uint64_t nextBatchIndex = 0;
  uint64_t appliedCommands = 0;
  unsigned formatVersion = 1;

  bool valid() const {
    return planIdentity.valid() && formatVersion == 1;
  }
};

struct WorldGenerationStructureStagingCell {
  Maybe<uint64_t> foregroundMaterial;
  Maybe<uint64_t> backgroundMaterial;
  Maybe<uint64_t> liquid;
  Maybe<uint64_t> dungeonId;
  uint32_t entityOps = 0;
  uint32_t markOps = 0;
};

inline WorldGenerationStructureBatchPhase worldGenerationStructureCommandPhase(
    WorldGenerationStructureCommandKind kind) {
  switch (kind) {
    case WorldGenerationStructureCommandKind::SetForegroundMaterial:
    case WorldGenerationStructureCommandKind::SetBackgroundMaterial:
    case WorldGenerationStructureCommandKind::SetForegroundMod:
    case WorldGenerationStructureCommandKind::SetBackgroundMod:
    case WorldGenerationStructureCommandKind::SetLiquid:
    case WorldGenerationStructureCommandKind::SetDungeonId:
    case WorldGenerationStructureCommandKind::SetTileProtection:
    case WorldGenerationStructureCommandKind::ClearTileEntities:
      return WorldGenerationStructureBatchPhase::Materials;
    case WorldGenerationStructureCommandKind::SetPlayerStart:
    case WorldGenerationStructureCommandKind::PlaceObject:
    case WorldGenerationStructureCommandKind::PlaceFlora:
    case WorldGenerationStructureCommandKind::SpawnNpc:
    case WorldGenerationStructureCommandKind::PlaceVehicle:
    case WorldGenerationStructureCommandKind::SpawnStagehand:
    case WorldGenerationStructureCommandKind::AddDrop:
    case WorldGenerationStructureCommandKind::ConnectWire:
      return WorldGenerationStructureBatchPhase::Entities;
    case WorldGenerationStructureCommandKind::MarkTerrain:
    case WorldGenerationStructureCommandKind::MarkSpace:
      return WorldGenerationStructureBatchPhase::Marks;
  }
  return WorldGenerationStructureBatchPhase::Mixed;
}

inline List<WorldGenerationStructureBatch> planWorldGenerationStructureBatches(
    List<WorldGenerationStructureCommand> const& commands, size_t maxCommandsPerBatch) {
  List<WorldGenerationStructureBatch> batches;
  if (commands.empty() || maxCommandsPerBatch == 0)
    return batches;
  uint64_t index = 0;
  for (size_t offset = 0; offset < commands.size(); offset += maxCommandsPerBatch) {
    WorldGenerationStructureBatch batch;
    batch.index = index++;
    size_t end = min(offset + maxCommandsPerBatch, commands.size());
    Maybe<WorldGenerationStructureBatchPhase> phase;
    for (size_t i = offset; i < end; ++i) {
      batch.commands.append(commands[i]);
      auto commandPhase = worldGenerationStructureCommandPhase(commands[i].kind);
      if (!phase)
        phase = commandPhase;
      else if (*phase != commandPhase)
        phase = WorldGenerationStructureBatchPhase::Mixed;
    }
    batch.phase = phase ? *phase : WorldGenerationStructureBatchPhase::Mixed;
    batches.append(std::move(batch));
  }
  return batches;
}

// Owner-local invisible staging. Partial batches stay private until commit, and
// commit only marks the staging unit complete — it never installs live tiles.
class WorldGenerationStructureBatchStaging {
public:
  static std::optional<WorldGenerationStructureBatchStaging> tryOpen(
      WorldGenerationStructureCommandPlan const& plan, size_t maxCommandsPerBatch) {
    if (!plan.active() || plan.commands().empty() || maxCommandsPerBatch == 0)
      return std::nullopt;
    if (plan.footprint().truncated)
      return std::nullopt;
    auto batches = planWorldGenerationStructureBatches(plan.commands(), maxCommandsPerBatch);
    if (batches.empty())
      return std::nullopt;
    return WorldGenerationStructureBatchStaging(plan.identity(), std::move(batches));
  }

  WorldGenerationStructureCommandPlanIdentity const& identity() const { return m_identity; }
  List<WorldGenerationStructureBatch> const& batches() const { return m_batches; }
  uint64_t nextBatchIndex() const { return m_nextBatchIndex; }
  uint64_t appliedCommands() const { return m_appliedCommands; }
  bool active() const { return m_active; }
  bool committed() const { return m_committed; }
  bool complete() const {
    return m_active && m_nextBatchIndex >= m_batches.size();
  }

  WorldGenerationStructureResumeToken resumeToken() const {
    return {m_identity, m_nextBatchIndex, m_appliedCommands, 1};
  }

  // Apply one batch into private staging. Never mutates live world storage.
  bool applyNextBatch() {
    if (!m_active || m_committed || m_nextBatchIndex >= m_batches.size())
      return false;
    auto const& batch = m_batches[(size_t)m_nextBatchIndex];
    for (auto const& command : batch.commands)
      applyCommand(command);
    m_appliedCommands += batch.commands.size();
    ++m_nextBatchIndex;
    return true;
  }

  bool applyAllRemaining() {
    bool any = false;
    while (applyNextBatch())
      any = true;
    return any;
  }

  // Resume only from a matching token; does not replay already-applied batches.
  bool resumeFrom(WorldGenerationStructureResumeToken const& token) {
    if (!m_active || m_committed || !token.valid())
      return false;
    if (!m_identity.matches(token.planIdentity))
      return false;
    if (token.nextBatchIndex > m_batches.size() || token.appliedCommands > m_appliedCommands)
      return false;
    if (token.nextBatchIndex < m_nextBatchIndex)
      return false;
    m_nextBatchIndex = token.nextBatchIndex;
    return true;
  }

  // Completes the logical unit only when every batch is staged and identity /
  // reservations still match. Does not publish.
  bool tryCommit(WorldGenerationStructureCommandPlanIdentity const& liveExpectation,
      HashMap<Vec2U, uint64_t> const& liveRevisions,
      WorldGenerationStructureCommandPlan const& plan) {
    if (!m_active || m_committed || !complete())
      return false;
    if (!plan.active() || !plan.matches(liveExpectation) || !m_identity.matches(liveExpectation))
      return false;
    if (!plan.tryConsume(liveExpectation, liveRevisions))
      return false;
    m_committed = true;
    return true;
  }

  // Diagnostic view of private staging only. Not a live tile lease.
  Maybe<WorldGenerationStructureStagingCell> stagingCell(Vec2I position) const {
    if (!m_active)
      return {};
    if (auto const* cell = m_cells.ptr(position))
      return *cell;
    return {};
  }

  void discard() {
    m_active = false;
    m_committed = false;
    m_nextBatchIndex = 0;
    m_appliedCommands = 0;
    m_batches.clear();
    m_cells.clear();
    m_identity = {};
  }

private:
  WorldGenerationStructureBatchStaging(WorldGenerationStructureCommandPlanIdentity identity,
      List<WorldGenerationStructureBatch> batches)
    : m_identity(std::move(identity)), m_batches(std::move(batches)), m_active(true) {}

  void applyCommand(WorldGenerationStructureCommand const& command) {
    auto& cell = m_cells[command.position];
    switch (command.kind) {
      case WorldGenerationStructureCommandKind::SetForegroundMaterial:
      case WorldGenerationStructureCommandKind::SetForegroundMod:
        cell.foregroundMaterial = command.value;
        break;
      case WorldGenerationStructureCommandKind::SetBackgroundMaterial:
      case WorldGenerationStructureCommandKind::SetBackgroundMod:
        cell.backgroundMaterial = command.value;
        break;
      case WorldGenerationStructureCommandKind::SetLiquid:
        cell.liquid = command.value;
        break;
      case WorldGenerationStructureCommandKind::SetDungeonId:
      case WorldGenerationStructureCommandKind::SetTileProtection:
        cell.dungeonId = command.value;
        break;
      case WorldGenerationStructureCommandKind::ClearTileEntities:
      case WorldGenerationStructureCommandKind::PlaceObject:
      case WorldGenerationStructureCommandKind::PlaceFlora:
      case WorldGenerationStructureCommandKind::SpawnNpc:
      case WorldGenerationStructureCommandKind::PlaceVehicle:
      case WorldGenerationStructureCommandKind::SpawnStagehand:
      case WorldGenerationStructureCommandKind::AddDrop:
      case WorldGenerationStructureCommandKind::ConnectWire:
      case WorldGenerationStructureCommandKind::SetPlayerStart:
        ++cell.entityOps;
        break;
      case WorldGenerationStructureCommandKind::MarkTerrain:
      case WorldGenerationStructureCommandKind::MarkSpace:
        ++cell.markOps;
        break;
    }
  }

  WorldGenerationStructureCommandPlanIdentity m_identity;
  List<WorldGenerationStructureBatch> m_batches;
  HashMap<Vec2I, WorldGenerationStructureStagingCell> m_cells;
  uint64_t m_nextBatchIndex = 0;
  uint64_t m_appliedCommands = 0;
  bool m_active = false;
  bool m_committed = false;
};

}
