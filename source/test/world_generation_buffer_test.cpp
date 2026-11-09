#include "StarWorldGenerationBufferAllocator.hpp"
#include "StarWorkerPool.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

#include "gtest/gtest.h"

namespace Star {
struct WorldGenerationBufferAllocatorTestAccess {
  template <typename T, typename Before>
  static T* beforeAllocation(WorldGenerationBufferAllocator<T>& allocator, size_t count, Before before) {
    return allocator.allocateWith(count, [&](size_t bytes) {
      before(bytes);
      return WorldGenerationBufferAllocator<T>::allocateStorage(bytes);
    });
  }
};
}

using namespace Star;

namespace {
auto const Plans = WorldGenerationMemoryClass::MicroDungeonPlanBuffer;
auto const Candidates = WorldGenerationMemoryClass::MicroDungeonCandidateBuffer;
template <typename T> using Allocator = WorldGenerationBufferAllocator<T>;
template <typename T> using Buffer = std::vector<T, Allocator<T>>;
using Access = WorldGenerationBufferAllocatorTestAccess;
using Demand = WorldGenerationMemoryDemand;

struct CopyProbe {
  CopyProbe(WorldGenerationMemoryLedger& ledger, size_t& observed, unsigned& copies, bool& fail, unsigned value)
    : ledger(&ledger), observed(&observed), copies(&copies), fail(&fail), value(value) {}
  CopyProbe(CopyProbe const& other)
    : ledger(other.ledger), observed(other.observed), copies(other.copies), fail(other.fail), value(other.value) {
    ++*copies;
    *observed = std::max(*observed, ledger->snapshot().reservedBufferBytes);
    if (*fail) throw std::runtime_error("controlled element copy failure");
  }
  CopyProbe(CopyProbe&& other) noexcept(false) : CopyProbe(other) {}
  WorldGenerationMemoryLedger* ledger;
  size_t* observed;
  unsigned* copies;
  bool* fail;
  unsigned value;
};
}

TEST(WorldGenerationBufferTest, ReservesWholeRequestBeforePhysicalAllocation) {
  WorldGenerationMemoryLedger ledger;
  Allocator<unsigned> allocator(ledger, Plans);
  bool called = false;
  auto pointer = Access::beforeAllocation(allocator, 8, [&](size_t bytes) {
    called = true;
    EXPECT_EQ(bytes, Allocator<unsigned>::requestBytes(8));
    EXPECT_EQ(ledger.snapshot().reservedBufferBytes, bytes);
    EXPECT_EQ(ledger.snapshot().estimatedBytes, 0u);
  });
  EXPECT_TRUE(called);
  EXPECT_EQ(ledger.snapshot().resources, 1u);
  allocator.deallocate(pointer, 8);
  EXPECT_EQ(ledger.snapshot().reservedBytes, 0u);
}

TEST(WorldGenerationBufferTest, FiniteRefusalDoesNotCallPhysicalAllocator) {
  auto bytes = Allocator<unsigned>::requestBytes(8);
  WorldGenerationMemoryLedger ledger(bytes - 1);
  Allocator<unsigned> allocator(ledger, Plans);
  bool called = false;
  EXPECT_THROW(Access::beforeAllocation(allocator, 8, [&](size_t) { called = true; }), std::bad_alloc);
  EXPECT_FALSE(called);
  EXPECT_EQ(ledger.snapshot().resources, 0u);
}

TEST(WorldGenerationBufferTest, PhysicalAllocationFailureRollsBackReservation) {
  WorldGenerationMemoryLedger ledger;
  Allocator<unsigned> allocator(ledger, Plans);
  EXPECT_THROW(Access::beforeAllocation(allocator, 8,
      [](size_t) { throw std::bad_alloc(); }), std::bad_alloc);
  EXPECT_EQ(ledger.snapshot().reservedBufferBytes, 0u);
  EXPECT_EQ(ledger.snapshot().resources, 0u);
  EXPECT_EQ(ledger.snapshot().peakReservedBufferBytes, Allocator<unsigned>::requestBytes(8));
}

