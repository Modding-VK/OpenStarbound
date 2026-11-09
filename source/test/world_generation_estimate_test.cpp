#include "StarWorldGenerationMemoryEstimate.hpp"
#include "StarWorkerPool.hpp"

#include <atomic>
#include <thread>

#include "gtest/gtest.h"

using namespace Star;

namespace {
auto const Base = WorldGenerationMemoryClass::BaseTilesArray;
auto const Metadata = WorldGenerationMemoryClass::SnapshotMetadataEstimate;
auto const Plans = WorldGenerationMemoryClass::MicroDungeonPlanEstimate;

Json tree() {
  Json leaf(String("a shared string longer than the small string buffer"));
  Json branch(JsonArray{leaf, leaf});
  return JsonArray{branch, branch};
}
}

TEST(WorldGenerationEstimateTest, ArraysAndEstimatesKeepSeparateCurrentAndPeakTotals) {
  WorldGenerationMemoryLedger ledger;
  auto array = ledger.reserve(Base, 64);
  auto metadata = ledger.reserve(Metadata, 32);
  auto plan = ledger.reserve(Plans, 16);
  auto s = ledger.snapshot();
  EXPECT_EQ(s.reservedBytes, 112u);
  EXPECT_EQ(s.reservedArrayBytes, 64u);
  EXPECT_EQ(s.estimatedBytes, 48u);
  EXPECT_EQ(s.peakReservedArrayBytes, 64u);
  EXPECT_EQ(s.peakEstimatedBytes, 48u);
  metadata.reset(); plan.reset(); array.reset();
  EXPECT_EQ(ledger.snapshot().reservedBytes, 0u);
  EXPECT_EQ(ledger.snapshot().peakReservedArrayBytes, 64u);
}

TEST(WorldGenerationEstimateTest, VariableEstimateCanGrowShrinkAndRelease) {
  WorldGenerationMemoryLedger ledger;
  auto output = ledger.reserve(Plans, 0);
  ASSERT_TRUE(output);
  EXPECT_TRUE(output->resizeEstimate(128));
  EXPECT_EQ(ledger.snapshot().estimatedBytes, 128u);
  EXPECT_TRUE(output->resizeEstimate(16));
  EXPECT_EQ(ledger.snapshot().estimatedBytes, 16u);
  EXPECT_EQ(ledger.snapshot().peakEstimatedBytes, 128u);
  output.reset();
  EXPECT_EQ(ledger.snapshot().estimatedBytes, 0u);
}

TEST(WorldGenerationEstimateTest, FailedResizeKeepsTheOldChargeAndArrayReservation) {
  WorldGenerationMemoryLedger ledger(64);
  auto array = ledger.reserve(Base, 32);
  auto output = ledger.reserve(Plans, 16);
  EXPECT_FALSE(array->resizeEstimate(16));
  EXPECT_FALSE(output->resizeEstimate(33));
  EXPECT_EQ(ledger.snapshot().reservedBytes, 48u);
  EXPECT_EQ(ledger.snapshot().reservedArrayBytes, 32u);
  EXPECT_TRUE(output->resizeEstimate(32));
  EXPECT_EQ(ledger.snapshot().reservedBytes, 64u);
}

TEST(WorldGenerationEstimateTest, SharedOwnershipNotAliasedPointerDeterminesIdentity) {
  WorldGenerationMemoryLedger ledger;
  auto data = std::make_shared<std::array<unsigned, 2>>();
  std::shared_ptr<void const> first(data, &(*data)[0]), second(data, &(*data)[1]);
  EXPECT_NE(first.get(), second.get());
  auto a = ledger.reserveSharedJson(first, 32);
  auto b = ledger.reserveSharedJson(second, 32);
  ASSERT_TRUE(a); ASSERT_TRUE(b);
  EXPECT_EQ(a, b);
  EXPECT_EQ(ledger.snapshot().estimatedBytes, 32u);
  EXPECT_EQ(ledger.snapshot().sharedRegistryEntries, 1u);
  EXPECT_FALSE(a->resizeEstimate(64));
  a.reset(); b.reset();
  EXPECT_EQ(ledger.snapshot().sharedRegistryEntries, 0u);
}

