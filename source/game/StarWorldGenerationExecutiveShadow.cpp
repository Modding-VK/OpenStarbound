#include "StarWorldGenerationExecutiveShadow.hpp"

#include <algorithm>

namespace Star {

bool WorldGenerationExecutiveConsumerId::operator==(WorldGenerationExecutiveConsumerId const& rhs) const {
  return worldEpoch == rhs.worldEpoch && sequence == rhs.sequence;
}

bool WorldGenerationExecutiveConsumerId::operator<(WorldGenerationExecutiveConsumerId const& rhs) const {
  if (worldEpoch != rhs.worldEpoch)
    return worldEpoch < rhs.worldEpoch;
  return sequence < rhs.sequence;
}

bool WorldGenerationExecutiveShadowGraph::containsEdge(List<WorldGenerationDependencyEdge> const& edges,
    WorldGenerationDependencyEdge const& edge) {
  return std::find_if(edges.begin(), edges.end(), [&](WorldGenerationDependencyEdge const& candidate) {
    return candidate.prerequisite == edge.prerequisite && candidate.dependent == edge.dependent;
  }) != edges.end();
}

void WorldGenerationExecutiveShadowGraph::appendUniqueEdge(List<WorldGenerationDependencyEdge>& edges,
    WorldGenerationDependencyEdge const& edge) {
  if (!containsEdge(edges, edge))
    edges.append(edge);
}

void WorldGenerationExecutiveShadowGraph::refreshCounts(WorldGenerationDependencyNode const& node) {
  auto it = m_nodes.find(node);
  if (it == m_nodes.end())
    return;

  size_t preparation = 0;
  size_t publication = 0;
  size_t dependents = 0;
  for (auto const& edge : m_preparationEdges) {
    if (edge.dependent == node)
      ++preparation;
    if (edge.prerequisite == node)
      ++dependents;
  }
  for (auto const& edge : m_publicationEdges) {
    if (edge.dependent == node)
      ++publication;
    if (edge.prerequisite == node)
      ++dependents;
  }
  it->second.preparationPrerequisiteCount = preparation;
  it->second.publicationPrerequisiteCount = publication;
  it->second.dependentCount = dependents;
}

void WorldGenerationExecutiveShadowGraph::ingestPreparationPlan(WorldGenerationDependencyPlan const& plan,
    size_t estimatedCostPerNode) {
  for (auto const& node : plan.nodes) {
    auto& record = m_nodes[node];
    record.identity = node;
    if (!record.estimatedCost)
      record.estimatedCost = estimatedCostPerNode;
    if (record.preparationPhase == WorldGenerationExecutiveNodePhase::Idle)
      record.preparationPhase = WorldGenerationExecutiveNodePhase::Reserved;
  }
  for (auto const& edge : plan.edges) {
    appendUniqueEdge(m_preparationEdges, edge);
    if (!m_nodes.contains(edge.prerequisite)) {
      auto& prerequisite = m_nodes[edge.prerequisite];
      prerequisite.identity = edge.prerequisite;
      prerequisite.estimatedCost = estimatedCostPerNode;
      prerequisite.preparationPhase = WorldGenerationExecutiveNodePhase::Reserved;
    }
    if (!m_nodes.contains(edge.dependent)) {
      auto& dependent = m_nodes[edge.dependent];
      dependent.identity = edge.dependent;
      dependent.estimatedCost = estimatedCostPerNode;
      dependent.preparationPhase = WorldGenerationExecutiveNodePhase::Reserved;
    }
  }
  for (auto const& node : plan.nodes)
    refreshCounts(node);
  for (auto const& edge : plan.edges) {
    refreshCounts(edge.prerequisite);
    refreshCounts(edge.dependent);
  }
}

void WorldGenerationExecutiveShadowGraph::addPublicationEdge(WorldGenerationDependencyEdge const& edge) {
  appendUniqueEdge(m_publicationEdges, edge);
  if (!m_nodes.contains(edge.prerequisite)) {
    auto& prerequisite = m_nodes[edge.prerequisite];
    prerequisite.identity = edge.prerequisite;
  }
  if (!m_nodes.contains(edge.dependent)) {
    auto& dependent = m_nodes[edge.dependent];
    dependent.identity = edge.dependent;
  }
  refreshCounts(edge.prerequisite);
  refreshCounts(edge.dependent);
}

bool WorldGenerationExecutiveShadowGraph::addDemand(WorldGenerationExecutiveConsumerId const& consumer,
    WorldGenerationDependencyNode const& node) {
  auto it = m_nodes.find(node);
  if (it == m_nodes.end())
    return false;
  it->second.consumers.add(consumer);
  return true;
}

bool WorldGenerationExecutiveShadowGraph::removeDemand(WorldGenerationExecutiveConsumerId const& consumer,
    WorldGenerationDependencyNode const& node) {
  auto it = m_nodes.find(node);
  if (it == m_nodes.end())
    return false;
  if (!it->second.consumers.contains(consumer))
    return false;
  it->second.consumers.remove(consumer);
  return true;
}

bool WorldGenerationExecutiveShadowGraph::contains(WorldGenerationDependencyNode const& node) const {
  return m_nodes.contains(node);
}

size_t WorldGenerationExecutiveShadowGraph::nodeCount() const {
  return m_nodes.size();
}

size_t WorldGenerationExecutiveShadowGraph::preparationEdgeCount() const {
  return m_preparationEdges.size();
}

size_t WorldGenerationExecutiveShadowGraph::publicationEdgeCount() const {
  return m_publicationEdges.size();
}

size_t WorldGenerationExecutiveShadowGraph::consumerCount(WorldGenerationDependencyNode const& node) const {
  auto it = m_nodes.find(node);
  if (it == m_nodes.end())
    return 0;
  return it->second.consumers.size();
}

Maybe<WorldGenerationExecutiveNodeRecord> WorldGenerationExecutiveShadowGraph::record(
    WorldGenerationDependencyNode const& node) const {
  auto it = m_nodes.find(node);
  if (it == m_nodes.end())
    return {};
  return it->second;
}

List<WorldGenerationDependencyEdge> WorldGenerationExecutiveShadowGraph::preparationEdges() const {
  return m_preparationEdges;
}

List<WorldGenerationDependencyEdge> WorldGenerationExecutiveShadowGraph::publicationEdges() const {
  return m_publicationEdges;
}

List<WorldGenerationExecutiveDivergence> WorldGenerationExecutiveShadowGraph::compareEdgeSet(
    List<WorldGenerationDependencyEdge> const& actual,
    List<WorldGenerationDependencyEdge> const& expected,
    WorldGenerationExecutiveDivergenceKind missing,
    WorldGenerationExecutiveDivergenceKind extra) {
  List<WorldGenerationExecutiveDivergence> divergences;
  for (auto const& edge : expected) {
    if (!containsEdge(actual, edge))
      divergences.append(WorldGenerationExecutiveDivergence{missing, {}, edge});
  }
  for (auto const& edge : actual) {
    if (!containsEdge(expected, edge))
      divergences.append(WorldGenerationExecutiveDivergence{extra, {}, edge});
  }
  return divergences;
}

List<WorldGenerationExecutiveDivergence> WorldGenerationExecutiveShadowGraph::comparePreparationToPlan(
    WorldGenerationDependencyPlan const& plan) const {
  List<WorldGenerationExecutiveDivergence> divergences;

  auto inPlanNodes = [&](WorldGenerationDependencyNode const& node) {
    return std::find(plan.nodes.begin(), plan.nodes.end(), node) != plan.nodes.end();
  };
  auto inPreparation = [&](WorldGenerationExecutiveNodeRecord const& record) {
    if (record.preparationPhase != WorldGenerationExecutiveNodePhase::Idle)
      return true;
    for (auto const& edge : m_preparationEdges) {
      if (edge.prerequisite == record.identity || edge.dependent == record.identity)
        return true;
    }
    return false;
  };

  for (auto const& node : plan.nodes) {
    if (!contains(node))
      divergences.append(WorldGenerationExecutiveDivergence{
          WorldGenerationExecutiveDivergenceKind::MissingNode, node, {}});
  }
  for (auto const& entry : m_nodes) {
    if (!inPlanNodes(entry.first) && inPreparation(entry.second))
      divergences.append(WorldGenerationExecutiveDivergence{
          WorldGenerationExecutiveDivergenceKind::ExtraNode, entry.first, {}});
  }

  auto edgeDivergences = compareEdgeSet(m_preparationEdges, plan.edges,
      WorldGenerationExecutiveDivergenceKind::MissingPreparationEdge,
      WorldGenerationExecutiveDivergenceKind::ExtraPreparationEdge);
  for (auto& divergence : edgeDivergences)
    divergences.append(std::move(divergence));
  return divergences;
}

List<WorldGenerationExecutiveDivergence> WorldGenerationExecutiveShadowGraph::comparePublicationToPlan(
    WorldGenerationDependencyPlan const& plan) const {
  return compareEdgeSet(m_publicationEdges, plan.edges,
      WorldGenerationExecutiveDivergenceKind::MissingPublicationEdge,
      WorldGenerationExecutiveDivergenceKind::ExtraPublicationEdge);
}

}