TEST(WorldGenerationBufferTest, ArithmeticOverflowDoesNotChargeOrAllocate) {
  WorldGenerationMemoryLedger ledger;
  Allocator<unsigned> allocator(ledger, Plans);
  bool called = false;
  EXPECT_THROW(Access::beforeAllocation(allocator, std::numeric_limits<size_t>::max(),
      [&](size_t) { called = true; }), std::bad_array_new_length);
  EXPECT_FALSE(called);
  EXPECT_EQ(ledger.snapshot().reservedBytes, 0u);
}

TEST(WorldGenerationBufferTest, ZeroLengthDirectAllocationStillOwnsItsHeader) {
  WorldGenerationMemoryLedger ledger;
  Allocator<unsigned> allocator(ledger, Plans);
  auto pointer = allocator.allocate(0);
  EXPECT_EQ(ledger.snapshot().reservedBufferBytes, Allocator<unsigned>::requestBytes(0));
  allocator.deallocate(pointer, 0);
  EXPECT_EQ(ledger.snapshot().resources, 0u);
}

TEST(WorldGenerationBufferTest, OveralignedElementsAndHeaderAreSupported) {
  struct alignas(128) Wide { unsigned value; };
  WorldGenerationMemoryLedger ledger;
  Allocator<Wide> allocator(ledger, Plans);
  auto pointer = allocator.allocate(3);
  EXPECT_EQ(reinterpret_cast<uintptr_t>(pointer) % alignof(Wide), 0u);
  std::allocator_traits<Allocator<Wide>>::construct(allocator, pointer, Wide{42});
  EXPECT_EQ(pointer->value, 42u);
  std::allocator_traits<Allocator<Wide>>::destroy(allocator, pointer);
  allocator.deallocate(pointer, 3);
  EXPECT_EQ(ledger.snapshot().reservedBufferBytes, 0u);
}

TEST(WorldGenerationBufferTest, ReallocationTracksBothBuffersDuringElementCopies) {
  WorldGenerationMemoryLedger ledger;
  Buffer<CopyProbe> values{Allocator<CopyProbe>(ledger, Plans)};
  size_t observed = 0;
  unsigned copies = 0;
  bool fail = false;
  values.reserve(1);
  values.emplace_back(ledger, observed, copies, fail, 7);
  auto old = Allocator<CopyProbe>::requestBytes(values.capacity());
  values.reserve(values.capacity() + 5);
  auto current = Allocator<CopyProbe>::requestBytes(values.capacity());
  EXPECT_EQ(copies, 1u);
  EXPECT_EQ(observed, old + current);
  EXPECT_EQ(ledger.snapshot().reservedBufferBytes, current);
  EXPECT_EQ(ledger.snapshot().peakReservedBufferBytes, old + current);
  EXPECT_EQ(values[0].value, 7u);
}

TEST(WorldGenerationBufferTest, ThrowingElementCopyRetainsOldBufferAndAllowsRetry) {
  WorldGenerationMemoryLedger ledger;
  Buffer<CopyProbe> values{Allocator<CopyProbe>(ledger, Plans)};
  size_t observed = 0;
  unsigned copies = 0;
  bool fail = false;
  values.reserve(1);
  values.emplace_back(ledger, observed, copies, fail, 9);
  auto oldCapacity = values.capacity();
  auto oldBytes = Allocator<CopyProbe>::requestBytes(oldCapacity);
  fail = true;
  EXPECT_THROW(values.reserve(oldCapacity + 5), std::runtime_error);
  EXPECT_EQ(values.capacity(), oldCapacity);
  EXPECT_EQ(values[0].value, 9u);
  EXPECT_EQ(ledger.snapshot().reservedBufferBytes, oldBytes);
  EXPECT_GE(observed, oldBytes + Allocator<CopyProbe>::requestBytes(oldCapacity + 5));
  fail = false;
  values.reserve(oldCapacity + 5);
  EXPECT_EQ(values[0].value, 9u);
  EXPECT_EQ(ledger.snapshot().resources, 1u);
}

TEST(WorldGenerationBufferTest, ClearRetainsCapacityUntilBufferDestruction) {
  WorldGenerationMemoryLedger ledger;
  {
    Buffer<unsigned> values{Allocator<unsigned>(ledger, Plans)};
    values.resize(12, 42);
    auto bytes = ledger.snapshot().reservedBufferBytes;
    values.clear();
    EXPECT_EQ(ledger.snapshot().reservedBufferBytes, bytes);
    EXPECT_GT(bytes, 0u);
  }
  EXPECT_EQ(ledger.snapshot().reservedBufferBytes, 0u);
}