TEST(WorldGenerationEstimateTest, SameAddressWithDifferentOwnersDoesNotMerge) {
  WorldGenerationMemoryLedger ledger;
  unsigned slot = 0;
  std::shared_ptr<unsigned> first(&slot, [](unsigned*) {}), second(&slot, [](unsigned*) {});
  EXPECT_EQ(first.get(), second.get());
  auto a = ledger.reserveSharedJson(first, 32);
  auto b = ledger.reserveSharedJson(second, 32);
  EXPECT_NE(a, b);
  EXPECT_EQ(ledger.snapshot().sharedRegistryEntries, 2u);
  EXPECT_EQ(ledger.snapshot().estimatedBytes, 64u);
}

TEST(WorldGenerationEstimateTest, ConflictingSharedModelsAreRefusedWithoutChangingBytes) {
  WorldGenerationMemoryLedger ledger;
  auto owner = std::make_shared<unsigned>(1);
  auto first = ledger.reserveSharedJson(owner, 64);
  EXPECT_FALSE(ledger.reserveSharedJson(owner, 32));
  EXPECT_FALSE(ledger.reserve(WorldGenerationMemoryClass::TemplateJsonEstimate, 64));
  EXPECT_FALSE(ledger.reserveSharedJson({}, 64));
  EXPECT_EQ(ledger.snapshot().estimatedBytes, 64u);
  first.reset();
  EXPECT_EQ(ledger.snapshot().sharedRegistryEntries, 0u);
}

TEST(WorldGenerationEstimateTest, UnmeasuredCaptureDoesNotWalkOrChargeJson) {
  WorldGenerationMemoryLedger ledger;
  auto value = tree();
  auto snapshot = captureWorldGenerationJson(ledger, value, false);
  EXPECT_FALSE(snapshot->measured);
  EXPECT_FALSE(snapshot->complete);
  EXPECT_EQ(snapshot->visitedValues, 0u);
  EXPECT_EQ(snapshot->value.arrayPtr(), value.arrayPtr());
  EXPECT_EQ(ledger.snapshot().resources, 0u);
}

TEST(WorldGenerationEstimateTest, PrimitiveRootsHaveNoSharedHeapNodesInThisModel) {
  WorldGenerationMemoryLedger ledger;
  for (auto value : {Json(), Json(1), Json(true), Json(1.5)}) {
    auto snapshot = captureWorldGenerationJson(ledger, value, true);
    EXPECT_TRUE(snapshot->complete);
    EXPECT_EQ(snapshot->value, value);
    EXPECT_EQ(snapshot->visitedValues, 1u);
    EXPECT_TRUE(snapshot->accounting.empty());
  }
  EXPECT_EQ(ledger.snapshot().estimatedBytes, 0u);
}

TEST(WorldGenerationEstimateTest, RepeatedSubtreesAndRepeatedCapturesCountUniqueBacking) {
  WorldGenerationMemoryLedger ledger;
  auto value = tree();
  auto a = captureWorldGenerationJson(ledger, value, true);
  ASSERT_TRUE(a->complete);
  EXPECT_EQ(a->accounting.size(), 3u);
  EXPECT_EQ(ledger.snapshot().sharedResources, 3u);
  auto bytes = ledger.snapshot().estimatedBytes;
  auto b = captureWorldGenerationJson(ledger, value, true);
  EXPECT_EQ(ledger.snapshot().estimatedBytes, bytes);
  EXPECT_EQ(ledger.snapshot().sharedResources, 3u);
  a.reset();
  EXPECT_EQ(ledger.snapshot().estimatedBytes, bytes);
  b.reset();
  EXPECT_EQ(ledger.snapshot().estimatedBytes, 0u);
  EXPECT_EQ(ledger.snapshot().sharedRegistryEntries, 0u);
  // Plain Json outside tracked input handles is deliberately outside scope.
  EXPECT_EQ(value.size(), 2u);
}

