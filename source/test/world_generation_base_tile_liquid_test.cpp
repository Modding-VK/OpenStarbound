#include "StarWorldGenerationBaseTileLiquid.hpp"
#include "StarWorkerPool.hpp"

#include <array>
#include <cstring>
#include <limits>
#include <vector>

#include "gtest/gtest.h"

using namespace Star;

namespace {
using Input = WorldGenerationBaseTileLiquidInput;
using Action = WorldGenerationBaseTileLiquidAction;
Input input() { return {EmptyMaterialId, EmptyMaterialId, 3, 10, false, 0}; }
void same(LiquidStore const& a, LiquidStore const& b) {
  EXPECT_EQ(a.liquid, b.liquid); EXPECT_EQ(a.source, b.source);
  EXPECT_EQ(std::memcmp(&a.level, &b.level, sizeof(float)), 0);
  EXPECT_EQ(std::memcmp(&a.pressure, &b.pressure, sizeof(float)), 0);
}
// Baseline expression from this repository's prepareBaseTileArray before the
// extraction. Keep independent of the new decision/apply functions.
void reference(LiquidStore& target, Input const& in) {
  if (in.foreground != EmptyMaterialId) {
    target = LiquidStore();
  } else if (in.oceanLiquid != EmptyLiquidId && in.y < in.oceanLiquidLevel) {
    float pressure = in.oceanLiquidLevel - in.y;
    if (in.background == EmptyMaterialId) target = LiquidStore::endless(in.oceanLiquid, pressure);
    else if (in.encloseLiquids) target = LiquidStore::filled(in.oceanLiquid, 1.0f, pressure);
  }
}
}

TEST(WorldGenerationBaseTileLiquidTest, SolidForegroundClearsEveryLiquidField) {
  auto in = input(); in.foreground = 1; auto target = LiquidStore::endless(7, 40);
  EXPECT_EQ(worldGenerationBaseTileLiquid(in).action, Action::Clear);
  applyWorldGenerationBaseTileLiquid(target, in); same(target, LiquidStore());
}

TEST(WorldGenerationBaseTileLiquidTest, NullAndMetaForegroundsAreNotAir) {
  for (auto material : {NullMaterialId, StructureMaterialId, BoundaryMaterialId}) {
    auto in = input(); in.foreground = material;
    EXPECT_EQ(worldGenerationBaseTileLiquid(in).action, Action::Clear);
  }
}

TEST(WorldGenerationBaseTileLiquidTest, NoOceanKeepsAnExistingLiquidEvenIfItIsASource) {
  auto in = input(); in.oceanLiquid = EmptyLiquidId;
  auto target = LiquidStore::endless(7, 40), before = target;
  EXPECT_EQ(worldGenerationBaseTileLiquid(in).action, Action::Keep);
  applyWorldGenerationBaseTileLiquid(target, in); same(target, before);
}

TEST(WorldGenerationBaseTileLiquidTest, AtTheOceanLevelDoesNotCreateLiquid) {
  auto in = input(); in.y = in.oceanLiquidLevel; auto target = LiquidStore::filled(7, 0.25f, 4.5f), before = target;
  EXPECT_EQ(worldGenerationBaseTileLiquid(in).action, Action::Keep);
  applyWorldGenerationBaseTileLiquid(target, in); same(target, before);
}

TEST(WorldGenerationBaseTileLiquidTest, AboveTheOceanLevelKeepsThePreviousStore) {
  auto in = input(); in.y = in.oceanLiquidLevel + 1; auto target = LiquidStore::endless(7, 40), before = target;
  applyWorldGenerationBaseTileLiquid(target, in); same(target, before);
}

TEST(WorldGenerationBaseTileLiquidTest, AirBackgroundCreatesTheSameEndlessOceanStore) {
  auto in = input(); auto target = LiquidStore::filled(7, 0.25f, 4.5f);
  auto decision = worldGenerationBaseTileLiquid(in); EXPECT_EQ(decision.action, Action::Endless);
  EXPECT_EQ(decision.liquid, 3u); EXPECT_EQ(decision.pressure, 10.0f);
  applyWorldGenerationBaseTileLiquid(target, in); same(target, LiquidStore::endless(3, 10));
}

