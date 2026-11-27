#pragma once

#include "StarWorldGenerationBaseTiles.hpp"
#include "StarWorldGenerationBlockInfoStaticInputs.hpp"
#include "StarWorldGenerationSelectorContext.hpp"
#include "StarWorldTemplateTypes.hpp"

#include <optional>
#include <utility>

namespace Star {

// Owner-thread composition only: admitted selector parameters + static
// block-info inputs + a BaseTiles product identity. Not a layout/biome/asset
// lease, not array allocation, not job adoption or commit authority. Callers
// must align templateRevision across selector identity, BaseTiles identity and
// the live WorldTemplate before capture. Polygons stay inside the custom-
// terrain value copy; Json accounting remains the bounded model.
class WorldGenerationCompositeContext {
public:
  WorldGenerationCompositeContext(WorldGenerationCompositeContext const&) = delete;
  WorldGenerationCompositeContext& operator=(WorldGenerationCompositeContext const&) = delete;
  WorldGenerationCompositeContext(WorldGenerationCompositeContext&&) = default;
  WorldGenerationCompositeContext& operator=(WorldGenerationCompositeContext&&) = default;

  // Validate identities and live revision first. Selector admission/RNG runs
  // only after those checks succeed; refused selector admission consumes no
  // seed. Static Json/custom-terrain capture follows on the same ledger.
  static std::optional<WorldGenerationCompositeContext> tryCapture(
      WorldGenerationMemoryLedger& ledger,
      WorldGenerationSelectorContextIdentity selectorIdentity,
      WorldGenerationBaseTilesIdentity baseTilesIdentity,
      WorldTemplate const& source,
      bool measureJson = true,
      size_t maximumJsonValues = 32768) {
    if (!selectorIdentity.valid() || !baseTilesIdentity.valid())
      return std::nullopt;
    if (selectorIdentity.templateRevision != baseTilesIdentity.templateRevision)
      return std::nullopt;
    if (selectorIdentity.worldEpoch != baseTilesIdentity.task.worldEpoch)
      return std::nullopt;
    if (selectorIdentity.worldWidth != baseTilesIdentity.worldSize[0])
      return std::nullopt;
    if (source.generationRevision() != selectorIdentity.templateRevision)
      return std::nullopt;

    auto selectors = WorldGenerationRegionSelectorContext::tryCapture(ledger, std::move(selectorIdentity));
    if (!selectors)
      return std::nullopt;

    auto staticInputs = source.captureBlockInfoStaticInputs(ledger, measureJson, maximumJsonValues);
    if (staticInputs.templateRevision() != selectors->identity().templateRevision)
      return std::nullopt;

    return WorldGenerationCompositeContext(std::move(*selectors), std::move(staticInputs),
        std::move(baseTilesIdentity));
  }

  WorldGenerationRegionSelectorContext const& selectors() const { return m_selectors; }
  WorldGenerationBlockInfoStaticInputs const& staticInputs() const { return m_staticInputs; }
  WorldGenerationBaseTilesIdentity const& baseTilesIdentity() const { return m_baseTiles; }

  bool matches(WorldGenerationSelectorContextIdentity const& selectorIdentity,
      WorldGenerationBaseTilesIdentity const& baseTilesIdentity) const {
    return m_selectors.matches(selectorIdentity) && baseTilesIdentity.matches(m_baseTiles);
  }

  float configFloat(char const* key, float fallback) const {
    return m_staticInputs.configFloat(key, fallback);
  }
  pair<float, float> customTerrainWeighting(int x, int y) const {
    return m_staticInputs.customTerrainWeighting(x, y);
  }
  TerrainSelectorParameters terrain() const { return m_selectors.terrain(); }
  TerrainSelectorParameters foregroundCave() const { return m_selectors.foregroundCave(); }
  TerrainSelectorParameters backgroundCave() const { return m_selectors.backgroundCave(); }

  // PureKernel consumption: materialize private selector PODs + BaseTiles
  // identity from an already-admitted composite. Does not re-run capture, RNG,
  // Assets, Root, or live-world mutation. Json/custom-terrain reads stay on the
  // composite (PreparedDependency) — this reconstructs only POD pure inputs.
  struct PureInputs {
    static constexpr unsigned ModelVersion = 1;
    TerrainSelectorParameters terrain;
    TerrainSelectorParameters foregroundCave;
    TerrainSelectorParameters backgroundCave;
    WorldGenerationBaseTilesIdentity baseTiles;
  };

  PureInputs reconstructPureInputs() const {
    return PureInputs{terrain(), foregroundCave(), backgroundCave(), m_baseTiles};
  }

private:
  WorldGenerationCompositeContext(WorldGenerationRegionSelectorContext selectors,
      WorldGenerationBlockInfoStaticInputs staticInputs,
      WorldGenerationBaseTilesIdentity baseTiles)
    : m_selectors(std::move(selectors)), m_staticInputs(std::move(staticInputs)),
      m_baseTiles(std::move(baseTiles)) {}

  // Selectors release metadata after static Json/custom-terrain tokens.
  WorldGenerationRegionSelectorContext m_selectors;
  WorldGenerationBlockInfoStaticInputs m_staticInputs;
  WorldGenerationBaseTilesIdentity m_baseTiles;
};

}
