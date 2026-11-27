#include "StarWorldGenerationPrefetchPlan.hpp"
#include "StarWorldGenerationProductRevisionCache.hpp"

#include "gtest/gtest.h"

using namespace Star;

TEST(WorldGenerationPrefetchPlanTest, EmptyInputsProduceNoSectors) {
  auto plan = planWorldGenerationPrefetch({0, 0}, {1, 0}, 0, 4, 3, 100);
  EXPECT_TRUE(plan.sectors.empty());
  plan = planWorldGenerationPrefetch({0, 0}, {1, 0}, 4, 4, 3, 0);
  EXPECT_TRUE(plan.sectors.empty());
}

TEST(WorldGenerationPrefetchPlanTest, DirectionalHorizonPrefersForwardSectors) {
  auto plan = planWorldGenerationPrefetch({2, 2}, {1, 0}, 8, 8, 3, 300, 100);
  ASSERT_FALSE(plan.sectors.empty());
  EXPECT_EQ(Vec2U(3, 2), plan.sectors[0]);
  for (auto const& sector : plan.sectors)
    EXPECT_GE(sector[0], 2u);
}

TEST(WorldGenerationPrefetchPlanTest, WrapsXAndRespectsBudget) {
  auto plan = planWorldGenerationPrefetch({7, 1}, {1, 0}, 8, 4, 4, 200, 100);
  ASSERT_EQ(2u, plan.sectors.size());
  EXPECT_EQ(Vec2U(0, 1), plan.sectors[0]);
}

TEST(WorldGenerationPrefetchPlanTest, StationaryExpandsNeighborhood) {
  auto plan = planWorldGenerationPrefetch({4, 4}, {0, 0}, 10, 10, 2, 1000, 1);
  ASSERT_GE(plan.sectors.size(), 1u);
  EXPECT_EQ(Vec2U(4, 4), plan.sectors[0]);
  EXPECT_GE(plan.sectors.size(), 5u);
}

TEST(WorldGenerationPrefetchPlanTest, SpeculativeInsertsFeedResidentCache) {
  auto plan = planWorldGenerationPrefetch({1, 1}, {0, 1}, 6, 6, 2, 300, 100);
  WorldGenerationProductRevisionCache cache(8);
  size_t inserted = 0;
  for (auto const& sector : plan.sectors) {
    WorldGenerationProductRevisionKey key{
        WorldGenerationResidentProductKind::PrivateBaseTiles, sector, 1, 1, 1};
    if (cache.insert(key, plan.estimatedBytesPerSector, WorldGenerationMemoryDemand::Speculative, 0))
      ++inserted;
  }
  EXPECT_EQ(plan.sectors.size(), inserted);
  EXPECT_EQ(plan.sectors.size() * plan.estimatedBytesPerSector, cache.retainedBytes());
}
