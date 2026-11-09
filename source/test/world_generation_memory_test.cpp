#include "StarWorldGenerationMemory.hpp"
#include "StarWorldGenerationTasks.hpp"
#include "StarWorkerPool.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <vector>

#include "gtest/gtest.h"

using namespace Star;

namespace {

auto const Base = WorldGenerationMemoryClass::BaseTilesArray;
auto const Halo = WorldGenerationMemoryClass::MicroDungeonHalo;
using Resource = WorldGenerationTrackedResource<std::unique_ptr<unsigned>>;
static_assert(!std::is_copy_assignable<WorldGenerationMemoryLedger>::value,
    "A process ledger must not forget live resources by replacing its state");

WorldRegionPartition partition() {
  return WorldRegionPartition(WorldGeometry(Vec2U{160, 128}));
}

struct ThrowingPayload {
  ThrowingPayload() { throw std::runtime_error("controlled payload failure"); }
};

struct DestructionProbe {
  std::function<void()> destroyed;
  ~DestructionProbe() { if (destroyed) destroyed(); }
};

struct Gate {
  std::mutex mutex;
  std::condition_variable changed;
  bool entered = false;
  bool released = false;

  void wait() {
    std::unique_lock<std::mutex> lock(mutex);
    entered = true;
    changed.notify_all();
    changed.wait(lock, [&] { return released; });
  }
  bool waitEntered() {
    std::unique_lock<std::mutex> lock(mutex);
    return changed.wait_for(lock, std::chrono::seconds(5), [&] { return entered; });
  }
  void open() {
    std::lock_guard<std::mutex> lock(mutex);
    released = true;
    changed.notify_all();
  }
};

struct Unblock {
  Gate& gate;
  ~Unblock() { gate.open(); }
};

}

TEST(WorldGenerationMemoryTest, CategoriesAndPeakHaveCoherentTotals) {
  WorldGenerationMemoryLedger ledger;
  auto base = ledger.reserve(Base, 32);
  auto halo = ledger.reserve(Halo, 96);
  ASSERT_TRUE(base);
  ASSERT_TRUE(halo);
  auto usage = ledger.snapshot();
  EXPECT_EQ(usage.reservedBytes, 128u);
  EXPECT_EQ(usage.peakReservedBytes, 128u);
  EXPECT_EQ(usage.resources, 2u);
  EXPECT_EQ(usage.classBytes[0], 32u);
  EXPECT_EQ(usage.classBytes[1], 96u);
  EXPECT_EQ(usage.classResources[0], 1u);
  EXPECT_EQ(usage.classResources[1], 1u);
  base.reset();
  EXPECT_EQ(ledger.snapshot().reservedBytes, 96u);
  halo.reset();
  EXPECT_EQ(ledger.snapshot().reservedBytes, 0u);
  EXPECT_EQ(ledger.snapshot().resources, 0u);
  EXPECT_EQ(ledger.snapshot().peakReservedBytes, 128u);
}

TEST(WorldGenerationMemoryTest, SharingAResourceChargesItOnlyOnce) {
  WorldGenerationMemoryLedger ledger;
  auto resource = makeWorldGenerationTrackedResource<std::unique_ptr<unsigned>>(ledger, Base, 64);
  ASSERT_TRUE(resource);
  resource->value = std::make_unique<unsigned>(7);
  auto queued = resource;
  auto result = queued;
  resource.reset();
  queued.reset();
  EXPECT_EQ(ledger.snapshot().reservedBytes, 64u);
  EXPECT_EQ(ledger.snapshot().resources, 1u);
  EXPECT_EQ(*result->value, 7u);
  result.reset();
  EXPECT_EQ(ledger.snapshot().reservedBytes, 0u);
}

TEST(WorldGenerationMemoryTest, PayloadIsDestroyedBeforeItsReservation) {
  WorldGenerationMemoryLedger ledger;
  bool destroyed = false;
  auto resource = makeWorldGenerationTrackedResource<DestructionProbe>(ledger, Base, 64);
  resource->value.destroyed = [&] {
    destroyed = true;
    EXPECT_EQ(ledger.snapshot().reservedBytes, 64u);
  };
  resource.reset();
  EXPECT_TRUE(destroyed);
  EXPECT_EQ(ledger.snapshot().reservedBytes, 0u);
}

