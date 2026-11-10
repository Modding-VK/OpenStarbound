#include "StarWorldTemplateTypes.hpp"

namespace Star {

// Existing mutation bodies relocated unchanged. Both normal generation and
// NoAssets tests use these same definitions; no fixture replacement symbols.
void WorldTemplate::addCustomTerrainRegion(PolyF poly) {
  m_customTerrainRegions.append({poly, poly.boundBox(), true});
  m_blockCache.clear();
  ++m_generationRevision;
}

void WorldTemplate::addCustomSpaceRegion(PolyF poly) {
  m_customTerrainRegions.append({poly, poly.boundBox(), false});
  m_blockCache.clear();
  ++m_generationRevision;
}

void WorldTemplate::clearCustomTerrains() {
  m_customTerrainRegions.clear();
  m_blockCache.clear();
  ++m_generationRevision;
}

WorldGenerationCustomTerrainContext WorldTemplate::captureCustomTerrain() const {
  List<WorldGenerationCustomTerrainContext::Region> regions;
  regions.reserve(m_customTerrainRegions.size());
  for (auto const& region : m_customTerrainRegions)
    regions.append({region.region, region.regionBounds, region.solid});
  return {m_geometry, std::move(regions), m_customTerrainBlendSize, m_customTerrainBlendWeight, m_generationRevision};
}

}
