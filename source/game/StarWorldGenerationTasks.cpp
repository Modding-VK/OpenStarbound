#include "StarWorldGenerationTasks.hpp"

#include <limits>

namespace Star {

bool WorldGenerationTaskIdentity::operator==(WorldGenerationTaskIdentity const& rhs) const {
  return worldEpoch == rhs.worldEpoch && sequence == rhs.sequence
      && sector == rhs.sector && region == rhs.region && stage == rhs.stage;
}

bool WorldGenerationTaskIdentity::operator!=(WorldGenerationTaskIdentity const& rhs) const {
  return !(*this == rhs);
}

WorldGenerationTask::WorldGenerationTask(WorldGenerationTaskIdentity identity, size_t reservedBytes)
  : identity(std::move(identity)), reservedBytes(reservedBytes) {}

bool WorldGenerationTask::start() {
  auto expected = WorldGenerationTaskState::Queued;
  return m_state.compare_exchange_strong(expected, WorldGenerationTaskState::Running);
}

bool WorldGenerationTask::ready() {
  auto expected = WorldGenerationTaskState::Running;
  return m_state.compare_exchange_strong(expected, WorldGenerationTaskState::Ready);
}

bool WorldGenerationTask::fail() {
  auto expected = m_state.load();
  while (expected == WorldGenerationTaskState::Queued || expected == WorldGenerationTaskState::Running) {
    if (m_state.compare_exchange_weak(expected, WorldGenerationTaskState::Failed))
      return true;
  }
  return false;
}

void WorldGenerationTask::invalidate() {
  m_valid.store(false);
}

bool WorldGenerationTask::valid() const {
  return m_valid.load();
}

WorldGenerationTaskState WorldGenerationTask::state() const {
  return m_state.load();
}

WorldGenerationTasks::WorldGenerationTasks(uint64_t worldEpoch, WorldRegionPartition partition,
    size_t maxTasks, size_t maxBytes)
  : m_worldEpoch(worldEpoch), m_partition(std::move(partition)),
    m_maxTasks(maxTasks), m_maxBytes(maxBytes) {}

WorldGenerationTasks::~WorldGenerationTasks() {
  for (auto const& entry : m_tasks)
    entry.second->invalidate();
}

shared_ptr<WorldGenerationTask> WorldGenerationTasks::reserve(Vec2U const& sector,
    WorldGenerationTaskStage stage, size_t bytes) {
  auto region = m_partition.regionForSector(sector);
  Key key{sector, stage};
  if (!region || stage != WorldGenerationTaskStage::BaseTiles || m_tasks.contains(key)
      || m_tasks.size() >= m_maxTasks || bytes > m_maxBytes - m_reservedBytes
      || m_sequence == std::numeric_limits<uint64_t>::max())
    return {};

  auto task = shared_ptr<WorldGenerationTask>(new WorldGenerationTask(
      WorldGenerationTaskIdentity{m_worldEpoch, ++m_sequence, sector, *region, stage}, bytes));
  m_tasks[key] = task;
  m_reservedBytes += bytes;
  return task;
}

bool WorldGenerationTasks::contains(shared_ptr<WorldGenerationTask> const& task) const {
  if (!task || task->identity.worldEpoch != m_worldEpoch)
    return false;
  auto current = m_tasks.ptr(Key{task->identity.sector, task->identity.stage});
  return current && *current == task && (*current)->identity == task->identity;
}

bool WorldGenerationTasks::retire(shared_ptr<WorldGenerationTask> const& task, WorldGenerationTaskOutcome outcome) {
  if (!contains(task))
    return false;
  auto state = task->state();
  if (state != WorldGenerationTaskState::Ready && state != WorldGenerationTaskState::Failed)
    return false;
  switch (outcome) {
    case WorldGenerationTaskOutcome::Committed:
      if (state != WorldGenerationTaskState::Ready || !task->valid())
        return false;
      break;
    case WorldGenerationTaskOutcome::Stale:
    case WorldGenerationTaskOutcome::Cancelled:
      break;
    case WorldGenerationTaskOutcome::Failed:
      if (state != WorldGenerationTaskState::Failed)
        return false;
      break;
    default:
      return false;
  }

  task->invalidate();
  m_reservedBytes -= task->reservedBytes;
  m_tasks.remove(Key{task->identity.sector, task->identity.stage});
  return true;
}

size_t WorldGenerationTasks::size() const {
  return m_tasks.size();
}

size_t WorldGenerationTasks::reservedBytes() const {
  return m_reservedBytes;
}

}