TEST(WorldGenerationMemoryTest, ConstructorFailureRollsBackAlreadyChargedBytes) {
  WorldGenerationMemoryLedger ledger;
  EXPECT_THROW(makeWorldGenerationTrackedResource<ThrowingPayload>(ledger, Base, 64), std::runtime_error);
  EXPECT_EQ(ledger.snapshot().reservedBytes, 0u);
  EXPECT_EQ(ledger.snapshot().resources, 0u);
  EXPECT_EQ(ledger.snapshot().peakReservedBytes, 64u);
}

TEST(WorldGenerationMemoryTest, RefusedReservationNeverConstructsThePayload) {
  WorldGenerationMemoryLedger ledger(0);
  EXPECT_NO_THROW({
    auto resource = makeWorldGenerationTrackedResource<ThrowingPayload>(ledger, Base, 64);
    EXPECT_FALSE(resource);
  });
  EXPECT_EQ(ledger.snapshot().resources, 0u);
  EXPECT_EQ(ledger.snapshot().peakReservedBytes, 0u);
}

TEST(WorldGenerationMemoryTest, FailureAfterPartialPreparationReleasesTheGroup) {
  WorldGenerationMemoryLedger ledger;
  EXPECT_THROW({
    auto resource = makeWorldGenerationTrackedResource<std::vector<std::unique_ptr<unsigned>>>(ledger, Halo, 128);
    resource->value.push_back(std::make_unique<unsigned>(1));
    resource->value.push_back(std::make_unique<unsigned>(2));
    throw std::runtime_error("controlled partial copy failure");
  }, std::runtime_error);
  EXPECT_EQ(ledger.snapshot().resources, 0u);
  EXPECT_EQ(ledger.snapshot().reservedBytes, 0u);
}

TEST(WorldGenerationMemoryTest, FiniteTestLedgerSharesAdmissionAcrossClasses) {
  WorldGenerationMemoryLedger ledger(128);
  auto base = ledger.reserve(Base, 32);
  auto halo = ledger.reserve(Halo, 96);
  EXPECT_FALSE(ledger.reserve(Base, 1));
  base.reset();
  EXPECT_FALSE(ledger.reserve(Base, 33));
  EXPECT_TRUE(ledger.reserve(Base, 32));
  EXPECT_EQ(ledger.snapshot().reservedBytes, 96u);
  halo.reset();
  EXPECT_EQ(ledger.snapshot().reservedBytes, 0u);
}

TEST(WorldGenerationMemoryTest, OverflowCannotWrapProcessOrClassCounters) {
  WorldGenerationMemoryLedger ledger;
  auto max = std::numeric_limits<size_t>::max();
  auto large = ledger.reserve(Base, max - 1);
  ASSERT_TRUE(large);
  EXPECT_FALSE(ledger.reserve(Halo, 2));
  auto last = ledger.reserve(Halo, 1);
  ASSERT_TRUE(last);
  EXPECT_EQ(ledger.snapshot().reservedBytes, max);
  EXPECT_FALSE(ledger.reserve(Base, 1));
  large.reset();
  last.reset();
  EXPECT_EQ(ledger.snapshot().reservedBytes, 0u);
  EXPECT_EQ(ledger.snapshot().peakReservedBytes, max);
}

TEST(WorldGenerationMemoryTest, InvalidClassesDoNotAffectTheLedger) {
  WorldGenerationMemoryLedger ledger;
  EXPECT_FALSE(ledger.reserve(WorldGenerationMemoryClass::Count, 1));
  EXPECT_FALSE(ledger.reserve(static_cast<WorldGenerationMemoryClass>(255), 1));
  EXPECT_EQ(ledger.snapshot().reservedBytes, 0u);
  EXPECT_EQ(ledger.snapshot().resources, 0u);
}

TEST(WorldGenerationMemoryTest, ZeroByteReservationStillHasOneUniqueOwnerGroup) {
  WorldGenerationMemoryLedger ledger(0);
  auto memory = ledger.reserve(Base, 0);
  ASSERT_TRUE(memory);
  EXPECT_EQ(ledger.snapshot().reservedBytes, 0u);
  EXPECT_EQ(ledger.snapshot().resources, 1u);
  memory.reset();
  EXPECT_EQ(ledger.snapshot().resources, 0u);
}

