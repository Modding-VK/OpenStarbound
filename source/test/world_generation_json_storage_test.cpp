#include "StarWorldGenerationMemoryEstimate.hpp"
#include "StarDataStreamDevices.hpp"
#include "StarWorkerPool.hpp"

#include <atomic>
#include <thread>

#include "gtest/gtest.h"

using namespace Star;

namespace {
size_t objectBytes(Json const& value) {
  auto object = value.objectPtr();
  WorldGenerationSizeEstimate estimate;
  estimate.add(sizeof(JsonObject)); estimate.add(object->bucketStorageBytes());
  for (auto const& entry : *object) estimate.stringCapacity(entry.first);
  return estimate.bytes;
}
size_t stringBytes(Json const& value) {
  return sizeof(String) + value.stringPtr()->utf8().capacity() + 1;
}
size_t arrayBytes(Json const& value) {
  return sizeof(JsonArray) + value.arrayPtr()->capacity() * sizeof(Json);
}
Json object(unsigned keys = 3) {
  JsonObject result;
  for (unsigned i = 0; i < keys; ++i) result.set(toString(i), Json(i));
  return Json(std::move(result));
}
ByteArray serialized(Json const& value) {
  DataStreamBuffer data; data << value; return data.data();
}
}

TEST(WorldGenerationJsonStorageTest, FullObjectUsesBucketsAndKeysWithoutDuplicateInlinePairs) {
  WorldGenerationMemoryLedger ledger; auto value = object();
  auto snapshot = captureWorldGenerationJson(ledger, value, true);
  ASSERT_TRUE(snapshot->complete);
  EXPECT_EQ(ledger.snapshot().estimatedBytes, objectBytes(value));
  EXPECT_EQ(snapshot->sizedObjectKeys, 3u); EXPECT_EQ(snapshot->omittedObjects, 0u);
  EXPECT_EQ(WorldGenerationSizeEstimate::JsonModelVersion, 2u);
}

TEST(WorldGenerationJsonStorageTest, ReservedEmptyObjectRetainsItsWholeBucketRequest) {
  JsonObject values; values.reserve(257);
  Json value(std::move(values)); WorldGenerationMemoryLedger ledger;
  auto snapshot = captureWorldGenerationJson(ledger, value, true, 1);
  EXPECT_TRUE(snapshot->complete); EXPECT_EQ(snapshot->visitedValues, 1u);
  EXPECT_EQ(snapshot->sizedObjectKeys, 0u);
  EXPECT_GT(value.objectPtr()->bucketStorageBytes(), 0u);
  EXPECT_EQ(ledger.snapshot().estimatedBytes, objectBytes(value));
}

TEST(WorldGenerationJsonStorageTest, ClearedObjectStillChargesTheRetainedVector) {
  JsonObject values; values.set("discarded", Json(1)); values.clear();
  Json value(std::move(values)); WorldGenerationMemoryLedger ledger;
  auto snapshot = captureWorldGenerationJson(ledger, value, true);
  EXPECT_TRUE(snapshot->complete); EXPECT_GT(value.objectPtr()->bucketStorageBytes(), 0u);
  EXPECT_EQ(ledger.snapshot().estimatedBytes, objectBytes(value));
}

TEST(WorldGenerationJsonStorageTest, LongAndUtf8KeysUseByteCapacityNotCharacterCount) {
  Json value(JsonObject{{String(std::string(8192, 'k')), Json(1)}, {"\xc3\xa1\xf0\x9f\x99\x82", Json(true)}});
  WorldGenerationMemoryLedger ledger;
  auto snapshot = captureWorldGenerationJson(ledger, value, true);
  EXPECT_TRUE(snapshot->complete); EXPECT_EQ(snapshot->sizedObjectKeys, 2u);
  EXPECT_EQ(ledger.snapshot().estimatedBytes, objectBytes(value));
  EXPECT_GT(ledger.snapshot().estimatedBytes, 8192u);
}

