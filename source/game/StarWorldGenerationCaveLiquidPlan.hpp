#pragma once

#include "StarGameTypes.hpp"
#include "StarMathCommon.hpp"
#include "StarHash.hpp"
#include "StarLiquidTypes.hpp"
#include "StarList.hpp"
#include "StarMap.hpp"
#include "StarMaterialTypes.hpp"
#include "StarSet.hpp"
#include "StarWorldGenerationTasks.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <utility>

namespace Star {

// CPU-04 first slice: private CaveLiquid flood-fill plan and ordered pressure
// delta. Operates on an owner-provided cell sample map; never installs liquids
// or backgrounds into a live WorldStorage/tile array.
struct WorldGenerationCaveLiquidSample {
  MaterialId foreground = EmptyMaterialId;
  MaterialId background = EmptyMaterialId;
  LiquidId liquid = EmptyLiquidId;
  DungeonId dungeonId = NoDungeonId;
  // Owner-resolved BlockCollisionSet membership for non-empty foreground.
  bool solidCollision = false;
};

struct WorldGenerationCaveLiquidPlanIdentity {
  static constexpr unsigned ModelVersion = 1;
  static constexpr unsigned ProductVersion = 1;
  WorldGenerationTaskIdentity task{};
  uint64_t tileRevision = 0;
  uint64_t templateRevision = 0;
  Vec2U worldSize{};
  RectI sectorTiles;
  LiquidId fillLiquid = EmptyLiquidId;
  bool fillMicrodungeons = false;
  bool encloseLiquids = false;
  float seedDensity = 0.0f;
  unsigned productVersion = ProductVersion;

  bool valid() const {
    return productVersion == ProductVersion
        && worldSize[0] != 0 && worldSize[1] != 0
        && sectorTiles.isNull() == false
        && tileRevision != 0
        && task.stage == WorldGenerationTaskStage::CaveLiquidPlan;
  }

  bool operator==(WorldGenerationCaveLiquidPlanIdentity const& other) const {
    return task == other.task
        && tileRevision == other.tileRevision
        && templateRevision == other.templateRevision
        && worldSize == other.worldSize
        && sectorTiles == other.sectorTiles
        && fillLiquid == other.fillLiquid
        && fillMicrodungeons == other.fillMicrodungeons
        && encloseLiquids == other.encloseLiquids
        && seedDensity == other.seedDensity
        && productVersion == other.productVersion;
  }

  bool matches(WorldGenerationCaveLiquidPlanIdentity const& expectation) const {
    return valid() && expectation.valid() && *this == expectation;
  }
};

struct WorldGenerationCaveLiquidDeltaEntry {
  Vec2I position;
  float pressure = 0.0f;
  bool fillBackground = false;

