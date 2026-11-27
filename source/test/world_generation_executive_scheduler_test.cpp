#include "gtest/gtest.h"

#include "StarWorldGenerationDependency.hpp"
#include "StarWorldGenerationExecutiveScheduler.hpp"

using namespace Star;

namespace {

List<Vec2U> crossNeighbors(Vec2U const& sector) {
  List<Vec2U> result;
  if (sector[0] > 0)
    result.append(Vec2U(sector[0] - 1, sector[1]));
  result.append(Vec2U(sector[0] + 1, sector[1]));
  if (sector[1] > 0)
    result.append(Vec2U(sector[0], sector[1] - 1));
  result.append(Vec2U(sector[0], sector[1] + 1));
  return result;
}

}

TEST(WorldGenerationExecutiveSchedulerTest, StartNextPicksFairFrontierAndDrainPublishes) {
  auto plan = WorldGenerationDependencyGraph::build({4, 7}, 2, crossNeighbors);
  WorldGenerationExecutiveShadowGraph shadow;
  shadow.applyInterest({1, 1}, plan);
  WorldGenerationExecutiveMailbox mailbox;
  WorldGenerationExecutiveScheduler scheduler(shadow, mailbox);

  List<WorldGenerationExecutiveDemand> demands{
      {{1, 1}, WorldGenerationInterestClass::Visible, {4, 7}, 20, 64}};

  auto step = scheduler.startNext([](Vec2U const&) { return uint8_t(0); }, demands, 42);
  ASSERT_TRUE(step);
  EXPECT_EQ(1u, step->node.level);
  auto running = shadow.record(step->node);
  ASSERT_TRUE(running);
  EXPECT_TRUE(running->executing);

  EXPECT_TRUE(scheduler.complete(*step, true, 100, 101));
  EXPECT_EQ(1u, scheduler.drainMailbox());
  auto published = shadow.record(step->node);
  ASSERT_TRUE(published);
  EXPECT_EQ(WorldGenerationExecutiveNodePhase::Published, published->publicationPhase);
  EXPECT_FALSE(published->executing);
}

TEST(WorldGenerationExecutiveSchedulerTest, FailureCompletionInvalidatesWithoutPublish) {
  auto plan = WorldGenerationDependencyGraph::build({4, 7}, 1, crossNeighbors);
  WorldGenerationExecutiveShadowGraph shadow;
  shadow.applyInterest({1, 2}, plan);
  WorldGenerationExecutiveMailbox mailbox;
  WorldGenerationExecutiveScheduler scheduler(shadow, mailbox);
  List<WorldGenerationExecutiveDemand> demands{
      {{1, 2}, WorldGenerationInterestClass::Immediate, {4, 7}, 1, 64}};

  auto step = scheduler.startNext([](Vec2U const&) { return uint8_t(0); }, demands, 7);
  ASSERT_TRUE(step);
  EXPECT_TRUE(scheduler.complete(*step, false, 5, 6));
  EXPECT_EQ(1u, scheduler.drainMailbox());
  auto record = shadow.record(step->node);
  ASSERT_TRUE(record);
  EXPECT_FALSE(record->publishAuthorized);
  EXPECT_NE(WorldGenerationExecutiveNodePhase::Published, record->publicationPhase);
}

TEST(WorldGenerationExecutiveSchedulerTest, SkipsExecutingAndUnauthorizedNodes) {
  auto plan = WorldGenerationDependencyGraph::build({4, 7}, 1, crossNeighbors);
  WorldGenerationExecutiveShadowGraph shadow;
  shadow.applyInterest({1, 3}, plan);
  WorldGenerationDependencyNode root{{4, 7}, 1};
  ASSERT_TRUE(shadow.markExecuting(root, true));

  WorldGenerationExecutiveMailbox mailbox;
  WorldGenerationExecutiveScheduler scheduler(shadow, mailbox);
  List<WorldGenerationExecutiveDemand> demands{
      {{1, 3}, WorldGenerationInterestClass::Visible, {4, 7}, 1, 64}};
  EXPECT_FALSE(scheduler.startNext([](Vec2U const&) { return uint8_t(0); }, demands, 1));
}