TEST(WorldGenerationJsonStorageTest, CapturePreservesOwnerOrderContentAndSerializedBytes) {
  auto value = object(32); auto before = serialized(value);
  auto keys = value.objectPtr()->keys(); auto capacity = value.objectPtr()->bucketStorageBytes();
  WorldGenerationMemoryLedger ledger;
  auto snapshot = captureWorldGenerationJson(ledger, value, true);
  EXPECT_TRUE(snapshot->complete); EXPECT_EQ(snapshot->value.objectPtr(), value.objectPtr());
  EXPECT_EQ(serialized(snapshot->value), before); EXPECT_EQ(serialized(value), before);
  EXPECT_EQ(value.objectPtr()->keys(), keys);
  EXPECT_EQ(value.objectPtr()->bucketStorageBytes(), capacity);
}

TEST(WorldGenerationJsonStorageTest, RepeatedObjectChargesItsKeysOnlyOnce) {
  auto child = object(); Json root(JsonArray{child, child, child});
  WorldGenerationMemoryLedger ledger;
  auto snapshot = captureWorldGenerationJson(ledger, root, true);
  EXPECT_TRUE(snapshot->complete); EXPECT_EQ(snapshot->sizedObjectKeys, 3u);
  EXPECT_EQ(ledger.snapshot().sharedResources, 2u);
  EXPECT_EQ(ledger.snapshot().estimatedBytes, arrayBytes(root) + objectBytes(child));
}

TEST(WorldGenerationJsonStorageTest, EqualButIndependentObjectsKeepDistinctBackingCharges) {
  auto left = object(), right = object(); EXPECT_EQ(left, right);
  ASSERT_NE(left.objectPtr(), right.objectPtr()); Json root(JsonArray{left, right});
  WorldGenerationMemoryLedger ledger;
  auto snapshot = captureWorldGenerationJson(ledger, root, true);
  EXPECT_TRUE(snapshot->complete); EXPECT_EQ(snapshot->sizedObjectKeys, 6u);
  EXPECT_EQ(ledger.snapshot().sharedResources, 3u);
  EXPECT_EQ(ledger.snapshot().estimatedBytes, arrayBytes(root) + objectBytes(left) + objectBytes(right));
}

TEST(WorldGenerationJsonStorageTest, DifferentObjectRootsDeduplicateTheSharedLeaf) {
  Json leaf(std::string(4096, 'v'));
  Json left(JsonObject{{"first", leaf}}), right(JsonObject{{"second", leaf}});
  WorldGenerationMemoryLedger ledger;
  auto first = captureWorldGenerationJson(ledger, left, true);
  auto second = captureWorldGenerationJson(ledger, right, true);
  EXPECT_TRUE(first->complete); EXPECT_TRUE(second->complete);
  EXPECT_EQ(ledger.snapshot().estimatedBytes, objectBytes(left) + objectBytes(right) + stringBytes(leaf));
  first.reset(); EXPECT_EQ(ledger.snapshot().estimatedBytes, objectBytes(right) + stringBytes(leaf));
  second.reset(); EXPECT_EQ(ledger.snapshot().reservedBytes, 0u);
}

TEST(WorldGenerationJsonStorageTest, RepeatedCapturesKeepOneObjectChargeUntilLastHandle) {
  auto value = object(); WorldGenerationMemoryLedger ledger;
  auto first = captureWorldGenerationJson(ledger, value, true);
  auto second = captureWorldGenerationJson(ledger, value, true);
  EXPECT_EQ(ledger.snapshot().estimatedBytes, objectBytes(value));
  first.reset(); EXPECT_EQ(ledger.snapshot().estimatedBytes, objectBytes(value));
  second.reset(); EXPECT_EQ(ledger.snapshot().sharedRegistryEntries, 0u);
  EXPECT_EQ(value.size(), 3u); // Ordinary Json ownership is outside tracked handles.
}

TEST(WorldGenerationJsonStorageTest, DisabledMeasurementDoesNotSizeOrChargeWideObjects) {
  auto value = object(1024); WorldGenerationMemoryLedger ledger;
  auto snapshot = captureWorldGenerationJson(ledger, value, false);
  EXPECT_FALSE(snapshot->measured); EXPECT_FALSE(snapshot->complete);
  EXPECT_EQ(snapshot->visitedValues, 0u); EXPECT_EQ(snapshot->sizedObjectKeys, 0u);
  EXPECT_EQ(snapshot->omittedObjects, 0u); EXPECT_EQ(ledger.snapshot().resources, 0u);
  EXPECT_EQ(snapshot->value.objectPtr(), value.objectPtr());
}