TEST(WorldGenerationBaseTileLiquidTest, EnclosedBackgroundCreatesFilledNonSourceLiquid) {
  auto in = input(); in.background = 1; in.encloseLiquids = true; auto target = LiquidStore::endless(7, 40);
  EXPECT_EQ(worldGenerationBaseTileLiquid(in).action, Action::Filled);
  applyWorldGenerationBaseTileLiquid(target, in); same(target, LiquidStore::filled(3, 1.0f, 10));
}

TEST(WorldGenerationBaseTileLiquidTest, UnenclosedBackgroundPreservesTheOriginalLiquid) {
  auto in = input(); in.background = 1; auto target = LiquidStore::filled(7, 0.25f, 4.5f), before = target;
  EXPECT_EQ(worldGenerationBaseTileLiquid(in).action, Action::Keep);
  applyWorldGenerationBaseTileLiquid(target, in); same(target, before);
}

TEST(WorldGenerationBaseTileLiquidTest, NullBackgroundPreservesTheLegacyEnclosureRule) {
  auto in = input(); in.background = NullMaterialId;
  EXPECT_EQ(worldGenerationBaseTileLiquid(in).action, Action::Keep);
  in.encloseLiquids = true; EXPECT_EQ(worldGenerationBaseTileLiquid(in).action, Action::Filled);
}

TEST(WorldGenerationBaseTileLiquidTest, KeepPreservesSignedZeroAndPressureSubtractionUsesTheOriginalIntegerRange) {
  auto in = input(); in.oceanLiquid = EmptyLiquidId; LiquidStore target(7, -0.0f, -0.0f, false), before = target;
  applyWorldGenerationBaseTileLiquid(target, in); same(target, before);
  in.oceanLiquid = 3; in.oceanLiquidLevel = std::numeric_limits<int>::max(); in.y = in.oceanLiquidLevel - 1;
  EXPECT_EQ(worldGenerationBaseTileLiquid(in).pressure, 1.0f);
  in.y = std::numeric_limits<int>::min(); in.oceanLiquidLevel = in.y + 1;
  EXPECT_EQ(worldGenerationBaseTileLiquid(in).pressure, 1.0f);
}

TEST(WorldGenerationBaseTileLiquidTest, TruthTableMatchesEveryLiquidFieldAgainstTheLegacyPath) {
  size_t cases = 0;
  std::array<LiquidStore, 3> originals{{LiquidStore(), LiquidStore::filled(7, 0.25f, 4.5f), LiquidStore::endless(9, 40)}};
  for (auto foreground : {EmptyMaterialId, NullMaterialId, MaterialId(1), StructureMaterialId, BoundaryMaterialId})
    for (auto background : {EmptyMaterialId, NullMaterialId, MaterialId(1)})
      for (LiquidId ocean : {LiquidId(0), LiquidId(3), LiquidId(255)})
        for (int level : {-5, 0, 10, 2000})
          for (int y : {-100, -5, 0, 1, 9, 10, 2000})
            for (bool enclose : {false, true})
              for (auto original : originals) {
                Input in{foreground, background, ocean, level, enclose, y};
                auto expected = original, actual = original;
                reference(expected, in); applyWorldGenerationBaseTileLiquid(actual, in);
                same(actual, expected); ++cases;
              }
  EXPECT_EQ(cases, 7560u);
}

TEST(WorldGenerationBaseTileLiquidTest, PartitionedWorkersMatchASerialSectorWithoutWorldOrRandomState) {
  std::array<Input, 1024> inputs;
  std::array<LiquidStore, 1024> serial, parallel;
  for (size_t i = 0; i < inputs.size(); ++i) {
    inputs[i] = {i % 3 ? EmptyMaterialId : MaterialId(1), i % 2 ? EmptyMaterialId : MaterialId(2),
        LiquidId(i % 4), 16, bool(i % 5), int(i / 32)};
    serial[i] = parallel[i] = LiquidStore::filled(7, 0.25f, 4.5f);
    reference(serial[i], inputs[i]);
  }
  WorkerPool pool("Scalar BaseTiles liquid contract", 4); std::vector<WorkerPoolPromise<bool>> promises;
  for (size_t part = 0; part < 4; ++part) promises.push_back(pool.addProducer<bool>([&, part] {
    for (size_t i = part * 256; i < (part + 1) * 256; ++i) applyWorldGenerationBaseTileLiquid(parallel[i], inputs[i]);
    return true;
  }));
  for (auto& promise : promises) EXPECT_TRUE(promise.get());
  pool.finish();
  for (size_t i = 0; i < inputs.size(); ++i) same(parallel[i], serial[i]);
}