TEST(WorldGenerationExecutiveSchedulerTest, RunPassReportsStepBudgetExhausted) {
  auto plan = WorldGenerationDependencyGraph::build({4, 7}, 2, crossNeighbors);
  WorldGenerationExecutiveShadowGraph shadow;
  shadow.applyInterest({2, 1}, plan);
  WorldGenerationExecutiveMailbox mailbox;
  WorldGenerationExecutiveScheduler scheduler(shadow, mailbox);
  List<WorldGenerationExecutiveDemand> demands{
      {{2, 1}, WorldGenerationInterestClass::Visible, {4, 7}, 10, 64}};

  WorldGenerationExecutivePassBudget budget;
  budget.maxStarts = 1;
  auto pass = scheduler.runPass([](Vec2U const&) { return uint8_t(0); }, demands, budget, 50, 900);
  EXPECT_EQ(WorldGenerationExecutivePassExitReason::StepBudgetExhausted, pass.reason);
  EXPECT_EQ(1u, pass.starts);
  EXPECT_EQ(50u, pass.beganTick);
}

TEST(WorldGenerationExecutiveSchedulerTest, RunPassReportsEmptyAndDeadline) {
  WorldGenerationExecutiveShadowGraph shadow;
  WorldGenerationExecutiveMailbox mailbox;
  WorldGenerationExecutiveScheduler scheduler(shadow, mailbox);
  List<WorldGenerationExecutiveDemand> demands;

  WorldGenerationExecutivePassBudget emptyBudget;
  auto empty = scheduler.runPass([](Vec2U const&) { return uint8_t(0); }, demands, emptyBudget, 1, 1);
  EXPECT_EQ(WorldGenerationExecutivePassExitReason::EmptyMailboxAndFrontier, empty.reason);

  WorldGenerationExecutivePassBudget late;
  late.deadlineTick = 10;
  auto deadline = scheduler.runPass([](Vec2U const&) { return uint8_t(0); }, demands, late, 10, 1);
  EXPECT_EQ(WorldGenerationExecutivePassExitReason::DeadlineReached, deadline.reason);
}

TEST(WorldGenerationExecutiveSchedulerTest, RunPassRespectsDrainBudgetBeforeStarts) {
  auto plan = WorldGenerationDependencyGraph::build({4, 7}, 1, crossNeighbors);
  WorldGenerationExecutiveShadowGraph shadow;
  shadow.applyInterest({3, 1}, plan);
  WorldGenerationExecutiveMailbox mailbox(8);
  WorldGenerationExecutiveScheduler scheduler(shadow, mailbox);
  WorldGenerationDependencyNode root{{4, 7}, 1};
  ASSERT_TRUE(shadow.markExecuting(root, true));
  ASSERT_TRUE(shadow.markPrepared(root));

  WorldGenerationExecutiveMailboxMessage first;
  first.node = root;
  first.consumer = {3, 1};
  first.resultToken = 1;
  first.workerFinishedTick = 1;
  first.firstObservedTick = 2;
  first.success = true;
  WorldGenerationExecutiveMailboxMessage second = first;
  second.resultToken = 2;
  ASSERT_TRUE(mailbox.post(std::move(first)));
  ASSERT_TRUE(mailbox.post(std::move(second)));

  WorldGenerationExecutivePassBudget budget;
  budget.maxDrainMessages = 1;
  budget.maxStarts = 4;
  List<WorldGenerationExecutiveDemand> demands{
      {{3, 1}, WorldGenerationInterestClass::Immediate, {4, 7}, 1, 64}};
  auto pass = scheduler.runPass([](Vec2U const&) { return uint8_t(0); }, demands, budget, 3, 10);
  EXPECT_EQ(WorldGenerationExecutivePassExitReason::DrainBudgetExhausted, pass.reason);
  EXPECT_EQ(1u, pass.drained);
  EXPECT_EQ(0u, pass.starts);
  EXPECT_EQ(1u, mailbox.size());
}
