#include "StarBiomePlacementTypes.hpp"
#include "StarWorldGenerationImmutableArray.hpp"
#include "StarWorldGenerationPlacementQueue.hpp"
#include "StarWorkerPool.hpp"
#include "StarStaticRandom.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

#include "gtest/gtest.h"

namespace Star {
// Typed fixtures bypass config/assets, not the production sampler or copier.
struct BiomeItemDistributionTestAccess {
  static BiomeItemDistribution random() {
    BiomeItemDistribution result;
    result.m_blockProbability = 1;
    result.m_blockSeed = 31;
    result.m_priority = 0.25f;
    result.m_randomItems.append(BiomeItem{MicroDungeonNames(StringSet{"alpha", "beta", "gamma", "delta"})});
    result.m_randomItems.first().get<MicroDungeonNames>().reserve(257);
    return result;
  }
  static BiomeItemDistribution periodic() {
    auto result = random();
    result.m_distribution = BiomeItemDistribution::DistributionType::Periodic;
    result.m_randomItems.clear();
    result.m_densityFunction = PerlinF(1, 0.03f, 0, 1, 2, 2, 41);
    result.m_modulusDistortion = PerlinF(1, 0.03f, 0, 0, 2, 2, 43);
    result.m_modulus = 3;
    result.m_weightedItems.append({BiomeItem{MicroDungeonNames(StringSet{"one", "two"})},
        PerlinF(2, 0.04f, 0.2f, 0.7f, 2, 2, 47)});
    result.m_weightedItems.append({BiomeItem{MicroDungeonNames(StringSet{"three", "four"})},
        PerlinF(2, 0.04f, 0.2f, 0.4f, 2, 2, 53)});
    return result;
  }
  static BiomeItemDistribution opaque() {
    BiomeItemDistribution result;
    result.m_blockProbability = 1;
    result.m_blockSeed = 17;
    result.m_randomItems.append(BiomeItem{TreasureBoxSet(String("opaque treasure name"))});
    return result;
  }
};
}

using namespace Star;

namespace {
struct Counts { unsigned copies = 0; bool fail = false; };
struct Probe {
  Probe(unsigned value, std::shared_ptr<Counts> counts) : value(value), counts(std::move(counts)), heap(64, value) {}
  Probe(Probe const& other) : value(other.value), counts(other.counts), heap(other.heap) {
    ++counts->copies;
    if (counts->fail) throw std::runtime_error("controlled distribution copy failure");
  }
  Probe(Probe&&) noexcept = default;
  unsigned value;
  std::shared_ptr<Counts> counts;
  std::vector<unsigned> heap;
};
struct ProbeSizer {
  template <typename Values>
  WorldGenerationBackingEstimate operator()(Values const& values) const {
    WorldGenerationBackingEstimate result;
    for (auto const& value : values) result.storage.add(value.heap.capacity(), sizeof(unsigned));
    return result;
  }
};
struct DistributionSizer {
  template <typename Values>
  WorldGenerationBackingEstimate operator()(Values const& values) const {
    WorldGenerationBackingEstimate result;
    for (auto const& value : values) result.add(value.generationBackingEstimate());
    return result;
  }
};
using Cache = WorldGenerationSnapshotCache<Probe, std::vector<Probe>>;
std::vector<Probe> source(std::shared_ptr<Counts> const& counts) {
  std::vector<Probe> result;
  result.reserve(3);
  for (unsigned i = 0; i < 3; ++i) result.emplace_back(i, counts);
  return result;
}
}

TEST(WorldGenerationDistributionTest, PerlinReportsKnownArraysAcrossCopyMoveAndEmptyState) {
  PerlinF empty;
  EXPECT_EQ(empty.allocatedStorageBytes(), 0u);
  PerlinF noise(2, 0.03f, 0.5f, 0, 2, 2, 17);
  auto bytes = (2 * PerlinSampleSize + 2) * (sizeof(int) + 6 * sizeof(float));
  EXPECT_EQ(noise.allocatedStorageBytes(), bytes);
  PerlinF copy(noise);
  PerlinF moved(std::move(copy));
  EXPECT_EQ(copy.allocatedStorageBytes(), 0u);
  EXPECT_EQ(moved.allocatedStorageBytes(), bytes);
  for (int x = -8; x < 8; ++x) EXPECT_FLOAT_EQ(noise.get(x * 0.5f, 1.5f), moved.get(x * 0.5f, 1.5f));
}

