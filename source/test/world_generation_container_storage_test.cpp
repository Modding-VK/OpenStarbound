#include "StarBiomeItemSelection.hpp"
#include "StarWorldGenerationImmutableArray.hpp"
#include "StarWorldGenerationPayloadBuffer.hpp"
#include "StarWorldGenerationTemporaryMemory.hpp"
#include "StarWorldGenerationPlacementQueue.hpp"
#include "StarDataStreamDevices.hpp"
#include "StarStaticRandom.hpp"

#include <atomic>
#include <cassert>
#include <map>
#include <thread>

#include "gtest/gtest.h"

namespace Star {
struct TileDamageParametersTestAccess {
  static void put(TileDamageParameters& value, TileDamageType type, float factor) { value.m_damages[type] = factor; }
  static auto const& factors(TileDamageParameters const& value) { return value.m_damages; }
};
}

using namespace Star;

namespace {
struct Requests {
  std::map<void*, size_t> active;
  size_t bytes = 0, allocations = 0;
};
// FlatHashTable grows through a default-constructed vector followed by swap.
// All allocator instances in this scoped test family must be interchangeable,
// including that temporary vector's default allocator. No production allocator
// or container operation is changed to accommodate the observer.
std::shared_ptr<Requests> defaultRequests;
struct RecordingFamily {
  RecordingFamily() : previous(defaultRequests), requests(std::make_shared<Requests>()) { defaultRequests = requests; }
  ~RecordingFamily() { defaultRequests = previous; }
  std::shared_ptr<Requests> previous, requests;
};
template <typename T>
struct RecordingAllocator {
  using value_type = T;
  template <typename U> struct rebind { using other = RecordingAllocator<U>; };
  RecordingAllocator() : requests(defaultRequests) { assert(requests); }
  explicit RecordingAllocator(std::shared_ptr<Requests> value) : requests(std::move(value)) {}
  RecordingAllocator(RecordingAllocator const&) = default;
  RecordingAllocator(RecordingAllocator&& other) noexcept : requests(other.requests) {}
  RecordingAllocator& operator=(RecordingAllocator const&) = default;
  RecordingAllocator& operator=(RecordingAllocator&& other) noexcept { requests = other.requests; return *this; }
  template <typename U> RecordingAllocator(RecordingAllocator<U> const& other) noexcept : requests(other.requests) {}
  T* allocate(size_t count) {
    auto pointer = std::allocator<T>{}.allocate(count);
    try { requests->active.emplace(pointer, count * sizeof(T)); }
    catch (...) { std::allocator<T>{}.deallocate(pointer, count); throw; }
    requests->bytes += count * sizeof(T); ++requests->allocations;
    return pointer;
  }
  void deallocate(T* pointer, size_t count) noexcept {
    auto found = requests->active.find(pointer); assert(found != requests->active.end());
    requests->bytes -= found->second; requests->active.erase(found);
    std::allocator<T>{}.deallocate(pointer, count);
  }
  template <typename U> bool operator==(RecordingAllocator<U> const& other) const { return requests == other.requests; }
  template <typename U> bool operator!=(RecordingAllocator<U> const& other) const { return !(*this == other); }
  std::shared_ptr<Requests> requests;
};
struct CountHash {
  unsigned* calls = nullptr;
  size_t operator()(String const& value) const { if (calls) ++*calls; return hash<String>{}(value); }
};
using RecordedSet = FlatHashSet<String, CountHash, std::equal_to<String>, RecordingAllocator<String>>;
struct alignas(64) AlignedKey {
  unsigned value;
  bool operator==(AlignedKey const& other) const { return value == other.value; }
};
struct GetAligned { AlignedKey const& operator()(AlignedKey const& value) const { return value; } };
struct HashAligned { size_t operator()(AlignedKey const& value) const { return value.value; } };
using AlignedTable = FlatHashTable<AlignedKey, AlignedKey, GetAligned, HashAligned,
    std::equal_to<AlignedKey>, RecordingAllocator<AlignedKey>>;
using Access = TileDamageParametersTestAccess;
void fill(TileDamageParameters& value) {
  Access::put(value, TileDamageType::Plantish, 0.5f);
  Access::put(value, TileDamageType::Blockish, 1.25f);
  Access::put(value, TileDamageType::Fire, 2.0f);
}
ByteArray factorBytes(TileDamageParameters const& value) {
  DataStreamBuffer data; data.writeMapContainer(Access::factors(value)); return data.data();
}
struct ItemSizer {
  template <typename Values>
  WorldGenerationBackingEstimate operator()(Values const& values) const noexcept {
    WorldGenerationBackingEstimate result;
    for (auto const& item : values) result.add(estimateBiomeItemGenerationBacking(item));
    return result;
  }
};
}

