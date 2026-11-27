#pragma once

#include <cstdint>

#include "StarList.hpp"
#include "StarMap.hpp"
#include "StarMaybe.hpp"
#include "StarString.hpp"
#include "StarVector.hpp"
#include "StarWorldGenerationBaseTiles.hpp"
#include "StarWorldGenerationPlacementQueue.hpp"

namespace Star {

// CPU-02 / MD-01 shadow slice: versioned prepared products, payload views, and
// a WorldStorage handoff path that never installs into the live world.

using WorldGenerationPreparedProductId = uint64_t;

enum class WorldGenerationPreparedProductKind : uint8_t {
  Empty = 0,
  OrderedCandidates,
  BaseTilesPrivate,
  TerrainFields,
  StepDelta
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

struct WorldGenerationPreparedBaseTilesRef {
  WorldGenerationBaseTilesIdentity ticket;
  size_t retainedBytes = 0;
  // Shadow products never authorize install into WorldStorage by default.
  bool installAuthorized = false;
};

// Opaque immutable payload descriptor for H2 decode/promotion. Not a live
// pointer into WorldStorage or a worker buffer.
struct WorldGenerationPreparedPayloadView {
  WorldGenerationPreparedProductId productId = 0;
  WorldGenerationPreparedProductKind kind = WorldGenerationPreparedProductKind::Empty;
  WorldGenerationPreparedProductIdentity identity;
  size_t payloadBytes = 0;
  uint64_t contentToken = 0;
  bool installAuthorized = false;
};

// Logical WorldStorage path key. Sector + epoch + product id; no filesystem
// path and no mutation of WorldStorage.
struct WorldGenerationPreparedWorldStoragePath {
  uint64_t worldEpoch = 0;
  Vec2I sector;
  WorldGenerationPreparedProductId productId = 0;
  WorldGenerationPreparedProductKind kind = WorldGenerationPreparedProductKind::Empty;

  bool operator==(WorldGenerationPreparedWorldStoragePath const& other) const;
};

enum class WorldGenerationPreparedHandoffOutcome : uint8_t {
  Ready = 0,
  MissingProduct,
  StaleIdentity,
  NotAuthorized,
  EmptyPayload
};

struct WorldGenerationPreparedHandoff {
  WorldGenerationPreparedHandoffOutcome outcome = WorldGenerationPreparedHandoffOutcome::MissingProduct;
  WorldGenerationPreparedWorldStoragePath path;
  WorldGenerationPreparedPayloadView payload;
  // True only when Ready; still does not write WorldStorage in this slice.
  bool writesWorldStorage = false;
};

struct WorldGenerationPreparedSectorProduct {
  WorldGenerationPreparedProductIdentity identity;
  WorldGenerationPreparedProductKind kind = WorldGenerationPreparedProductKind::Empty;
  List<WorldGenerationMicroDungeonCandidate> candidates;
  Maybe<WorldGenerationPreparedBaseTilesRef> baseTiles;
  // TerrainFields / StepDelta carry only an opaque content token + byte size
  // until their typed payloads land in a later slice.
  uint64_t opaqueContentToken = 0;
  size_t opaquePayloadBytes = 0;

  bool matches(WorldGenerationPreparedProductIdentity const& expected) const;
  // Preparing never mutates a persisted world level in this shadow store.
  bool mutatesPersistedLevel() const;
  bool canInstall() const;
};

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

  // Payload / WorldStorage path APIs for H2. Probe never installs.
  Maybe<WorldGenerationPreparedPayloadView> payloadView(WorldGenerationPreparedProductId id) const;
  WorldGenerationPreparedWorldStoragePath worldStoragePath(uint64_t worldEpoch,
      WorldGenerationPreparedProductId id) const;
  WorldGenerationPreparedHandoff probeHandoff(uint64_t worldEpoch, WorldGenerationPreparedProductId id,
      WorldGenerationPreparedProductIdentity const& liveIdentity) const;

  static WorldGenerationPreparedSectorProduct makeEmpty(WorldGenerationPreparedProductIdentity identity);
  static WorldGenerationPreparedSectorProduct makeOrderedCandidates(
      WorldGenerationPreparedProductIdentity identity,
      List<WorldGenerationMicroDungeonCandidate> candidates);
  static WorldGenerationPreparedSectorProduct makeBaseTilesPrivate(
      WorldGenerationPreparedProductIdentity identity,
      WorldGenerationBaseTilesIdentity ticket,
      size_t retainedBytes);
  static WorldGenerationPreparedSectorProduct makeTerrainFields(
      WorldGenerationPreparedProductIdentity identity, uint64_t contentToken, size_t payloadBytes);
  static WorldGenerationPreparedSectorProduct makeStepDelta(
      WorldGenerationPreparedProductIdentity identity, uint64_t contentToken, size_t payloadBytes);

private:
  static uint64_t hashIdentity(WorldGenerationPreparedProductIdentity const& identity);
  static WorldGenerationPreparedPayloadView buildPayloadView(WorldGenerationPreparedProductId id,
      WorldGenerationPreparedSectorProduct const& product);

  WorldGenerationPreparedProductId m_nextId = 1;
  Map<WorldGenerationPreparedProductId, WorldGenerationPreparedSectorProduct> m_products;
};

}
