#include "gtest/gtest.h"

#include "StarWorldGenerationDependency.hpp"

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

uint8_t noGeneration(Vec2U const&) {
  return 0;
}

}

TEST(WorldGenerationDependencyTest, BaseTilesHaveNoPrerequisites) {
  auto plan = WorldGenerationDependencyGraph::build({4, 7}, 1, crossNeighbors);

  ASSERT_FALSE(plan.truncated);
  ASSERT_EQ(plan.nodes.size(), 1u);
  EXPECT_EQ(plan.nodes[0], (WorldGenerationDependencyNode{{4, 7}, 1}));
  EXPECT_TRUE(plan.edges.empty());

  auto analysis = WorldGenerationDependencyGraph::analyze(plan, noGeneration);
  EXPECT_EQ(analysis.runnableNodes, 1u);
  EXPECT_EQ(analysis.blockedNodes, 0u);
}

TEST(WorldGenerationDependencyTest, HigherStageDependsOnOwnAndNeighborPreviousStages) {
  auto plan = WorldGenerationDependencyGraph::build({4, 7}, 2, crossNeighbors);

  ASSERT_FALSE(plan.truncated);
  EXPECT_EQ(plan.nodes.size(), 6u);
  ASSERT_EQ(plan.edges.size(), 5u);
  for (auto const& edge : plan.edges) {
    EXPECT_EQ(edge.dependent, (WorldGenerationDependencyNode{{4, 7}, 2}));
    EXPECT_EQ(edge.prerequisite.level, 1);
  }

  auto analysis = WorldGenerationDependencyGraph::analyze(plan, noGeneration);
  EXPECT_EQ(analysis.runnableNodes, 5u);
  EXPECT_EQ(analysis.blockedNodes, 1u);
  EXPECT_EQ(analysis.directBlockers, 5u);
  ASSERT_TRUE(analysis.firstBlocker);
  EXPECT_EQ(*analysis.firstBlocker, (WorldGenerationDependencyNode{{3, 7}, 1}));
}

TEST(WorldGenerationDependencyTest, ACompletedPrerequisiteMakesOnlyItsDependentEdgeReady) {
  auto plan = WorldGenerationDependencyGraph::build({4, 7}, 2, crossNeighbors);
  auto analysis = WorldGenerationDependencyGraph::analyze(plan, [](Vec2U const& sector) {
      return sector == Vec2U(4, 7) ? uint8_t(1) : uint8_t(0);
    });

  EXPECT_EQ(analysis.satisfiedNodes, 1u);
  EXPECT_EQ(analysis.runnableNodes, 4u);
  EXPECT_EQ(analysis.blockedNodes, 1u);
  EXPECT_EQ(analysis.directBlockers, 4u);
  ASSERT_EQ(analysis.frontier.size(), 4u);
  EXPECT_EQ(analysis.frontier[0], (WorldGenerationDependencyNode{{3, 7}, 1}));
}

TEST(WorldGenerationDependencyTest, MultipleRootsShareDependenciesAndExposeOneFrontier) {
  auto plan = WorldGenerationDependencyGraph::build(
      List<WorldGenerationDependencyNode>{{{4, 7}, 2}, {{5, 7}, 2}}, crossNeighbors);

  ASSERT_FALSE(plan.truncated);
  auto analysis = WorldGenerationDependencyGraph::analyze(plan, noGeneration);
  EXPECT_EQ(analysis.runnableNodes, analysis.frontier.size());
  EXPECT_EQ(analysis.frontier.size(), 8u);
  EXPECT_EQ(std::count(analysis.frontier.begin(), analysis.frontier.end(),
      WorldGenerationDependencyNode{{5, 7}, 1}), 1);
}

TEST(WorldGenerationDependencyTest, ExpansionStopsAtTheDeclaredBound) {
  auto plan = WorldGenerationDependencyGraph::build({4, 7}, 4, crossNeighbors, 8);

  EXPECT_TRUE(plan.truncated);
  EXPECT_EQ(plan.nodes.size(), 8u);
  EXPECT_FALSE(plan.nodes.empty());
  EXPECT_EQ(plan.nodes.front(), (WorldGenerationDependencyNode{{4, 7}, 4}));
}

TEST(WorldGenerationDependencyTest, DuplicateNeighborsDoNotDuplicatePrerequisites) {
  auto plan = WorldGenerationDependencyGraph::build({4, 7}, 2, [](Vec2U const& sector) {
      return List<Vec2U>{sector, sector, Vec2U(sector[0] + 1, sector[1])};
    });

  EXPECT_EQ(plan.nodes.size(), 3u);
  EXPECT_EQ(plan.edges.size(), 2u);
}
