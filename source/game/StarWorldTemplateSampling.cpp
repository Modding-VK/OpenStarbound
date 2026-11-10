#include "StarWorldTemplateTypes.hpp"
#include "StarWorldGenerationBlockInfo.hpp"
#include "StarRoot.hpp"
#include "StarAssets.hpp"
#include "StarTerrainDatabase.hpp"
#include "StarRandom.hpp"

namespace Star {

// Inherited bodies relocated unchanged for real sampling tests and normal
// generation to use the same implementation, without linking unrelated factories.
WorldTemplate::WorldTemplate(Vec2U const& size) : WorldTemplate() {
  m_geometry = size;
}

WorldTemplate::WorldTemplate() {
  auto assets = Root::singleton().assets();
  m_templateConfig = Root::singleton().assets()->json("/world_template.config");
  m_customTerrainBlendSize = m_templateConfig.getFloat("customTerrainBlendSize");
  m_customTerrainBlendWeight = m_templateConfig.getFloat("customTerrainBlendWeight");

  m_blockCache.setMaxSize(m_templateConfig.getInt("blockCacheSize"));
  m_geometry = Vec2U(2048, 2048);
  m_seed = Random::randu64();
}

Vec2U WorldTemplate::size() const {
  return m_geometry.size();
}

WorldTemplate::BlockInfo WorldTemplate::blockInfo(int x, int y) const {
  return getBlockInfo(m_geometry.xwrap(x), y);
}

BiomeConstPtr WorldTemplate::biome(BiomeIndex biomeIndex) const {
  if (!m_layout)
    return {};
  if (biomeIndex == NullBiomeIndex)
    return {};
  return m_layout->getBiome(biomeIndex);
}

pair<float, float> WorldTemplate::customTerrainWeighting(int x, int y) const {
  auto assets = Root::singleton().assets();

  return calculateWorldGenerationCustomTerrainWeighting(m_geometry, m_customTerrainRegions,
      m_customTerrainBlendSize, m_customTerrainBlendWeight, x, y);
}

WorldTemplate::BlockInfo WorldTemplate::getBlockInfo(uint32_t x, uint32_t y) const {
  return m_blockCache.get(Vector<uint32_t, 2>(x, y), [this, x, y](Vector<uint32_t, 2>) {
      // Non-owning adapter on the current worker's WorldTemplate. This does not
      // publish a context or make mutable selector caches safe to share.
      struct Source {
        WorldTemplate const& world;
        bool hasLayout() const { return bool(world.m_layout); }
        auto getWeighting(int a, int b) const { return world.m_layout->getWeighting(a, b); }
        int blendNoiseOffset(uint32_t a, uint32_t b) const {
          if (auto const& noise = world.m_layout->blendNoise())
            return (int)noise->get(a, b);
          return 0;
        }
        Maybe<Vec2I> blockNoisePosition(uint32_t a, uint32_t b) const {
          if (auto const& noise = world.m_layout->blockNoise())
            return noise->apply(Vec2I(a, b), world.m_geometry.size());
          return {};
        }
        float configFloat(char const* key, float fallback) const { return world.m_templateConfig.getFloat(key, fallback); }
        float terrainSelectorValue(TerrainSelectorIndex index, int a, int b) const {
          return world.m_layout->getTerrainSelector(index)->get(a, b);
        }
        pair<float, float> customTerrainWeighting(int a, int b) const { return world.customTerrainWeighting(a, b); }
        BiomeConstPtr biome(BiomeIndex index) const { return world.biome(index); }
      } source{*this};
      return calculateWorldGenerationBlockInfo(source, x, y);
    });
}

}
