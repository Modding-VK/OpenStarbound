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

void WorldGenerationExecutiveShadowGraph::refreshAllCounts() {
  for (auto const& entry : m_nodes)
    refreshCounts(entry.first);
}

bool WorldGenerationExecutiveShadowGraph::nodeStillRequired(WorldGenerationDependencyNode const& node) const {
  auto it = m_nodes.find(node);
  if (it == m_nodes.end())
    return false;
  if (!it->second.consumers.empty())
    return true;
  for (auto const& edge : m_preparationEdges) {
    if (!(edge.prerequisite == node))
      continue;
    auto dependent = m_nodes.find(edge.dependent);
    if (dependent != m_nodes.end() && !dependent->second.consumers.empty())
      return true;
  }
  for (auto const& edge : m_publicationEdges) {
    if (!(edge.prerequisite == node))
      continue;
    auto dependent = m_nodes.find(edge.dependent);
    if (dependent != m_nodes.end() && !dependent->second.consumers.empty())
      return true;
  }
  return false;
}

size_t WorldGenerationExecutiveShadowGraph::applyInterest(WorldGenerationExecutiveConsumerId const& consumer,
    WorldGenerationDependencyPlan const& plan, size_t estimatedCostPerNode) {
  ingestPreparationPlan(plan, estimatedCostPerNode);
  size_t attached = 0;
  for (auto const& node : plan.nodes) {
    if (addDemand(consumer, node))
      ++attached;
  }
  return attached;
}

size_t WorldGenerationExecutiveShadowGraph::clearInterest(WorldGenerationExecutiveConsumerId const& consumer) {
  size_t removed = 0;
  for (auto& entry : m_nodes) {
    if (entry.second.consumers.remove(consumer))
      ++removed;
  }
  return removed;
}

WorldGenerationDependencyPlan WorldGenerationExecutiveShadowGraph::retainedPreparationPlan() const {
  WorldGenerationDependencyPlan plan;
  for (auto const& entry : m_nodes) {
    if (entry.second.preparationPhase == WorldGenerationExecutiveNodePhase::Idle
        || entry.second.preparationPhase == WorldGenerationExecutiveNodePhase::Discarded)
      continue;
    plan.nodes.append(entry.first);
  }
  std::sort(plan.nodes.begin(), plan.nodes.end());
  for (auto const& edge : m_preparationEdges) {
    if (contains(edge.prerequisite) && contains(edge.dependent))
      plan.edges.append(edge);
  }
  return plan;
}

List<WorldGenerationDependencyNode> WorldGenerationExecutiveShadowGraph::preparationFrontier(
    WorldGenerationDependencyGraph::CurrentLevel const& currentLevel) const {
  auto analysis = WorldGenerationDependencyGraph::analyze(retainedPreparationPlan(), currentLevel);
  return analysis.frontier;
}

size_t WorldGenerationExecutiveShadowGraph::anticipatePreparation(
    WorldGenerationDependencyGraph::CurrentLevel const& currentLevel) {
  size_t advanced = 0;
  for (auto const& node : preparationFrontier(currentLevel)) {
    auto it = m_nodes.find(node);
    if (it == m_nodes.end())
      continue;
    if (it->second.preparationPhase == WorldGenerationExecutiveNodePhase::Reserved
        || it->second.preparationPhase == WorldGenerationExecutiveNodePhase::Preparing) {
      it->second.preparationPhase = WorldGenerationExecutiveNodePhase::Prepared;
      ++advanced;
    }
  }
  return advanced;
}

bool WorldGenerationExecutiveShadowGraph::markPublished(WorldGenerationDependencyNode const& node) {
  auto it = m_nodes.find(node);
  if (it == m_nodes.end())
    return false;
  if (it->second.preparationPhase != WorldGenerationExecutiveNodePhase::Prepared
      && it->second.preparationPhase != WorldGenerationExecutiveNodePhase::Eligible)
    return false;
  it->second.preparationPhase = WorldGenerationExecutiveNodePhase::Published;
  it->second.publicationPhase = WorldGenerationExecutiveNodePhase::Published;
  return true;
}

size_t WorldGenerationExecutiveShadowGraph::unloadSector(Vec2U const& sector) {
  size_t discarded = 0;
  List<WorldGenerationDependencyNode> victims;
  for (auto const& entry : m_nodes) {
    if (entry.first.sector != sector)
      continue;
    if (nodeStillRequired(entry.first))
      continue;
    victims.append(entry.first);
  }
  auto isVictim = [&](WorldGenerationDependencyNode const& node) {
    return std::find(victims.begin(), victims.end(), node) != victims.end();
  };
  for (auto const& node : victims) {
    auto it = m_nodes.find(node);
    if (it == m_nodes.end())
      continue;
    it->second.preparationPhase = WorldGenerationExecutiveNodePhase::Discarded;
    it->second.publicationPhase = WorldGenerationExecutiveNodePhase::Discarded;
    it->second.consumers.clear();
    ++discarded;
  }
  eraseWhere(m_preparationEdges, [&](WorldGenerationDependencyEdge const& edge) {
    return isVictim(edge.prerequisite) || isVictim(edge.dependent);
  });
  eraseWhere(m_publicationEdges, [&](WorldGenerationDependencyEdge const& edge) {
    return isVictim(edge.prerequisite) || isVictim(edge.dependent);
  });
  for (auto const& node : victims)
    m_nodes.erase(node);
  refreshAllCounts();
  return discarded;
}

size_t WorldGenerationExecutiveShadowGraph::activeConsumerCount() const {
  Set<WorldGenerationExecutiveConsumerId> consumers;
  for (auto const& entry : m_nodes) {
    for (auto const& consumer : entry.second.consumers)
      consumers.add(consumer);
  }
  return consumers.size();
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
