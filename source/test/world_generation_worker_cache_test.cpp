#include "StarWorldGenerationWorkerCache.hpp"
#include "StarLruCache.hpp"
#include "StarWorkerPool.hpp"

#include <stdexcept>
#include <chrono>
#include <condition_variable>
#include <mutex>

#include "gtest/gtest.h"

using namespace Star;

namespace {
struct Payload {
  size_t bytes = 64;
  bool throwOnMeasure = false;
};
using Cache = WorldGenerationWorkerCache<unsigned, Payload>;
size_t measure(Payload const& value) {
  if (value.throwOnMeasure) throw std::runtime_error("controlled sizing failure");
  return value.bytes;
}
auto producer() { return std::make_shared<Payload>(); }
}

TEST(WorldGenerationWorkerCacheTest, AllocatorCopiesObserveOneFamily) {
  BlockAllocator<unsigned, 16> allocator;
  auto copy = allocator;
  auto empty = allocator.requestedStorageBytes();
  auto pointer = copy.allocate(1);
  EXPECT_GT(allocator.requestedStorageBytes(), empty);
  EXPECT_EQ(allocator.requestedStorageBytes(), copy.requestedStorageBytes());
  copy.deallocate(pointer, 1);
  // The spare block and vector capacity really remain allocated.
  EXPECT_GT(allocator.requestedStorageBytes(), empty);
}

TEST(WorldGenerationWorkerCacheTest, ReboundFamilyCountsMultiRequestsOnce) {
  BlockAllocator<unsigned, 16> first;
  BlockAllocator<double, 16> second(first);
  auto before = first.requestedStorageBytes();
  auto pointer = second.allocate(7);
  EXPECT_EQ(first.requestedStorageBytes(), before + 7 * sizeof(double));
  EXPECT_EQ(first.requestedStorageBytes(), second.requestedStorageBytes());
  second.deallocate(pointer, 7);
  EXPECT_EQ(first.requestedStorageBytes(), before);
}

TEST(WorldGenerationWorkerCacheTest, IndependentAllocatorFamiliesRemainSeparate) {
  BlockAllocator<unsigned, 16> first, second;
  auto before = second.requestedStorageBytes();
  auto pointer = first.allocate(3);
  EXPECT_EQ(second.requestedStorageBytes(), before);
  EXPECT_EQ(first.requestedStorageBytes(), before + 3 * sizeof(unsigned));
  first.deallocate(pointer, 3);
}

TEST(WorldGenerationWorkerCacheTest, FailedMultiAllocationDoesNotCharge) {
  BlockAllocator<unsigned, 16> allocator;
  auto before = allocator.requestedStorageBytes();
  EXPECT_THROW(allocator.allocate(std::numeric_limits<size_t>::max()), std::bad_alloc);
  EXPECT_EQ(allocator.requestedStorageBytes(), before);
}

TEST(WorldGenerationWorkerCacheTest, PoolReleasesExcessBlocksButKeepsSpareStorage) {
  BlockAllocator<unsigned, 16> allocator;
  std::vector<unsigned*> pointers;
  for (unsigned i = 0; i < 64; ++i) pointers.push_back(allocator.allocate(1));
  auto peak = allocator.requestedStorageBytes();
  for (auto pointer : pointers) allocator.deallocate(pointer, 1);
  auto retained = allocator.requestedStorageBytes();
  EXPECT_LT(retained, peak);
  auto pointer = allocator.allocate(1);
  EXPECT_EQ(allocator.requestedStorageBytes(), retained);
  allocator.deallocate(pointer, 1);
}

TEST(WorldGenerationWorkerCacheTest, LruStorageIncludesBucketsAndRetainedBlocks) {
  // Fixed hash: this test must not seed the global RNG.
  HashLruCache<unsigned, unsigned, std::hash<unsigned>> cache(2);
  auto empty = cache.allocatorStorageBytes();
  EXPECT_EQ(cache.allocatorStorageBytes(), empty);
  for (unsigned i = 0; i < 1000; ++i) {
    EXPECT_EQ(cache.get(i, [](unsigned key) { return key + 1; }), i + 1);
    EXPECT_LE(cache.currentSize(), 2u);
  }
  auto filled = cache.allocatorStorageBytes();
  EXPECT_GT(filled, empty + cache.currentSize() * sizeof(pair<unsigned const, unsigned>));
  cache.clear();
  EXPECT_EQ(cache.currentSize(), 0u);
  EXPECT_GT(cache.allocatorStorageBytes(), empty);
  EXPECT_LE(cache.allocatorStorageBytes(), filled);
}

