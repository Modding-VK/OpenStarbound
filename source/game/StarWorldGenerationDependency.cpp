#include "StarWorldGenerationDependency.hpp"

#include <algorithm>

namespace Star {

bool WorldGenerationDependencyNode::operator==(WorldGenerationDependencyNode const& rhs) const {
  return sector == rhs.sector && level == rhs.level;
}

bool WorldGenerationDependencyNode::operator<(WorldGenerationDependencyNode const& rhs) const {
  if (sector[0] != rhs.sector[0])
    return sector[0] < rhs.sector[0];
  if (sector[1] != rhs.sector[1])
    return sector[1] < rhs.sector[1];
  return level < rhs.level;
}

namespace {

void appendUnique(List<WorldGenerationDependencyNode>& nodes, WorldGenerationDependencyNode const& node) {
  if (std::find(nodes.begin(), nodes.end(), node) == nodes.end())
    nodes.append(node);
}

bool containsNode(List<WorldGenerationDependencyNode> const& nodes, WorldGenerationDependencyNode const& node) {
  return std::find(nodes.begin(), nodes.end(), node) != nodes.end();
}

bool hasIncomingEdge(WorldGenerationDependencyPlan const& plan, WorldGenerationDependencyNode const& node,
    WorldGenerationDependencyGraph::CurrentLevel const& currentLevel, size_t& directBlockers,
    Maybe<WorldGenerationDependencyNode>& firstBlocker) {
  bool blocked = false;
  for (auto const& edge : plan.edges) {
    if (!(edge.dependent == node) || currentLevel(edge.prerequisite.sector) >= edge.prerequisite.level)
      continue;
    blocked = true;
    ++directBlockers;
    if (!firstBlocker)
      firstBlocker = edge.prerequisite;
  }
  return blocked;
}

}

WorldGenerationDependencyPlan WorldGenerationDependencyGraph::build(Vec2U const& root, uint8_t targetLevel,
    AdjacentSectors const& adjacentSectors, size_t maxNodes) {
  WorldGenerationDependencyPlan plan;
  if (!targetLevel || !adjacentSectors || !maxNodes)
    return plan;

  auto add = [&](auto&& self, WorldGenerationDependencyNode const& node) -> bool {
    if (!node.level)
      return true;
    if (containsNode(plan.nodes, node))
      return true;
    if (plan.nodes.size() >= maxNodes) {
      plan.truncated = true;
      return false;
    }

    plan.nodes.append(node);
    if (node.level == 1)
      return true;

    List<WorldGenerationDependencyNode> prerequisites;
    prerequisites.append(WorldGenerationDependencyNode{node.sector, static_cast<uint8_t>(node.level - 1)});
    for (auto const& sector : adjacentSectors(node.sector))
      appendUnique(prerequisites, WorldGenerationDependencyNode{sector, static_cast<uint8_t>(node.level - 1)});
    std::sort(prerequisites.begin(), prerequisites.end());

    for (auto const& prerequisite : prerequisites) {
      if (!self(self, prerequisite))
        return false;
      plan.edges.append(WorldGenerationDependencyEdge{prerequisite, node});
    }
    return true;
  };

  add(add, WorldGenerationDependencyNode{root, targetLevel});
  return plan;
}

WorldGenerationDependencyAnalysis WorldGenerationDependencyGraph::analyze(WorldGenerationDependencyPlan const& plan,
    CurrentLevel const& currentLevel) {
  WorldGenerationDependencyAnalysis analysis;
  if (!currentLevel)
    return analysis;

  for (auto const& node : plan.nodes) {
    if (currentLevel(node.sector) >= node.level) {
      ++analysis.satisfiedNodes;
      continue;
    }
    if (hasIncomingEdge(plan, node, currentLevel, analysis.directBlockers, analysis.firstBlocker)) {
      ++analysis.blockedNodes;
    } else {
      ++analysis.runnableNodes;
    }
  }
  return analysis;
}

}
