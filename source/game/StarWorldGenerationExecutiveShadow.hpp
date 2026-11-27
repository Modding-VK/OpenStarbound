#pragma once

#include <cstddef>
#include <cstdint>

#include "StarList.hpp"
#include "StarMap.hpp"
#include "StarMaybe.hpp"
#include "StarSet.hpp"
#include "StarWorldGenerationDependency.hpp"

namespace Star {

// Shadow-only executive graph for DAG-02. It retains shared preparation nodes
// and per-consumer demand without driving live-world publication. Production
// paths keep using WorldGenerationDependencyGraph::build/analyze.

enum class WorldGenerationExecutiveNodePhase : uint8_t {
  Idle = 0,
  Reserved,
  Preparing,
  Prepared,
  Eligible,
  Published,
  Discarded
};

struct WorldGenerationExecutiveConsumerId {
  uint64_t worldEpoch = 0;
  uint64_t sequence = 0;

  bool operator==(WorldGenerationExecutiveConsumerId const& rhs) const;
  bool operator<(WorldGenerationExecutiveConsumerId const& rhs) const;
};

struct WorldGenerationExecutiveNodeRecord {
  WorldGenerationDependencyNode identity;
  WorldGenerationExecutiveNodePhase preparationPhase = WorldGenerationExecutiveNodePhase::Idle;
  WorldGenerationExecutiveNodePhase publicationPhase = WorldGenerationExecutiveNodePhase::Idle;
  size_t preparationPrerequisiteCount = 0;
  size_t publicationPrerequisiteCount = 0;
  size_t dependentCount = 0;
  size_t estimatedCost = 0;
  Set<WorldGenerationExecutiveConsumerId> consumers;
};

enum class WorldGenerationExecutiveDivergenceKind : uint8_t {
  MissingNode,
  ExtraNode,
  MissingPreparationEdge,
  ExtraPreparationEdge,
  MissingPublicationEdge,
  ExtraPublicationEdge
};

struct WorldGenerationExecutiveDivergence {
  WorldGenerationExecutiveDivergenceKind kind;
  Maybe<WorldGenerationDependencyNode> node;
  Maybe<WorldGenerationDependencyEdge> edge;
};

// Persistent shadow graph: preparation edges mirror the recursive dependency
// plan; publication edges are a separate set that never authorizes writes here.
class WorldGenerationExecutiveShadowGraph {
public:
  void ingestPreparationPlan(WorldGenerationDependencyPlan const& plan, size_t estimatedCostPerNode = 1);
  void addPublicationEdge(WorldGenerationDependencyEdge const& edge);

  bool addDemand(WorldGenerationExecutiveConsumerId const& consumer, WorldGenerationDependencyNode const& node);
  // Returns true when the consumer was present. Leaving zero consumers does not
  // discard a node still retained by the preparation plan.
  bool removeDemand(WorldGenerationExecutiveConsumerId const& consumer, WorldGenerationDependencyNode const& node);

  bool contains(WorldGenerationDependencyNode const& node) const;
  size_t nodeCount() const;
  size_t preparationEdgeCount() const;
  size_t publicationEdgeCount() const;
  size_t consumerCount(WorldGenerationDependencyNode const& node) const;
  Maybe<WorldGenerationExecutiveNodeRecord> record(WorldGenerationDependencyNode const& node) const;

  List<WorldGenerationDependencyEdge> preparationEdges() const;
  List<WorldGenerationDependencyEdge> publicationEdges() const;

  // Compare the retained preparation subgraph to a freshly built recursive
  // plan. Divergences are recorded only; nothing is published or discarded.
  List<WorldGenerationExecutiveDivergence> comparePreparationToPlan(
      WorldGenerationDependencyPlan const& plan) const;
  List<WorldGenerationExecutiveDivergence> comparePublicationToPlan(
      WorldGenerationDependencyPlan const& plan) const;

private:
  Map<WorldGenerationDependencyNode, WorldGenerationExecutiveNodeRecord> m_nodes;
  List<WorldGenerationDependencyEdge> m_preparationEdges;
  List<WorldGenerationDependencyEdge> m_publicationEdges;

  void refreshCounts(WorldGenerationDependencyNode const& node);
  static bool containsEdge(List<WorldGenerationDependencyEdge> const& edges,
      WorldGenerationDependencyEdge const& edge);
  static void appendUniqueEdge(List<WorldGenerationDependencyEdge>& edges,
      WorldGenerationDependencyEdge const& edge);
  static List<WorldGenerationExecutiveDivergence> compareEdgeSet(
      List<WorldGenerationDependencyEdge> const& actual,
      List<WorldGenerationDependencyEdge> const& expected,
      WorldGenerationExecutiveDivergenceKind missing,
      WorldGenerationExecutiveDivergenceKind extra);
};

}
