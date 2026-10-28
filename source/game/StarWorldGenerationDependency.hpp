#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>

#include "StarList.hpp"
#include "StarMaybe.hpp"
#include "StarVector.hpp"

namespace Star {

// A value-only description of a generation stage.  Level zero means that no
// generation work is required; it is never emitted as a graph node.
struct WorldGenerationDependencyNode {
  Vec2U sector;
  uint8_t level = 0;

  bool operator==(WorldGenerationDependencyNode const& rhs) const;
  bool operator<(WorldGenerationDependencyNode const& rhs) const;
};

struct WorldGenerationDependencyEdge {
  WorldGenerationDependencyNode prerequisite;
  WorldGenerationDependencyNode dependent;
};

struct WorldGenerationDependencyPlan {
  List<WorldGenerationDependencyNode> nodes;
  List<WorldGenerationDependencyEdge> edges;
  bool truncated = false;
};

struct WorldGenerationDependencyAnalysis {
  size_t satisfiedNodes = 0;
  size_t runnableNodes = 0;
  size_t blockedNodes = 0;
  size_t directBlockers = 0;
  Maybe<WorldGenerationDependencyNode> firstBlocker;
  // Stable, deduplicated stages whose direct prerequisites already existed in
  // the sampled state.  A truncated plan must never authorize work from this
  // list because it may omit a prerequisite.
  List<WorldGenerationDependencyNode> frontier;
};

// Mirrors the dependency rule currently implemented by
// WorldStorage::generateSectorToLevel without retaining WorldStorage, tiles,
// entities or callbacks.  A stage L of a sector depends on stage L - 1 of the
// sector itself and of its adjacent sectors.  BaseTiles (L=1) therefore has no
// prerequisite.  The caller owns all geometry normalization through the
// adjacency callback.
class WorldGenerationDependencyGraph {
public:
  static constexpr size_t DefaultMaxNodes = 256;

  using AdjacentSectors = std::function<List<Vec2U>(Vec2U const&)>;
  using CurrentLevel = std::function<uint8_t(Vec2U const&)>;

  static WorldGenerationDependencyPlan build(Vec2U const& root, uint8_t targetLevel,
      AdjacentSectors const& adjacentSectors, size_t maxNodes = DefaultMaxNodes);
  static WorldGenerationDependencyPlan build(List<WorldGenerationDependencyNode> roots,
      AdjacentSectors const& adjacentSectors, size_t maxNodes = DefaultMaxNodes);
  static WorldGenerationDependencyAnalysis analyze(WorldGenerationDependencyPlan const& plan,
      CurrentLevel const& currentLevel);
};

}
