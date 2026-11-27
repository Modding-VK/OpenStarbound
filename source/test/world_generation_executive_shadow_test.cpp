#include "gtest/gtest.h"

#include "StarWorldGenerationDependency.hpp"
#include "StarWorldGenerationExecutiveShadow.hpp"

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

TEST(WorldGenerationExecutiveShadowTest, IngestedPlanMatchesRecursiveBuildWithoutDivergence) {
  auto plan = WorldGenerationDependencyGraph::build({4, 7}, 2, crossNeighbors);
  WorldGenerationExecutiveShadowGraph shadow;
  shadow.ingestPreparationPlan(plan, 3);

  EXPECT_EQ(shadow.nodeCount(), plan.nodes.size());
  EXPECT_EQ(shadow.preparationEdgeCount(), plan.edges.size());
  EXPECT_EQ(0u, shadow.publicationEdgeCount());
  EXPECT_TRUE(shadow.comparePreparationToPlan(plan).empty());

  auto record = shadow.record({{4, 7}, 2});
  ASSERT_TRUE(record);
  EXPECT_EQ(5u, record->preparationPrerequisiteCount);
  EXPECT_EQ(0u, record->publicationPrerequisiteCount);
  EXPECT_EQ(3u, record->estimatedCost);
  EXPECT_EQ(WorldGenerationExecutiveNodePhase::Reserved, record->preparationPhase);
}

TEST(WorldGenerationExecutiveShadowTest, SharedRootsDeduplicateNodesAndKeepPreparationEdges) {
  auto first = WorldGenerationDependencyGraph::build({4, 7}, 2, crossNeighbors);
  auto second = WorldGenerationDependencyGraph::build({5, 7}, 2, crossNeighbors);
  WorldGenerationExecutiveShadowGraph shadow;
  shadow.ingestPreparationPlan(first);
  size_t afterFirst = shadow.nodeCount();
  shadow.ingestPreparationPlan(second);

  EXPECT_GT(shadow.nodeCount(), afterFirst);
  EXPECT_TRUE(shadow.contains({{4, 7}, 1}));
  EXPECT_TRUE(shadow.contains({{5, 7}, 1}));
  EXPECT_EQ(0u, shadow.consumerCount({{4, 7}, 1}));

  auto shared = WorldGenerationDependencyGraph::build(
      List<WorldGenerationDependencyNode>{{{4, 7}, 2}, {{5, 7}, 2}}, crossNeighbors);
  // Re-ingest the merged recursive plan; preparation edges stay unique.
  shadow.ingestPreparationPlan(shared);
  EXPECT_EQ(shadow.preparationEdgeCount(), shared.edges.size());
  EXPECT_TRUE(shadow.comparePreparationToPlan(shared).empty());
}

TEST(WorldGenerationExecutiveShadowTest, RemovingOneConsumerLeavesDemandRequiredByAnother) {
  auto plan = WorldGenerationDependencyGraph::build({4, 7}, 1, crossNeighbors);
  WorldGenerationExecutiveShadowGraph shadow;
  shadow.ingestPreparationPlan(plan);

  WorldGenerationExecutiveConsumerId a{1, 10};
  WorldGenerationExecutiveConsumerId b{1, 11};
  WorldGenerationDependencyNode root{{4, 7}, 1};
  ASSERT_TRUE(shadow.addDemand(a, root));
  ASSERT_TRUE(shadow.addDemand(b, root));
  EXPECT_EQ(2u, shadow.consumerCount(root));

  EXPECT_TRUE(shadow.removeDemand(a, root));
  EXPECT_EQ(1u, shadow.consumerCount(root));
  EXPECT_TRUE(shadow.contains(root));
  auto record = shadow.record(root);
  ASSERT_TRUE(record);
  EXPECT_TRUE(record->consumers.contains(b));
  EXPECT_FALSE(record->consumers.contains(a));
}

TEST(WorldGenerationExecutiveShadowTest, PublicationEdgesStaySeparateFromPreparationComparison) {
  auto plan = WorldGenerationDependencyGraph::build({4, 7}, 2, crossNeighbors);
  WorldGenerationExecutiveShadowGraph shadow;
  shadow.ingestPreparationPlan(plan);

  WorldGenerationDependencyEdge publication{
      WorldGenerationDependencyNode{{4, 7}, 1},
      WorldGenerationDependencyNode{{4, 7}, 2}};
  shadow.addPublicationEdge(publication);

  EXPECT_EQ(1u, shadow.publicationEdgeCount());
  EXPECT_TRUE(shadow.comparePreparationToPlan(plan).empty());

  auto publicationOnly = WorldGenerationDependencyPlan{};
  publicationOnly.edges.append(publication);
  EXPECT_TRUE(shadow.comparePublicationToPlan(publicationOnly).empty());

  auto emptyPublication = WorldGenerationDependencyPlan{};
  auto divergences = shadow.comparePublicationToPlan(emptyPublication);
  ASSERT_EQ(1u, divergences.size());
  EXPECT_EQ(WorldGenerationExecutiveDivergenceKind::ExtraPublicationEdge, divergences[0].kind);
}

