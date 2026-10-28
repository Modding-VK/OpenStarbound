#pragma once

#include <atomic>

#include "StarMap.hpp"
#include "StarWorldRegionPartition.hpp"

namespace Star {

enum class WorldGenerationTaskStage : uint8_t {
  BaseTiles
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

struct WorldGenerationTaskIdentity {
  uint64_t worldEpoch;
  uint64_t sequence;
  Vec2U sector;
  Vec2U region;
  WorldGenerationTaskStage stage;

  bool operator==(WorldGenerationTaskIdentity const& rhs) const;
  bool operator!=(WorldGenerationTaskIdentity const& rhs) const;
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
  using Key = pair<Vec2U, WorldGenerationTaskStage>;

  uint64_t const m_worldEpoch;
  WorldRegionPartition m_partition;
  size_t const m_maxTasks;
  size_t const m_maxBytes;
  uint64_t m_sequence = 0;
  size_t m_reservedBytes = 0;
  HashMap<Key, shared_ptr<WorldGenerationTask>> m_tasks;
};

}
