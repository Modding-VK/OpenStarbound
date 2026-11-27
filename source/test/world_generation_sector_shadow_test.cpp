#include "StarWorldGenerationSectorShadow.hpp"
#include "StarWorldGenerationCompositeContext.hpp"
#include "StarRandom.hpp"
#include "StarWorldTemplateTypes.hpp"
#include "gtest/gtest.h"

#include <type_traits>

namespace Star {
struct WorldGenerationSectorShadowTestAccess {
  static WorldTemplatePtr planet(Json config = JsonObject{}, Vec2U size = {128, 128}) {
    auto result = WorldTemplatePtr(new WorldTemplate(WorldTemplate::LookupFixtureTag{}));
    result->m_geometry = WorldGeometry(size);
    result->m_templateConfig = std::move(config);
    result->m_customTerrainBlendSize = 10;
    result->m_customTerrainBlendWeight = 8;
    return result;
  }
};
}

using namespace Star;

namespace {
using Access = WorldGenerationSectorShadowTestAccess;

WorldGenerationBaseTilesIdentity baseTilesIdentity(uint64_t revision = 1, Vec2U worldSize = {128, 128}) {
  WorldGenerationBaseTilesIdentity identity;
  identity.task = {1, 1, {0, 0}, {0, 0}, WorldGenerationTaskStage::BaseTiles};
  identity.tileRevision = 5;
  identity.templateRevision = revision;
  identity.worldSize = worldSize;
  identity.tileRegion = RectI(0, 0, int(std::min<unsigned>(WorldSectorSize, worldSize[0])),
      int(std::min<unsigned>(WorldSectorSize, worldSize[1])));
  return identity;
}

WorldGenerationSelectorContextIdentity selectorIdentity(uint64_t revision, unsigned width = 128) {
  WorldGenerationSelectorContextIdentity identity;
  identity.worldEpoch = 1;
  identity.sequence = 1;
  identity.templateRevision = revision;
  identity.worldWidth = width;
  identity.baseHeight = 16;
  identity.regionSeed = 42;
  return identity;
}
}

static_assert(!std::is_copy_constructible<WorldGenerationSectorShadow>::value,
    "Sector shadow must not deep-copy the private array");
static_assert(std::is_move_constructible<WorldGenerationSectorShadow>::value,
    "Sector shadow must support owner handoff");

TEST(WorldGenerationSectorShadowTest, TryAdmitHoldsPrivateArrayAgainstValidatedIdentity) {
  WorldGenerationMemoryLedger ledger;
  auto before = ledger.snapshot();
  auto identity = baseTilesIdentity();
  auto shadow = WorldGenerationSectorShadow::tryAdmit(ledger, identity);
  ASSERT_TRUE(shadow.has_value());
  EXPECT_TRUE(shadow->active());
  EXPECT_TRUE(shadow->matches(identity));
  EXPECT_EQ(identity, shadow->identity());
  EXPECT_EQ(before.reservedBytes + WorldGenerationSectorShadow::retainedBytes(),
      ledger.snapshot().reservedBytes);
  EXPECT_EQ(before.classResources[size_t(WorldGenerationMemoryClass::BaseTilesArray)] + 1,
      ledger.snapshot().classResources[size_t(WorldGenerationMemoryClass::BaseTilesArray)]);
  shadow->tiles()(0, 0).foreground = MaterialId(21);
  EXPECT_EQ(MaterialId(21), shadow->tiles()(0, 0).foreground);
}

TEST(WorldGenerationSectorShadowTest, InvalidIdentityAndRefusedAdmissionAllocateNothing) {
  WorldGenerationMemoryLedger ledger;
  auto invalid = baseTilesIdentity();
  invalid.task.worldEpoch = 0;
  EXPECT_FALSE(WorldGenerationSectorShadow::tryAdmit(ledger, invalid).has_value());
  EXPECT_EQ(0u, ledger.snapshot().resources);

  WorldGenerationMemoryLedger tight(WorldGenerationSectorShadow::retainedBytes() - 1);
  EXPECT_FALSE(WorldGenerationSectorShadow::tryAdmit(tight, baseTilesIdentity()).has_value());
  EXPECT_EQ(0u, tight.snapshot().resources);
}

TEST(WorldGenerationSectorShadowTest, DiscardReleasesAdmissionWithoutPublicationPath) {
  WorldGenerationMemoryLedger ledger;
  auto identity = baseTilesIdentity();
  auto shadow = WorldGenerationSectorShadow::tryAdmit(ledger, identity);
  ASSERT_TRUE(shadow.has_value());
  EXPECT_EQ(1u, ledger.snapshot().resources);
  shadow->discard();
  EXPECT_FALSE(shadow->active());
  EXPECT_FALSE(shadow->matches(identity));
  EXPECT_EQ(0u, ledger.snapshot().resources);
}