TEST(WorldGenerationContainerStorageTest, DefaultSetHasNoBucketRequest) {
  RecordingFamily family; auto requests = family.requests;
  RecordedSet values{RecordingAllocator<String>(requests)};
  EXPECT_EQ(values.bucketStorageBytes(), 0u);
  EXPECT_EQ(requests->allocations, 0u);
  static_assert(noexcept(values.bucketStorageBytes()), "Storage query cannot throw");
}

TEST(WorldGenerationContainerStorageTest, ReservedEmptySetReportsFullPhysicalVectorRequest) {
  RecordingFamily family; auto requests = family.requests;
  { RecordedSet values{RecordingAllocator<String>(requests)};
    values.reserve(257);
    EXPECT_EQ(values.size(), 0u);
    EXPECT_EQ(values.bucketStorageBytes(), requests->bytes);
    EXPECT_GT(values.bucketStorageBytes(), 257 * sizeof(String));
    EXPECT_EQ(requests->active.size(), 1u); }
  EXPECT_EQ(requests->bytes, 0u);
}

TEST(WorldGenerationContainerStorageTest, GrowingSetMatchesRequestedStorageAtEveryInsertion) {
  RecordingFamily family; auto requests = family.requests;
  RecordedSet values{RecordingAllocator<String>(requests)};
  for (unsigned i = 0; i < 256; ++i) {
    values.insert(toString(i));
    EXPECT_EQ(values.bucketStorageBytes(), requests->bytes);
  }
}

TEST(WorldGenerationContainerStorageTest, ClearAndEraseRetainBucketsAfterKeysAreDestroyed) {
  RecordingFamily family; auto requests = family.requests;
  RecordedSet values{RecordingAllocator<String>(requests)};
  values.insert("one"); values.insert("two");
  auto retained = values.bucketStorageBytes();
  values.erase(String("one")); EXPECT_EQ(values.bucketStorageBytes(), retained);
  values.clear();
  EXPECT_EQ(values.size(), 0u);
  EXPECT_EQ(values.bucketStorageBytes(), retained);
  EXPECT_EQ(values.bucketStorageBytes(), requests->bytes);
}

TEST(WorldGenerationContainerStorageTest, QueryDoesNotHashAllocateOrChangeIteration) {
  RecordingFamily family; auto requests = family.requests; unsigned calls = 0;
  RecordedSet values(0, CountHash{&calls}, std::equal_to<String>{}, RecordingAllocator<String>(requests));
  values.insert("alpha"); values.insert("beta"); values.insert("gamma");
  std::vector<String> before(values.begin(), values.end());
  auto allocations = requests->allocations; auto hashes = calls;
  for (unsigned i = 0; i < 1000; ++i) EXPECT_EQ(values.bucketStorageBytes(), requests->bytes);
  EXPECT_EQ(calls, hashes); EXPECT_EQ(requests->allocations, allocations);
  EXPECT_EQ(std::vector<String>(values.begin(), values.end()), before);
}

TEST(WorldGenerationContainerStorageTest, CopyOwnsAnotherRequestAndCanNormalizeCapacity) {
  RecordingFamily family; auto requests = family.requests;
  RecordedSet original{RecordingAllocator<String>(requests)};
  original.reserve(257); original.insert("one"); original.insert("two");
  auto originalBytes = original.bucketStorageBytes();
  {
    RecordedSet copy(original);
    EXPECT_LT(copy.bucketStorageBytes(), originalBytes);
    EXPECT_EQ(requests->bytes, originalBytes + copy.bucketStorageBytes());
    EXPECT_EQ(copy.count("one"), 1u);
  }
  EXPECT_EQ(requests->bytes, originalBytes);
}

TEST(WorldGenerationContainerStorageTest, MoveTransfersBucketRequestWithoutSharingOwnership) {
  RecordingFamily family; auto requests = family.requests;
  RecordedSet original{RecordingAllocator<String>(requests)}; original.insert("one");
  auto before = original.bucketStorageBytes();
  RecordedSet moved(std::move(original));
  EXPECT_EQ(moved.bucketStorageBytes(), before);
  EXPECT_EQ(original.bucketStorageBytes(), 0u);
  EXPECT_EQ(requests->bytes, before);
}