TEST(WorldGenerationWorkerCacheTest, SameSourceAndRevisionReuseWithoutReconstruction) {
  WorldGenerationMemoryLedger ledger;
  Cache cache;
  auto source = std::make_shared<unsigned>(0);
  unsigned builds = 0;
  auto create = [&] { ++builds; return producer(); };
  {
    auto first = cache.get(source, 1, ledger, 32, measure, create);
    auto second = cache.get(source, 1, ledger, 32, measure, create);
    EXPECT_EQ(first.value(), second.value());
    EXPECT_EQ(builds, 1u);
    EXPECT_EQ(ledger.snapshot().resources, 1u);
    EXPECT_EQ(ledger.snapshot().estimatedBytes, 64u);
  }
  EXPECT_EQ(ledger.snapshot().estimatedBytes, 64u);
}

TEST(WorldGenerationWorkerCacheTest, RevisionReplacementRetainsAnActiveOldUse) {
  WorldGenerationMemoryLedger ledger;
  Cache cache;
  auto source = std::make_shared<unsigned>(0);
  {
    auto old = cache.get(source, 1, ledger, 32, measure, producer);
    auto next = cache.get(source, 2, ledger, 32, measure, producer);
    EXPECT_NE(old.value(), next.value());
    EXPECT_EQ(ledger.snapshot().resources, 2u);
    EXPECT_EQ(ledger.snapshot().estimatedBytes, 128u);
  }
  EXPECT_EQ(ledger.snapshot().estimatedBytes, 64u);
  cache = Cache();
  EXPECT_EQ(ledger.snapshot().estimatedBytes, 0u);
}

TEST(WorldGenerationWorkerCacheTest, WeakSourceDoesNotDiscardTheIdlePrivatePayload) {
  WorldGenerationMemoryLedger ledger;
  Cache cache;
  auto source = std::make_shared<unsigned>(0);
  std::weak_ptr<unsigned> weakSource = source;
  { auto use = cache.get(source, 1, ledger, 32, measure, producer); }
  source.reset();
  EXPECT_TRUE(weakSource.expired());
  EXPECT_EQ(ledger.snapshot().estimatedBytes, 64u);
  auto newSource = std::make_shared<unsigned>(0);
  { auto use = cache.get(newSource, 1, ledger, 32, measure, producer); }
  EXPECT_EQ(ledger.snapshot().resources, 1u);
}

TEST(WorldGenerationWorkerCacheTest, FailedReplacementPreservesIdentityAndRollsBack) {
  WorldGenerationMemoryLedger ledger;
  Cache cache;
  auto source = std::make_shared<unsigned>(0);
  std::weak_ptr<Payload> old;
  { auto use = cache.get(source, 1, ledger, 32, measure, producer); old = use.value(); }
  auto fail = []() -> std::shared_ptr<Payload> { throw std::runtime_error("controlled build failure"); };
  EXPECT_THROW(cache.get(source, 2, ledger, 32, measure, fail), std::runtime_error);
  auto use = cache.get(source, 1, ledger, 32, measure, fail);
  EXPECT_EQ(use.value(), old.lock());
  EXPECT_EQ(ledger.snapshot().estimatedBytes, 64u);
  EXPECT_EQ(ledger.snapshot().resources, 1u);
}

TEST(WorldGenerationWorkerCacheTest, NullReplacementDoesNotPublishAnEmptyCache) {
  WorldGenerationMemoryLedger ledger;
  Cache cache;
  auto source = std::make_shared<unsigned>(0);
  { auto use = cache.get(source, 1, ledger, 32, measure, producer); }
  EXPECT_FALSE(cache.get(source, 2, ledger, 32, measure,
      [] { return std::shared_ptr<Payload>(); }));
  EXPECT_EQ(ledger.snapshot().estimatedBytes, 64u);
}

TEST(WorldGenerationWorkerCacheTest, InitialChargePrecedesConstructionAndRefusal) {
  WorldGenerationMemoryLedger ledger(64);
  Cache cache;
  auto source = std::make_shared<unsigned>(0);
  bool called = false;
  {
    auto use = cache.get(source, 1, ledger, 64, measure, [&] {
      EXPECT_EQ(ledger.snapshot().estimatedBytes, 64u);
      return producer();
    });
  }
  EXPECT_FALSE(cache.get(source, 2, ledger, 1, measure, [&] { called = true; return producer(); }));
  EXPECT_FALSE(called);
  EXPECT_EQ(ledger.snapshot().estimatedBytes, 64u);
}

