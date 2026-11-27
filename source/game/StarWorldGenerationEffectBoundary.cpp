#include "StarWorldGenerationEffectBoundary.hpp"

#include <cstring>

namespace Star {
namespace {

constexpr WorldGenerationEffectBoundary kCatalog[] = {
    {"base-tiles-apply", "BaseTiles", WorldGenerationEffectKind::PureKernel, false, false, false, false,
        "applyWorldGenerationBaseTile with materials already resolved; no WorldServer/Lua"},
    {"base-tiles-compare", "BaseTiles", WorldGenerationEffectKind::PureKernel, false, false, false, false,
        "Diagnostic field compare; never memcmp padding"},
    {"composite-context-capture", "CompositeContext", WorldGenerationEffectKind::PreparedDependency, false, false, true,
        false, "Owner-thread capture of selectors/static inputs/BaseTiles identity"},
    {"sector-shadow-admit", "SectorShadow", WorldGenerationEffectKind::PreparedDependency, false, false, true, false,
        "Ledger admit of private BaseTiles array; discard without publication"},
    {"sector-shadow-discard", "SectorShadow", WorldGenerationEffectKind::PreparedDependency, false, false, true, false,
        "Owner discards private array; does not write live tiles"},
    {"candidate-sequence-compare", "VAL-02", WorldGenerationEffectKind::PureKernel, false, false, false, false,
        "Diagnostic placement id/position/priority/itemKind/payload compare"},
    {"biome-item-compare", "VAL-02", WorldGenerationEffectKind::PureKernel, false, false, false, false,
        "SupportedPayload BiomeItem compare without Root/Assets"},
    {"perlin-sample-initialized", "Perlin", WorldGenerationEffectKind::PureKernel, false, false, false, false,
        "Initialized Perlin tables are const for sampling; no mutable cache"},
    {"perlin-uninitialized-copy", "Perlin", WorldGenerationEffectKind::PreparedDependency, false, false, false, true,
        "Copy of Uninitialized preserves metadata; do not sample until initialized"},
    {"world-tile-assign-biome-transition", "WorldTile", WorldGenerationEffectKind::PreparedDependency, false, false,
        false, true, "operator= omits biomeTransition; BaseTiles overwrites on valid tiles"},
    {"sky-parameters-horizon-clouds", "SkyParameters", WorldGenerationEffectKind::PreparedDependency, false, false, false,
        true, "Default LookupDefaults ctor omits horizonClouds; fixtures must assign"},
    {"terrain-selector-base-height", "TerrainSelectorParameters", WorldGenerationEffectKind::PreparedDependency, false,
        false, false, true, "Default leaves baseHeight unset until owner assignment"},
    {"assets-hot-reload", "Assets", WorldGenerationEffectKind::AuthoritativeCallback, false, false, true, false,
        "Owner/Root lifecycle invalidates lookup bindings; not a worker kernel"},
    {"root-load-mods", "Root", WorldGenerationEffectKind::AuthoritativeCallback, true, false, true, false,
        "May run patch scripts through audited Assets API; never on generation workers"},
    {"placement-queue-take", "PlacementQueue", WorldGenerationEffectKind::AuthoritativeCallback, false, false, true,
        false, "Owner retires IDs before promise fulfill; workers receive IDs only"},
    {"worker-pool-handoff", "WorkerPool", WorldGenerationEffectKind::PreparedDependency, false, false, true, false,
        "Stale/Cancelled/Failed retire private outputs; no live-world publish"},
    {"block-info-kernel", "BlockInfo", WorldGenerationEffectKind::PureKernel, false, false, false, false,
        "Separated blockInfo math; customTerrainWeighting still may touch Assets on normal path"},
    {"custom-terrain-capture", "CustomTerrain", WorldGenerationEffectKind::PreparedDependency, false, false, false, false,
        "Root-free geometry capture; normal adapter may still consult Assets"},
    {"base-tile-liquid-subkernel", "BaseTiles", WorldGenerationEffectKind::PureKernel, false, false, false, false,
        "Scalar liquid decision with materials resolved; Keep preserves prior liquid"},
    {"base-tile-lookup-catalog", "BaseTiles", WorldGenerationEffectKind::PreparedDependency, false, false, true, false,
        "Sealed response catalog; miss does not write; owner chooses legacy path"},
    {"selector-context-capture", "SelectorContext", WorldGenerationEffectKind::PreparedDependency, false, false, true,
        false, "Admit POD selector parameters before RNG; invalid identity refuses without seed"},
    {"region-selector-snapshot", "SelectorContext", WorldGenerationEffectKind::PreparedDependency, false, false, false,
        false, "Private region selector trees; not shared CacheSelector LRU across workers"},
    {"memory-ledger-admit", "Memory", WorldGenerationEffectKind::PreparedDependency, false, false, true, false,
        "Opt-in ledger tryReserve/admit; default game singleton remains off"},
    {"memory-ledger-refuse", "Memory", WorldGenerationEffectKind::PreparedDependency, false, false, true, false,
        "Refuse leaves no live resource; no live-world side effect"},
    {"memory-pressure-sample", "Memory", WorldGenerationEffectKind::PreparedDependency, false, false, true, false,
        "Owner-local pressure metrics; not eviction/cancel of live buffers"},
    {"distribution-bundle-share", "Distribution", WorldGenerationEffectKind::PreparedDependency, false, false, true, false,
        "Const shared snapshots after one equivalent copy; queue/promises stay on owner"},
    {"planning-scratch-reuse", "Planning", WorldGenerationEffectKind::PreparedDependency, false, false, false, false,
        "Scratch reuse only without MicroDungeonNames inputs; preserves copy history"},
    {"planning-name-share", "Planning", WorldGenerationEffectKind::PureKernel, false, false, false, false,
        "Share chosen dungeon name only after selection; diagnostic compare of order/IDs"},
    {"json-object-capture", "Json", WorldGenerationEffectKind::PreparedDependency, false, false, false, false,
        "Immutable JsonObject buckets/keys; omission when over capture limit"},
    {"json-variant-enqueue", "Json", WorldGenerationEffectKind::PreparedDependency, false, false, false, false,
        "Enumerate variant Json refs without allItems/toJson or BiomeItem copy"},
    {"payload-buffer-reserve", "Payload", WorldGenerationEffectKind::PreparedDependency, false, false, false, false,
        "Reserve candidate/plan buffers before allocate; growth keeps both charged"},
    {"lookup-source-binding", "Lookup", WorldGenerationEffectKind::PreparedDependency, false, false, true, false,
        "Root/Assets clocks + material versions; refused binding writes nothing"},
    {"lookup-source-stale", "Lookup", WorldGenerationEffectKind::PreparedDependency, false, false, true, false,
        "Stale after apply requires discard of private output; not silent fallback"},
    {"task-preparation-abandon", "Tasks", WorldGenerationEffectKind::PreparedDependency, false, false, true, false,
        "Abandon before handoff discards filled private shadow; no publish"},
    {"task-producer-failure", "Tasks", WorldGenerationEffectKind::PreparedDependency, false, false, true, false,
        "Failed producer retires private output; owner observes Failed outcome"},
    {"task-invalidate", "Tasks", WorldGenerationEffectKind::AuthoritativeCallback, false, false, true, false,
        "Owner invalidate yields Cancelled; workers must not publish cancelled work"},
    {"world-region-liquids-default", "WorldRegion", WorldGenerationEffectKind::PreparedDependency, false, false, false,
        true, "WorldRegion() leaves regionLiquids unset; contract tests may omit the object"},
    {"immutable-array-view", "Memory", WorldGenerationEffectKind::PureKernel, false, false, false, false,
        "Const view over admitted backing; no allocator/mutation from readers"},
    {"sector-shadow-matches", "SectorShadow", WorldGenerationEffectKind::PureKernel, false, false, false, false,
        "Identity-only matches check; not commit authorization"},
    {"streaming-pass-harness", "VAL-01", WorldGenerationEffectKind::PreparedDependency, false, false, false, false,
        "Synthetic pass harness with virtual clock; not real streaming benchmark"},
    {"terrestrial-builder-capture", "Terrestrial", WorldGenerationEffectKind::PreparedDependency, false, false, false,
        false, "Minimal-asset terrestrial layout vs pre-snapshot reference; Root opt-in"},
    {"terrestrial-builder-failure", "Terrestrial", WorldGenerationEffectKind::PreparedDependency, false, false, false,
        false, "Failure path leaves database usable; no live publish"},
    {"rng-explicit-sequence", "RNG", WorldGenerationEffectKind::PureKernel, false, false, false, false,
        "Explicit seeds/sequences verified; selection/order unchanged"},
    {"rng-automatic-seed-overflow", "RNG", WorldGenerationEffectKind::PreparedDependency, false, false, false, false,
        "Automatic seed multiply made unsigned modular; not a license to change selection"},
    {"microdungeon-plan-candidates", "MicroDungeon", WorldGenerationEffectKind::PreparedDependency, false, false, true,
        false, "Owner builds candidates/plans; workers get immutable inputs only"},
    {"microdungeon-plan-apply", "MicroDungeon", WorldGenerationEffectKind::AuthoritativeCallback, false, true, true,
        false, "Factory apply on owner before tile post-process; promise once"},
    {"base-tile-observation-replay", "BaseTiles", WorldGenerationEffectKind::PureKernel, false, false, false, false,
        "Diagnostic observation replay; does not call live resolver"},
    {"base-tile-input-adapter", "BaseTiles", WorldGenerationEffectKind::PureKernel, false, false, false, false,
        "Preserves query/write order and partial state across exceptions"},
    {"composite-vs-region-sample", "CompositeContext", WorldGenerationEffectKind::PureKernel, false, false, false, false,
        "Diagnostic equality of composite samples vs real region builder"},
    {"sector-shadow-template-stale", "SectorShadow", WorldGenerationEffectKind::PreparedDependency, false, false, true,
        false, "matches fails after templateRevision bump; payload until owner discard"},
};

bool sameId(char const* a, char const* b) {
  return a && b && std::strcmp(a, b) == 0;
}

}

size_t worldGenerationEffectBoundaryCount() {
  return sizeof(kCatalog) / sizeof(kCatalog[0]);
}

WorldGenerationEffectBoundary const* worldGenerationEffectBoundaryAt(size_t index) {
  if (index >= worldGenerationEffectBoundaryCount())
    return nullptr;
  return &kCatalog[index];
}

Maybe<WorldGenerationEffectBoundary> worldGenerationEffectBoundaryFind(char const* id) {
  if (!id)
    return {};
  for (size_t i = 0; i != worldGenerationEffectBoundaryCount(); ++i) {
    if (sameId(kCatalog[i].id, id))
      return kCatalog[i];
  }
  return {};
}

size_t worldGenerationEffectBoundaryCountForSurface(char const* surface) {
  if (!surface)
    return 0;
  size_t count = 0;
  for (size_t i = 0; i != worldGenerationEffectBoundaryCount(); ++i) {
    if (sameId(kCatalog[i].surface, surface))
      ++count;
  }
  return count;
}

bool worldGenerationEffectBoundaryCatalogValid() {
  size_t const count = worldGenerationEffectBoundaryCount();
  if (count == 0)
    return false;
  for (size_t i = 0; i != count; ++i) {
    auto const& entry = kCatalog[i];
    if (!entry.id || !entry.surface || !entry.notes)
      return false;
    for (size_t j = i + 1; j != count; ++j) {
      if (sameId(entry.id, kCatalog[j].id))
        return false;
    }
    if (entry.kind == WorldGenerationEffectKind::PureKernel) {
      if (entry.mayInvokeScript || entry.mutatesLiveWorld || entry.requiresOwnerThread)
        return false;
    }
    if (entry.kind == WorldGenerationEffectKind::AuthoritativeCallback) {
      if (!entry.requiresOwnerThread)
        return false;
    }
    if (entry.kind == WorldGenerationEffectKind::MultiRegionEffect)
      return false; // H0 catalog must not claim multi-region authority yet
    if (entry.readsUninitializedDefault && entry.kind == WorldGenerationEffectKind::PureKernel)
      return false;
  }
  return true;
}

}
