#pragma once

#include <cstdint>

#include "StarList.hpp"
#include "StarMap.hpp"
#include "StarMaybe.hpp"
#include "StarString.hpp"
#include "StarVector.hpp"
#include "StarWorldGenerationPlacementQueue.hpp"

namespace Star {

// CPU-02 / MD-01 shadow slice: versioned prepared products and empty/candidate
// results without a tile halo or live-world install.

enum class WorldGenerationPreparedProductKind : uint8_t {
  Empty = 0,
  OrderedCandidates
};

struct WorldGenerationPreparedProductIdentity {
  Vec2I sector;
  uint64_t templateRevision = 0;
  uint64_t distributionRevision = 0;
  uint64_t placementEpoch = 0;

  bool operator==(WorldGenerationPreparedProductIdentity const& other) const;
};

struct WorldGenerationMicroDungeonCandidate {
  WorldGenerationPlacementId placementId = 0;
  String dungeonName;
  Vec2I position;
  uint64_t seed = 0;
};

struct WorldGenerationPreparedSectorProduct {
  WorldGenerationPreparedProductIdentity identity;
  WorldGenerationPreparedProductKind kind = WorldGenerationPreparedProductKind::Empty;
  List<WorldGenerationMicroDungeonCandidate> candidates;

  bool matches(WorldGenerationPreparedProductIdentity const& expected) const;
  // Preparing never mutates a persisted world level; this flag stays false here.
  bool mutatesPersistedLevel() const;
};

using WorldGenerationPreparedProductId = uint64_t;

class WorldGenerationPreparedSectorStore {
public:
  WorldGenerationPreparedProductId put(WorldGenerationPreparedSectorProduct product);
  Maybe<WorldGenerationPreparedSectorProduct> get(WorldGenerationPreparedProductId id) const;
  bool contains(WorldGenerationPreparedProductId id) const;
  bool erase(WorldGenerationPreparedProductId id);
  size_t size() const;

  // Share an existing product when identity and kind match; otherwise insert.
  WorldGenerationPreparedProductId shareOrPut(WorldGenerationPreparedSectorProduct product);

  // Drop products whose identity no longer matches the live revisions.
  size_t invalidateStale(WorldGenerationPreparedProductIdentity const& live);

  static WorldGenerationPreparedSectorProduct makeEmpty(WorldGenerationPreparedProductIdentity identity);
  static WorldGenerationPreparedSectorProduct makeOrderedCandidates(
      WorldGenerationPreparedProductIdentity identity,
      List<WorldGenerationMicroDungeonCandidate> candidates);

private:
  WorldGenerationPreparedProductId m_nextId = 1;
  Map<WorldGenerationPreparedProductId, WorldGenerationPreparedSectorProduct> m_products;
};

}