TEST(WorldGenerationBufferTest, PhysicalCopiesHaveIndependentReservations) {
  WorldGenerationMemoryLedger ledger;
  Buffer<unsigned> original{Allocator<unsigned>(ledger, Plans)};
  original.resize(7, 11);
  auto originalBytes = ledger.snapshot().reservedBufferBytes;
  {
    auto copy = original;
    EXPECT_EQ(ledger.snapshot().resources, 2u);
    EXPECT_EQ(ledger.snapshot().reservedBufferBytes,
        originalBytes + Allocator<unsigned>::requestBytes(copy.capacity()));
    copy[0] = 12;
    EXPECT_EQ(original[0], 11u);
  }
  EXPECT_EQ(ledger.snapshot().reservedBufferBytes, originalBytes);
}

TEST(WorldGenerationBufferTest, CopyAssignmentPropagatesLedgerAndChargesAnotherPhysicalBuffer) {
  WorldGenerationMemoryLedger first, second;
  Buffer<unsigned> a{Allocator<unsigned>(first, Plans)}, b{Allocator<unsigned>(second, Plans)};
  a.resize(8, 21); b.resize(2, 12);
  b = a;
  EXPECT_NE(a.data(), b.data());
  EXPECT_EQ(b.get_allocator(), a.get_allocator());
  EXPECT_EQ(b[0], 21u);
  EXPECT_EQ(first.snapshot().resources, 2u);
  EXPECT_EQ(second.snapshot().reservedBufferBytes, 0u);
}

TEST(WorldGenerationBufferTest, RefusedVectorGrowthRetainsOldDataAndReservation) {
  WorldGenerationMemoryLedger ledger(Allocator<unsigned>::requestBytes(4));
  Buffer<unsigned> values{Allocator<unsigned>(ledger, Plans)};
  values.reserve(4); values.resize(4, 21);
  auto pointer = values.data();
  auto bytes = ledger.snapshot().reservedBufferBytes;
  EXPECT_THROW(values.reserve(8), std::bad_alloc);
  EXPECT_EQ(values.data(), pointer);
  EXPECT_EQ(values.size(), 4u);
  EXPECT_EQ(values[0], 21u);
  EXPECT_EQ(ledger.snapshot().reservedBufferBytes, bytes);
  EXPECT_EQ(ledger.snapshot().peakReservedBufferBytes, bytes);
}

TEST(WorldGenerationBufferTest, MoveAssignmentPropagatesLedgerAndKeepsSourceUsable) {
  WorldGenerationMemoryLedger first, second;
  Buffer<unsigned> a{Allocator<unsigned>(first, Plans)}, b{Allocator<unsigned>(second, Plans)};
  a.resize(8, 21); b.resize(3, 12);
  auto pointer = a.data();
  auto bytes = first.snapshot().reservedBufferBytes;
  b = std::move(a);
  EXPECT_EQ(b.data(), pointer);
  EXPECT_EQ(second.snapshot().reservedBufferBytes, 0u);
  EXPECT_EQ(first.snapshot().reservedBufferBytes, bytes);
  a.push_back(99);
  EXPECT_EQ(first.snapshot().resources, 2u);
  EXPECT_EQ(second.snapshot().resources, 0u);
}

TEST(WorldGenerationBufferTest, SwapPropagatesClassesWithoutChangingReservations) {
  WorldGenerationMemoryLedger ledger;
  Allocator<unsigned> plans(ledger, Plans), candidates(plans, Candidates);
  Buffer<unsigned> a(plans), b(candidates);
  a.resize(8, 1); b.resize(2, 2);
  auto bytes = ledger.snapshot().reservedBufferBytes;
  auto peak = ledger.snapshot().peakReservedBufferBytes;
  a.swap(b);
  EXPECT_EQ(a.get_allocator(), candidates);
  EXPECT_EQ(b.get_allocator(), plans);
  EXPECT_EQ(a[0], 2u); EXPECT_EQ(b[0], 1u);
  EXPECT_EQ(ledger.snapshot().reservedBufferBytes, bytes);
  EXPECT_EQ(ledger.snapshot().peakReservedBufferBytes, peak);
}

