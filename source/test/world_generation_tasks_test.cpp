#include "StarWorldGenerationTasks.hpp"

#include <limits>
#include <thread>

#include "gtest/gtest.h"

using namespace Star;

namespace {

WorldRegionPartition partition() {
  return WorldRegionPartition(WorldGeometry(Vec2U{160, 128}));
}

auto const BaseTiles = WorldGenerationTaskStage::BaseTiles;

}

TEST(WorldGenerationTasksTest, LifecycleAndIdentity) {
  WorldGenerationTasks tasks(31, partition(), 2, 1024);
  auto task = tasks.reserve({2, 3}, BaseTiles, 1024);
  ASSERT_TRUE(task);
  EXPECT_EQ(task->identity.worldEpoch, 31u);
  EXPECT_EQ(task->identity.sequence, 1u);
  EXPECT_EQ(task->identity.sector, Vec2U(2, 3));
  EXPECT_EQ(task->identity.region, Vec2U(1, 1));
  EXPECT_EQ(task->identity.stage, BaseTiles);
  EXPECT_EQ(task->reservedBytes, 1024u);
  EXPECT_EQ(tasks.reservedBytes(), 1024u);
  EXPECT_EQ(tasks.size(), 1u);
  EXPECT_TRUE(tasks.contains(task));
  EXPECT_TRUE(task->valid());
  EXPECT_EQ(task->state(), WorldGenerationTaskState::Queued);
  EXPECT_FALSE(task->ready());
  EXPECT_FALSE(tasks.retire(task, WorldGenerationTaskOutcome::Committed));
  EXPECT_TRUE(task->start());
  EXPECT_FALSE(task->start());
  EXPECT_EQ(task->state(), WorldGenerationTaskState::Running);
  EXPECT_FALSE(tasks.retire(task, WorldGenerationTaskOutcome::Cancelled));
  EXPECT_TRUE(task->ready());
  EXPECT_FALSE(task->ready());
  EXPECT_FALSE(task->fail());
  EXPECT_FALSE(tasks.retire(task, WorldGenerationTaskOutcome::Failed));
  EXPECT_TRUE(tasks.retire(task, WorldGenerationTaskOutcome::Committed));
  EXPECT_FALSE(task->valid());
  EXPECT_FALSE(tasks.contains(task));
  EXPECT_FALSE(tasks.retire(task, WorldGenerationTaskOutcome::Committed));
  EXPECT_EQ(tasks.reservedBytes(), 0u);
  EXPECT_EQ(tasks.size(), 0u);
}

TEST(WorldGenerationTasksTest, UnloadRetainsKeyUntilProducerCompletes) {
  WorldGenerationTasks tasks(5, partition(), 2, 2048);
  auto oldTask = tasks.reserve({1, 1}, BaseTiles, 512);
  ASSERT_TRUE(oldTask);
  oldTask->invalidate();
  EXPECT_FALSE(oldTask->valid());
  EXPECT_EQ(oldTask->state(), WorldGenerationTaskState::Queued);
  EXPECT_FALSE(tasks.reserve({1, 1}, BaseTiles, 512));
  EXPECT_FALSE(tasks.retire(oldTask, WorldGenerationTaskOutcome::Cancelled));
  // A queued producer can still acknowledge start and completion after unload.
  EXPECT_TRUE(oldTask->start());
  EXPECT_FALSE(tasks.retire(oldTask, WorldGenerationTaskOutcome::Stale));
  EXPECT_FALSE(tasks.reserve({1, 1}, BaseTiles, 512));
  EXPECT_TRUE(oldTask->ready());
  EXPECT_FALSE(tasks.retire(oldTask, WorldGenerationTaskOutcome::Committed));
  EXPECT_TRUE(tasks.retire(oldTask, WorldGenerationTaskOutcome::Cancelled));

  auto newTask = tasks.reserve({1, 1}, BaseTiles, 512);
  ASSERT_TRUE(newTask);
  EXPECT_GT(newTask->identity.sequence, oldTask->identity.sequence);
  EXPECT_FALSE(tasks.contains(oldTask));
  EXPECT_FALSE(tasks.retire(oldTask, WorldGenerationTaskOutcome::Stale));
  EXPECT_TRUE(tasks.contains(newTask));
  EXPECT_EQ(tasks.reservedBytes(), 512u);
  EXPECT_TRUE(newTask->start());
  EXPECT_TRUE(newTask->ready());
  EXPECT_TRUE(tasks.retire(newTask, WorldGenerationTaskOutcome::Stale));
}

TEST(WorldGenerationTasksTest, FailureIsTerminal) {
  WorldGenerationTasks tasks(7, partition(), 2, 2048);
  auto queued = tasks.reserve({0, 0}, BaseTiles, 512);
  auto running = tasks.reserve({1, 0}, BaseTiles, 512);
  ASSERT_TRUE(queued);
  ASSERT_TRUE(running);
  EXPECT_TRUE(queued->fail());
  EXPECT_TRUE(running->start());
  EXPECT_TRUE(running->fail());
  for (auto const& task : {queued, running}) {
    EXPECT_EQ(task->state(), WorldGenerationTaskState::Failed);
    EXPECT_FALSE(task->start());
    EXPECT_FALSE(task->ready());
    EXPECT_FALSE(task->fail());
    EXPECT_FALSE(tasks.retire(task, WorldGenerationTaskOutcome::Committed));
    EXPECT_TRUE(tasks.retire(task, WorldGenerationTaskOutcome::Failed));
    EXPECT_FALSE(task->valid());
  }
  EXPECT_EQ(tasks.size(), 0u);
  EXPECT_EQ(tasks.reservedBytes(), 0u);
}