TEST(WorldGenerationJsonStorageTest, ZeroTraversalAllowanceLeavesTheEntireInputUntouched) {
  auto value = object(1024); WorldGenerationMemoryLedger ledger;
  auto snapshot = captureWorldGenerationJson(ledger, value, true, 0);
  EXPECT_FALSE(snapshot->complete); EXPECT_EQ(snapshot->visitedValues, 0u);
  EXPECT_EQ(snapshot->sizedObjectKeys, 0u); EXPECT_EQ(snapshot->omittedObjects, 0u);
  EXPECT_EQ(ledger.snapshot().resources, 0u); EXPECT_EQ(snapshot->value.objectPtr(), value.objectPtr());
}

TEST(WorldGenerationJsonStorageTest, ExactTraversalAllowanceIncludesAllObjectKeys) {
  auto value = object(); WorldGenerationMemoryLedger ledger;
  auto snapshot = captureWorldGenerationJson(ledger, value, true, 4);
  EXPECT_TRUE(snapshot->complete); EXPECT_EQ(snapshot->visitedValues, 4u);
  EXPECT_EQ(snapshot->sizedObjectKeys, 3u); EXPECT_EQ(snapshot->omittedObjects, 0u);
  EXPECT_EQ(ledger.snapshot().estimatedBytes, objectBytes(value));
}

TEST(WorldGenerationJsonStorageTest, InsufficientAllowanceDoesNotRegisterAPartialObjectModel) {
  auto value = object(); WorldGenerationMemoryLedger ledger;
  auto snapshot = captureWorldGenerationJson(ledger, value, true, 3);
  EXPECT_FALSE(snapshot->complete); EXPECT_EQ(snapshot->visitedValues, 3u);
  EXPECT_EQ(snapshot->sizedObjectKeys, 0u); EXPECT_EQ(snapshot->omittedObjects, 1u);
  EXPECT_EQ(ledger.snapshot().sharedResources, 0u); EXPECT_EQ(snapshot->value, value);
}

TEST(WorldGenerationJsonStorageTest, PartialThenFullCaptureCannotPoisonTheSharedByteModel) {
  auto value = object(); WorldGenerationMemoryLedger ledger;
  auto partial = captureWorldGenerationJson(ledger, value, true, 1);
  auto full = captureWorldGenerationJson(ledger, value, true);
  EXPECT_FALSE(partial->complete); EXPECT_TRUE(full->complete);
  EXPECT_EQ(ledger.snapshot().estimatedBytes, objectBytes(value));
  full.reset(); EXPECT_EQ(ledger.snapshot().reservedBytes, 0u);
  EXPECT_EQ(partial->value, value);
}

TEST(WorldGenerationJsonStorageTest, FullThenPartialCaptureDoesNotConflictOrResizeTheCharge) {
  auto value = object(); WorldGenerationMemoryLedger ledger;
  auto full = captureWorldGenerationJson(ledger, value, true);
  auto partial = captureWorldGenerationJson(ledger, value, true, 1);
  EXPECT_TRUE(full->complete); EXPECT_FALSE(partial->complete);
  EXPECT_EQ(ledger.snapshot().estimatedBytes, objectBytes(value));
  partial.reset(); EXPECT_EQ(ledger.snapshot().estimatedBytes, objectBytes(value));
}

TEST(WorldGenerationJsonStorageTest, OmittedObjectStillAllowsBoundedChildMeasurement) {
  Json leaf(std::string(1024, 'v')); Json value(JsonObject{{"one", leaf}, {"two", leaf}});
  WorldGenerationMemoryLedger ledger;
  auto snapshot = captureWorldGenerationJson(ledger, value, true, 2);
  EXPECT_FALSE(snapshot->complete); EXPECT_EQ(snapshot->visitedValues, 2u);
  EXPECT_EQ(snapshot->omittedObjects, 1u); EXPECT_EQ(snapshot->sizedObjectKeys, 0u);
  EXPECT_EQ(ledger.snapshot().estimatedBytes, stringBytes(leaf));
  EXPECT_EQ(snapshot->value, value);
}