TEST(WorldGenerationEstimateTest, DifferentRootsSharingALeafChargeTheLeafOnce) {
  WorldGenerationMemoryLedger ledger;
  Json leaf("shared leaf");
  Json left(JsonObject{{"leaf", leaf}}), right(JsonArray{leaf});
  auto a = captureWorldGenerationJson(ledger, left, true);
  auto b = captureWorldGenerationJson(ledger, right, true);
  EXPECT_TRUE(a->complete); EXPECT_TRUE(b->complete);
  EXPECT_EQ(ledger.snapshot().sharedResources, 3u);
  a.reset();
  EXPECT_EQ(ledger.snapshot().sharedResources, 2u);
}

TEST(WorldGenerationEstimateTest, EqualContentsWithIndependentStorageRemainDistinct) {
  WorldGenerationMemoryLedger ledger;
  Json a("equal but independent strings"), b("equal but independent strings");
  EXPECT_EQ(a, b);
  EXPECT_NE(a.stringPtr(), b.stringPtr());
  auto snapshot = captureWorldGenerationJson(ledger, JsonArray{a, b}, true);
  EXPECT_TRUE(snapshot->complete);
  EXPECT_EQ(ledger.snapshot().sharedResources, 3u);
}

TEST(WorldGenerationEstimateTest, TraversalLimitMarksPartialWithoutChangingGenerationInputs) {
  WorldGenerationMemoryLedger ledger;
  auto value = tree();
  auto snapshot = captureWorldGenerationJson(ledger, value, true, 1);
  EXPECT_TRUE(snapshot->measured);
  EXPECT_FALSE(snapshot->complete);
  EXPECT_EQ(snapshot->visitedValues, 1u);
  EXPECT_EQ(snapshot->value, value);
  EXPECT_EQ(snapshot->value.arrayPtr(), value.arrayPtr());
  auto none = captureWorldGenerationJson(ledger, value, true, 0);
  EXPECT_FALSE(none->complete);
  EXPECT_EQ(none->visitedValues, 0u);
}

TEST(WorldGenerationEstimateTest, RefusedEstimateDoesNotRefuseTheJsonInput) {
  WorldGenerationMemoryLedger ledger(0);
  auto value = tree();
  auto snapshot = captureWorldGenerationJson(ledger, value, true);
  EXPECT_FALSE(snapshot->complete);
  EXPECT_EQ(snapshot->value, value);
  EXPECT_EQ(ledger.snapshot().estimatedBytes, 0u);
}

TEST(WorldGenerationEstimateTest, IterativeTraversalHandlesDeepInputWithinItsBound) {
  WorldGenerationMemoryLedger ledger;
  Json value(1);
  for (unsigned i = 0; i < 512; ++i) value = JsonArray{value};
  auto snapshot = captureWorldGenerationJson(ledger, value, true, 1024);
  EXPECT_TRUE(snapshot->complete);
  EXPECT_EQ(snapshot->visitedValues, 513u);
  EXPECT_EQ(ledger.snapshot().sharedResources, 512u);
}

TEST(WorldGenerationEstimateTest, SizeArithmeticDoesNotWrap) {
  WorldGenerationSizeEstimate estimate;
  auto maximum = std::numeric_limits<size_t>::max();
  estimate.add(maximum - 1);
  estimate.add(2);
  EXPECT_FALSE(estimate.complete);
  EXPECT_EQ(estimate.bytes, maximum - 1);
  WorldGenerationSizeEstimate product;
  product.add(maximum, 2);
  EXPECT_FALSE(product.complete);
  EXPECT_EQ(product.bytes, 0u);
}

TEST(WorldGenerationEstimateTest, StringModelUsesUtf8CapacityNotCharacterCount) {
  String string("\xc3\xa1\xf0\x9f\x99\x82");
  WorldGenerationSizeEstimate estimate;
  estimate.stringCapacity(string);
  EXPECT_TRUE(estimate.complete);
  EXPECT_EQ(estimate.bytes, string.utf8().capacity() + 1);
  EXPECT_EQ(WorldGenerationSizeEstimate::ModelVersion, 2u);
}

