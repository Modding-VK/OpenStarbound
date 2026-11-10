#pragma once

#include "StarWorldGeometry.hpp"

namespace Star {

// Inherited WorldTemplate mathematics. Geometry is real (including horizontal
// wrap); region order, stored bounds, comparisons and float operations remain
// unchanged. No Root, callbacks, RNG, mutable caches or ownership acquisition.
// RegionRange may borrow the legacy private regions OR an owned capture.
template <typename RegionRange>
pair<float, float> calculateWorldGenerationCustomTerrainWeighting(WorldGeometry const& geometry,
    RegionRange const& regions, float blendSize, float blendWeight, int x, int y) {
  float minimumDistance = highest<float>();
  float finalSolidWeight = 0.0f;
  float totalWeight = 0.0f;

  for (auto const& region : regions) {
    if (!geometry.rectContains(region.regionBounds.padded(blendSize), Vec2F(x, y)))
      continue;

    float distance = geometry.polyDistance(region.region, Vec2F(x, y));
    if (distance >= blendSize)
      continue;

    float weight = 1.0f - distance / blendSize;
    totalWeight += weight;
    if (!region.solid)
      weight *= -1.0f;

    finalSolidWeight += weight;
    minimumDistance = min(minimumDistance, distance);
  }

  if (minimumDistance > blendSize)
    return {0.0f, 0.0f};

  finalSolidWeight /= totalWeight;

  return {finalSolidWeight * blendWeight, 1.0f - minimumDistance / blendSize};
}

class WorldTemplate;

// Read-only, move-only owned values for the custom-terrain slice ONLY. Capture
// on the owner thread. No WorldTemplate/Root/Assets handles survive capture.
// Not the full generation context, a read lease, memory admission or permission
// to commit. templateRevision is an observation, NOT a complete world identity.
// Normal generation does not allocate this snapshot; do not adopt it in jobs
// until the full context's admission, lifetime and revalidation are implemented.
class WorldGenerationCustomTerrainContext {
public:
  WorldGenerationCustomTerrainContext(WorldGenerationCustomTerrainContext const&) = delete;
  WorldGenerationCustomTerrainContext& operator=(WorldGenerationCustomTerrainContext const&) = delete;
  WorldGenerationCustomTerrainContext(WorldGenerationCustomTerrainContext&&) = default;
  WorldGenerationCustomTerrainContext& operator=(WorldGenerationCustomTerrainContext&&) = default;

  pair<float, float> weighting(int x, int y) const {
    return calculateWorldGenerationCustomTerrainWeighting(m_geometry, m_regions, m_blendSize, m_blendWeight, x, y);
  }
  size_t regionCount() const { return m_regions.size(); }
  uint64_t templateRevision() const { return m_templateRevision; }

private:
  friend class WorldTemplate;
  struct Region {
    PolyF region;
    RectF regionBounds;
    bool solid;
  };
  WorldGenerationCustomTerrainContext(WorldGeometry geometry, List<Region> regions,
      float blendSize, float blendWeight, uint64_t revision)
    : m_geometry(std::move(geometry)), m_regions(std::move(regions)), m_blendSize(blendSize),
      m_blendWeight(blendWeight), m_templateRevision(revision) {}

  WorldGeometry m_geometry;
  List<Region> m_regions;
  float m_blendSize;
  float m_blendWeight;
  uint64_t m_templateRevision;
};

}