TEST(WorldGenerationJsonStorageTest, ExactFiniteLimitAcceptsRepeatedCanonicalObjectCharge) {
  auto value = object(); WorldGenerationMemoryLedger ledger(objectBytes(value));
  auto first = captureWorldGenerationJson(ledger, value, true);
  auto second = captureWorldGenerationJson(ledger, value, true);
  EXPECT_TRUE(first->complete); EXPECT_TRUE(second->complete);
  EXPECT_EQ(ledger.snapshot().estimatedBytes, objectBytes(value));
  EXPECT_EQ(ledger.snapshot().sharedResources, 1u);
}

TEST(WorldGenerationJsonStorageTest, RefusedObjectChargeDoesNotRefuseTheInputOrHideItsKeys) {
  auto value = object(); WorldGenerationMemoryLedger ledger(objectBytes(value) - 1);
  auto snapshot = captureWorldGenerationJson(ledger, value, true);
  EXPECT_FALSE(snapshot->complete); EXPECT_EQ(snapshot->omittedObjects, 0u);
  EXPECT_EQ(snapshot->sizedObjectKeys, 3u); EXPECT_EQ(ledger.snapshot().reservedBytes, 0u);
  EXPECT_EQ(snapshot->value.objectPtr(), value.objectPtr());
}

TEST(WorldGenerationJsonStorageTest, WorkerCaptureOutlivesTheOriginalLedgerFacade) {
  auto original = std::make_unique<WorldGenerationMemoryLedger>(); auto view = *original;
  auto value = object(); WorkerPool pool("Json storage lifecycle", 1);
  {
    auto promise = pool.addProducer<WorldGenerationJsonSnapshotPtr>([ledger = *original, value]() mutable {
      return captureWorldGenerationJson(ledger, value, true);
    });
    original.reset();
    auto snapshot = promise.get(); pool.finish();
    EXPECT_TRUE(snapshot->complete); EXPECT_EQ(view.snapshot().estimatedBytes, objectBytes(value));
  }
  EXPECT_EQ(view.snapshot().reservedBytes, 0u);
}

TEST(WorldGenerationJsonStorageTest, EightConcurrentCapturesShareOneCanonicalObjectModel) {
  auto value = object(64); auto before = serialized(value); WorldGenerationMemoryLedger ledger;
  std::array<WorldGenerationJsonSnapshotPtr, 8> snapshots;
  std::vector<std::thread> threads; std::atomic<unsigned> failed{0};
  for (unsigned i = 0; i < snapshots.size(); ++i) threads.emplace_back([&, i] {
    snapshots[i] = captureWorldGenerationJson(ledger, value, true);
    if (!snapshots[i]->complete) ++failed;
  });
  for (auto& thread : threads) thread.join();
  EXPECT_EQ(failed.load(), 0u); EXPECT_EQ(ledger.snapshot().sharedResources, 1u);
  EXPECT_EQ(ledger.snapshot().estimatedBytes, objectBytes(value));
  EXPECT_EQ(serialized(value), before);
  for (auto& snapshot : snapshots) snapshot.reset();
  EXPECT_EQ(ledger.snapshot().reservedBytes, 0u); EXPECT_EQ(ledger.snapshot().sharedRegistryEntries, 0u);
}

TEST(WorldGenerationJsonStorageTest, DeepObjectsAreMeasuredIterativelyWithinTheSameValueBound) {
  Json value(1); size_t expected = 0;
  for (unsigned i = 0; i < 512; ++i) {
    value = JsonObject{{"child", value}}; expected += objectBytes(value);
  }
  WorldGenerationMemoryLedger ledger;
  auto snapshot = captureWorldGenerationJson(ledger, value, true, 513);
  EXPECT_TRUE(snapshot->complete); EXPECT_EQ(snapshot->visitedValues, 513u);
  EXPECT_EQ(snapshot->sizedObjectKeys, 512u); EXPECT_EQ(snapshot->omittedObjects, 0u);
  EXPECT_EQ(ledger.snapshot().estimatedBytes, expected);
}