TEST(WorldGenerationMemoryTest, LogicalRetirementDoesNotReleaseAPhysicalResult) {
  WorldGenerationMemoryLedger ledger;
  WorldGenerationTasks tasks(1, partition(), 1, 64);
  auto task = tasks.reserve({0, 0}, WorldGenerationTaskStage::BaseTiles, 64);
  auto result = makeWorldGenerationTrackedResource<std::unique_ptr<unsigned>>(ledger, Base, 64);
  ASSERT_TRUE(task);
  ASSERT_TRUE(task->start());
  ASSERT_TRUE(task->ready());
  ASSERT_TRUE(tasks.retire(task, WorldGenerationTaskOutcome::Committed));
  EXPECT_EQ(tasks.reservedBytes(), 0u);
  EXPECT_EQ(ledger.snapshot().reservedBytes, 64u);
  result.reset();
  EXPECT_EQ(ledger.snapshot().reservedBytes, 0u);
  // Keeping the old task handle cannot keep physical memory charged.
  EXPECT_EQ(task->state(), WorldGenerationTaskState::Ready);
}

TEST(WorldGenerationMemoryTest, WorldsCanCloseAndReopenWithoutForgettingOldResources) {
  WorldGenerationMemoryLedger ledger;
  std::shared_ptr<Resource> survivor;
  shared_ptr<WorldGenerationTask> oldTask;
  {
    WorldGenerationTasks first(1, partition(), 1, 64);
    oldTask = first.reserve({0, 0}, WorldGenerationTaskStage::BaseTiles, 64);
    survivor = makeWorldGenerationTrackedResource<std::unique_ptr<unsigned>>(ledger, Base, 64);
  }
  EXPECT_FALSE(oldTask->valid());
  WorldGenerationTasks second(2, partition(), 1, 32);
  ASSERT_TRUE(second.reserve({0, 0}, WorldGenerationTaskStage::BaseTiles, 32));
  auto next = makeWorldGenerationTrackedResource<std::unique_ptr<unsigned>>(ledger, Base, 32);
  EXPECT_EQ(ledger.snapshot().reservedBytes, 96u);
  survivor.reset();
  EXPECT_EQ(ledger.snapshot().reservedBytes, 32u);
  next.reset();
  EXPECT_EQ(ledger.snapshot().reservedBytes, 0u);
}

TEST(WorldGenerationMemoryTest, ReservationSurvivesTheLedgerFacade) {
  std::shared_ptr<Resource> survivor;
  {
    WorldGenerationMemoryLedger ledger;
    survivor = makeWorldGenerationTrackedResource<std::unique_ptr<unsigned>>(ledger, Base, 64);
    survivor->value = std::make_unique<unsigned>(9);
  }
  EXPECT_EQ(*survivor->value, 9u);
  // ASan checks the shared accounting state remains alive for this release.
  survivor.reset();
}

TEST(WorldGenerationMemoryTest, WorkerAndReadyResultOutliveTheWorldRegistry) {
  WorldGenerationMemoryLedger ledger;
  Gate gate;
  WorkerPool pool("Memory lifecycle contract", 1);
  Unblock unblock{gate};
  Maybe<WorkerPoolPromise<std::shared_ptr<Resource>>> promise;
  shared_ptr<WorldGenerationTask> task;
  {
    WorldGenerationTasks world(1, partition(), 1, 64);
    task = world.reserve({0, 0}, WorldGenerationTaskStage::BaseTiles, 64);
    auto resource = makeWorldGenerationTrackedResource<std::unique_ptr<unsigned>>(ledger, Base, 64);
    resource->value = std::make_unique<unsigned>(42);
    promise = pool.addProducer<std::shared_ptr<Resource>>([task, resource, &gate] {
      if (!task->start()) throw std::runtime_error("duplicate producer start");
      gate.wait();
      if (!task->ready()) throw std::runtime_error("duplicate producer completion");
      return resource;
    });
    ASSERT_TRUE(gate.waitEntered());
  }
  EXPECT_FALSE(task->valid());
  EXPECT_EQ(ledger.snapshot().reservedBytes, 64u);
  gate.open();
  auto result = std::move(promise->get());
  pool.finish();
  EXPECT_EQ(*result->value, 42u);
  EXPECT_EQ(ledger.snapshot().resources, 1u);
  result.reset();
  EXPECT_EQ(ledger.snapshot().resources, 0u);
}

