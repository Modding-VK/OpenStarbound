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
  uint64_t contentRevision = 0;
  uint64_t templateRevision = 0;
  bool executing = false;
  bool publishAuthorized = true;
  size_t retryCount = 0;
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

  // Interest update: ingest the recursive plan and attach the consumer to every
  // node it requires. Auxiliaries inherit presence without becoming activation
  // roots of their own.
  size_t applyInterest(WorldGenerationExecutiveConsumerId const& consumer,
      WorldGenerationDependencyPlan const& plan, size_t estimatedCostPerNode = 1);
  // Remove one consumer everywhere. Nodes required by others stay reserved.
  size_t clearInterest(WorldGenerationExecutiveConsumerId const& consumer);

  // Pure anticipation: advance preparation phase for runnable nodes without
  // authorizing publication. Returns how many nodes moved to Prepared.
  size_t anticipatePreparation(WorldGenerationDependencyGraph::CurrentLevel const& currentLevel);
  bool markPrepared(WorldGenerationDependencyNode const& node);
  bool markPublished(WorldGenerationDependencyNode const& node);
  // DAG-05 shadow: invalidate revokes publish rights without proving the
  // producer stopped. Nodes with executing=true stay reserved until drain.
  bool markExecuting(WorldGenerationDependencyNode const& node, bool executing);
  bool invalidate(WorldGenerationDependencyNode const& node);
  bool cancel(WorldGenerationDependencyNode const& node);
  size_t drainDiscarded();
  bool noteRetry(WorldGenerationDependencyNode const& node, size_t retryLimit);
  // Sector unload in shadow: discard preparation for that sector when no
  // consumer remains. Shared demand from another consumer blocks discard.
  size_t unloadSector(Vec2U const& sector);

  bool contains(WorldGenerationDependencyNode const& node) const;
  size_t nodeCount() const;
  size_t preparationEdgeCount() const;
  size_t publicationEdgeCount() const;
  size_t consumerCount(WorldGenerationDependencyNode const& node) const;
  size_t activeConsumerCount() const;
  Maybe<WorldGenerationExecutiveNodeRecord> record(WorldGenerationDependencyNode const& node) const;
  WorldGenerationDependencyPlan retainedPreparationPlan() const;
  List<WorldGenerationDependencyNode> preparationFrontier(
      WorldGenerationDependencyGraph::CurrentLevel const& currentLevel) const;

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
  void refreshAllCounts();
  bool nodeStillRequired(WorldGenerationDependencyNode const& node) const;
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