TEST(WorldGenerationJsonStorageTest, ParentModelRemainsCanonicalWhenADescendantIsOmitted) {
  auto wide = object(128); Json value(JsonObject{{"child", wide}});
  WorldGenerationMemoryLedger ledger;
  auto partial = captureWorldGenerationJson(ledger, value, true, 2);
  EXPECT_FALSE(partial->complete); EXPECT_EQ(partial->sizedObjectKeys, 1u);
  EXPECT_EQ(partial->omittedObjects, 1u);
  EXPECT_EQ(ledger.snapshot().estimatedBytes, objectBytes(value));
  auto full = captureWorldGenerationJson(ledger, value, true);
  EXPECT_TRUE(full->complete);
  EXPECT_EQ(ledger.snapshot().estimatedBytes, objectBytes(value) + objectBytes(wide));
}

TEST(WorldGenerationVariantJsonTest, EmptyForestDoesNotCreateASyntheticArrayCharge) {
  WorldGenerationMemoryLedger ledger;
  auto capture = captureWorldGenerationJsonRoots(ledger, WorldGenerationJsonRoots{});
  EXPECT_TRUE(capture->complete); EXPECT_TRUE(capture->measured);
  EXPECT_TRUE(capture->roots.empty()); EXPECT_EQ(capture->visitedValues, 0u);
  EXPECT_EQ(ledger.snapshot().resources, 0u);
  EXPECT_EQ(WorldGenerationSizeEstimate::DistributionJsonModelVersion, 1u);
}

TEST(WorldGenerationVariantJsonTest, ScalarForestRootsHaveNoBackingOrCollectionCost) {
  WorldGenerationMemoryLedger ledger; WorldGenerationJsonRoots roots(0);
  for (auto value : {Json(), Json(true), Json(1), Json(0.5)}) EXPECT_TRUE(roots.append(value));
  auto capture = captureWorldGenerationJsonRoots(ledger, std::move(roots));
  EXPECT_TRUE(capture->complete); EXPECT_TRUE(capture->rootsComplete);
  EXPECT_EQ(capture->visitedValues, 0u); EXPECT_EQ(ledger.snapshot().resources, 0u);
}

TEST(WorldGenerationVariantJsonTest, RepeatedForestRootsChargeOnePhysicalString) {
  WorldGenerationMemoryLedger ledger; WorldGenerationJsonRoots roots;
  Json leaf(std::string(4096, 'v'));
  for (unsigned i = 0; i < 5; ++i) ASSERT_TRUE(roots.append(leaf));
  auto capture = captureWorldGenerationJsonRoots(ledger, std::move(roots));
  EXPECT_TRUE(capture->complete); EXPECT_EQ(capture->roots.size(), 5u);
  EXPECT_EQ(capture->visitedValues, 5u); EXPECT_EQ(ledger.snapshot().sharedResources, 1u);
  EXPECT_EQ(ledger.snapshot().estimatedBytes, stringBytes(leaf));
}

TEST(WorldGenerationVariantJsonTest, ForestAndTemplateCapturesShareTheSameOwnerRegistry) {
  WorldGenerationMemoryLedger ledger; auto value = object();
  auto single = captureWorldGenerationJson(ledger, value, true);
  WorldGenerationJsonRoots roots; ASSERT_TRUE(roots.append(value));
  auto forest = captureWorldGenerationJsonRoots(ledger, std::move(roots));
  EXPECT_TRUE(forest->complete); EXPECT_EQ(ledger.snapshot().sharedResources, 1u);
  EXPECT_EQ(ledger.snapshot().estimatedBytes, objectBytes(value));
  single.reset(); EXPECT_EQ(ledger.snapshot().estimatedBytes, objectBytes(value));
  forest.reset(); EXPECT_EQ(ledger.snapshot().reservedBytes, 0u);
}

TEST(WorldGenerationVariantJsonTest, EqualIndependentForestRootsRemainDistinct) {
  WorldGenerationMemoryLedger ledger; auto left = object(), right = object();
  WorldGenerationJsonRoots roots; ASSERT_TRUE(roots.append(left)); ASSERT_TRUE(roots.append(right));
  auto capture = captureWorldGenerationJsonRoots(ledger, std::move(roots));
  EXPECT_TRUE(capture->complete); EXPECT_EQ(ledger.snapshot().sharedResources, 2u);
  EXPECT_EQ(ledger.snapshot().estimatedBytes, objectBytes(left) + objectBytes(right));
}