TEST(WorldGenerationTasksTest, EpochAndPointerRejectForeignTickets) {
  WorldGenerationTasks first(10, partition(), 2, 2048);
  WorldGenerationTasks second(11, partition(), 2, 2048);
  WorldGenerationTasks sameEpoch(10, partition(), 2, 2048);
  auto a = first.reserve({0, 0}, BaseTiles, 1);
  auto b = second.reserve({0, 0}, BaseTiles, 1);
  auto c = sameEpoch.reserve({0, 0}, BaseTiles, 1);
  ASSERT_TRUE(a);
  ASSERT_TRUE(b);
  ASSERT_TRUE(c);
  EXPECT_EQ(a->identity.sequence, b->identity.sequence);
  EXPECT_NE(a->identity, b->identity);
  EXPECT_EQ(a->identity, c->identity);
  EXPECT_TRUE(a->start());
  EXPECT_TRUE(a->ready());
  EXPECT_FALSE(second.contains(a));
  EXPECT_FALSE(sameEpoch.contains(a));
  EXPECT_FALSE(second.retire(a, WorldGenerationTaskOutcome::Committed));
  EXPECT_FALSE(sameEpoch.retire(a, WorldGenerationTaskOutcome::Committed));
  EXPECT_FALSE(first.contains({}));
  EXPECT_FALSE(first.retire({}, WorldGenerationTaskOutcome::Cancelled));
  EXPECT_TRUE(first.retire(a, WorldGenerationTaskOutcome::Committed));
  EXPECT_TRUE(second.contains(b));
  EXPECT_TRUE(sameEpoch.contains(c));
}

TEST(WorldGenerationTasksTest, AdmissionLimitsAndOverflow) {
  WorldGenerationTasks none(1, partition(), 0, 100);
  EXPECT_FALSE(none.reserve({0, 0}, BaseTiles, 1));
  WorldGenerationTasks byteLimit(1, partition(), 3, 1024);
  auto first = byteLimit.reserve({0, 0}, BaseTiles, 1024);
  ASSERT_TRUE(first);
  EXPECT_FALSE(byteLimit.reserve({1, 0}, BaseTiles, 1));
  EXPECT_EQ(byteLimit.reservedBytes(), 1024u);
  EXPECT_TRUE(first->fail());
  EXPECT_TRUE(byteLimit.retire(first, WorldGenerationTaskOutcome::Failed));
  EXPECT_TRUE(byteLimit.reserve({1, 0}, BaseTiles, 1024));

  WorldGenerationTasks taskLimit(1, partition(), 1, 2048);
  EXPECT_TRUE(taskLimit.reserve({0, 0}, BaseTiles, 1));
  EXPECT_FALSE(taskLimit.reserve({1, 0}, BaseTiles, 1));
  auto max = std::numeric_limits<size_t>::max();
  WorldGenerationTasks overflow(1, partition(), 3, max);
  EXPECT_TRUE(overflow.reserve({0, 0}, BaseTiles, max - 1));
  EXPECT_FALSE(overflow.reserve({1, 0}, BaseTiles, 2));
  EXPECT_TRUE(overflow.reserve({1, 0}, BaseTiles, 1));
  EXPECT_EQ(overflow.reservedBytes(), max);
  EXPECT_FALSE(overflow.reserve({2, 0}, BaseTiles, 1));
}

TEST(WorldGenerationTasksTest, InvalidSectorAndStageDoNotReserve) {
  WorldGenerationTasks tasks(1, partition(), 2, 1024);
  EXPECT_FALSE(tasks.reserve({5, 0}, BaseTiles, 1));
  EXPECT_FALSE(tasks.reserve({0, 4}, BaseTiles, 1));
  EXPECT_FALSE(tasks.reserve({0, 0}, static_cast<WorldGenerationTaskStage>(255), 1));
  EXPECT_EQ(tasks.size(), 0u);
  EXPECT_EQ(tasks.reservedBytes(), 0u);
  auto first = tasks.reserve({4, 3}, BaseTiles, 1);
  ASSERT_TRUE(first);
  EXPECT_EQ(first->identity.sequence, 1u);
}

TEST(WorldGenerationTasksTest, RegistryDestructionRevokesSurvivingHandle) {
  shared_ptr<WorldGenerationTask> surviving;
  {
    WorldGenerationTasks tasks(1, partition(), 1, 1024);
    surviving = tasks.reserve({0, 0}, BaseTiles, 1024);
    ASSERT_TRUE(surviving);
    EXPECT_TRUE(surviving->valid());
  }
  EXPECT_FALSE(surviving->valid());
  // Worker completion does not need to dereference the destroyed registry.
  EXPECT_TRUE(surviving->start());
  EXPECT_TRUE(surviving->ready());
}

TEST(WorldGenerationTasksTest, StateTransitionsHaveSingleWinner) {
  WorldGenerationTasks tasks(1, partition(), 1, 1);
  auto task = tasks.reserve({0, 0}, BaseTiles, 1);
  ASSERT_TRUE(task);
  atomic<unsigned> started{0};
  auto start = [&]() { if (task->start()) ++started; };
  std::thread a(start), b(start);
  a.join();
  b.join();
  EXPECT_EQ(started.load(), 1u);
  atomic<unsigned> finished{0};
  auto complete = [&]() { if (task->ready()) ++finished; };
  std::thread c(complete), d(complete);
  c.join();
  d.join();
  EXPECT_EQ(finished.load(), 1u);
  EXPECT_TRUE(tasks.retire(task, WorldGenerationTaskOutcome::Committed));
}