TEST(WorldGenerationMemoryTest, HaloInputIsReleasedWithoutChargingAPlanResult) {
  WorldGenerationMemoryLedger ledger;
  WorkerPool pool("Memory halo contract", 1);
  auto halo = makeWorldGenerationTrackedResource<std::vector<unsigned>>(ledger, Halo, 96);
  halo->value = {1, 2, 3};
  auto plan = pool.addProducer<unsigned>([halo] { return halo->value.front(); });
  halo.reset();
  EXPECT_EQ(plan.get(), 1u);
  pool.finish();
  EXPECT_EQ(ledger.snapshot().reservedBytes, 0u);
  EXPECT_EQ(ledger.snapshot().resources, 0u);
}

TEST(WorldGenerationMemoryTest, FailedProducerReleasesItsCapturedInputAfterExit) {
  WorldGenerationMemoryLedger ledger;
  WorkerPool pool("Memory failure contract", 1);
  auto input = makeWorldGenerationTrackedResource<std::vector<unsigned>>(ledger, Halo, 96);
  auto promise = pool.addProducer<unsigned>([input]() -> unsigned {
    throw std::runtime_error("controlled producer failure");
  });
  input.reset();
  EXPECT_THROW(promise.get(), std::runtime_error);
  pool.finish();
  EXPECT_EQ(ledger.snapshot().reservedBytes, 0u);
  EXPECT_EQ(ledger.snapshot().resources, 0u);
}

TEST(WorldGenerationMemoryTest, PublishedArrayIsOutsideThePrivateGenerationLedger) {
  WorldGenerationMemoryLedger ledger;
  auto input = makeWorldGenerationTrackedResource<std::unique_ptr<unsigned>>(ledger, Base, sizeof(unsigned));
  input->value = std::make_unique<unsigned>(42);
  auto published = std::move(input->value);
  input.reset();
  EXPECT_EQ(*published, 42u);
  EXPECT_EQ(ledger.snapshot().reservedBytes, 0u);
  // Publication transfers ownership, it does not free the authoritative data.
}

TEST(WorldGenerationMemoryTest, CopiedLedgerFacadesObserveTheSameStateAndLimit) {
  WorldGenerationMemoryLedger first(64);
  auto second = first;
  auto memory = first.reserve(Base, 64);
  EXPECT_FALSE(second.reserve(Halo, 1));
  EXPECT_EQ(second.snapshot().reservedBytes, 64u);
  memory.reset();
  EXPECT_TRUE(second.reserve(Halo, 64));
  EXPECT_EQ(first.snapshot().reservedBytes, 0u);
}

TEST(WorldGenerationMemoryTest, ConcurrentReserveReleaseAndSnapshotsStayBalanced) {
  WorldGenerationMemoryLedger ledger;
  std::atomic<unsigned> failures{0};
  std::vector<std::thread> threads;
  for (unsigned t = 0; t < 8; ++t) {
    threads.emplace_back([&, t] {
      for (unsigned i = 0; i < 1000; ++i) {
        auto memory = ledger.reserve(t % 2 ? Halo : Base, 16);
        if (!memory) ++failures;
        auto s = ledger.snapshot();
        if (s.reservedBytes != s.classBytes[0] + s.classBytes[1]
            || s.resources != s.classResources[0] + s.classResources[1]
            || s.reservedBytes > 8 * 16 || s.peakReservedBytes < s.reservedBytes)
          ++failures;
      }
    });
  }
  for (auto& thread : threads) thread.join();
  EXPECT_EQ(failures.load(), 0u);
  EXPECT_EQ(ledger.snapshot().reservedBytes, 0u);
  EXPECT_EQ(ledger.snapshot().resources, 0u);
}

TEST(WorldGenerationMemoryTest, RuntimeAccessorSharesOneProcessLedger) {
  auto& first = worldGenerationMemoryLedger();
  auto& second = worldGenerationMemoryLedger();
  EXPECT_EQ(&first, &second);
  auto before = first.snapshot();
  {
    auto memory = first.reserve(Base, 64);
    ASSERT_TRUE(memory);
    EXPECT_EQ(second.snapshot().reservedBytes, before.reservedBytes + 64);
    EXPECT_EQ(second.snapshot().resources, before.resources + 1);
  }
  EXPECT_EQ(second.snapshot().reservedBytes, before.reservedBytes);
}