TEST(WorldGenerationDistributionTest, InputAdoptionIsLazyAndFirstFreezeCopiesOnlyOnce) {
  WorldGenerationMemoryLedger ledger;
  auto counts = std::make_shared<Counts>();
  Cache input(ledger, source(counts), ProbeSizer{});
  EXPECT_FALSE(input.frozen());
  EXPECT_EQ(counts->copies, 0u);
  EXPECT_GT(ledger.snapshot().estimatedBytes, 0u);
  EXPECT_EQ(ledger.snapshot().reservedBufferBytes, 0u);
  auto first = input.freeze(ProbeSizer{});
  auto bytes = ledger.snapshot().reservedBytes;
  EXPECT_EQ(counts->copies, 3u);
  EXPECT_TRUE(input.source().empty());
  for (unsigned i = 0; i < 64; ++i) EXPECT_EQ(input.freeze(ProbeSizer{}), first);
  EXPECT_EQ(counts->copies, 3u);
  EXPECT_EQ(ledger.snapshot().reservedBytes, bytes);
  static_assert(std::is_const<typename decltype(first)::element_type>::value, "Snapshot payload must be immutable");
}

TEST(WorldGenerationDistributionTest, UnusedQueuedInputIsReleasedWithoutEverCloning) {
  WorldGenerationMemoryLedger ledger;
  auto counts = std::make_shared<Counts>();
  { Cache input(ledger, source(counts), ProbeSizer{}); }
  EXPECT_EQ(counts->copies, 0u);
  EXPECT_EQ(ledger.snapshot().reservedBytes, 0u);
}

TEST(WorldGenerationDistributionTest, FailedCopyPreservesOriginalInputAndCanRetry) {
  WorldGenerationMemoryLedger ledger;
  auto counts = std::make_shared<Counts>();
  Cache input(ledger, source(counts), ProbeSizer{});
  auto before = ledger.snapshot().reservedBytes;
  counts->fail = true;
  EXPECT_THROW(input.freeze(ProbeSizer{}), std::runtime_error);
  EXPECT_FALSE(input.frozen());
  EXPECT_EQ(input.source().size(), 3u);
  EXPECT_EQ(input.source()[2].value, 2u);
  EXPECT_EQ(ledger.snapshot().reservedBytes, before);
  EXPECT_EQ(ledger.snapshot().reservedBufferBytes, 0u);
  counts->fail = false;
  auto frozen = input.freeze(ProbeSizer{});
  EXPECT_EQ(frozen->values()[2].value, 2u);
  EXPECT_TRUE(input.frozen());
}

TEST(WorldGenerationDistributionTest, FiniteRefusalDoesNotDiscardOriginalInput) {
  constexpr size_t limit = 1024 * 1024;
  WorldGenerationMemoryLedger ledger(limit);
  auto counts = std::make_shared<Counts>();
  Cache input(ledger, source(counts), ProbeSizer{});
  auto blocker = ledger.reserve(WorldGenerationMemoryClass::BaseTilesArray, limit - ledger.snapshot().reservedBytes);
  EXPECT_THROW(input.freeze(ProbeSizer{}), std::bad_alloc);
  EXPECT_FALSE(input.frozen());
  EXPECT_EQ(input.source().size(), 3u);
  EXPECT_EQ(counts->copies, 0u);
  blocker.reset();
  EXPECT_TRUE(input.freeze(ProbeSizer{}));
}

