#pragma once

#include "StarLiquidTypes.hpp"
#include "StarBiomePlacementTypes.hpp"

#include <utility>

namespace Star {

// Reusing capacity can suppress copies during vector growth. Hash-set copies
// can change iteration/static choice, so MicroDungeonNames must retain fresh
// output storage and the inherited growth/copy history. Non-micro candidates
// are discarded by the planner after sorting by priority/queued ID only.
template <typename Potential, typename Output>
void resetBiomeItemSelectionOutput(Potential const& potential, Output& output) {
  output.clear();
  if (!output.capacity()) return;
  for (auto list : {&potential.surfaceBiomeItems, &potential.caveSurfaceBiomeItems,
      &potential.caveCeilingBiomeItems, &potential.caveBackgroundBiomeItems, &potential.oceanItems}) {
    for (auto const& placement : *list) {
      if (placement.item.template is<MicroDungeonNames>()) {
        output = Output{};
        return;
      }
    }
  }
}

// Value-only filtering extracted from WorldTemplate::validBiomeItems. Keep
// list order and element moves unchanged; the caller owns geometry/cache reads
// and supplies a non-aliasing output, which may retain capacity between calls.
template <typename Block, typename Potential, typename Output>
void appendSelectedBiomeItems(int y, Block const& block, Block const& blockAbove,
    Block const& blockBelow, Potential& potential, Output& output) {
  if (block.biomeTransition) return;
  if (!blockBelow.biomeTransition && blockBelow.terrain && !block.terrain && !blockBelow.foregroundCave)
    output.appendAll(std::move(potential.surfaceBiomeItems));

  if (!blockBelow.biomeTransition && blockBelow.terrain && block.terrain && !blockBelow.foregroundCave && block.foregroundCave)
    output.appendAll(std::move(potential.caveSurfaceBiomeItems));

  if (!blockAbove.biomeTransition && blockAbove.terrain && block.terrain && !blockAbove.foregroundCave && block.foregroundCave)
    output.appendAll(std::move(potential.caveCeilingBiomeItems));

  if (block.terrain && block.foregroundCave && !block.backgroundCave)
    output.appendAll(std::move(potential.caveBackgroundBiomeItems));

  if (block.oceanLiquid != EmptyLiquidId && y == block.oceanLiquidLevel)
    output.appendAll(std::move(potential.oceanItems));
}

}