TEST(WorldGenerationBufferTest, FixedBufferReservationsCannotBeResizedAsEstimates) {
  WorldGenerationMemoryLedger ledger;
  auto reservation = ledger.reserve(Plans, 64);
  ASSERT_TRUE(reservation);
  EXPECT_FALSE(reservation->resizeEstimate(128));
  EXPECT_EQ(ledger.snapshot().reservedBufferBytes, 64u);
  EXPECT_EQ(ledger.snapshot().estimatedBytes, 0u);
}

TEST(WorldGenerationBufferTest, RebindAndSameLedgerShareDomainButClassesAreDistinct) {
  WorldGenerationMemoryLedger ledger;
  Allocator<unsigned> a(ledger, Plans), independent(ledger, Plans);
  Allocator<double> b(a), candidates(a, Candidates);
  EXPECT_TRUE(a == b);
  EXPECT_TRUE(a != candidates);
  EXPECT_TRUE(a == independent);
  EXPECT_THROW(Allocator<unsigned>(ledger, WorldGenerationMemoryClass::MicroDungeonPlanEstimate), std::invalid_argument);
  auto x = a.allocate(4); auto y = candidates.allocate(5);
  EXPECT_EQ(ledger.snapshot().classBytes[6], Allocator<unsigned>::requestBytes(4));
  EXPECT_EQ(ledger.snapshot().classBytes[7], Allocator<double>::requestBytes(5));
  a.deallocate(x, 4); candidates.deallocate(y, 5);
  EXPECT_EQ(ledger.snapshot().reservedBufferBytes, 0u);
}

TEST(WorldGenerationBufferTest, AllocatorSurvivesItsOriginalLedgerFacade) {
  WorldGenerationMemoryLedger observer;
  auto facade = std::make_unique<WorldGenerationMemoryLedger>(observer);
  Allocator<unsigned> allocator(*facade, Plans);
  facade.reset();
  auto pointer = allocator.allocate(8);
  EXPECT_EQ(observer.snapshot().reservedBufferBytes, Allocator<unsigned>::requestBytes(8));
  allocator.deallocate(pointer, 8);
  EXPECT_EQ(observer.snapshot().reservedBufferBytes, 0u);
}

TEST(WorldGenerationBufferTest, ReadyPromiseHandleAvoidsCopyAndRetainsResultAcrossFailedApply) {
  struct Result {
    explicit Result(Buffer<unsigned> data) : data(std::move(data)) {}
    Result(Result const&) = delete;
    Result& operator=(Result const&) = delete;
    Result(Result&&) = default;
    Result& operator=(Result&&) = default;
    Buffer<unsigned> data;
  };
  WorldGenerationMemoryLedger ledger;
  WorkerPool pool("Ready buffer contract", 1);
  Maybe<WorkerPoolPromise<Result>> pending = pool.addProducer<Result>([allocator = Allocator<unsigned>(ledger, Plans)] {
    Buffer<unsigned> data(allocator); data.resize(8, 21); return Result(std::move(data));
  });
  auto pointer = pending->get().data.data();
  pool.finish();
  auto bytes = ledger.snapshot().reservedBufferBytes;
  try {
    auto owner = *pending;
    auto const& result = owner.get();
    EXPECT_EQ(result.data.data(), pointer);
    throw std::runtime_error("controlled apply failure");
  } catch (std::runtime_error const&) {}
  EXPECT_EQ(pending->get().data.size(), 8u);
  EXPECT_EQ(ledger.snapshot().reservedBufferBytes, bytes);
  {
    auto owner = *pending;
    auto const& result = owner.get();
    pending.reset();
    EXPECT_EQ(result.data.data(), pointer);
    EXPECT_EQ(ledger.snapshot().resources, 1u);
    EXPECT_EQ(ledger.snapshot().peakReservedBufferBytes, bytes);
  }
  EXPECT_EQ(ledger.snapshot().reservedBufferBytes, 0u);
}

