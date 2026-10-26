#pragma once

#include "StarPlatformerPathSnapshot.hpp"
#include "StarWorkerPool.hpp"

namespace Star {

class WorldServer;

struct NpcShadowPathIdentity {
  uint64_t worldEpoch = 0;
  EntityId npcId = NullEntityId;
  uint64_t incarnation = 0;
  uint64_t sequence = 0;
  uint64_t captureStep = 0;
};

enum class NpcShadowPathOutcome { Route, PartialRoute, NoRoute, MissingCoverage, SearchLimit, Cancelled, Error };

struct NpcShadowPathReport {
  NpcShadowPathIdentity identity;
  RectI region;
  List<pair<Vec2S, Maybe<uint64_t>>> sectorRevisions;
  NpcShadowPathOutcome reference = NpcShadowPathOutcome::Error;
  NpcShadowPathOutcome worker = NpcShadowPathOutcome::Error;
  bool matches = false;
  size_t capturedTiles = 0;
  size_t missingTiles = 0;
  size_t capturedBlocks = 0;
  double captureSeconds = 0;
  double referenceSeconds = 0;
  double queueSeconds = 0;
  double workerSeconds = 0;
};

struct NpcShadowPathSubmission {
  enum class Status { Accepted, Disabled, Saturated, MissingCoverage, CaptureUnavailable };
  Status status = Status::Disabled;
  Maybe<WorkerPoolPromise<NpcShadowPathReport>> promise;
};

// Diagnostics only. The worker receives a snapshot and value parameters, never
// World, Npc, Lua, or a pointer into their mutable state.
class NpcPathShadow {
public:
  static bool enabled();
  static NpcShadowPathSubmission submit(WorldServer* world, NpcShadowPathIdentity identity,
      Vec2F from, Vec2F to, ActorMovementParameters movement,
      PlatformerAStar::Parameters search, weak_ptr<atomic<bool>> alive);
};

}