TEST(WorldGenerationDistributionTest, SnapshotOutlivesQueueEntryWithoutReusingItsId) {
  WorldGenerationMemoryLedger ledger;
  auto counts = std::make_shared<Counts>();
  WorldGenerationPlacementQueue<Cache> queue;
  auto first = queue.append(Cache(ledger, source(counts), ProbeSizer{}));
  std::shared_ptr<WorldGenerationImmutableArray<Probe> const> snapshot;
  queue.prepareSnapshots([&](WorldGenerationPlacementId id, Cache& input) {
    EXPECT_EQ(id, first); snapshot = input.freeze(ProbeSizer{});
  });
  { auto retired = queue.take(first); EXPECT_TRUE(bool(retired)); }
  EXPECT_FALSE(queue.contains(first));
  EXPECT_EQ(snapshot->values()[1].value, 1u);
  auto second = queue.append(Cache(ledger, source(counts), ProbeSizer{}));
  EXPECT_GT(second, first);
  snapshot.reset();
  { auto retired = queue.take(second); }
  EXPECT_EQ(ledger.snapshot().reservedBytes, 0u);
}

TEST(WorldGenerationDistributionTest, PreparationVisitorKeepsSurvivingIdOrderAndPayloads) {
  WorldGenerationPlacementQueue<unsigned> queue;
  auto a = queue.append(1), b = queue.append(2), c = queue.append(3);
  queue.take(b);
  std::vector<WorldGenerationPlacementId> visited;
  queue.prepareSnapshots([&](WorldGenerationPlacementId id, unsigned& value) { visited.push_back(id); value += 10; });
  EXPECT_EQ(visited, (std::vector<WorldGenerationPlacementId>{a, c}));
  EXPECT_EQ(*queue.take(c), 13u);
  EXPECT_EQ(*queue.take(a), 11u);
}

TEST(WorldGenerationDistributionTest, WeakControlBlockRetainsOnlyItsPhysicalMetadata) {
  WorldGenerationMemoryLedger ledger;
  auto counts = std::make_shared<Counts>();
  std::weak_ptr<WorldGenerationImmutableArray<Probe> const> weak;
  {
    auto frozen = makeWorldGenerationImmutableArray<Probe>(ledger, source(counts), ProbeSizer{});
    weak = frozen;
    EXPECT_GT(ledger.snapshot().estimatedBytes, 0u);
  }
  EXPECT_TRUE(weak.expired());
  EXPECT_EQ(ledger.snapshot().estimatedBytes, 0u);
  EXPECT_GT(ledger.snapshot().reservedBufferBytes, 0u);
  weak.reset();
  EXPECT_EQ(ledger.snapshot().reservedBytes, 0u);
}

TEST(WorldGenerationDistributionTest, EmptyInputIsImmutableAndHasNoElementBuffer) {
  WorldGenerationMemoryLedger ledger;
  std::vector<Probe> values;
  {
    auto frozen = makeWorldGenerationImmutableArray<Probe>(ledger, values, ProbeSizer{});
    EXPECT_TRUE(frozen->values().empty());
    EXPECT_EQ(ledger.snapshot().classResources[8], 1u); // shared object/control block only
  }
  EXPECT_EQ(ledger.snapshot().reservedBytes, 0u);
}

TEST(WorldGenerationDistributionTest, SnapshotCopyPreservesHashNormalizationRatherThanMovingSourceBuckets) {
  WorldGenerationMemoryLedger ledger;
  std::vector<StringSet> values(1);
  values[0].reserve(257);
  for (auto const& name : {"alpha", "beta", "gamma", "delta", "epsilon"}) values[0].add(String(name));
  auto reference = values; // exactly the old one-copy snapshot operation
  EXPECT_NE(values[0].values(), reference[0].values()); // exercise actual rehash normalization
  auto size = [](auto const&) { return WorldGenerationBackingEstimate{}; };
  auto frozen = makeWorldGenerationImmutableArray<StringSet>(ledger, values, size);
  EXPECT_EQ(frozen->values()[0].values(), reference[0].values());
  for (uint64_t seed = 0; seed < 64; ++seed)
    EXPECT_EQ(staticRandomFrom(frozen->values()[0], seed), staticRandomFrom(reference[0], seed));
}