TEST(WorldGenerationContainerStorageTest, OveralignedTableIncludesSentinelMetadataAndPadding) {
  RecordingFamily family; auto requests = family.requests;
  AlignedTable values(0, GetAligned{}, HashAligned{}, std::equal_to<AlignedKey>{}, RecordingAllocator<AlignedKey>(requests));
  values.reserve(100); values.insert(AlignedKey{7});
  EXPECT_EQ(values.bucketStorageBytes(), requests->bytes);
  EXPECT_GT(values.bucketStorageBytes(), 100 * sizeof(AlignedKey));
  EXPECT_EQ(values.bucketStorageBytes() % alignof(AlignedKey), 0u);
}

TEST(WorldGenerationContainerStorageTest, NestedStringHeapIsNotIncludedInBucketPayload) {
  RecordingFamily family; auto requests = family.requests;
  RecordedSet shortKey{RecordingAllocator<String>(requests)}, longKey{RecordingAllocator<String>(requests)};
  shortKey.insert("a"); longKey.insert(String(std::string(8192, 'x')));
  EXPECT_EQ(shortKey.bucketStorageBytes(), longKey.bucketStorageBytes());
  EXPECT_EQ(shortKey.bucketStorageBytes() + longKey.bucketStorageBytes(), requests->bytes);
}

TEST(WorldGenerationContainerStorageTest, NativeNameModelReplacesOccupiedObjectsWithFullBuckets) {
  BiomeItem item{MicroDungeonNames(StringSet{"one", "two", "three"})};
  auto& names = item.get<MicroDungeonNames>(); names.reserve(257);
  WorldGenerationSizeEstimate expected; expected.add(names.bucketStorageBytes());
  for (auto const& name : names) expected.stringCapacity(name);
  auto before = names.values();
  auto actual = estimateBiomeItemGenerationBacking(item);
  EXPECT_EQ(actual.storage.bytes, expected.bytes);
  EXPECT_GT(names.bucketStorageBytes(), names.size() * sizeof(String));
  EXPECT_EQ(actual.opaqueItems, 0u);
  EXPECT_EQ(names.values(), before);
}

TEST(WorldGenerationContainerStorageTest, CopiedNativeNamesReconcileToTheirOwnCapacityInSnapshot) {
  WorldGenerationMemoryLedger ledger;
  std::vector<BiomeItem> input;
  input.emplace_back(MicroDungeonNames(StringSet{"one", "two"}));
  input[0].get<MicroDungeonNames>().reserve(257);
  auto original = ItemSizer{}(input).storage.bytes;
  auto snapshot = makeWorldGenerationImmutableArray<BiomeItem>(ledger, input, ItemSizer{});
  auto actual = ItemSizer{}(snapshot->values());
  EXPECT_LT(actual.storage.bytes, original);
  EXPECT_EQ(ledger.snapshot().classBytes[static_cast<size_t>(WorldGenerationMemoryClass::MicroDungeonDistributionEstimate)], actual.storage.bytes);
  snapshot.reset();
  EXPECT_EQ(ledger.snapshot().reservedBytes, 0u);
}

TEST(WorldGenerationContainerStorageTest, EmptyButReservedNativeNamesStayCountedUntilElementDestruction) {
  WorldGenerationMemoryLedger ledger;
  WorldGenerationTemporaryMemory memory(ledger);
  WorldGenerationScopedTemporary<List<BiomeItemPlacement>, BiomeItemListBackingSizer> output(
      memory, WorldGenerationTemporarySlot::Output, {}, [] { return List<BiomeItemPlacement>{}; }, BiomeItemListBackingSizer{});
  MicroDungeonNames names; names.reserve(257); auto buckets = names.bucketStorageBytes();
  output.mutate([&](auto& values) { values.append({BiomeItem{std::move(names)}, Vec2I(1, 7), 0.5f}); });
  auto raw = output.value().capacity() * sizeof(BiomeItemPlacement);
  EXPECT_EQ(output.knownBacking().storage.bytes, raw + buckets);
  output.mutate([](auto& values) { values.clear(); });
  EXPECT_EQ(output.knownBacking().storage.bytes, raw);
  EXPECT_EQ(ledger.snapshot().estimatedBytes, raw);
}

TEST(WorldGenerationContainerStorageTest, DefaultDamageFactorsHaveNoOccupiedPayload) {
  TileDamageParameters value;
  EXPECT_EQ(value.damageFactorPayloadBytes(), 0u);
  static_assert(noexcept(value.damageFactorPayloadBytes()), "Factor query cannot throw");
}

