#include "StarWorldLayout.hpp"
#include "StarWorldGeometry.hpp"

namespace Star {

// Inherited bodies relocated unchanged for real sampling tests and normal
// generation to use the same implementation, without linking unrelated factories.
WorldRegion::WorldRegion()
  : terrainSelectorIndex(NullTerrainSelectorIndex),
    foregroundCaveSelectorIndex(NullTerrainSelectorIndex),
    backgroundCaveSelectorIndex(NullTerrainSelectorIndex),
    blockBiomeIndex(NullBiomeIndex),
    environmentBiomeIndex(NullBiomeIndex) {}

Maybe<WorldLayout::BlockNoise> const& WorldLayout::blockNoise() const {
  return m_blockNoise;
}

Maybe<PerlinF> const& WorldLayout::blendNoise() const {
  return m_blendNoise;
}

List<WorldLayout::RegionWeighting> WorldLayout::getWeighting(int x, int y) const {
  List<RegionWeighting> weighting;
  WorldGeometry geometry(m_worldSize);

  auto cellWeighting = [&](WorldLayer const& layer, size_t cellIndex, int x) -> float {
    int xMin = 0;
    if (cellIndex > 0)
      xMin = layer.boundaries[cellIndex - 1];

    int xMax = m_worldSize[0];
    if (cellIndex < layer.boundaries.size())
      xMax = layer.boundaries[cellIndex];

    if (x > (xMin + xMax) / 2.0f)
      return clamp<float>(0.5f - (x - xMax) / m_regionBlending, 0.0f, 1.0f);
    else
      return clamp<float>(0.5f - (xMin - x) / m_regionBlending, 0.0f, 1.0f);
  };

  auto addLayerWeighting = [&](WorldLayer const& layer, int x, float weightFactor) {
    if (layer.cells.empty())
      return;

    size_t innerCellIndex;
    int innerCellXValue;
    tie(innerCellIndex, innerCellXValue) = findContainingCell(layer, x);
    float innerCellWeight = cellWeighting(layer, innerCellIndex, innerCellXValue);

    size_t leftCellIndex;
    int leftCellXValue;
    tie(leftCellIndex, leftCellXValue) = leftCell(layer, innerCellIndex, innerCellXValue);
    float leftCellWeight = cellWeighting(layer, leftCellIndex, leftCellXValue);

    size_t rightCellIndex;
    int rightCellXValue;
    tie(rightCellIndex, rightCellXValue) = rightCell(layer, innerCellIndex, innerCellXValue);
    float rightCellWeight = cellWeighting(layer, rightCellIndex, rightCellXValue);

    float totalWeight = innerCellWeight + leftCellWeight + rightCellWeight;
    if (totalWeight <= 0.0f)
      return;

    innerCellWeight *= weightFactor / totalWeight;
    leftCellWeight *= weightFactor / totalWeight;
    rightCellWeight *= weightFactor / totalWeight;

    if (innerCellWeight > 0.0f)
      weighting.append(RegionWeighting{innerCellWeight, innerCellXValue, layer.cells[innerCellIndex].get()});

    if (leftCellWeight > 0.0f)
      weighting.append(RegionWeighting{leftCellWeight, leftCellXValue, layer.cells[leftCellIndex].get()});

    if (rightCellWeight > 0.0f)
      weighting.append(RegionWeighting{rightCellWeight, rightCellXValue, layer.cells[rightCellIndex].get()});
  };

  auto yi = std::lower_bound(m_layers.begin(), m_layers.end(), y, [](WorldLayer const& layer, int y) {
      return layer.yStart < y;
    });

  if (yi == m_layers.end() || yi->yStart != y) {
    if (yi == m_layers.begin())
      return {};
    else
      --yi;
  }

  if (y - yi->yStart < (m_regionBlending / 2)) {
    if (yi == m_layers.begin()) {
      addLayerWeighting(*yi, x, 1.0f);
    } else {
      auto ypi = yi;
      --ypi;

      float yWeight = 0.5f + (y - yi->yStart) / m_regionBlending;
      addLayerWeighting(*yi, x, yWeight);
      addLayerWeighting(*ypi, x, 1.0f - yWeight);
    }
  } else {
    auto yni = yi;
    ++yni;
    if (yni == m_layers.end()) {
      addLayerWeighting(*yi, x, 1.0f);
    } else if (y <= yni->yStart - (m_regionBlending / 2)) {
      addLayerWeighting(*yi, x, 1.0f);
    } else {
      float yWeight = 0.5f - (yni->yStart - y) / m_regionBlending;
      addLayerWeighting(*yi, x, 1.0f - yWeight);
      addLayerWeighting(*yni, x, yWeight);
    }
  }

  // Need to return weighting in order of greatest to least
  sort(weighting, [](RegionWeighting const& lhs, RegionWeighting const& rhs) {
      return lhs.weight > rhs.weight;
    });

  return weighting;
}

WorldLayout::WorldLayer::WorldLayer() : yStart(0) {}

pair<size_t, int> WorldLayout::findContainingCell(WorldLayer const& layer, int x) const {
  x = WorldGeometry(m_worldSize).xwrap(x);
  auto xi = std::lower_bound(layer.boundaries.begin(), layer.boundaries.end(), x);
  return {xi - layer.boundaries.begin(), x};
}

pair<size_t, int> WorldLayout::leftCell(WorldLayer const& layer, size_t cellIndex, int x) const {
  if (cellIndex == 0)
    return {layer.cells.size() - 1, x + (int)m_worldSize[0]};
  else
    return {cellIndex - 1, x};
}

pair<size_t, int> WorldLayout::rightCell(WorldLayer const& layer, size_t cellIndex, int x) const {
  if (cellIndex >= layer.cells.size() - 1)
    return {0, x - (int)m_worldSize[0]};
  else
    return {cellIndex + 1, x};
}

Vec2I WorldLayout::BlockNoise::apply(Vec2I const& input, Vec2U const& worldSize) const {
  float angle = (input[0] / (float)worldSize[0]) * 2 * Constants::pi;
  float xc = std::sin(angle) / (2 * Constants::pi) * worldSize[0];
  float zc = std::cos(angle) / (2 * Constants::pi) * worldSize[0];

  Vec2I noisePos = Vec2I(
      floor(input[0] + horizontalNoise.get(input[1]) + xNoise.get(xc, input[1], zc)),
      floor(input[1] + verticalNoise.get(xc, zc) + yNoise.get(xc, input[1], zc))
    );
  noisePos[1] = clamp<int>(noisePos[1], 0, worldSize[1]);

  return noisePos;
}

}