TEST(WorldGenerationBufferTest, DroppingPromiseWhileWorkerRunsDoesNotReleaseItsBuffer) {
  WorldGenerationMemoryLedger ledger;
  std::mutex mutex;
  std::condition_variable condition;
  bool started = false, released = false;
  WorkerPool pool("Dropped buffer consumer contract", 1);
  Maybe<WorkerPoolPromise<Buffer<unsigned>>> pending = pool.addProducer<Buffer<unsigned>>(
      [&, allocator = Allocator<unsigned>(ledger, Plans)] {
        Buffer<unsigned> data(allocator); data.resize(8, 42);
        std::unique_lock<std::mutex> lock(mutex);
        started = true; condition.notify_all();
        condition.wait(lock, [&] { return released; });
        return data;
      });
  {
    std::unique_lock<std::mutex> lock(mutex);
    EXPECT_TRUE(condition.wait_for(lock, std::chrono::seconds(5), [&] { return started; }));
    pending.reset();
    EXPECT_GT(ledger.snapshot().reservedBufferBytes, 0u);
    released = true;
  }
  condition.notify_all();
  pool.finish();
  EXPECT_EQ(ledger.snapshot().reservedBufferBytes, 0u);
}

TEST(WorldGenerationBufferTest, ConcurrentBuffersHaveCoherentTotalsAndReleaseAllStorage) {
  WorldGenerationMemoryLedger ledger;
  Allocator<unsigned> allocator(ledger, Plans);
  std::atomic<unsigned> failures{0};
  std::vector<std::thread> threads;
  for (unsigned t = 0; t < 8; ++t) threads.emplace_back([&] {
    for (unsigned i = 0; i < 100; ++i) {
      auto pointer = allocator.allocate(8);
      auto usage = ledger.snapshot();
      if (usage.reservedBytes != usage.reservedBufferBytes || usage.estimatedBytes != 0
          || usage.reservedBufferBytes != usage.resources * Allocator<unsigned>::requestBytes(8)) ++failures;
      allocator.deallocate(pointer, 8);
    }
  });
  for (auto& thread : threads) thread.join();
  EXPECT_EQ(failures.load(), 0u);
  EXPECT_EQ(ledger.snapshot().reservedBytes, 0u);
  EXPECT_LE(ledger.snapshot().peakReservedBufferBytes, 8 * Allocator<unsigned>::requestBytes(8));
}

TEST(WorldGenerationBufferTest, PreReservedOutputAvoidsReallocationAndCopies) {
  WorldGenerationMemoryLedger ledger;
  Buffer<CopyProbe> values{Allocator<CopyProbe>(ledger, Plans)};
  size_t observed = 0;
  unsigned copies = 0;
  bool fail = false;
  values.reserve(64);
  auto bytes = ledger.snapshot().reservedBufferBytes;
  for (unsigned i = 0; i < 64; ++i) values.emplace_back(ledger, observed, copies, fail, i);
  EXPECT_EQ(copies, 0u);
  EXPECT_EQ(ledger.snapshot().reservedBufferBytes, bytes);
  EXPECT_EQ(ledger.snapshot().peakReservedBufferBytes, bytes);
}

TEST(WorldGenerationBufferTest, SyntheticSortedSelectionMatchesOrdinaryVectorsIncludingEmptyOutput) {
  using Candidate = std::pair<unsigned, unsigned>;
  WorldGenerationMemoryLedger ledger;
  Allocator<unsigned> plans(ledger, Plans);
  Buffer<Candidate> candidates{Allocator<Candidate>(plans, Candidates)};
  std::vector<Candidate> reference;
  for (unsigned i = 0; i < 512; ++i) {
    Candidate candidate((i * 17) % 31, i);
    candidates.push_back(candidate); reference.push_back(candidate);
  }
  std::sort(candidates.begin(), candidates.end()); std::sort(reference.begin(), reference.end());
  Buffer<unsigned> output(plans), empty(plans);
  auto eligible = [](Candidate const& c) { return c.second % 7 == 0; };
  output.reserve(std::count_if(candidates.begin(), candidates.end(), eligible));
  std::vector<unsigned> expected;
  for (auto const& c : candidates) if (eligible(c)) output.push_back(c.second);
  for (auto const& c : reference) if (eligible(c)) expected.push_back(c.second);
  EXPECT_TRUE(std::equal(output.begin(), output.end(), expected.begin(), expected.end()));
  auto before = ledger.snapshot().reservedBufferBytes;
  empty.reserve(0);
  EXPECT_EQ(ledger.snapshot().reservedBufferBytes, before);
  EXPECT_TRUE(empty.empty());
}