TEST(WorldGenerationContainerStorageTest, DamageFactorPayloadCountsPairsNotGuessedNodeSizes) {
  TileDamageParameters value; fill(value);
  EXPECT_EQ(value.damageFactorPayloadBytes(), 3 * sizeof(Map<TileDamageType, float>::value_type));
  EXPECT_EQ(Access::factors(value).size(), 3u);
}

TEST(WorldGenerationContainerStorageTest, FactorQueriesPreserveOrderValuesAndSerializedFactorBytes) {
  TileDamageParameters value; fill(value);
  auto before = factorBytes(value);
  for (unsigned i = 0; i < 1000; ++i) EXPECT_GT(value.damageFactorPayloadBytes(), 0u);
  EXPECT_EQ(factorBytes(value), before);
  EXPECT_FLOAT_EQ(Access::factors(value).get(TileDamageType::Blockish), 1.25f);
}

TEST(WorldGenerationContainerStorageTest, CopiedFactorMapsOwnIndependentPayloads) {
  TileDamageParameters value; fill(value);
  auto copy = value;
  EXPECT_EQ(copy.damageFactorPayloadBytes(), value.damageFactorPayloadBytes());
  Access::put(copy, TileDamageType::Tilling, 3.0f);
  EXPECT_GT(copy.damageFactorPayloadBytes(), value.damageFactorPayloadBytes());
  EXPECT_EQ(Access::factors(value).size(), 3u);
}

TEST(WorldGenerationContainerStorageTest, GrassModelIncludesFactorsWhileJsonAndNodeOverheadStayOpaque) {
  GrassVariant value;
  auto before = estimateBiomeItemGenerationBacking(BiomeItem{value});
  fill(value.tileDamageParameters);
  auto after = estimateBiomeItemGenerationBacking(BiomeItem{value});
  EXPECT_EQ(after.storage.bytes - before.storage.bytes, value.tileDamageParameters.damageFactorPayloadBytes());
  EXPECT_EQ(after.opaqueItems, 1u);
}

TEST(WorldGenerationContainerStorageTest, BushModelIncludesItsFactorPayloadWithoutChangingValues) {
  BushVariant value;
  auto before = estimateBiomeItemGenerationBacking(BiomeItem{value});
  fill(value.tileDamageParameters); auto factorState = factorBytes(value.tileDamageParameters);
  auto after = estimateBiomeItemGenerationBacking(BiomeItem{value});
  EXPECT_EQ(after.storage.bytes - before.storage.bytes, value.tileDamageParameters.damageFactorPayloadBytes());
  EXPECT_EQ(factorBytes(value.tileDamageParameters), factorState);
  EXPECT_EQ(after.opaqueItems, 1u);
}

TEST(WorldGenerationContainerStorageTest, TreePairModelCountsBothMapsWithoutDroppingJsonOpacity) {
  TreePair value;
  auto before = estimateBiomeItemGenerationBacking(BiomeItem{value});
  fill(value.first.tileDamageParameters); fill(value.second.tileDamageParameters);
  auto after = estimateBiomeItemGenerationBacking(BiomeItem{value});
  EXPECT_EQ(after.storage.bytes - before.storage.bytes,
      value.first.tileDamageParameters.damageFactorPayloadBytes() + value.second.tileDamageParameters.damageFactorPayloadBytes());
  EXPECT_EQ(after.opaqueItems, 1u);
}

TEST(WorldGenerationContainerStorageTest, ConcurrentReadOnlyQueriesDoNotMutateStorageOrFactors) {
  RecordingFamily family; auto requests = family.requests;
  RecordedSet values{RecordingAllocator<String>(requests)}; values.insert("one"); values.reserve(257);
  TileDamageParameters factors; fill(factors);
  auto expected = values.bucketStorageBytes(), factorPayload = factors.damageFactorPayloadBytes();
  auto allocations = requests->allocations;
  std::atomic<unsigned> failed{0}; std::vector<std::thread> readers;
  for (unsigned i = 0; i < 8; ++i) readers.emplace_back([&] {
    for (unsigned j = 0; j < 1000; ++j)
      if (values.bucketStorageBytes() != expected || factors.damageFactorPayloadBytes() != factorPayload) ++failed;
  });
  for (auto& reader : readers) reader.join();
  EXPECT_EQ(failed.load(), 0u); EXPECT_EQ(requests->allocations, allocations);
}
