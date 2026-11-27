#pragma once

#include "StarMaybe.hpp"

#include <cstddef>
#include <cstdint>

namespace Star {

// MOD-01: classify generation operations by effect. Diagnostic inventory for
// H0 foundations — not permission to publish, invoke Lua on workers, or
// normalize audited uninitialized defaults.
enum class WorldGenerationEffectKind : uint8_t {
  PureKernel = 0,
  PreparedDependency = 1,
  AuthoritativeCallback = 2,
  MultiRegionEffect = 3
};

// Who is asking to run a classified operation. Enforcement is diagnostic for
// H0: it refuses Worker paths that the catalog marks owner/script/live-only.
// Diagnostic mirrors Owner for contract tests without implying production
// publication authority.
enum class WorldGenerationEffectExecutionRole : uint8_t {
  Owner = 0,
  Worker = 1,
  Diagnostic = 2
};

enum class WorldGenerationEffectBoundaryDecision : uint8_t {
  Allow = 0,
  RefuseUnknown = 1,
  RefuseWorkerAuthoritative = 2,
  RefuseWorkerOwnerThread = 3,
  RefuseWorkerScript = 4,
  RefuseWorkerLiveMutation = 5,
  RefuseMultiRegion = 6
};

struct WorldGenerationEffectBoundary {
  static constexpr unsigned ModelVersion = 1;
  char const* id = nullptr;
  char const* surface = nullptr;
  WorldGenerationEffectKind kind = WorldGenerationEffectKind::PureKernel;
  bool mayInvokeScript = false;
  bool mutatesLiveWorld = false;
  bool requiresOwnerThread = false;
  // Inherited constructor/copy hazards recorded in H0 audits; do not "fix"
  // production defaults when this flag is set.
  bool readsUninitializedDefault = false;
  char const* notes = nullptr;
};

size_t worldGenerationEffectBoundaryCount();
WorldGenerationEffectBoundary const* worldGenerationEffectBoundaryAt(size_t index);
Maybe<WorldGenerationEffectBoundary> worldGenerationEffectBoundaryFind(char const* id);
// Count entries whose surface string equals the argument (exact match).
size_t worldGenerationEffectBoundaryCountForSurface(char const* surface);

// Catalog invariants for H0: unique ids, PureKernel never scripts/mutates live
// world, AuthoritativeCallback requires owner thread, no MultiRegionEffect yet.
bool worldGenerationEffectBoundaryCatalogValid();

// Enforce PureKernel vs AuthoritativeCallback (and related flags) for a role.
// Unknown ids and MultiRegionEffect always refuse. Owner/Diagnostic allow any
// catalogued non-multi-region entry; Worker allows PureKernel and only those
// PreparedDependency entries that need neither owner thread, scripts, nor live
// mutation. Never authorizes publication or Lua on workers by itself.
WorldGenerationEffectBoundaryDecision worldGenerationEffectBoundaryDecide(
    char const* id, WorldGenerationEffectExecutionRole role);
bool worldGenerationEffectBoundaryAllows(char const* id, WorldGenerationEffectExecutionRole role);
char const* worldGenerationEffectBoundaryDecisionName(WorldGenerationEffectBoundaryDecision decision);

}