TEST(WorldGenerationBufferTest, BufferArrayAndEstimateTotalsRemainDisjoint) {
  WorldGenerationMemoryLedger ledger;
  auto array = ledger.reserve(WorldGenerationMemoryClass::BaseTilesArray, 32);
  auto estimate = ledger.reserve(WorldGenerationMemoryClass::MicroDungeonPlanEstimate, 16);
  Allocator<unsigned> allocator(ledger, Plans);
  auto pointer = allocator.allocate(8);
  auto usage = ledger.snapshot();
  EXPECT_EQ(usage.reservedArrayBytes, 32u);
  EXPECT_EQ(usage.estimatedBytes, 16u);
  EXPECT_EQ(usage.reservedBytes, 48u + usage.reservedBufferBytes);
  allocator.deallocate(pointer, 8);
}

TEST(WorldGenerationMemoryAdmissionTest, ScopedAllocatorRefusalHappensBeforeCallingTheRawAllocator) {
  auto soft = Allocator<unsigned>::requestBytes(4);
  WorldGenerationMemoryLedger ledger(4096, {true, 0, soft, 4096});
  auto spec = ledger.withDemand(Demand::Speculative); Allocator<unsigned> allocator(spec, Plans);
  bool called = false;
  EXPECT_THROW(Access::beforeAllocation(allocator, 8, [&](size_t) { called = true; }), std::bad_alloc);
  EXPECT_FALSE(called); EXPECT_EQ(ledger.snapshot().reservedBytes, 0u);
  EXPECT_EQ(ledger.snapshot().admission.pressureRefusals, 1u);
}

TEST(WorldGenerationMemoryAdmissionTest, RawAllocatorFailureRollsBackScopedAdmissionAndCanRetry) {
  auto bytes = Allocator<unsigned>::requestBytes(8);
  WorldGenerationMemoryLedger ledger(4096, {true, 0, bytes * 2, 4096});
  auto spec = ledger.withDemand(Demand::Speculative); Allocator<unsigned> allocator(spec, Plans);
  EXPECT_THROW(Access::beforeAllocation(allocator, 8, [](size_t) { throw std::bad_alloc(); }), std::bad_alloc);
  EXPECT_EQ(ledger.snapshot().reservedBytes, 0u); EXPECT_EQ(ledger.snapshot().peakReservedBytes, bytes);
  auto value = allocator.allocate(8); allocator.deallocate(value, 8);
  EXPECT_EQ(ledger.snapshot().reservedBytes, 0u);
}

TEST(WorldGenerationMemoryAdmissionTest, RefusedReallocationRetainsOldCapacityAndAllowsDependencyHeadroom) {
  auto overlap = Allocator<unsigned>::requestBytes(4) + Allocator<unsigned>::requestBytes(8);
  WorldGenerationMemoryLedger ledger(overlap, {true, 0, overlap - 1, overlap});
  auto spec = ledger.withDemand(Demand::Speculative), dependency = ledger.withDemand(Demand::Dependency);
  Buffer<unsigned> values{Allocator<unsigned>(spec, Plans)};
  values.reserve(4); values.assign(4, 7); auto old = ledger.snapshot().reservedBytes;
  EXPECT_THROW(values.reserve(8), std::bad_alloc);
  EXPECT_EQ(values.capacity(), 4u); EXPECT_EQ(values.size(), 4u); EXPECT_EQ(values[0], 7u);
  EXPECT_EQ(ledger.snapshot().reservedBytes, old);
  Allocator<unsigned> urgent(dependency, Plans); auto value = urgent.allocate(8);
  EXPECT_EQ(ledger.snapshot().reservedBytes, old + Allocator<unsigned>::requestBytes(8));
  urgent.deallocate(value, 8);
}