TEST(WorldGenerationVariantJsonTest, RootCollectionLimitDoesNotPoisonALaterCompleteCapture) {
  WorldGenerationMemoryLedger ledger; Json one("first"), two("second");
  WorldGenerationJsonRoots roots(1); ASSERT_TRUE(roots.append(one)); EXPECT_FALSE(roots.append(two));
  auto partial = captureWorldGenerationJsonRoots(ledger, std::move(roots));
  EXPECT_FALSE(partial->complete); EXPECT_FALSE(partial->rootsComplete);
  EXPECT_EQ(partial->roots.size(), 1u); EXPECT_EQ(partial->visitedValues, 1u);
  WorldGenerationJsonRoots all; ASSERT_TRUE(all.append(one)); ASSERT_TRUE(all.append(two));
  auto full = captureWorldGenerationJsonRoots(ledger, std::move(all));
  EXPECT_TRUE(full->complete);
  EXPECT_EQ(ledger.snapshot().estimatedBytes, stringBytes(one) + stringBytes(two));
}

TEST(WorldGenerationVariantJsonTest, ForestSharesOneTraversalAllowanceAcrossAllRoots) {
  WorldGenerationMemoryLedger ledger; auto child = object(); Json leaf("leaf");
  WorldGenerationJsonRoots roots(2); ASSERT_TRUE(roots.append(child)); ASSERT_TRUE(roots.append(leaf));
  auto capture = captureWorldGenerationJsonRoots(ledger, std::move(roots));
  EXPECT_TRUE(capture->rootsComplete); EXPECT_FALSE(capture->complete);
  EXPECT_EQ(capture->visitedValues, 2u); EXPECT_EQ(capture->omittedObjects, 1u);
  EXPECT_EQ(ledger.snapshot().estimatedBytes, stringBytes(leaf));
  EXPECT_EQ(child.size(), 3u);
}

TEST(WorldGenerationVariantJsonTest, ForestKeepsRootsAliveUntilItsAccountingHandleDies) {
  WorldGenerationMemoryLedger ledger; std::weak_ptr<String const> weak;
  WorldGenerationJsonSnapshotPtr capture;
  {
    Json leaf(std::string(4096, 'v')); weak = leaf.stringPtr();
    WorldGenerationJsonRoots roots; ASSERT_TRUE(roots.append(leaf));
    capture = captureWorldGenerationJsonRoots(ledger, std::move(roots));
  }
  EXPECT_FALSE(weak.expired()); EXPECT_GT(ledger.snapshot().reservedBytes, 0u);
  capture.reset(); EXPECT_TRUE(weak.expired()); EXPECT_EQ(ledger.snapshot().reservedBytes, 0u);
}

TEST(WorldGenerationVariantJsonTest, RefusedForestEstimatePreservesItsRootHandles) {
  WorldGenerationMemoryLedger ledger(0); Json leaf("retained");
  WorldGenerationJsonRoots roots; ASSERT_TRUE(roots.append(leaf));
  auto capture = captureWorldGenerationJsonRoots(ledger, std::move(roots));
  EXPECT_FALSE(capture->complete); ASSERT_EQ(capture->roots.size(), 1u);
  EXPECT_EQ(capture->roots[0].stringPtr(), leaf.stringPtr());
  EXPECT_EQ(ledger.snapshot().resources, 0u);
}

TEST(WorldGenerationVariantJsonTest, ConcurrentForestsShareBackingWithoutSharedMutableScannerState) {
  WorldGenerationMemoryLedger ledger; auto value = object(64);
  std::array<WorldGenerationJsonSnapshotPtr, 8> captures; std::vector<std::thread> threads;
  for (unsigned i = 0; i < captures.size(); ++i) threads.emplace_back([&, i] {
    WorldGenerationJsonRoots roots; roots.append(value);
    captures[i] = captureWorldGenerationJsonRoots(ledger, std::move(roots));
  });
  for (auto& thread : threads) thread.join();
  for (auto const& capture : captures) EXPECT_TRUE(capture->complete);
  EXPECT_EQ(ledger.snapshot().sharedResources, 1u);
  EXPECT_EQ(ledger.snapshot().estimatedBytes, objectBytes(value));
  for (auto& capture : captures) capture.reset();
  EXPECT_EQ(ledger.snapshot().sharedRegistryEntries, 0u);
}