TEST(WorldGenerationExecutiveShadowTest, ApplyInterestAndUnloadPreserveSharedDemand) {
  auto left = WorldGenerationDependencyGraph::build({4, 7}, 2, crossNeighbors);
  auto right = WorldGenerationDependencyGraph::build({5, 7}, 2, crossNeighbors);
  WorldGenerationExecutiveShadowGraph shadow;
  WorldGenerationExecutiveConsumerId a{1, 1};
  WorldGenerationExecutiveConsumerId b{1, 2};
  EXPECT_EQ(left.nodes.size(), shadow.applyInterest(a, left));
  EXPECT_EQ(right.nodes.size(), shadow.applyInterest(b, right));
  EXPECT_EQ(2u, shadow.activeConsumerCount());
  EXPECT_GE(shadow.consumerCount({{5, 7}, 1}), 1u);

  EXPECT_EQ(left.nodes.size(), shadow.clearInterest(a));
  EXPECT_EQ(1u, shadow.activeConsumerCount());
  EXPECT_TRUE(shadow.contains({{4, 7}, 1}));
  EXPECT_TRUE(shadow.contains({{5, 7}, 2}));

  // Shared neighbor still demanded by b blocks unload of sector 5.
  EXPECT_EQ(0u, shadow.unloadSector({5, 7}));
  EXPECT_TRUE(shadow.contains({{5, 7}, 2}));

  shadow.clearInterest(b);
  EXPECT_GT(shadow.unloadSector({5, 7}), 0u);
  EXPECT_FALSE(shadow.contains({{5, 7}, 2}));
}

TEST(WorldGenerationExecutiveShadowTest, AnticipatePreparationMarksRunnableWithoutPublish) {
  auto plan = WorldGenerationDependencyGraph::build({4, 7}, 2, crossNeighbors);
  WorldGenerationExecutiveShadowGraph shadow;
  shadow.applyInterest({1, 9}, plan);
  auto advanced = shadow.anticipatePreparation([](Vec2U const&) { return uint8_t(0); });
  EXPECT_EQ(5u, advanced);
  auto preparedLeaf = shadow.record({{3, 7}, 1});
  ASSERT_TRUE(preparedLeaf);
  EXPECT_EQ(WorldGenerationExecutiveNodePhase::Prepared, preparedLeaf->preparationPhase);
  EXPECT_FALSE(shadow.markPublished({{4, 7}, 2}));
  EXPECT_TRUE(shadow.markPublished({{3, 7}, 1}));
  auto published = shadow.record({{3, 7}, 1});
  ASSERT_TRUE(published);
  EXPECT_EQ(WorldGenerationExecutiveNodePhase::Published, published->publicationPhase);
}

TEST(WorldGenerationExecutiveShadowTest, CompareRecordsMissingAndExtraPreparationNodes) {
  auto full = WorldGenerationDependencyGraph::build({4, 7}, 2, crossNeighbors);
  WorldGenerationExecutiveShadowGraph shadow;
  shadow.ingestPreparationPlan(full);

  auto smaller = WorldGenerationDependencyGraph::build({4, 7}, 1, crossNeighbors);
  auto divergences = shadow.comparePreparationToPlan(smaller);
  EXPECT_FALSE(divergences.empty());

  bool sawExtraNode = false;
  bool sawExtraEdge = false;
  for (auto const& divergence : divergences) {
    if (divergence.kind == WorldGenerationExecutiveDivergenceKind::ExtraNode)
      sawExtraNode = true;
    if (divergence.kind == WorldGenerationExecutiveDivergenceKind::ExtraPreparationEdge)
      sawExtraEdge = true;
  }
  EXPECT_TRUE(sawExtraNode);
  EXPECT_TRUE(sawExtraEdge);

  WorldGenerationExecutiveShadowGraph empty;
  auto missing = empty.comparePreparationToPlan(smaller);
  ASSERT_EQ(1u, missing.size());
  EXPECT_EQ(WorldGenerationExecutiveDivergenceKind::MissingNode, missing[0].kind);
}