TEST(WorldGenerationDistributionTest, RandomDistributionSamplerMatchesLegacyDeepCopy) {
  WorldGenerationMemoryLedger ledger;
  List<BiomeItemDistribution> values{BiomeItemDistributionTestAccess::random()};
  auto reference = values;
  auto frozen = makeWorldGenerationImmutableArray<BiomeItemDistribution>(ledger, values, DistributionSizer{});
  for (int x = -16; x <= 16; ++x) for (int y = -8; y <= 8; ++y) {
    auto old = reference[0].itemToPlace(x, y);
    auto current = frozen->values()[0].itemToPlace(x, y);
    ASSERT_EQ(bool(old), bool(current));
    if (old) {
      EXPECT_EQ(old->position, current->position);
      EXPECT_FLOAT_EQ(old->priority, current->priority);
      EXPECT_EQ(old->item.get<MicroDungeonNames>().values(), current->item.get<MicroDungeonNames>().values());
      EXPECT_EQ(staticRandomFrom(old->item.get<MicroDungeonNames>(), uint64_t(37)),
          staticRandomFrom(current->item.get<MicroDungeonNames>(), uint64_t(37)));
    }
  }
}

TEST(WorldGenerationDistributionTest, PeriodicDistributionSamplerAndPerlinBackingMatchLegacyCopy) {
  WorldGenerationMemoryLedger ledger;
  List<BiomeItemDistribution> values{BiomeItemDistributionTestAccess::periodic()};
  auto reference = values;
  auto frozen = makeWorldGenerationImmutableArray<BiomeItemDistribution>(ledger, values, DistributionSizer{});
  auto estimate = frozen->values()[0].generationBackingEstimate();
  EXPECT_GE(estimate.storage.bytes, 4 * (2 * PerlinSampleSize + 2) * (sizeof(int) + 6 * sizeof(float)));
  EXPECT_EQ(estimate.opaqueItems, 0u);
  for (int x = -16; x <= 16; ++x) for (int y = -8; y <= 8; ++y) {
    auto old = reference[0].itemToPlace(x, y), current = frozen->values()[0].itemToPlace(x, y);
    ASSERT_EQ(bool(old), bool(current));
    if (old) {
      EXPECT_EQ(old->item.get<MicroDungeonNames>().values(), current->item.get<MicroDungeonNames>().values());
    }
  }
}

TEST(WorldGenerationDistributionTest, OpaqueItemsAreReportedWithoutDroppingKnownBacking) {
  WorldGenerationMemoryLedger ledger;
  auto value = BiomeItemDistributionTestAccess::opaque();
  auto estimate = value.generationBackingEstimate();
  EXPECT_TRUE(estimate.storage.complete);
  EXPECT_GT(estimate.storage.bytes, 0u);
  EXPECT_EQ(estimate.opaqueItems, 1u);
  std::vector<BiomeItemDistribution> source{value};
  auto frozen = makeWorldGenerationImmutableArray<BiomeItemDistribution>(ledger, source, DistributionSizer{});
  auto sampled = frozen->values()[0].itemToPlace(4, 7);
  ASSERT_TRUE(bool(sampled));
  EXPECT_TRUE(sampled->item.is<TreasureBoxSet>());
  EXPECT_EQ(String(sampled->item.get<TreasureBoxSet>()), String("opaque treasure name"));
  EXPECT_EQ(frozen->opaqueItems(), 1u);
}

TEST(WorldGenerationDistributionTest, ParallelReadersSampleTheSameImmutableDistribution) {
  WorldGenerationMemoryLedger ledger;
  List<BiomeItemDistribution> values{BiomeItemDistributionTestAccess::periodic()};
  auto frozen = makeWorldGenerationImmutableArray<BiomeItemDistribution>(ledger, values, DistributionSizer{});
  std::vector<StringList> expected;
  for (int x = 0; x < 128; ++x) {
    auto item = frozen->values()[0].itemToPlace(x, 7);
    expected.push_back(item ? item->item.get<MicroDungeonNames>().values() : StringList{});
  }
  std::atomic<unsigned> failures{0};
  std::vector<std::thread> threads;
  auto bytes = ledger.snapshot().reservedBytes;
  for (unsigned i = 0; i < 8; ++i) threads.emplace_back([&] {
    for (int x = 0; x < 128; ++x) {
      auto item = frozen->values()[0].itemToPlace(x, 7);
      auto names = item ? item->item.get<MicroDungeonNames>().values() : StringList{};
      if (names != expected[x]) ++failures;
    }
  });
  for (auto& thread : threads) thread.join();
  EXPECT_EQ(failures.load(), 0u);
  EXPECT_EQ(ledger.snapshot().reservedBytes, bytes);
}

