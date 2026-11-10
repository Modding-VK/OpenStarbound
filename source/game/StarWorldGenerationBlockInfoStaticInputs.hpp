#pragma once

#include "StarWorldGenerationCustomTerrain.hpp"
#include "StarWorldGenerationMemoryEstimate.hpp"

namespace Star {

class WorldTemplate;

// Owned static subset ONLY: immutable Json root + custom-terrain values. No
// layout/selectors/materials, Root, world authority or complete task identity.
// Capture on the template's owner thread; do not move/destroy during reads.
// Normal jobs do not use this prototype. Json accounting is the existing
// bounded model, NOT whole-context admission (polygons/metadata are excluded).
class WorldGenerationBlockInfoStaticInputs {
public:
  WorldGenerationBlockInfoStaticInputs(WorldGenerationBlockInfoStaticInputs const&) = delete;
  WorldGenerationBlockInfoStaticInputs& operator=(WorldGenerationBlockInfoStaticInputs const&) = delete;
  WorldGenerationBlockInfoStaticInputs(WorldGenerationBlockInfoStaticInputs&&) = default;
  WorldGenerationBlockInfoStaticInputs& operator=(WorldGenerationBlockInfoStaticInputs&&) = default;

  // Keep getFloat here, not at capture: wrong types/root must throw only when
  // the kernel reaches this query; missing/null keys keep the caller fallback.
  float configFloat(char const* key, float fallback) const {
    return m_configuration->value.getFloat(key, fallback);
  }
  pair<float, float> customTerrainWeighting(int x, int y) const { return m_customTerrain.weighting(x, y); }
  uint64_t templateRevision() const { return m_customTerrain.templateRevision(); }
  size_t customTerrainRegionCount() const { return m_customTerrain.regionCount(); }
  bool jsonStorageMeasured() const { return m_configuration->measured; }
  bool jsonStorageModelComplete() const { return m_configuration->measured && m_configuration->complete; }

private:
  friend class WorldTemplate;
  WorldGenerationBlockInfoStaticInputs(WorldGenerationJsonSnapshotPtr configuration,
      WorldGenerationCustomTerrainContext customTerrain)
    : m_configuration(std::move(configuration)), m_customTerrain(std::move(customTerrain)) {}

  // Shared snapshot destroys its Json handles BEFORE releasing its accounting
  // tokens. Sharing immutable Json owners does not charge duplicate storage.
  WorldGenerationJsonSnapshotPtr m_configuration;
  WorldGenerationCustomTerrainContext m_customTerrain;
};

}