TEST(WorldGenerationWorkerCacheTest, ScopeRefreshesGrowthAndExceptionUnwinding) {
  WorldGenerationMemoryLedger ledger;
  Cache cache;
  auto source = std::make_shared<unsigned>(0);
  try {
    auto use = cache.get(source, 1, ledger, 32, measure, producer);
    use.value()->bytes = 256;
    throw std::runtime_error("controlled kernel failure");
  } catch (std::runtime_error const&) {}
  EXPECT_EQ(ledger.snapshot().estimatedBytes, 256u);
  { auto use = cache.get(source, 1, ledger, 32, measure, producer); use.value()->bytes = 16; }
  EXPECT_EQ(ledger.snapshot().estimatedBytes, 16u);
}

TEST(WorldGenerationWorkerCacheTest, SizingFailureKeepsPriorChargeAndDoesNotThrow) {
  WorldGenerationMemoryLedger ledger;
  Cache cache;
  auto source = std::make_shared<unsigned>(0);
  { auto use = cache.get(source, 1, ledger, 32, measure, producer); use.value()->throwOnMeasure = true; }
  EXPECT_EQ(ledger.snapshot().estimatedBytes, 64u);
  EXPECT_EQ(ledger.snapshot().incompleteEstimateUpdates, 1u);
  { auto use = cache.get(source, 1, ledger, 32, measure, producer);
    EXPECT_FALSE(use.estimateComplete()); use.value()->throwOnMeasure = false; }
  EXPECT_EQ(ledger.snapshot().incompleteEstimateUpdates, 1u);
}

TEST(WorldGenerationWorkerCacheTest, ResizeRefusalIsDiagnosticNotOutputRejection) {
  WorldGenerationMemoryLedger ledger(64);
  Cache cache;
  auto source = std::make_shared<unsigned>(0);
  { auto use = cache.get(source, 1, ledger, 32, measure, producer); use.value()->bytes = 128; }
  EXPECT_EQ(ledger.snapshot().estimatedBytes, 64u);
  EXPECT_EQ(ledger.snapshot().incompleteEstimateUpdates, 1u);
  auto use = cache.get(source, 1, ledger, 32, measure, producer);
  EXPECT_EQ(use.value()->bytes, 128u);
  EXPECT_FALSE(use.estimateComplete());
}

TEST(WorldGenerationWorkerCacheTest, MovingAScopeRefreshesOnce) {
  WorldGenerationMemoryLedger ledger;
  Cache cache;
  auto source = std::make_shared<unsigned>(0);
  unsigned measurements = 0;
  // Use a noncapturing callback with a counter stored in the payload itself.
  struct Counted { mutable unsigned calls = 0; };
  WorldGenerationWorkerCache<unsigned, Counted> counted;
  {
    auto first = counted.get(source, 1, ledger, 32,
        +[](Counted const& p) -> size_t { return ++p.calls; }, [] { return std::make_shared<Counted>(); });
    { auto second = std::move(first); measurements = second.value()->calls; }
  }
  EXPECT_EQ(measurements, 1u);
  EXPECT_EQ(ledger.snapshot().estimatedBytes, 2u);
}

TEST(WorldGenerationWorkerCacheTest, WorkerThreadCacheSurvivesFacadeAndReleasesAtThreadExit) {
  WorldGenerationMemoryLedger observer;
  auto facade = std::make_unique<WorldGenerationMemoryLedger>(observer);
  WorkerPool pool("Worker template storage contract", 1);
  auto source = std::make_shared<unsigned>(0);
  {
    auto copy = *facade;
    auto promise = pool.addProducer<bool>([copy, source]() mutable {
      static thread_local Cache cache;
      auto use = cache.get(source, 1, copy, 32, measure, producer);
      return bool(use);
    });
    EXPECT_TRUE(promise.get());
  }
  facade.reset(); source.reset();
  EXPECT_EQ(observer.snapshot().estimatedBytes, 64u);
  pool.finish();
  EXPECT_EQ(observer.snapshot().estimatedBytes, 0u);
}

