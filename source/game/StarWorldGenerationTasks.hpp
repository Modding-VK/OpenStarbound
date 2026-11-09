#pragma once

#include <atomic>

#include "StarMap.hpp"
#include "StarWorldRegionPartition.hpp"

namespace Star {

enum class WorldGenerationTaskStage : uint8_t {
  BaseTiles,
  // These stages reserve identity, memory and a region owner before their
  // producers are introduced. They do not authorize live-world writes.
  MicroDungeonPlan,
  CaveLiquidPlan
};

enum class WorldGenerationTaskState : uint8_t {
  Queued,
  Running,
  Ready,
  Failed
};

enum class WorldGenerationTaskOutcome : uint8_t {
  Committed,
  Stale,
  Cancelled,
  Failed
};

// A worker never mutates WorldServer directly. It returns a batch whose
// operations are validated and applied by the owning logical region.
enum class WorldTileMutationKind : uint8_t {
  ForegroundMaterial,
  BackgroundMaterial,
  ForegroundMod,
  BackgroundMod,
  DungeonId,
  Liquid,
  TileProtection,
  CollisionDirty
};

struct WorldGenerationTaskIdentity {
  uint64_t worldEpoch;
  uint64_t sequence;
  Vec2U sector;
  Vec2U region;
  WorldGenerationTaskStage stage;

  bool operator==(WorldGenerationTaskIdentity const& rhs) const;
  bool operator!=(WorldGenerationTaskIdentity const& rhs) const;
};

struct WorldTileMutation {
  Vec2I position;
  WorldTileMutationKind kind;
  uint64_t value = 0;
  uint64_t auxiliaryValue = 0;
  uint64_t sequence = 0;
};

struct WorldMutationBatch {
  WorldGenerationTaskIdentity identity;
  HashMap<Vec2U, uint64_t> expectedSectorRevisions;
  List<WorldTileMutation> tileMutations;

  bool empty() const;
  bool matches(WorldGenerationTaskIdentity const& task,
      HashMap<Vec2U, uint64_t> const& currentSectorRevisions) const;
  bool ordered() const;
};

// A worker owns only this handle. Invalidation revokes permission to commit;
// it does not stop an executing producer or release its registry reservation.
class WorldGenerationTask {
public:
  WorldGenerationTaskIdentity const identity;
  size_t const reservedBytes;

  bool start();
  bool ready();
  bool fail();
  void invalidate();
  bool valid() const;
  WorldGenerationTaskState state() const;

private:
  friend class WorldGenerationTasks;
  WorldGenerationTask(WorldGenerationTaskIdentity identity, size_t reservedBytes);

  atomic<WorldGenerationTaskState> m_state{WorldGenerationTaskState::Queued};
  atomic<bool> m_valid{true};
};

// The world thread exclusively owns this registry. Workers may transition a
// task's atomics but never access the registry, world, or partition. The caller
// supplies an epoch unique to the world instance and retires only after the
// producer promise confirms completion, including on unload and failure.
class WorldGenerationTasks {
public:
  WorldGenerationTasks(uint64_t worldEpoch, WorldRegionPartition partition,
      size_t maxTasks, size_t maxBytes);
  ~WorldGenerationTasks();

  WorldGenerationTasks(WorldGenerationTasks const&) = delete;
  WorldGenerationTasks& operator=(WorldGenerationTasks const&) = delete;

  shared_ptr<WorldGenerationTask> reserve(Vec2U const& sector,
      WorldGenerationTaskStage stage, size_t bytes);
  bool contains(shared_ptr<WorldGenerationTask> const& task) const;
  bool retire(shared_ptr<WorldGenerationTask> const& task, WorldGenerationTaskOutcome outcome);
  size_t size() const;
  size_t reservedBytes() const;

private:
  friend class WorldGenerationTaskPreparation;
  bool abandonPreparation(shared_ptr<WorldGenerationTask> const& task);

  using Key = pair<Vec2U, WorldGenerationTaskStage>;

  uint64_t const m_worldEpoch;
  WorldRegionPartition m_partition;
  size_t const m_maxTasks;
  size_t const m_maxBytes;
  uint64_t m_sequence = 0;
  size_t m_reservedBytes = 0;
  HashMap<Key, shared_ptr<WorldGenerationTask>> m_tasks;
};

// Checked shallow buffer accounting; this is not a measurement of retained
// allocator overhead or dynamic data owned/referenced by templates,
// distributions and producer outputs.
Maybe<size_t> worldGenerationSnapshotBytes(size_t bufferCount, size_t bufferBytes);

// Owner-thread reservation acquired before any private snapshot is copied.
// Until handoff, unwinding/early return retires an unsubmitted Queued task.
// A handed-off producer retains the normal registry reservation until drain.
class WorldGenerationTaskPreparation {
public:
  WorldGenerationTaskPreparation(WorldGenerationTasks& tasks, Vec2U const& sector,
      WorldGenerationTaskStage stage, size_t bytes);
  ~WorldGenerationTaskPreparation();

  WorldGenerationTaskPreparation(WorldGenerationTaskPreparation const&) = delete;
  WorldGenerationTaskPreparation& operator=(WorldGenerationTaskPreparation const&) = delete;

  shared_ptr<WorldGenerationTask> const& task() const { return m_task; }
  void handoff() noexcept { m_task.reset(); }

private:
  WorldGenerationTasks& m_tasks;
  shared_ptr<WorldGenerationTask> m_task;
};

}
