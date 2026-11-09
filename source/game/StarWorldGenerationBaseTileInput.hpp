#pragma once

#include "StarWorldGenerationBaseTileLiquid.hpp"
#include "StarWorldTiles.hpp"

namespace Star {

struct WorldGenerationBaseTileLayerInput {
  MaterialId material = EmptyMaterialId;
  ModId mod = NoModId;
};

// Scalar blockInfo fields consumed by BaseTiles, plus its y coordinate. No
// template, cache, Root, Json, seed or mutable world reference is stored.
struct WorldGenerationBaseTileInput {
  static constexpr unsigned ModelVersion = 1;
  BiomeIndex blockBiome = NullBiomeIndex;
  BiomeIndex environmentBiome = NullBiomeIndex;
  bool biomeTransition = false;
  WorldGenerationBaseTileLayerInput foreground;
  WorldGenerationBaseTileLayerInput background;
  LiquidId oceanLiquid = EmptyLiquidId;
  int oceanLiquidLevel = 0;
  bool encloseLiquids = false;
  int y = 0;
};

// Sequencing adapter, NOT proof of resolver purity. Production binds the
// existing path's template/material database; a future pure producer must use
// immutable resolved values. Preserve partial writes before each throwing call,
// including the synchronous path; never prefetch/skip NoModId or initialized layers.
template <typename Resolver>
void applyWorldGenerationBaseTile(ServerTile& tile, WorldGenerationBaseTileInput const& input, Resolver const& resolver) {
  tile.blockBiomeIndex = input.blockBiome;
  tile.environmentBiomeIndex = input.environmentBiome;
  tile.biomeTransition = input.biomeTransition;
  if (tile.foreground == NullMaterialId) {
    tile.foreground = input.foreground.material;
    tile.foregroundColorVariant = DefaultMaterialColorVariant;
    tile.foregroundHueShift = resolver.materialHue(tile.blockBiomeIndex, tile.foreground);
    if (resolver.supportsMod(tile.foreground, input.foreground.mod)) {
      tile.foregroundMod = input.foreground.mod;
      tile.foregroundModHueShift = resolver.modHue(tile.blockBiomeIndex, tile.foregroundMod);
    }
  }
  if (tile.background == NullMaterialId) {
    tile.background = input.background.material;
    tile.backgroundColorVariant = DefaultMaterialColorVariant;
    tile.backgroundHueShift = resolver.materialHue(tile.blockBiomeIndex, tile.background);
    if (resolver.supportsMod(tile.background, input.background.mod)) {
      tile.backgroundMod = input.background.mod;
      tile.backgroundModHueShift = resolver.modHue(tile.blockBiomeIndex, tile.backgroundMod);
    }
  }
  applyWorldGenerationBaseTileLiquid(tile.liquid, {tile.foreground, tile.background,
      input.oceanLiquid, input.oceanLiquidLevel, input.encloseLiquids, input.y});
}

}