TEST(WorldGenerationEstimateTest, ConcurrentSharedAcquireAndLastReleaseRemainUnique) {
  WorldGenerationMemoryLedger ledger(64);
  auto owner = std::make_shared<unsigned>(1);
  std::atomic<unsigned> failures{0};
  std::vector<std::thread> threads;
  for (unsigned t = 0; t < 8; ++t) threads.emplace_back([&] {
    for (unsigned i = 0; i < 1000; ++i) {
      auto token = ledger.reserveSharedJson(owner, 64);
      auto usage = ledger.snapshot();
      if (!token || usage.estimatedBytes > 64 || usage.sharedResources > 1
          || usage.sharedRegistryEntries != usage.sharedResources) ++failures;
    }
  });
  for (auto& thread : threads) thread.join();
  EXPECT_EQ(failures.load(), 0u);
  EXPECT_EQ(ledger.snapshot().reservedBytes, 0u);
  EXPECT_EQ(ledger.snapshot().sharedRegistryEntries, 0u);
  EXPECT_LE(ledger.snapshot().peakEstimatedBytes, 64u);
}

TEST(WorldGenerationEstimateTest, ConcurrentConflictsCannotDestroyATokenUnderTheLedgerLock) {
  WorldGenerationMemoryLedger ledger(64);
  auto owner = std::make_shared<unsigned>(1);
  std::vector<std::thread> threads;
  for (unsigned t = 0; t < 4; ++t) threads.emplace_back([&, t] {
    for (unsigned i = 0; i < 1000; ++i) {
      auto token = ledger.reserveSharedJson(owner, t % 2 ? 32 : 64);
    }
  });
  for (auto& thread : threads) thread.join();
  EXPECT_EQ(ledger.snapshot().reservedBytes, 0u);
  EXPECT_EQ(ledger.snapshot().sharedRegistryEntries, 0u);
}

TEST(WorldGenerationEstimateTest, OutputEstimateCanBeSizedByAWorkerAfterTheFacadeCloses) {
  auto ledger = std::make_unique<WorldGenerationMemoryLedger>();
  auto memory = ledger->reserve(Plans, 0);
  WorkerPool pool("Estimate lifecycle contract", 1);
  auto promise = pool.addProducer<bool>([memory] { return memory->resizeEstimate(128); });
  ledger.reset();
  EXPECT_TRUE(promise.get());
  pool.finish();
  memory.reset();
}

TEST(WorldGenerationEstimateTest, ReadyOutputKeepsItsCapacityEstimateUntilConsumption) {
  struct Output {
    std::shared_ptr<WorldGenerationMemoryReservation> memory;
    std::vector<String> plans;
  };
  WorldGenerationMemoryLedger ledger;
  WorkerPool pool("Estimate result contract", 1);
  auto memory = ledger.reserve(Plans, 0);
  auto promise = pool.addProducer<Output>([memory] {
    Output result{memory, {"first plan", "second plan"}};
    WorldGenerationSizeEstimate estimate;
    estimate.add(sizeof(Output));
    estimate.add(result.plans.capacity(), sizeof(String));
    for (auto const& name : result.plans) estimate.stringCapacity(name);
    if (!estimate.complete || !memory->resizeEstimate(estimate.bytes))
      throw std::runtime_error("controlled output could not be measured");
    return result;
  });
  memory.reset();
  {
    auto result = std::move(promise.get());
    pool.finish();
    auto bytes = ledger.snapshot().estimatedBytes;
    EXPECT_GT(bytes, sizeof(Output));
    result.plans.clear();
    // clear() does not free vector capacity or the still-live result group.
    EXPECT_EQ(ledger.snapshot().estimatedBytes, bytes);
  }
  EXPECT_EQ(ledger.snapshot().estimatedBytes, 0u);
}