  bool operator==(WorldGenerationCaveLiquidDeltaEntry const& other) const {
    return position == other.position
        && pressure == other.pressure
        && fillBackground == other.fillBackground;
  }
};

using WorldGenerationCaveLiquidSamples = HashMap<Vec2I, WorldGenerationCaveLiquidSample>;

inline Vec2I worldGenerationCaveLiquidWrap(Vec2I coord, Vec2U worldSize) {
  return Vec2I{pmod<int>(coord[0], (int)worldSize[0]), coord[1]};
}

inline RectI worldGenerationCaveLiquidReadBounds(RectI sectorTiles, Vec2U worldSize) {
  RectI bounds = sectorTiles.padded(Vec2I((int)WorldSectorSize - 1, (int)WorldSectorSize - 1));
  int maxY = (int)worldSize[1] - 1;
  bounds.min()[1] = clamp<int>(bounds.min()[1], 0, maxY);
  bounds.max()[1] = clamp<int>(bounds.max()[1], 0, maxY);
  return bounds;
}

// Same seed lattice as the legacy caveLiquidSeeds path: frequency = 100/density,
// points that fall inside the sector rectangle.
inline List<Vec2I> planWorldGenerationCaveLiquidSeeds(RectI sectorTiles, float seedDensity) {
  List<Vec2I> nodes;
  if (!(seedDensity > 0.0f))
    return nodes;
  int frequency = int(100 / seedDensity);
  if (frequency <= 0)
    return nodes;
  for (int y = frequency * (int)floor((double)sectorTiles.min()[1] / frequency); y < sectorTiles.max()[1];
       y += frequency) {
    for (int x = frequency * (int)floor((double)sectorTiles.min()[0] / frequency); x < sectorTiles.max()[0];
         x += frequency) {
      if (sectorTiles.contains(Vec2I(x, y)))
        nodes.append(Vec2I(x, y));
    }
  }
  sort(nodes.begin(), nodes.end(), [](Vec2I const& a, Vec2I const& b) {
    return a[1] != b[1] ? a[1] < b[1] : a[0] < b[0];
  });
  return nodes;
}

namespace detail {

inline void caveLiquidLevelCluster(Set<Vec2I>& cluster, Set<Vec2I> const& filled,
    Map<Vec2I, float>& results) {
  int maxY = std::numeric_limits<int>::min();
  int minY = std::numeric_limits<int>::max();
  for (auto const& droplet : cluster) {
    if (filled.contains(droplet + Vec2I(1, 0)) && filled.contains(droplet + Vec2I(-1, 0))
        && filled.contains(droplet + Vec2I(0, -1))) {
      if (droplet.y() > maxY)
        maxY = droplet.y();
      if (!filled.contains(droplet + Vec2I(0, 1))) {
        if (droplet.y() <= minY)
          minY = droplet.y();
      }
    } else {
      if (droplet.y() <= minY)
        minY = droplet.y() - 1;
    }
  }
  int liquidLevel = std::min(maxY, minY);
  for (auto const& droplet : cluster) {
    int pressure = liquidLevel - droplet.y();
    if (pressure >= 0)
      results[droplet] = 1.0f + pressure;
  }
}

inline Map<Vec2I, float> caveLiquidDetermineLevel(Set<Vec2I> const& spots, Set<Vec2I> const& filled,
    Vec2U worldSize) {
  Set<Vec2I> openSet(spots);
  Map<Vec2I, float> results;
  while (openSet.size() > 0) {
    Set<Vec2I> cluster;
    Set<Vec2I> openCluster;
    openCluster.add(*openSet.begin());
    while (openCluster.size() > 0) {
      Vec2I node = *openCluster.begin();
      openCluster.remove(node);
      if (openSet.contains(node)) {
        openSet.remove(node);
        cluster.add(node);
        openCluster.add(worldGenerationCaveLiquidWrap(Vec2I(node.x(), node.y() + 1), worldSize));
        openCluster.add(worldGenerationCaveLiquidWrap(Vec2I(node.x(), node.y() - 1), worldSize));
        openCluster.add(worldGenerationCaveLiquidWrap(Vec2I(node.x() + 1, node.y()), worldSize));
        openCluster.add(worldGenerationCaveLiquidWrap(Vec2I(node.x() - 1, node.y()), worldSize));
      }
    }
    caveLiquidLevelCluster(cluster, filled, results);
  }
  return results;
}

}

inline List<WorldGenerationCaveLiquidDeltaEntry> planWorldGenerationCaveLiquidDelta(
    WorldGenerationCaveLiquidPlanIdentity const& identity,
    WorldGenerationCaveLiquidSamples const& samples) {
  List<WorldGenerationCaveLiquidDeltaEntry> delta;
  if (!identity.valid())
    return delta;

  auto seeds = planWorldGenerationCaveLiquidSeeds(identity.sectorTiles, identity.seedDensity);
  if (seeds.empty())
    return delta;

  RectI bounds = worldGenerationCaveLiquidReadBounds(identity.sectorTiles, identity.worldSize);
  Set<Vec2I> openNodes;
  for (auto const& seed : seeds)
    openNodes.add(seed);

  Set<Vec2I> badNodes;
  for (int i = bounds.xMin(); i <= bounds.xMax(); ++i) {
    badNodes.add({i, bounds.yMin()});
    badNodes.add({i, bounds.yMax()});
  }
  for (int i = bounds.yMin(); i <= bounds.yMax(); ++i) {
    badNodes.add({bounds.xMin(), i});
    badNodes.add({bounds.xMax(), i});
  }

  Set<Vec2I> candidateNodes;
  auto propose = [&](Vec2I const& position) {
    if (!bounds.contains(position))
      return;
    if (candidateNodes.contains(position) || badNodes.contains(position))
      return;
    auto wrapped = worldGenerationCaveLiquidWrap(position, identity.worldSize);
    auto const* cell = samples.ptr(wrapped);
    if (!cell)
      return;
    if (cell->foreground != EmptyMaterialId) {
      if (!cell->solidCollision)
        badNodes.add(position);
      return;
    }
    if ((cell->dungeonId != NoDungeonId
            && (!identity.fillMicrodungeons || cell->dungeonId != BiomeMicroDungeonId))
        || (!identity.encloseLiquids && cell->background == EmptyMaterialId)
        || (cell->liquid != identity.fillLiquid && cell->liquid != EmptyLiquidId)) {
      badNodes.add(position);
      return;
    }
    candidateNodes.add(position);
    openNodes.add(position);
  };

  while (openNodes.size()) {
    auto node = *openNodes.begin();
    openNodes.remove(node);
    propose(node + Vec2I(-1, 0));
    propose(node + Vec2I(1, 0));
    propose(node + Vec2I(0, -1));
  }

  Set<Vec2I> visitedNodes;
  auto poison = [&](Vec2I position) {
    if (!bounds.contains(position) || visitedNodes.contains(position))
      return;
    visitedNodes.add(position);
    auto wrapped = worldGenerationCaveLiquidWrap(position, identity.worldSize);
    auto const* cell = samples.ptr(wrapped);
    if (!cell || cell->foreground != EmptyMaterialId)
      return;
    badNodes.add(position);
  };

  while (badNodes.size()) {
    auto node = *badNodes.begin();
    badNodes.remove(node);
    candidateNodes.remove(node);
    poison(node + Vec2I(-1, 0));
    poison(node + Vec2I(1, 0));
    poison(node + Vec2I(0, 1));
  }

  Set<Vec2I> solidSurroundings(candidateNodes);
  auto solids = [&](Vec2I position) {
    auto wrapped = worldGenerationCaveLiquidWrap(position, identity.worldSize);
    auto const* cell = samples.ptr(wrapped);
    if (cell && cell->foreground != EmptyMaterialId)
      solidSurroundings.add(position);
  };
  for (auto position : candidateNodes) {
    solids(position + Vec2I(1, 0));
    solids(position + Vec2I(-1, 0));
    solids(position + Vec2I(0, 1));
    solids(position + Vec2I(0, -1));
  }

  auto drops = detail::caveLiquidDetermineLevel(candidateNodes, solidSurroundings, identity.worldSize);
  List<Vec2I> ordered;
  ordered.reserve(drops.size());
  for (auto const& entry : drops)
    ordered.append(entry.first);
  sort(ordered.begin(), ordered.end(), [](Vec2I const& a, Vec2I const& b) {
    return a[1] != b[1] ? a[1] < b[1] : a[0] < b[0];
  });

  for (auto const& position : ordered) {
    float pressure = drops.get(position);
    if (!(pressure > 0.0f))
      continue;
    WorldGenerationCaveLiquidDeltaEntry entry;
    entry.position = position;
    entry.pressure = pressure;
    if (identity.encloseLiquids) {
      auto wrapped = worldGenerationCaveLiquidWrap(position, identity.worldSize);
      if (auto const* cell = samples.ptr(wrapped))
        entry.fillBackground = cell->background == EmptyMaterialId;
    }
    delta.append(entry);
  }
  return delta;
}

// Owner-local plan handle: retains an ordered private delta and never publishes.
class WorldGenerationCaveLiquidPlan {
public:
  static std::optional<WorldGenerationCaveLiquidPlan> tryPlan(
      WorldGenerationCaveLiquidPlanIdentity identity,
      WorldGenerationCaveLiquidSamples const& samples) {
    if (!identity.valid())
      return std::nullopt;
    auto delta = planWorldGenerationCaveLiquidDelta(identity, samples);
    return WorldGenerationCaveLiquidPlan(std::move(identity), std::move(delta));
  }

  WorldGenerationCaveLiquidPlanIdentity const& identity() const { return m_identity; }
  List<WorldGenerationCaveLiquidDeltaEntry> const& delta() const { return m_delta; }
  bool active() const { return m_active; }

  bool matches(WorldGenerationCaveLiquidPlanIdentity const& expectation) const {
    return active() && m_identity.matches(expectation);
  }

  // Owner revalidation only. Success does not install liquids or backgrounds.
  bool tryConsume(WorldGenerationCaveLiquidPlanIdentity const& liveExpectation) {
    if (!active() || !m_identity.matches(liveExpectation))
      return false;
    return true;
  }

  void discard() {
    m_active = false;
    m_delta.clear();
    m_identity = {};
  }

private:
  WorldGenerationCaveLiquidPlan(WorldGenerationCaveLiquidPlanIdentity identity,
      List<WorldGenerationCaveLiquidDeltaEntry> delta)
    : m_identity(std::move(identity)), m_delta(std::move(delta)), m_active(true) {}

  WorldGenerationCaveLiquidPlanIdentity m_identity;
  List<WorldGenerationCaveLiquidDeltaEntry> m_delta;
  bool m_active = false;
};

}
