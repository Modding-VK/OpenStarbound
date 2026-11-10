#include "StarWorldTemplateTypes.hpp"
#include "gtest/gtest.h"

#include <array>
#include <cstring>
#include <future>
#include <type_traits>
#include <vector>

namespace Star {
// Only controlled custom-terrain metadata is initialized here, not a complete
// world, Root, layout or terrain selectors. Actual mutation/capture/math bodies
// are used. The existing private fixture ctor is never used by production.
struct WorldGenerationCustomTerrainTestAccess {
  static WorldTemplatePtr planet(Vec2U size = {100, 100}, float blendSize = 10, float blendWeight = 8) {
    auto result = WorldTemplatePtr(new WorldTemplate(WorldTemplate::LookupFixtureTag{}));
    result->m_geometry = WorldGeometry(size);
    result->m_customTerrainBlendSize = blendSize; result->m_customTerrainBlendWeight = blendWeight;
    return result;
  }
  static void parameters(WorldTemplate& world, Vec2U size, float blendSize, float blendWeight) {
    world.m_geometry = WorldGeometry(size);
    world.m_customTerrainBlendSize = blendSize; world.m_customTerrainBlendWeight = blendWeight;
  }
  static void translateFirst(WorldTemplate& world, Vec2F offset) {
    auto& region = world.m_customTerrainRegions.first();
    region.region.translate(offset); region.regionBounds = region.region.boundBox();
  }
  static void bounds(WorldTemplate& world, RectF value) { world.m_customTerrainRegions.first().regionBounds = value; }
  static void primeCache(WorldTemplate& world) { world.m_blockCache.set({7, 13}, WorldTemplate::BlockInfo{}); }
  static size_t cacheSize(WorldTemplate const& world) { return world.m_blockCache.currentSize(); }
  static pair<float, float> legacy(WorldTemplate const& world, int x, int y);
};
}