TEST(WorldGenerationDistributionTest, WorkerRetainsBundleAfterInputAndFacadeAreDestroyed) {
  WorldGenerationMemoryLedger observer;
  std::mutex mutex;
  std::condition_variable condition;
  bool started = false, released = false;
  WorkerPool pool("Immutable distribution contract", 1);
  auto counts = std::make_shared<Counts>();
  Maybe<WorkerPoolPromise<unsigned>> result;
  {
    WorldGenerationMemoryLedger facade(observer);
    Cache input(facade, source(counts), ProbeSizer{});
    auto frozen = input.freeze(ProbeSizer{});
    result = pool.addProducer<unsigned>([&, frozen] {
      std::unique_lock<std::mutex> lock(mutex);
      started = true; condition.notify_all();
      condition.wait(lock, [&] { return released; });
      return frozen->values()[2].value;
    });
  }
  {
    std::unique_lock<std::mutex> lock(mutex);
    EXPECT_TRUE(condition.wait_for(lock, std::chrono::seconds(5), [&] { return started; }));
    EXPECT_GT(observer.snapshot().reservedBytes, 0u);
    released = true;
  }
  condition.notify_all();
  EXPECT_EQ(result->get(), 2u);
  pool.finish();
  result.reset();
  EXPECT_EQ(observer.snapshot().reservedBytes, 0u);
}

TEST(WorldGenerationDistributionTest, CacheCanFreezeAfterOriginalFacadeCloses) {
  WorldGenerationMemoryLedger observer;
  auto counts = std::make_shared<Counts>();
  std::unique_ptr<Cache> input;
  {
    WorldGenerationMemoryLedger facade(observer);
    input = std::make_unique<Cache>(facade, source(counts), ProbeSizer{});
  }
  auto frozen = input->freeze(ProbeSizer{});
  input.reset();
  EXPECT_EQ(frozen->values().size(), 3u);
  frozen.reset();
  EXPECT_EQ(observer.snapshot().reservedBytes, 0u);
}

TEST(WorldGenerationDistributionTest, EstimateOverflowRefusesBeforeCopyAndChargesNothing) {
  WorldGenerationMemoryLedger ledger;
  auto counts = std::make_shared<Counts>();
  auto values = source(counts);
  auto size = [](auto const&) {
    WorldGenerationBackingEstimate result;
    result.storage.complete = false;
    return result;
  };
  EXPECT_THROW(makeWorldGenerationImmutableArray<Probe>(ledger, values, size), std::bad_alloc);
  EXPECT_EQ(counts->copies, 0u);
  EXPECT_EQ(ledger.snapshot().reservedBytes, 0u);
  WorldGenerationBackingEstimate a, b;
  a.opaqueItems = std::numeric_limits<size_t>::max(); b.opaqueItems = 1;
  a.add(b);
  EXPECT_FALSE(a.storage.complete);
}

TEST(WorldGenerationDistributionTest, IncompleteReconciliationIsDiagnosticAndKeepsPayload) {
  WorldGenerationMemoryLedger ledger;
  auto counts = std::make_shared<Counts>();
  auto values = source(counts);
  auto calls = std::make_shared<unsigned>(0);
  auto size = [calls](auto const&) {
    WorldGenerationBackingEstimate result;
    result.storage.bytes = 16;
    if (++*calls == 2) result.storage.complete = false;
    return result;
  };
  auto frozen = makeWorldGenerationImmutableArray<Probe>(ledger, values, size);
  EXPECT_FALSE(frozen->estimateComplete());
  EXPECT_EQ(frozen->values()[1].value, 1u);
  EXPECT_EQ(ledger.snapshot().estimatedBytes, 16u);
  EXPECT_EQ(ledger.snapshot().incompleteEstimateUpdates, 1u);
}
