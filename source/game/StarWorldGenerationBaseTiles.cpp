#include "StarWorldGenerationBaseTiles.hpp"

#include <cstring>
#include <limits>

namespace Star {
namespace {
bool sameFloat(float const& a, float const& b) { return std::memcmp(&a, &b, sizeof(float)) == 0; }
bool samePoint(Vec2F const& a, Vec2F const& b) { return sameFloat(a[0], b[0]) && sameFloat(a[1], b[1]); }
bool sameCache(ServerTile const& a, ServerTile const& b) {
  if (a.collisionCache.size() != b.collisionCache.size()) return false;
  for (size_t i = 0; i < a.collisionCache.size(); ++i) {
    auto const& x = a.collisionCache[i]; auto const& y = b.collisionCache[i];
    if (x.kind != y.kind || x.space != y.space
        || !sameFloat(x.polyBounds.xMin(), y.polyBounds.xMin()) || !sameFloat(x.polyBounds.yMin(), y.polyBounds.yMin())
        || !sameFloat(x.polyBounds.xMax(), y.polyBounds.xMax()) || !sameFloat(x.polyBounds.yMax(), y.polyBounds.yMax())
        || x.poly.vertexes().size() != y.poly.vertexes().size()) return false;
    for (size_t j = 0; j < x.poly.vertexes().size(); ++j)
      if (!samePoint(x.poly.vertexes()[j], y.poly.vertexes()[j])) return false;
  }
  return true;
}
bool validMode(WorldGenerationBaseTileComparisonMode mode) {
  return mode == WorldGenerationBaseTileComparisonMode::Exact || mode == WorldGenerationBaseTileComparisonMode::IgnoreCollisionCache;
}
}

// Read-only access includes hidden effect-time and derived state omitted by the
// tile save format. This type never constructs, resets or mutates damage state.
struct WorldGenerationBaseTileComparisonAccess {
  static bool sameDamage(TileDamageStatus const& a, TileDamageStatus const& b) {
    return sameFloat(a.m_damagePercentage, b.m_damagePercentage)
        && sameFloat(a.m_damageEffectTimeFactor, b.m_damageEffectTimeFactor)
        && a.m_harvested == b.m_harvested && samePoint(a.m_damageSourcePosition, b.m_damageSourcePosition)
        && a.m_damageType == b.m_damageType && sameFloat(a.m_damageEffectPercentage, b.m_damageEffectPercentage);
  }
};

bool WorldGenerationBaseTilesIdentity::valid() const {
  if (!task.worldEpoch || !task.sequence || task.stage != WorldGenerationTaskStage::BaseTiles
      || productVersion != ProductVersion || liquidVersion != WorldGenerationBaseTileLiquidDecision::ModelVersion
      || !worldSize[0] || !worldSize[1]
      || worldSize[0] > unsigned(std::numeric_limits<int>::max()) || worldSize[1] > unsigned(std::numeric_limits<int>::max())) return false;
  uint64_t x = uint64_t(task.sector[0]) * WorldSectorSize, y = uint64_t(task.sector[1]) * WorldSectorSize;
  if (x >= worldSize[0] || y >= worldSize[1]
      || task.region != task.sector / WorldRegionPartition::SectorsPerRegion) return false;
  return tileRegion.xMin() == int(x) && tileRegion.yMin() == int(y)
      && tileRegion.xMax() == int(std::min<uint64_t>(x + WorldSectorSize, worldSize[0]))
      && tileRegion.yMax() == int(std::min<uint64_t>(y + WorldSectorSize, worldSize[1]));
}

bool WorldGenerationBaseTilesIdentity::operator==(WorldGenerationBaseTilesIdentity const& other) const {
  return task == other.task && tileRevision == other.tileRevision && templateRevision == other.templateRevision
      && worldSize == other.worldSize && tileRegion == other.tileRegion
      && productVersion == other.productVersion && liquidVersion == other.liquidVersion;
}

namespace {
uint32_t differentFields(ServerTile const& a, ServerTile const& b, WorldGenerationBaseTileComparisonMode mode) {
  uint32_t result = 0;
  auto check = [&](WorldGenerationBaseTileField field, bool same) { if (!same) result |= static_cast<uint32_t>(field); };
  using Field = WorldGenerationBaseTileField;
  check(Field::Foreground, a.foreground == b.foreground);
  check(Field::ForegroundHue, a.foregroundHueShift == b.foregroundHueShift);
  check(Field::ForegroundMod, a.foregroundMod == b.foregroundMod);
  check(Field::ForegroundModHue, a.foregroundModHueShift == b.foregroundModHueShift);
  check(Field::ForegroundColor, a.foregroundColorVariant == b.foregroundColorVariant);
  check(Field::Background, a.background == b.background);
  check(Field::BackgroundHue, a.backgroundHueShift == b.backgroundHueShift);
  check(Field::BackgroundMod, a.backgroundMod == b.backgroundMod);
  check(Field::BackgroundModHue, a.backgroundModHueShift == b.backgroundModHueShift);
  check(Field::BackgroundColor, a.backgroundColorVariant == b.backgroundColorVariant);
  check(Field::Collision, a.collision == b.collision);
  check(Field::ObjectCollision, a.objectCollision == b.objectCollision);
  if (mode == WorldGenerationBaseTileComparisonMode::Exact) {
    check(Field::CollisionDirty, a.collisionCacheDirty == b.collisionCacheDirty);
    check(Field::CollisionCache, sameCache(a, b));
  }
  check(Field::BlockBiome, a.blockBiomeIndex == b.blockBiomeIndex);
  check(Field::EnvironmentBiome, a.environmentBiomeIndex == b.environmentBiomeIndex);
  check(Field::BiomeTransition, a.biomeTransition == b.biomeTransition);
  check(Field::ForegroundDamage, WorldGenerationBaseTileComparisonAccess::sameDamage(a.foregroundDamage, b.foregroundDamage));
  check(Field::BackgroundDamage, WorldGenerationBaseTileComparisonAccess::sameDamage(a.backgroundDamage, b.backgroundDamage));
  check(Field::Dungeon, a.dungeonId == b.dungeonId);
  check(Field::LiquidId, a.liquid.liquid == b.liquid.liquid);
  check(Field::LiquidLevel, sameFloat(a.liquid.level, b.liquid.level));
  check(Field::LiquidPressure, sameFloat(a.liquid.pressure, b.liquid.pressure));
  check(Field::LiquidSource, a.liquid.source == b.liquid.source);
  check(Field::RootSource, a.rootSource == b.rootSource);
  return result;
}
void compareAt(WorldGenerationBaseTilesComparison& result, ServerTile const& a, ServerTile const& b,
    Vec2U position, WorldGenerationBaseTileComparisonMode mode) {
  ++result.comparedTiles;
  auto fields = differentFields(a, b, mode);
  result.differentFields |= fields;
  if (fields) {
    ++result.differentTiles;
    if (!result.firstDifference) result.firstDifference = position;
  }
}
}

WorldGenerationBaseTilesComparison compareWorldGenerationBaseTile(ServerTile const& a, ServerTile const& b,
    WorldGenerationBaseTileComparisonMode mode) {
  WorldGenerationBaseTilesComparison result;
  result.valid = validMode(mode);
  if (result.valid) compareAt(result, a, b, {0, 0}, mode);
  return result;
}

WorldGenerationBaseTilesComparison compareWorldGenerationBaseTiles(ServerTileSectorArray::Array const& a,
    ServerTileSectorArray::Array const& b, WorldGenerationBaseTilesIdentity const& identity, WorldGenerationBaseTileComparisonMode mode) {
  WorldGenerationBaseTilesComparison result;
  result.valid = identity.valid() && validMode(mode);
  if (result.valid)
    for (int x = 0; x < identity.tileRegion.width(); ++x)
      for (int y = 0; y < identity.tileRegion.height(); ++y)
        compareAt(result, a(x, y), b(x, y), Vec2U(unsigned(x), unsigned(y)), mode);
  return result;
}

}