using namespace Star;
namespace {
using Access = WorldGenerationCustomTerrainTestAccess;
using Context = WorldGenerationCustomTerrainContext;
static_assert(!std::is_copy_constructible<Context>::value, "No accidental deep copy per job");
static_assert(std::is_move_constructible<Context>::value, "Owned capture must support handoff");
uint32_t bits(float value) { uint32_t out; std::memcpy(&out, &value, sizeof(out)); return out; }
std::array<uint32_t, 2> fields(pair<float, float> value) { return {bits(value.first), bits(value.second)}; }
PolyF square(float x = 10, float y = 10, float width = 10) { return PolyF(RectF::withSize({x, y}, {width, width})); }
void compare(WorldTemplate const& world, Context const& context, int x, int y) {
  EXPECT_EQ(fields(Access::legacy(world, x, y)), fields(context.weighting(x, y))) << x << ',' << y;
}
void compareGrid(WorldTemplate const& world, int lo = -10, int hi = 110, int step = 3) {
  auto context = world.captureCustomTerrain();
  for (int x = lo; x <= hi; x += step) for (int y = lo; y <= hi; y += step)
    compare(world, context, x, y);
}

TEST(WorldGenerationCustomTerrainTest, EmptyCaptureHasNoInfluence) {
  auto world = Access::planet(); auto context = world->captureCustomTerrain();
  EXPECT_EQ(0u, context.regionCount()); EXPECT_EQ(0u, context.templateRevision());
  EXPECT_EQ(fields({0, 0}), fields(context.weighting(15, 15))); compareGrid(*world);
}
TEST(WorldGenerationCustomTerrainTest, SolidInteriorUsesFullWeight) {
  auto world = Access::planet(); world->addCustomTerrainRegion(square()); auto context = world->captureCustomTerrain();
  EXPECT_EQ(fields({8, 1}), fields(context.weighting(15, 15))); compareGrid(*world);
}
TEST(WorldGenerationCustomTerrainTest, SpaceInteriorUsesNegativeWeight) {
  auto world = Access::planet(); world->addCustomSpaceRegion(square()); auto context = world->captureCustomTerrain();
  EXPECT_EQ(fields({-8, 1}), fields(context.weighting(15, 15))); compareGrid(*world);
}
TEST(WorldGenerationCustomTerrainTest, ExteriorBlendsWithActualPolygonDistance) {
  auto world = Access::planet(); world->addCustomTerrainRegion(square()); auto context = world->captureCustomTerrain();
  EXPECT_EQ(fields({8, 0.5f}), fields(context.weighting(25, 15))); compare(*world, context, 25, 15);
}
TEST(WorldGenerationCustomTerrainTest, ExactBlendBoundaryHasNoInfluence) {
  auto world = Access::planet(); world->addCustomTerrainRegion(square()); auto context = world->captureCustomTerrain();
  EXPECT_EQ(fields({0, 0}), fields(context.weighting(30, 15))); compare(*world, context, 30, 15);
}
TEST(WorldGenerationCustomTerrainTest, PaddedBoxCornerStillUsesPolygonDistance) {
  auto world = Access::planet(); world->addCustomTerrainRegion(square()); auto context = world->captureCustomTerrain();
  EXPECT_EQ(fields({0, 0}), fields(context.weighting(28, 28)));
  EXPECT_GT(context.weighting(27, 27).second, 0); compare(*world, context, 28, 28); compare(*world, context, 27, 27);
}
TEST(WorldGenerationCustomTerrainTest, EqualSolidAndSpaceCancelWithoutRemovingInfluence) {
  auto world = Access::planet(); world->addCustomTerrainRegion(square()); world->addCustomSpaceRegion(square());
  EXPECT_EQ(fields({0, 1}), fields(world->captureCustomTerrain().weighting(15, 15))); compareGrid(*world);
}
TEST(WorldGenerationCustomTerrainTest, UnequalDistancesUseNormalizedWeights) {
  auto world = Access::planet(); world->addCustomTerrainRegion(square()); world->addCustomSpaceRegion(square(25));
  auto value = world->captureCustomTerrain().weighting(20, 15);
  EXPECT_FLOAT_EQ(8.0f / 3.0f, value.first); EXPECT_FLOAT_EQ(1, value.second); compareGrid(*world);
}
TEST(WorldGenerationCustomTerrainTest, MultipleRegionsRetainInsertionOrder) {
  auto world = Access::planet();
  for (int i = 0; i != 24; ++i) {
    auto shape = square(8 + i % 9, 9 + i % 7, 4 + i % 5);
    if (i % 3) world->addCustomTerrainRegion(std::move(shape)); else world->addCustomSpaceRegion(std::move(shape));
  }
  compareGrid(*world);
}
TEST(WorldGenerationCustomTerrainTest, TriangleIsNotReplacedByItsBoundingBox) {
  auto world = Access::planet(); world->addCustomTerrainRegion({{10, 10}, {20, 10}, {10, 20}});
  auto value = world->captureCustomTerrain().weighting(17, 17);
  EXPECT_GT(value.second, 0); EXPECT_LT(value.second, 1); compareGrid(*world);
}
TEST(WorldGenerationCustomTerrainTest, ConcavePolygonKeepsTheNotch) {
  auto world = Access::planet(); world->addCustomTerrainRegion({{10, 10}, {20, 10}, {20, 14}, {14, 14}, {14, 20}, {10, 20}});
  auto context = world->captureCustomTerrain();
  EXPECT_EQ(fields({8, 1}), fields(context.weighting(12, 18)));
  EXPECT_FLOAT_EQ(0.6f, context.weighting(18, 18).second); compareGrid(*world);
}
TEST(WorldGenerationCustomTerrainTest, HorizontalWrapUsesCapturedWorldWidth) {
  auto world = Access::planet(); world->addCustomTerrainRegion(square(-5)); auto context = world->captureCustomTerrain();
  for (int x : {-101, -1, 99, 199}) {
    EXPECT_EQ(fields({8, 1}), fields(context.weighting(x, 15))); compare(*world, context, x, 15);
  }
  compareGrid(*world);
}
TEST(WorldGenerationCustomTerrainTest, VerticalCoordinatesAreNotClamped) {
  auto world = Access::planet(); world->addCustomTerrainRegion(square(10, -10));
  EXPECT_EQ(fields({8, 1}), fields(world->captureCustomTerrain().weighting(15, -5))); compareGrid(*world, -20, 30, 2);
}
TEST(WorldGenerationCustomTerrainTest, NullGeometryKeepsEuclideanBehavior) {
  auto world = Access::planet({0, 0}); world->addCustomTerrainRegion(square(-5)); auto context = world->captureCustomTerrain();
  EXPECT_EQ(fields({8, 1}), fields(context.weighting(-1, 15)));
  EXPECT_EQ(fields({0, 0}), fields(context.weighting(99, 15))); compareGrid(*world);
}
TEST(WorldGenerationCustomTerrainTest, ZeroAndNegativeBlendSizeRetainLegacyReturn) {
  for (float size : {0.0f, -1.0f}) {
    auto world = Access::planet({100, 100}, size); world->addCustomTerrainRegion(square());
    EXPECT_EQ(fields({0, 0}), fields(world->captureCustomTerrain().weighting(15, 15))); compareGrid(*world);
  }
}
TEST(WorldGenerationCustomTerrainTest, ZeroAndNegativeAmplitudeAreNotNormalized) {
  for (float amplitude : {0.0f, -8.0f}) {
    auto world = Access::planet({100, 100}, 10, amplitude); world->addCustomTerrainRegion(square());
    EXPECT_EQ(fields({amplitude, 1}), fields(world->captureCustomTerrain().weighting(15, 15))); compareGrid(*world);
  }
}
TEST(WorldGenerationCustomTerrainTest, StoredBoundsAreCopiedRatherThanRecomputed) {
  auto world = Access::planet(); world->addCustomTerrainRegion(square()); Access::bounds(*world, RectF::withSize({70, 70}, {1, 1}));
  auto context = world->captureCustomTerrain(); EXPECT_EQ(fields({0, 0}), fields(context.weighting(15, 15)));
  compareGrid(*world); // Deliberately inconsistent fixture metadata, not a normal constructor policy.
}
TEST(WorldGenerationCustomTerrainTest, CaptureDoesNotChangeRevisionOrExistingCacheEntry) {
  auto world = Access::planet(); world->addCustomTerrainRegion(square()); Access::primeCache(*world);
  auto revision = world->generationRevision(); auto context = world->captureCustomTerrain();
  EXPECT_EQ(revision, world->generationRevision()); EXPECT_EQ(revision, context.templateRevision()); EXPECT_EQ(1u, Access::cacheSize(*world));
}
TEST(WorldGenerationCustomTerrainTest, RealMutatorsStillInvalidateCacheAndAdvanceRevision) {
  auto world = Access::planet(); Access::primeCache(*world); world->addCustomTerrainRegion(square());
  EXPECT_EQ(0u, Access::cacheSize(*world)); EXPECT_EQ(1u, world->generationRevision());
  Access::primeCache(*world); world->addCustomSpaceRegion(square());
  EXPECT_EQ(0u, Access::cacheSize(*world)); EXPECT_EQ(2u, world->generationRevision());
  Access::primeCache(*world); world->clearCustomTerrains();
  EXPECT_EQ(0u, Access::cacheSize(*world)); EXPECT_EQ(3u, world->generationRevision());
}
TEST(WorldGenerationCustomTerrainTest, OldCaptureSurvivesSourceClearAndReallocation) {
  auto world = Access::planet(); world->addCustomTerrainRegion(square()); auto old = world->captureCustomTerrain();
  world->clearCustomTerrains();
  for (int i = 0; i != 128; ++i) world->addCustomSpaceRegion(square(40 + i % 10, 40, 3));
  EXPECT_EQ(1u, old.regionCount()); EXPECT_EQ(1u, old.templateRevision()); EXPECT_EQ(fields({8, 1}), fields(old.weighting(15, 15)));
  auto current = world->captureCustomTerrain(); EXPECT_EQ(128u, current.regionCount()); EXPECT_EQ(130u, current.templateRevision()); compare(*world, current, 15, 15);
}
TEST(WorldGenerationCustomTerrainTest, VerticesAreDeepCopiesNotAliases) {
  auto world = Access::planet(); world->addCustomTerrainRegion(square()); auto old = world->captureCustomTerrain();
  Access::translateFirst(*world, {40, 40});
  EXPECT_EQ(fields({8, 1}), fields(old.weighting(15, 15)));
  EXPECT_EQ(fields({0, 0}), fields(world->captureCustomTerrain().weighting(15, 15)));
}
TEST(WorldGenerationCustomTerrainTest, GeometryAndParametersAreOwnedValues) {
  auto world = Access::planet(); world->addCustomTerrainRegion(square(-5)); auto old = world->captureCustomTerrain();
  Access::parameters(*world, {200, 100}, 2, 16);
  EXPECT_EQ(fields({8, 1}), fields(old.weighting(99, 15)));
  EXPECT_EQ(fields({0, 0}), fields(world->captureCustomTerrain().weighting(99, 15)));
  EXPECT_EQ(fields({16, 1}), fields(world->captureCustomTerrain().weighting(-1, 15)));
}
TEST(WorldGenerationCustomTerrainTest, DoesNotRetainTheTemplateAfterOwnerDestruction) {
  std::weak_ptr<WorldTemplate> owner;
  auto context = [&] { auto world = Access::planet(); owner = world; world->addCustomTerrainRegion(square()); return world->captureCustomTerrain(); }();
  EXPECT_TRUE(owner.expired()); EXPECT_EQ(fields({8, 1}), fields(context.weighting(15, 15)));
}
TEST(WorldGenerationCustomTerrainTest, MoveHandoffKeepsTheOwnedData) {
  auto world = Access::planet(); world->addCustomTerrainRegion(square()); auto first = world->captureCustomTerrain();
  auto second = std::move(first); auto third = world->captureCustomTerrain(); third = std::move(second); world.reset();
  EXPECT_EQ(1u, third.regionCount()); EXPECT_EQ(fields({8, 1}), fields(third.weighting(15, 15)));
}
TEST(WorldGenerationCustomTerrainTest, TemplateRevisionAloneIsNotWorldIdentity) {
  auto a = Access::planet(), b = Access::planet({100, 100}, 10, 16);
  a->addCustomTerrainRegion(square()); b->addCustomTerrainRegion(square());
  auto ca = a->captureCustomTerrain(), cb = b->captureCustomTerrain();
  EXPECT_EQ(ca.templateRevision(), cb.templateRevision()); EXPECT_NE(fields(ca.weighting(15, 15)), fields(cb.weighting(15, 15)));
}
TEST(WorldGenerationCustomTerrainTest, MultipleReadersUseOwnedDataAfterOwnerDestruction) {
  auto world = Access::planet(); world->addCustomTerrainRegion(square(-5)); world->addCustomSpaceRegion(square(25));
  auto context = world->captureCustomTerrain(); std::vector<std::array<uint32_t, 2>> expected;
  for (int x = -20; x < 120; ++x) expected.push_back(fields(Access::legacy(*world, x, 15)));
  world.reset();
  std::array<std::future<std::vector<std::array<uint32_t, 2>>>, 3> results;
  for (auto& result : results) result = std::async(std::launch::async, [&context] {
    std::vector<std::array<uint32_t, 2>> output;
    for (int x = -20; x < 120; ++x) output.push_back(fields(context.weighting(x, 15)));
    return output;
  });
  for (auto& result : results) EXPECT_EQ(expected, result.get());
}

} // namespace

