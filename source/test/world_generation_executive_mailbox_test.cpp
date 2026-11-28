#include "gtest/gtest.h"

#include "StarWorldGenerationDependency.hpp"
#include "StarWorldGenerationExecutiveMailbox.hpp"

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

TEST(WorldGenerationExecutiveMailboxTest, PostConsumeSeparatesWorkerAndObservationTicks) {
  WorldGenerationExecutiveMailbox mailbox(2);
  WorldGenerationExecutiveMailboxMessage message;
  message.node = {{4, 7}, 1};
  message.consumer = {1, 1};
  message.resultToken = 99;
  message.workerFinishedTick = 10;
  EXPECT_TRUE(mailbox.post(message));
  auto observed = mailbox.consume();
  ASSERT_TRUE(observed);
  EXPECT_EQ(10u, observed->workerFinishedTick);
  EXPECT_EQ(10u, observed->firstObservedTick);
  EXPECT_TRUE(mailbox.empty());
}

TEST(WorldGenerationExecutiveMailboxTest, CapacityRejectsOverflowWithoutDroppingExisting) {
  WorldGenerationExecutiveMailbox mailbox(1);
  WorldGenerationExecutiveMailboxMessage first{{{1, 1}, 1}, {1, 1}, 1, 1, 0, true};
  WorldGenerationExecutiveMailboxMessage second{{{2, 2}, 1}, {1, 2}, 2, 2, 0, true};
  EXPECT_TRUE(mailbox.post(first));
  EXPECT_FALSE(mailbox.post(second));
  EXPECT_EQ(1u, mailbox.size());
}

TEST(WorldGenerationExecutiveMailboxTest, ApplySuccessPublishesAuthorizedPreparedNode) {
  auto plan = WorldGenerationDependencyGraph::build({4, 7}, 1, crossNeighbors);
  WorldGenerationExecutiveShadowGraph shadow;
  shadow.applyInterest({1, 1}, plan);
  WorldGenerationDependencyNode root{{4, 7}, 1};
  shadow.markExecuting(root, true);

  WorldGenerationExecutiveMailbox mailbox;
  WorldGenerationExecutiveMailboxMessage message{root, {1, 1}, 7, 5, 6, true};
  ASSERT_TRUE(mailbox.post(message));
  auto consumed = mailbox.consume();
  ASSERT_TRUE(consumed);
  EXPECT_TRUE(mailbox.applyToShadow(shadow, *consumed));
  auto record = shadow.record(root);
  ASSERT_TRUE(record);
  EXPECT_EQ(WorldGenerationExecutiveNodePhase::Published, record->publicationPhase);
  EXPECT_FALSE(record->executing);
}

TEST(WorldGenerationExecutiveMailboxTest, ApplyFailureInvalidatesWithoutPublish) {
  auto plan = WorldGenerationDependencyGraph::build({4, 7}, 1, crossNeighbors);
  WorldGenerationExecutiveShadowGraph shadow;
  shadow.applyInterest({1, 1}, plan);
  WorldGenerationDependencyNode root{{4, 7}, 1};
  shadow.markExecuting(root, true);

  WorldGenerationExecutiveMailbox mailbox;
  WorldGenerationExecutiveMailboxMessage message{root, {1, 1}, 0, 3, 4, false};
  ASSERT_TRUE(mailbox.applyToShadow(shadow, message));
  auto record = shadow.record(root);
  ASSERT_TRUE(record);
  EXPECT_FALSE(record->publishAuthorized);
  EXPECT_FALSE(record->executing);
  EXPECT_NE(WorldGenerationExecutiveNodePhase::Published, record->publicationPhase);
}
