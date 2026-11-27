#include "gtest/gtest.h"

#include "StarWorldGenerationDependency.hpp"
#include "StarWorldGenerationExecutivePriority.hpp"

using namespace Star;

TEST(WorldGenerationExecutivePriorityTest, HorizontalWrapUsesShorterArc) {
  EXPECT_EQ(1u, WorldGenerationExecutivePriority::sectorDistance({0, 0}, {9, 0}, 10));
  EXPECT_EQ(2u, WorldGenerationExecutivePriority::sectorDistance({0, 0}, {2, 0}, 10));
  EXPECT_EQ(3u, WorldGenerationExecutivePriority::sectorDistance({0, 0}, {2, 1}, 0));
}

TEST(WorldGenerationExecutivePriorityTest, InterestClassOutranksAgeAndDistance) {
  WorldGenerationDependencyNode node{{4, 7}, 1};
  WorldGenerationExecutiveDemand farOld{
      {1, 1}, WorldGenerationInterestClass::Nearby, {0, 0}, 900, 64};
  WorldGenerationExecutiveDemand nearNew{
      {1, 2}, WorldGenerationInterestClass::Immediate, {4, 7}, 1, 64};
  auto far = WorldGenerationExecutivePriority::score(node, farOld);
  auto near = WorldGenerationExecutivePriority::score(node, nearNew);
  EXPECT_GT(near.total, far.total);
}

TEST(WorldGenerationExecutivePriorityTest, SelectFairKeepsOlderVisibleOverDenseBackground) {
  List<WorldGenerationDependencyNode> frontier{{{4, 7}, 1}, {{5, 7}, 1}};
  List<WorldGenerationExecutiveDemand> demands{
      {{1, 10}, WorldGenerationInterestClass::Visible, {4, 7}, 50, 64},
      {{1, 11}, WorldGenerationInterestClass::Background, {5, 7}, 1, 64},
      {{1, 12}, WorldGenerationInterestClass::Background, {5, 7}, 2, 64},
      {{1, 13}, WorldGenerationInterestClass::Background, {5, 7}, 3, 64}};

  auto selected = WorldGenerationExecutivePriority::selectFair(frontier, demands);
  ASSERT_TRUE(selected);
  EXPECT_EQ((WorldGenerationDependencyNode{{4, 7}, 1}), selected->node);
  EXPECT_EQ(WorldGenerationInterestClass::Visible, selected->interest);
  EXPECT_EQ(50u, selected->ageTick);
}

TEST(WorldGenerationExecutivePriorityTest, DedupKeepsMostUrgentConsumerAgeForSameNode) {
  List<WorldGenerationDependencyNode> frontier{{{4, 7}, 1}};
  List<WorldGenerationExecutiveDemand> demands{
      {{1, 1}, WorldGenerationInterestClass::Visible, {4, 7}, 10, 64},
      {{1, 2}, WorldGenerationInterestClass::Visible, {4, 7}, 40, 64}};
  auto selected = WorldGenerationExecutivePriority::selectFair(frontier, demands);
  ASSERT_TRUE(selected);
  EXPECT_EQ(40u, selected->ageTick);
  EXPECT_EQ(2u, selected->consumer.sequence);
}
