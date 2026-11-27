#pragma once

#include "StarList.hpp"
#include "StarVector.hpp"

#include <cmath>
#include <cstdint>

namespace Star {

// RAM-04: private speculative sector plan. Never loads WorldStorage tiles and
// never starts scripts/entities. Callers feed the resulting sectors into a
// resident cache under Speculative demand and a separate byte budget.
struct WorldGenerationPrefetchPlan {
  static constexpr unsigned ModelVersion = 1;
  List<Vec2U> sectors;
  size_t speculativeBudgetBytes = 0;
  size_t estimatedBytesPerSector = 0;
};

inline unsigned wrapSectorX(int x, unsigned sectorCountX) {
  if (sectorCountX == 0)
    return 0;
  int width = (int)sectorCountX;
  int wrapped = x % width;
  if (wrapped < 0)
    wrapped += width;
  return (unsigned)wrapped;
}

// Build an ordered speculative horizon from position/velocity. When velocity
// is near zero, expand a small Moore neighborhood. World wrap applies only on X.
inline WorldGenerationPrefetchPlan planWorldGenerationPrefetch(Vec2F position, Vec2F velocity,
    unsigned sectorCountX, unsigned sectorCountY, unsigned horizonSectors,
    size_t speculativeBudgetBytes, size_t estimatedBytesPerSector = 1) {
  WorldGenerationPrefetchPlan plan;
  plan.speculativeBudgetBytes = speculativeBudgetBytes;
  plan.estimatedBytesPerSector = estimatedBytesPerSector ? estimatedBytesPerSector : 1;
  if (sectorCountX == 0 || sectorCountY == 0 || horizonSectors == 0 || speculativeBudgetBytes == 0)
    return plan;

  int originX = (int)std::floor(position[0]);
  int originY = (int)std::floor(position[1]);
  float speed = std::sqrt(velocity[0] * velocity[0] + velocity[1] * velocity[1]);
  int dirX = 0;
  int dirY = 0;
  if (speed > 1e-4f) {
    dirX = velocity[0] > 0.25f ? 1 : velocity[0] < -0.25f ? -1 : 0;
    dirY = velocity[1] > 0.25f ? 1 : velocity[1] < -0.25f ? -1 : 0;
    if (dirX == 0 && dirY == 0) {
      dirX = std::fabs(velocity[0]) >= std::fabs(velocity[1]) ? (velocity[0] >= 0 ? 1 : -1) : 0;
      dirY = dirX == 0 ? (velocity[1] >= 0 ? 1 : -1) : (std::fabs(velocity[1]) > 0.25f ? (velocity[1] >= 0 ? 1 : -1) : 0);
    }
  }

  size_t maxSectors = speculativeBudgetBytes / plan.estimatedBytesPerSector;
  if (maxSectors == 0)
    return plan;

  auto tryAdd = [&](int sx, int sy) {
    if (sy < 0 || (unsigned)sy >= sectorCountY)
      return;
    if (plan.sectors.size() >= maxSectors)
      return;
    Vec2U sector{wrapSectorX(sx, sectorCountX), (unsigned)sy};
    for (auto const& existing : plan.sectors) {
      if (existing == sector)
        return;
    }
    plan.sectors.append(sector);
  };

  if (dirX == 0 && dirY == 0) {
    tryAdd(originX, originY);
    for (unsigned ring = 1; ring <= horizonSectors && plan.sectors.size() < maxSectors; ++ring) {
      int r = (int)ring;
      for (int dx = -r; dx <= r; ++dx) {
        tryAdd(originX + dx, originY - r);
        tryAdd(originX + dx, originY + r);
      }
      for (int dy = -r + 1; dy <= r - 1; ++dy) {
        tryAdd(originX - r, originY + dy);
        tryAdd(originX + r, originY + dy);
      }
    }
  } else {
    for (unsigned step = 1; step <= horizonSectors && plan.sectors.size() < maxSectors; ++step) {
      int s = (int)step;
      tryAdd(originX + dirX * s, originY + dirY * s);
      if (dirX != 0 && dirY != 0) {
        tryAdd(originX + dirX * s, originY);
        tryAdd(originX, originY + dirY * s);
      } else if (dirX != 0) {
        tryAdd(originX + dirX * s, originY - 1);
        tryAdd(originX + dirX * s, originY + 1);
      } else {
        tryAdd(originX - 1, originY + dirY * s);
        tryAdd(originX + 1, originY + dirY * s);
      }
    }
  }
  return plan;
}

}