// Frozen inherited mathematics, independent from the extracted helper. Root's
// acquisition is intentionally NOT replaced or executed by this reference.
pair<float, float> Star::WorldGenerationCustomTerrainTestAccess::legacy(WorldTemplate const& world, int x, int y) {
  auto const& m_geometry = world.m_geometry;
  auto const& m_customTerrainRegions = world.m_customTerrainRegions;
  float m_customTerrainBlendSize = world.m_customTerrainBlendSize;
  float m_customTerrainBlendWeight = world.m_customTerrainBlendWeight;
  float minimumDistance = highest<float>();
  float finalSolidWeight = 0.0f;
  float totalWeight = 0.0f;

  for (auto const& region : m_customTerrainRegions) {
    if (!m_geometry.rectContains(region.regionBounds.padded(m_customTerrainBlendSize), Vec2F(x, y)))
      continue;

    float distance = m_geometry.polyDistance(region.region, Vec2F(x, y));
    if (distance >= m_customTerrainBlendSize)
      continue;

    float weight = 1.0f - distance / m_customTerrainBlendSize;
    totalWeight += weight;
    if (!region.solid)
      weight *= -1.0f;

    finalSolidWeight += weight;
    minimumDistance = min(minimumDistance, distance);
  }

  if (minimumDistance > m_customTerrainBlendSize)
    return {0.0f, 0.0f};

  finalSolidWeight /= totalWeight;

  return {finalSolidWeight * m_customTerrainBlendWeight, 1.0f - minimumDistance / m_customTerrainBlendSize};
}
