#include "StarWorldGenerationPlacementQueue.hpp"

#include <memory>
#include <string>
#include <vector>

#include "gtest/gtest.h"

using namespace Star;

TEST(WorldGenerationPlacementQueueTest, PendingIdsSurviveEarlierRetirement) {
  WorldGenerationPlacementQueue<std::string> queue;
  auto first = queue.append("first");
  auto secondSnapshot = queue.append("second");
  auto thirdSnapshot = queue.append("third");

  // Retiring the front previously compacted the list, redirecting the saved
  // second index to the third entry and putting the third index out of range.
  ASSERT_EQ(queue.take(first).value(), "first");
  EXPECT_EQ(queue.take(secondSnapshot).value(), "second");
  EXPECT_EQ(queue.take(thirdSnapshot).value(), "third");
  EXPECT_TRUE(queue.entries().empty());
}

TEST(WorldGenerationPlacementQueueTest, RetiredIdCannotTargetNewInsertion) {
  WorldGenerationPlacementQueue<std::string> queue;
  auto oldSnapshot = queue.append("old");
  ASSERT_TRUE(queue.take(oldSnapshot));
  auto newId = queue.append("new");
  EXPECT_GT(newId, oldSnapshot);
  EXPECT_FALSE(queue.contains(oldSnapshot));
  EXPECT_FALSE(queue.take(oldSnapshot));
  EXPECT_EQ(queue.take(newId).value(), "new");
}

TEST(WorldGenerationPlacementQueueTest, ReversedAndDuplicateResultsCompleteOnce) {
  WorldGenerationPlacementQueue<unsigned> queue;
  auto first = queue.append(0);
  auto second = queue.append(1);
  unsigned fulfilled[2] = {};
  for (auto snapshotId : {second, second, first, first, second}) {
    if (!queue.contains(snapshotId))
      continue;
    auto completed = queue.take(snapshotId);
    ASSERT_TRUE(completed);
    // Promise callback observes that identity has already been retired.
    EXPECT_FALSE(queue.contains(snapshotId));
    EXPECT_FALSE(queue.take(snapshotId));
    ++fulfilled[*completed];
  }
  EXPECT_EQ(fulfilled[0], 1u);
  EXPECT_EQ(fulfilled[1], 1u);
}

TEST(WorldGenerationPlacementQueueTest, FailedAttemptRetainsPendingEntry) {
  WorldGenerationPlacementQueue<std::string> queue;
  auto snapshotId = queue.append("retry");
  for (unsigned attempt = 0; attempt != 3; ++attempt) {
    ASSERT_TRUE(queue.contains(snapshotId));
    if (attempt < 2)
      continue; // Failed factory: do not retire or fulfill.
    EXPECT_EQ(queue.take(snapshotId).value(), "retry");
  }
  EXPECT_FALSE(queue.contains(snapshotId));
}

TEST(WorldGenerationPlacementQueueTest, InsertionsDuringFactoryPreserveSnapshot) {
  WorldGenerationPlacementQueue<std::string> queue;
  auto snapshotId = queue.append("snapshot");
  ASSERT_TRUE(queue.contains(snapshotId));
  for (unsigned i = 0; i != 1024; ++i)
    queue.append("inserted during generation");
  EXPECT_EQ(queue.take(snapshotId).value(), "snapshot");
  EXPECT_EQ(queue.entries().size(), 1024u);
}

TEST(WorldGenerationPlacementQueueTest, SurvivingEntriesKeepInsertionOrder) {
  WorldGenerationPlacementQueue<int> queue;
  auto first = queue.append(10);
  auto second = queue.append(20);
  auto third = queue.append(30);
  ASSERT_TRUE(queue.take(second));
  auto fourth = queue.append(40);
  std::vector<WorldGenerationPlacementId> ids;
  std::vector<int> values;
  for (auto const& entry : queue.entries()) {
    ids.push_back(entry.first);
    values.push_back(entry.second);
  }
  EXPECT_EQ(ids, (std::vector<WorldGenerationPlacementId>{first, third, fourth}));
  EXPECT_EQ(values, (std::vector<int>{10, 30, 40}));
}

TEST(WorldGenerationPlacementQueueTest, UnknownIdHasNoEffect) {
  WorldGenerationPlacementQueue<int> queue;
  auto id = queue.append(42);
  EXPECT_FALSE(queue.contains(0));
  EXPECT_FALSE(queue.take(0));
  EXPECT_FALSE(queue.take(id + 1));
  EXPECT_EQ(queue.entries().size(), 1u);
  EXPECT_EQ(queue.take(id).value(), 42);
}

TEST(WorldGenerationPlacementQueueTest, MoveOnlyPayloadCanBeRetired) {
  WorldGenerationPlacementQueue<std::unique_ptr<int>> queue;
  auto id = queue.append(std::make_unique<int>(7));
  auto payload = queue.take(id);
  ASSERT_TRUE(payload);
  ASSERT_TRUE(*payload);
  EXPECT_EQ(**payload, 7);
  EXPECT_FALSE(queue.take(id));
}