TEST(WorldGenerationWorkerCacheTest, PayloadDestructionPrecedesReservationRelease) {
  struct Probe {
    WorldGenerationMemoryLedger* observer;
    bool* retained;
    ~Probe() { *retained = observer->snapshot().estimatedBytes > 0; }
  };
  WorldGenerationMemoryLedger ledger;
  bool retained = false;
  {
    WorldGenerationWorkerCache<unsigned, Probe> cache;
    auto source = std::make_shared<unsigned>(0);
    auto use = cache.get(source, 1, ledger, 64,
        +[](Probe const&) -> size_t { return 64; },
        [&] { return std::shared_ptr<Probe>(new Probe{&ledger, &retained}); });
  }
  EXPECT_TRUE(retained);
  EXPECT_EQ(ledger.snapshot().estimatedBytes, 0u);
}

TEST(WorldGenerationWorkerCacheTest, RealLruRetentionIsTrackedAtJobBoundaries) {
  using Lru = HashLruCache<unsigned, unsigned, std::hash<unsigned>>;
  WorldGenerationMemoryLedger ledger;
  WorldGenerationWorkerCache<unsigned, Lru> cache;
  static_assert(!std::is_copy_constructible<decltype(cache)>::value,
      "A worker-private cache must not silently share mutable payloads");
  auto source = std::make_shared<unsigned>(0);
  auto size = +[](Lru const& lru) { return sizeof(Lru) + lru.allocatorStorageBytes(); };
  auto create = [] { return std::make_shared<Lru>(2); };
  size_t filled = 0, cleared = 0;
  {
    auto use = cache.get(source, 1, ledger, sizeof(Lru), size, create);
    for (unsigned i = 0; i < 100; ++i) use.value()->get(i, [](unsigned key) { return key; });
    filled = size(*use.value());
    EXPECT_LT(ledger.snapshot().estimatedBytes, filled);
  }
  EXPECT_EQ(ledger.snapshot().estimatedBytes, filled);
  {
    auto use = cache.get(source, 1, ledger, sizeof(Lru), size, create);
    use.value()->clear();
    cleared = size(*use.value());
  }
  EXPECT_EQ(ledger.snapshot().estimatedBytes, cleared);
  EXPECT_GT(cleared, sizeof(Lru));
}

TEST(WorldGenerationWorkerCacheTest, LargeFlatMapPreservesReboundAllocationAndValues) {
  HashMap<unsigned, unsigned, std::hash<unsigned>, std::equal_to<unsigned>,
      BlockAllocator<pair<unsigned const, unsigned>, 1024>> map;
  map.reserve(8192);
  for (unsigned i = 0; i < 16384; ++i) map[i] = i * 3;
  for (auto const& pair : map) EXPECT_EQ(pair.second, pair.first * 3);
  EXPECT_EQ(map.size(), 16384u);
  map.clear();
  EXPECT_TRUE(map.empty());
}

TEST(WorldGenerationWorkerCacheTest, ConcurrentWorkerCachesRemainSeparateAndDrainAtThreadExit) {
  WorldGenerationMemoryLedger ledger;
  auto source = std::make_shared<unsigned>(0);
  std::mutex mutex;
  std::condition_variable condition;
  unsigned arrived = 0;
  bool released = false;
  // Join workers before destroying their gate, including exceptional exits.
  WorkerPool pool("Concurrent private cache contract", 8);
  std::vector<WorkerPoolPromise<bool>> promises;
  for (unsigned i = 0; i < 8; ++i) promises.push_back(pool.addProducer<bool>([&, source, copy = ledger]() mutable {
    static thread_local Cache cache;
    auto use = cache.get(source, 1, copy, 32, measure, producer);
    std::unique_lock<std::mutex> lock(mutex);
    ++arrived;
    condition.notify_all();
    condition.wait(lock, [&] { return released; });
    return bool(use);
  }));
  {
    std::unique_lock<std::mutex> lock(mutex);
    bool allArrived = condition.wait_for(lock, std::chrono::seconds(5), [&] { return arrived == 8; });
    EXPECT_TRUE(allArrived);
    if (allArrived) {
      EXPECT_EQ(ledger.snapshot().estimatedBytes, 8u * 64);
      EXPECT_EQ(ledger.snapshot().resources, 8u);
    }
    released = true;
  }
  condition.notify_all();
  for (auto& promise : promises) EXPECT_TRUE(promise.get());
  EXPECT_EQ(ledger.snapshot().estimatedBytes, 8u * 64);
  pool.finish();
  EXPECT_EQ(ledger.snapshot().estimatedBytes, 0u);
}
