#pragma once

#include "StarGenerationVersion.hpp"
#include "StarWorldGenerationBaseTileLookupContext.hpp"

#include <memory>
#include <optional>

namespace Star {

enum class WorldGenerationLookupApplyStatus { Refused, Applied, Stale };

// Strong handles retain the selected sources, NOT Root/the world. Root's real
// factory binds its clock; generic source types let NoAssets exercise lifetime
// and races without claiming to run Root/assets/reload integration.
template <typename AssetsSource, typename MaterialsSource>
class WorldGenerationLookupSources {
public:
  static constexpr unsigned ModelVersion = 1;
  static WorldGenerationLookupSources capture(std::shared_ptr<GenerationVersionClock const> root, uint64_t rootVersion,
      std::shared_ptr<AssetsSource const> assets, std::shared_ptr<MaterialsSource const> materials) {
    if (!root || !rootVersion || !assets || !materials) return {};
    WorldGenerationLookupSources result;
    result.m_root = std::move(root);
    result.m_rootVersion = rootVersion;
    result.m_assets = std::move(assets);
    result.m_materials = std::move(materials);
    result.m_assetsVersion = result.m_assets->generationRevision();
    result.m_materialsVersion = result.m_materials->generationRevision();
    return result.valid() ? result : WorldGenerationLookupSources{};
  }
  bool valid() const {
    return m_root && m_rootVersion && m_root->current() == m_rootVersion
        && m_assets && m_materials && m_assetsVersion && m_materialsVersion
        && m_assets->generationRevision() == m_assetsVersion && m_materials->generationRevision() == m_materialsVersion
        && m_root->current() == m_rootVersion;
  }
  std::optional<WorldGenerationBaseTileLookupScope> scope(uint64_t worldEpoch, uint64_t templateRevision) const {
    if (!worldEpoch || !valid()) return {};
    return WorldGenerationBaseTileLookupScope{worldEpoch, templateRevision, m_assetsVersion, m_materialsVersion, m_rootVersion};
  }
  std::shared_ptr<AssetsSource const> const& assets() const { return m_assets; }
  std::shared_ptr<MaterialsSource const> const& materials() const { return m_materials; }

  // Refused guarantees no writes. Stale AFTER applying means private output
  // must be discarded, never fallback/retry on that partial buffer. Applied
  // is only a local observation: owner still revalidates sources/task/template/
  // tile revisions before commit. This is NOT a read lease or mutation lock.
  template <size_t Capacity>
  WorldGenerationLookupApplyStatus apply(WorldGenerationBaseTileLookupContext<Capacity> const& context,
      ServerTile& tile, WorldGenerationBaseTileInput const& input, uint64_t worldEpoch, uint64_t templateRevision) const {
    auto expected = scope(worldEpoch, templateRevision);
    if (!expected || !context.apply(tile, input, *expected)) return WorldGenerationLookupApplyStatus::Refused;
    return valid() ? WorldGenerationLookupApplyStatus::Applied : WorldGenerationLookupApplyStatus::Stale;
  }

private:
  std::shared_ptr<GenerationVersionClock const> m_root;
  std::shared_ptr<AssetsSource const> m_assets;
  std::shared_ptr<MaterialsSource const> m_materials;
  uint64_t m_rootVersion = 0;
  uint64_t m_assetsVersion = 0;
  uint64_t m_materialsVersion = 0;
};

}
