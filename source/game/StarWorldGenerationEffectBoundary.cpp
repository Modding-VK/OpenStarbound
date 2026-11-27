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
