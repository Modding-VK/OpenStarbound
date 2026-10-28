#include "StarWorldRegionPartition.hpp"

#include <limits>

#include "gtest/gtest.h"

using namespace Star;

TEST(WorldRegionPartitionTest, WrapsExactWidthBeforeMappingPartialRegions) {
  WorldRegionPartition partition(WorldGeometry(70, 100));
  ASSERT_TRUE(partition.regionForPosition(Vec2I(-1, 0)));
  EXPECT_EQ(*partition.regionForPosition(Vec2I(-1, 0)), Vec2U(1, 0));
  EXPECT_EQ(*partition.regionForPosition(Vec2I(-70, 0)), Vec2U(0, 0));
  EXPECT_EQ(*partition.regionForPosition(Vec2I(-71, 0)), Vec2U(1, 0));
  EXPECT_EQ(*partition.regionForPosition(Vec2I(63, 0)), Vec2U(0, 0));
  EXPECT_EQ(*partition.regionForPosition(Vec2I(64, 0)), Vec2U(1, 0));
  EXPECT_EQ(*partition.regionForPosition(Vec2I(69, 0)), Vec2U(1, 0));
  EXPECT_EQ(*partition.regionForPosition(Vec2I(70, 0)), Vec2U(0, 0));
  EXPECT_EQ(*partition.regionForPosition(Vec2I(134, 0)), Vec2U(1, 0));
  EXPECT_EQ(*partition.regionForPosition(Vec2I(140, 0)), Vec2U(0, 0));
}

TEST(WorldRegionPartitionTest, RejectsVerticalPositionsOutsideWorld) {
  WorldRegionPartition partition(WorldGeometry(70, 100));
  EXPECT_FALSE(partition.regionForPosition(Vec2I(0, -1)));
  EXPECT_FALSE(partition.regionForPosition(Vec2I(0, 100)));
  EXPECT_FALSE(partition.regionForPosition(Vec2I(0, std::numeric_limits<int>::max())));
  EXPECT_EQ(*partition.regionForPosition(Vec2I(0, 63)), Vec2U(0, 0));
  EXPECT_EQ(*partition.regionForPosition(Vec2I(0, 64)), Vec2U(0, 1));
  EXPECT_EQ(*partition.regionForPosition(Vec2I(0, 99)), Vec2U(0, 1));
}

TEST(WorldRegionPartitionTest, MapsOnlyCanonicalPhysicalSectors) {
  WorldRegionPartition partition(WorldGeometry(70, 100));
  EXPECT_EQ(*partition.regionForSector(Vec2U(0, 0)), Vec2U(0, 0));
  EXPECT_EQ(*partition.regionForSector(Vec2U(1, 1)), Vec2U(0, 0));
  EXPECT_EQ(*partition.regionForSector(Vec2U(2, 3)), Vec2U(1, 1));
  EXPECT_FALSE(partition.regionForSector(Vec2U(3, 0)));
  EXPECT_FALSE(partition.regionForSector(Vec2U(0, 4)));
  EXPECT_FALSE(partition.regionForSector(Vec2U(std::numeric_limits<unsigned>::max(), 0)));
}

TEST(WorldRegionPartitionTest, ClipsPartialRegionBoundsWithoutWrapping) {
  WorldRegionPartition partition(WorldGeometry(70, 100));
  EXPECT_EQ(*partition.regionBounds(Vec2U(0, 0)), RectI(0, 0, 64, 64));
  EXPECT_EQ(*partition.regionBounds(Vec2U(1, 0)), RectI(64, 0, 70, 64));
  EXPECT_EQ(*partition.regionBounds(Vec2U(0, 1)), RectI(0, 64, 64, 100));
  EXPECT_EQ(*partition.regionBounds(Vec2U(1, 1)), RectI(64, 64, 70, 100));
  EXPECT_FALSE(partition.regionBounds(Vec2U(2, 0)));
  EXPECT_FALSE(partition.regionBounds(Vec2U(0, 2)));
  EXPECT_FALSE(partition.regionBounds(Vec2U(std::numeric_limits<unsigned>::max(), 0)));
}

TEST(WorldRegionPartitionTest, CoversEachCanonicalTileOnce) {
  WorldRegionPartition partition(WorldGeometry(97, 65));
  for (int y = 0; y < 65; ++y) {
    for (int x = 0; x < 97; ++x) {
      auto region = partition.regionForPosition(Vec2I(x, y));
      ASSERT_TRUE(region);
      auto bounds = partition.regionBounds(*region);
      ASSERT_TRUE(bounds);
      EXPECT_LE(bounds->xMin(), x);
      EXPECT_GT(bounds->xMax(), x);
      EXPECT_LE(bounds->yMin(), y);
      EXPECT_GT(bounds->yMax(), y);
      EXPECT_EQ(*partition.regionForPosition(Vec2I(x - 97, y)), *region);
      EXPECT_EQ(*partition.regionForPosition(Vec2I(x + 97, y)), *region);
    }
  }
}

TEST(WorldRegionPartitionTest, SupportsWorldSmallerThanPhysicalSector) {
  WorldRegionPartition partition(WorldGeometry(1, 1));
  EXPECT_EQ(*partition.regionForPosition(Vec2I(-123, 0)), Vec2U(0, 0));
  EXPECT_EQ(*partition.regionForPosition(Vec2I(123, 0)), Vec2U(0, 0));
  EXPECT_EQ(*partition.regionForSector(Vec2U(0, 0)), Vec2U(0, 0));
  EXPECT_EQ(*partition.regionBounds(Vec2U(0, 0)), RectI(0, 0, 1, 1));
  EXPECT_FALSE(partition.regionForSector(Vec2U(1, 0)));
  EXPECT_FALSE(partition.regionBounds(Vec2U(0, 1)));
  EXPECT_FALSE(partition.regionForPosition(Vec2I(0, 1)));
}

TEST(WorldRegionPartitionTest, RejectsEmptyAndUnrepresentableGeometry) {
  for (auto const& size : {Vec2U(0, 0), Vec2U(0, 70), Vec2U(70, 0),
      Vec2U(std::numeric_limits<unsigned>::max(), 70)}) {
    WorldRegionPartition partition{WorldGeometry(size)};
    EXPECT_FALSE(partition.regionForPosition(Vec2I(0, 0)));
    EXPECT_FALSE(partition.regionForSector(Vec2U(0, 0)));
    EXPECT_FALSE(partition.regionBounds(Vec2U(0, 0)));
  }
}
