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

TEST(WorldGenerationExecutivePriorityTest, TtlExpiresDemandWithoutPriorityRenewal) {
  List<WorldGenerationDependencyNode> frontier{{{4, 7}, 1}};
  List<WorldGenerationExecutiveDemand> demands{
      {{1, 1}, WorldGenerationInterestClass::Immediate, {4, 7}, 1, 64}};
  WorldGenerationExecutivePriorityPolicy policy;
  policy.ttlTicks = 10;
  policy.nowTick = 20; // age 1 + ttl 10 expired
  auto expired = WorldGenerationExecutivePriority::selectFairTraced(frontier, demands, policy);
  EXPECT_FALSE(expired.chosen);
  EXPECT_EQ(1u, expired.expiredDemands);
  ASSERT_FALSE(expired.blockers.empty());
  EXPECT_EQ(WorldGenerationExecutiveBlockerKind::TtlExpired, expired.blockers[0].kind);

  // Scoring alone must not renew ageTick.
  auto scored = WorldGenerationExecutivePriority::score(frontier[0], demands[0]);
  EXPECT_EQ(1u, scored.ageTick);
  EXPECT_EQ(1u, demands[0].ageTick);
}

TEST(WorldGenerationExecutivePriorityTest, LoadCapSkipsBackgroundWhenVisibleExists) {
  List<WorldGenerationDependencyNode> frontier{{{5, 7}, 1}, {{4, 7}, 1}};
  List<WorldGenerationExecutiveDemand> demands{
      {{1, 1}, WorldGenerationInterestClass::Background, {5, 7}, 99, 64},
      {{1, 2}, WorldGenerationInterestClass::Visible, {4, 7}, 10, 64}};
  WorldGenerationExecutivePriorityPolicy policy;
  policy.maxBackgroundSelections = 0; // disallow background picks
  auto trace = WorldGenerationExecutivePriority::selectFairTraced(frontier, demands, policy);
  ASSERT_TRUE(trace.chosen);
  EXPECT_EQ((WorldGenerationDependencyNode{{4, 7}, 1}), trace.chosen->node);
  EXPECT_EQ(WorldGenerationInterestClass::Visible, trace.chosen->interest);
}

TEST(WorldGenerationExecutivePriorityTest, AuxiliaryInheritsVisibleUrgencyAndUnlockBenefit) {
  // Visible demand focuses on the root; frontier only has an auxiliary producer.
  WorldGenerationDependencyNode auxiliary{{5, 7}, 1};
  List<WorldGenerationDependencyNode> frontier{auxiliary};
  List<WorldGenerationExecutiveDemand> demands{
      {{1, 1}, WorldGenerationInterestClass::Visible, {4, 7}, 40, 64}};
  Map<WorldGenerationDependencyNode, size_t> unlock{{auxiliary, 8}};
  WorldGenerationExecutivePriorityPolicy policy;
  auto trace = WorldGenerationExecutivePriority::selectFairTraced(frontier, demands, policy, unlock);
  ASSERT_TRUE(trace.chosen);
  EXPECT_EQ(WorldGenerationInterestClass::Visible, trace.chosen->interest);
  EXPECT_EQ(8u, trace.chosen->unlockBenefit);
  EXPECT_EQ(WorldGenerationExecutiveSelectionReason::UnlockBenefit, trace.reason);
}

TEST(WorldGenerationExecutivePriorityTest, EarlierEtaOutranksLaterWithinSameClass) {
  WorldGenerationDependencyNode node{{4, 7}, 1};
  WorldGenerationExecutiveDemand late{
      {1, 1}, WorldGenerationInterestClass::Visible, {4, 7}, 10, 64, 500};
  WorldGenerationExecutiveDemand early{
      {1, 2}, WorldGenerationInterestClass::Visible, {4, 7}, 10, 64, 50};
  auto lateScore = WorldGenerationExecutivePriority::score(node, late);
  auto earlyScore = WorldGenerationExecutivePriority::score(node, early);
  EXPECT_GT(earlyScore.total, lateScore.total);
}