TEST(WorldGenerationSectorShadowTest, StaleExpectationFailsMatchesAndPrivatePayloadDrainsOnDiscard) {
  WorldGenerationMemoryLedger ledger;
  auto identity = baseTilesIdentity();
  auto shadow = WorldGenerationSectorShadow::tryAdmit(ledger, identity);
  ASSERT_TRUE(shadow.has_value());
  shadow->tiles()(1, 1).foreground = MaterialId(33);
  auto stale = identity;
  ++stale.tileRevision;
  EXPECT_FALSE(shadow->matches(stale));
  EXPECT_TRUE(shadow->active());
  EXPECT_EQ(MaterialId(33), shadow->tiles()(1, 1).foreground);
  EXPECT_EQ(1u, ledger.snapshot().resources);
  shadow->discard();
  EXPECT_EQ(0u, ledger.snapshot().resources);
}

TEST(WorldGenerationSectorShadowTest, TryAdmitFromCompositeReusesBaseTilesIdentity) {
  auto world = Access::planet(JsonObject{{"biomeTransitionThreshold", 0.5}});
  auto revision = world->generationRevision();
  WorldGenerationMemoryLedger ledger;
  Random::init(918);
  auto composite = WorldGenerationCompositeContext::tryCapture(
      ledger, selectorIdentity(revision), baseTilesIdentity(revision), *world);
  ASSERT_TRUE(composite.has_value());
  auto before = ledger.snapshot();
  auto shadow = WorldGenerationSectorShadow::tryAdmit(ledger, *composite);
  ASSERT_TRUE(shadow.has_value());
  EXPECT_TRUE(shadow->matches(composite->baseTilesIdentity()));
  EXPECT_EQ(before.reservedBytes + WorldGenerationSectorShadow::retainedBytes(),
      ledger.snapshot().reservedBytes);
  shadow->discard();
  EXPECT_EQ(before.resources, ledger.snapshot().resources);
}

TEST(WorldGenerationSectorShadowTest, TightLedgerAdmitsOneShadowAndRefusesTheSecond) {
  WorldGenerationMemoryLedger ledger(WorldGenerationSectorShadow::retainedBytes());
  auto origin = baseTilesIdentity(1, {64, 64});
  auto adjacent = origin;
  adjacent.task.sequence = 2;
  adjacent.task.sector = {1, 0};
  adjacent.tileRegion = RectI(int(WorldSectorSize), 0, int(WorldSectorSize) * 2, int(WorldSectorSize));

  auto first = WorldGenerationSectorShadow::tryAdmit(ledger, origin);
  ASSERT_TRUE(first.has_value());
  EXPECT_FALSE(WorldGenerationSectorShadow::tryAdmit(ledger, adjacent).has_value());
  EXPECT_EQ(1u, ledger.snapshot().resources);
  first->discard();
  auto resumed = WorldGenerationSectorShadow::tryAdmit(ledger, adjacent);
  ASSERT_TRUE(resumed.has_value());
  EXPECT_TRUE(resumed->matches(adjacent));
  resumed->discard();
  EXPECT_EQ(0u, ledger.snapshot().resources);
}

TEST(WorldGenerationSectorShadowTest, ConcurrentSectorShadowsReleaseIndependently) {
  WorldGenerationMemoryLedger ledger;
  auto origin = baseTilesIdentity(1, {64, 64});
  auto adjacent = origin;
  adjacent.task.sequence = 2;
  adjacent.task.sector = {1, 0};
  adjacent.task.region = {0, 0};
  adjacent.tileRegion = RectI(int(WorldSectorSize), 0, int(WorldSectorSize) * 2, int(WorldSectorSize));

  auto first = WorldGenerationSectorShadow::tryAdmit(ledger, origin);
  auto second = WorldGenerationSectorShadow::tryAdmit(ledger, adjacent);
  ASSERT_TRUE(first.has_value());
  ASSERT_TRUE(second.has_value());
  EXPECT_EQ(2u, ledger.snapshot().classResources[size_t(WorldGenerationMemoryClass::BaseTilesArray)]);
  first->tiles()(0, 0).foreground = MaterialId(21);
  second->tiles()(0, 0).foreground = MaterialId(22);
  EXPECT_EQ(MaterialId(21), first->tiles()(0, 0).foreground);
  EXPECT_EQ(MaterialId(22), second->tiles()(0, 0).foreground);
  EXPECT_TRUE(first->matches(origin));
  EXPECT_TRUE(second->matches(adjacent));
  EXPECT_FALSE(first->matches(adjacent));

  first->discard();
  EXPECT_FALSE(first->active());
  EXPECT_TRUE(second->active());
  EXPECT_EQ(1u, ledger.snapshot().classResources[size_t(WorldGenerationMemoryClass::BaseTilesArray)]);
  second->discard();
  EXPECT_EQ(0u, ledger.snapshot().resources);
}