TEST(WorldGenerationMemoryAdmissionTest, DependencyReallocationChargesOldAndNewBuffersDuringElementCopies) {
  auto old = Allocator<CopyProbe>::requestBytes(1), next = Allocator<CopyProbe>::requestBytes(2);
  WorldGenerationMemoryLedger ledger(old + next, {true, 0, old, old + next});
  auto dependency = ledger.withDemand(Demand::Dependency);
  size_t observed = 0; unsigned copies = 0; bool fail = false;
  {
    Buffer<CopyProbe> values{Allocator<CopyProbe>(dependency, Candidates)};
    values.reserve(1); values.emplace_back(ledger, observed, copies, fail, 7);
    values.reserve(2);
    EXPECT_EQ(observed, old + next); EXPECT_EQ(ledger.snapshot().reservedBytes, next);
    EXPECT_EQ(ledger.snapshot().peakReservedBytes, old + next); EXPECT_EQ(values[0].value, 7u);
  }
  EXPECT_EQ(ledger.snapshot().reservedBytes, 0u);
}

TEST(WorldGenerationMemoryAdmissionTest, ThrowingElementCopyRollsBackNewBufferWithoutDroppingOldReservation) {
  auto old = Allocator<CopyProbe>::requestBytes(1), next = Allocator<CopyProbe>::requestBytes(2);
  WorldGenerationMemoryLedger ledger(old + next, {true, 0, old, old + next});
  auto dependency = ledger.withDemand(Demand::Dependency);
  size_t observed = 0; unsigned copies = 0; bool fail = false;
  Buffer<CopyProbe> values{Allocator<CopyProbe>(dependency, Candidates)};
  values.reserve(1); values.emplace_back(ledger, observed, copies, fail, 7);
  fail = true; EXPECT_THROW(values.reserve(2), std::runtime_error);
  EXPECT_EQ(observed, old + next); EXPECT_EQ(ledger.snapshot().reservedBytes, old);
  EXPECT_EQ(values.capacity(), 1u); EXPECT_EQ(values[0].value, 7u);
  fail = false; EXPECT_NO_THROW(values.reserve(2));
  EXPECT_EQ(ledger.snapshot().reservedBytes, next);
}

TEST(WorldGenerationMemoryAdmissionTest, AllocatorCopyMoveRebindAndAssignmentPreserveDemand) {
  WorldGenerationMemoryLedger ledger(4096, {true, 0, 64, 4096});
  auto spec = ledger.withDemand(Demand::Speculative), dependency = ledger.withDemand(Demand::Dependency);
  Allocator<unsigned> original(spec, Plans), copy(original), moved(std::move(copy)), assigned(dependency, Plans);
  Allocator<char> rebound(moved, Candidates);
  EXPECT_EQ(original, moved); EXPECT_NE(original, assigned);
  assigned = std::move(moved);
  EXPECT_THROW(original.allocate(128), std::bad_alloc);
  EXPECT_THROW(copy.allocate(128), std::bad_alloc); // Moved-from allocator is still scoped.
  EXPECT_THROW(assigned.allocate(128), std::bad_alloc);
  EXPECT_THROW(rebound.allocate(128), std::bad_alloc);
  EXPECT_EQ(ledger.snapshot().reservedBytes, 0u);
}

TEST(WorldGenerationMemoryAdmissionTest, VectorMovePropagationDoesNotTurnSpeculationIntoUrgentAllocation) {
  WorldGenerationMemoryLedger ledger(4096, {true, 0, 64, 4096});
  auto spec = ledger.withDemand(Demand::Speculative), dependency = ledger.withDemand(Demand::Dependency);
  Buffer<unsigned> first{Allocator<unsigned>(spec, Plans)}, second{Allocator<unsigned>(dependency, Plans)};
  first.reserve(4); first.assign(4, 7); second = std::move(first);
  EXPECT_EQ(second.get_allocator(), Allocator<unsigned>(spec, Plans));
  EXPECT_THROW(second.reserve(128), std::bad_alloc); EXPECT_EQ(second[0], 7u);
}

TEST(WorldGenerationMemoryAdmissionTest, ScopedRequestIsChargedBeforePhysicalDependencyAllocation) {
  WorldGenerationMemoryLedger ledger(4096, {true, 0, 32, 4096});
  auto dependency = ledger.withDemand(Demand::Dependency); Allocator<unsigned> allocator(dependency, Plans);
  auto value = Access::beforeAllocation(allocator, 8, [&](size_t bytes) {
    EXPECT_EQ(ledger.snapshot().reservedBytes, bytes);
    EXPECT_TRUE(ledger.snapshot().admission.lastAdvice.allowRequest);
  });
  allocator.deallocate(value, 8); EXPECT_EQ(ledger.snapshot().reservedBytes, 0u);
}
