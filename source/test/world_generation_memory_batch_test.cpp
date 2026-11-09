#include "StarWorldGenerationMemory.hpp"
#include "StarWorldGenerationMemoryEnvelope.hpp"
#include "StarWorldGenerationTasks.hpp"
#include "StarWorldGenerationWorkerCache.hpp"
#include "StarWorkerPool.hpp"

#include <atomic>
#include <future>
#include <stdexcept>
#include <thread>
#include <vector>

#include "gtest/gtest.h"

using namespace Star;

namespace {
using Class = WorldGenerationMemoryClass;
using Demand = WorldGenerationMemoryDemand;
using Request = WorldGenerationMemoryRequest;
using Token = std::shared_ptr<WorldGenerationMemoryReservation>;
using Pair = std::array<Token, 2>;
WorldGenerationMemoryPressureLimits const Limits{true, 80, 100, 200};
auto requests(size_t arrayBytes = 60, size_t templateBytes = 40) {
  return std::array<Request, 2>{{{Class::BaseTilesArray, arrayBytes}, {Class::WorkerTemplateEstimate, templateBytes}}};
}
struct Payload { size_t bytes = 64; };
using Cache = WorldGenerationWorkerCache<unsigned, Payload>;
size_t measure(Payload const& value) { return value.bytes; }
auto produce() { return std::make_shared<Payload>(); }
WorldRegionPartition partition() { return WorldRegionPartition(WorldGeometry(Vec2U{160, 128})); }
template <typename Work> void simultaneous(Work work) {
  std::promise<void> start; auto gate = start.get_future().share();
  std::vector<std::thread> threads;
  for (unsigned i = 0; i < 8; ++i) threads.emplace_back([&, i, gate] { gate.wait(); work(i); });
  start.set_value();
  for (auto& thread : threads) thread.join();
}
}

TEST(WorldGenerationMemoryBatchTest, ChargesEveryPartWithOnePressureCheck) {
  WorldGenerationMemoryLedger ledger(1000, Limits);
  auto batch = ledger.reserveBatch(requests()); ASSERT_TRUE(batch);
  auto s = ledger.snapshot();
  EXPECT_EQ(s.reservedBytes, 100u); EXPECT_EQ(s.resources, 2u);
  EXPECT_EQ(s.reservedArrayBytes, 60u); EXPECT_EQ(s.estimatedBytes, 40u);
  EXPECT_EQ(s.admission.pressureChecks, 1u);
  EXPECT_TRUE(worldGenerationMemoryEnvelope(s).coherent);
}

TEST(WorldGenerationMemoryBatchTest, SoftRefusalLeavesNoAdmittedPrefixOrPeak) {
  WorldGenerationMemoryLedger ledger(1000, Limits); auto spec = ledger.withDemand(Demand::Speculative);
  EXPECT_FALSE(spec.reserveBatch(requests(60, 41)));
  auto s = ledger.snapshot(); EXPECT_EQ(s.resources, 0u); EXPECT_EQ(s.reservedBytes, 0u);
  EXPECT_EQ(s.peakReservedBytes, 0u); EXPECT_EQ(s.admission.pressureChecks, 1u);
  EXPECT_EQ(s.admission.pressureRefusals, 1u);
  EXPECT_TRUE(spec.reserveBatch(requests()));
}

TEST(WorldGenerationMemoryBatchTest, DependencyCanReserveAllTheHardHeadroom) {
  WorldGenerationMemoryLedger ledger(1000, Limits);
  auto low = ledger.withDemand(Demand::Speculative).reserveBatch(requests()); ASSERT_TRUE(low);
  EXPECT_FALSE(ledger.withDemand(Demand::Speculative).reserveBatch(requests(1, 1)));
  auto high = ledger.withDemand(Demand::Dependency).reserveBatch(requests()); ASSERT_TRUE(high);
  EXPECT_EQ(ledger.snapshot().reservedBytes, 200u);
  EXPECT_FALSE(ledger.reserveBatch(requests(0, 1)));
}

TEST(WorldGenerationMemoryBatchTest, LegacyMaximumAppliesToTheWholeBatchWithPressureDisabled) {
  WorldGenerationMemoryLedger ledger(99);
  EXPECT_FALSE(ledger.reserveBatch(requests()));
  EXPECT_EQ(ledger.snapshot().resources, 0u); EXPECT_EQ(ledger.snapshot().admission.pressureChecks, 0u);
  EXPECT_TRUE(ledger.reserveBatch(requests(59, 40)));
}

TEST(WorldGenerationMemoryBatchTest, InvalidLaterClassDoesNotChargeTheValidPrefix) {
  WorldGenerationMemoryLedger ledger(1000, Limits); auto input = requests(); input[1].kind = Class::Count;
  EXPECT_FALSE(ledger.reserveBatch(input));
  input[1].kind = static_cast<Class>(std::numeric_limits<size_t>::max());
  EXPECT_FALSE(ledger.reserveBatch(input));
  EXPECT_EQ(ledger.snapshot().admission.pressureChecks, 0u); EXPECT_EQ(ledger.snapshot().resources, 0u);
}

TEST(WorldGenerationMemoryBatchTest, SharedJsonCannotBypassItsOwnerRegistryThroughABatch) {
  WorldGenerationMemoryLedger ledger(1000, Limits); auto input = requests(); input[1].kind = Class::TemplateJsonEstimate;
  EXPECT_FALSE(ledger.reserveBatch(input));
  EXPECT_EQ(ledger.snapshot().sharedRegistryEntries, 0u); EXPECT_EQ(ledger.snapshot().reservedBytes, 0u);
}

TEST(WorldGenerationMemoryBatchTest, SumOverflowIsRejectedBeforeAdmission) {
  WorldGenerationMemoryLedger ledger;
  EXPECT_FALSE(ledger.reserveBatch(requests(std::numeric_limits<size_t>::max(), 1)));
  EXPECT_EQ(ledger.snapshot().resources, 0u); EXPECT_EQ(ledger.snapshot().peakReservedBytes, 0u);
}

TEST(WorldGenerationMemoryBatchTest, SizeMaxCanBeExactlyPartitionedWithoutOverflow) {
  auto max = std::numeric_limits<size_t>::max(); WorldGenerationMemoryLedger ledger;
  auto batch = ledger.reserveBatch(requests(max - 1, 1)); ASSERT_TRUE(batch);
  EXPECT_EQ(ledger.snapshot().reservedBytes, max); EXPECT_TRUE(worldGenerationMemoryEnvelope(ledger.snapshot()).coherent);
  EXPECT_FALSE(ledger.reserveBatch(requests(0, 1)));
  batch.reset(); EXPECT_EQ(ledger.snapshot().reservedBytes, 0u);
}

TEST(WorldGenerationMemoryBatchTest, EmptyBatchIsAnAllocationFreeNoOp) {
  WorldGenerationMemoryLedger ledger(1000, Limits);
  auto empty = ledger.reserveBatch(std::array<Request, 0>{}); ASSERT_TRUE(empty);
  EXPECT_TRUE(empty->empty()); EXPECT_EQ(ledger.snapshot().admission.pressureChecks, 0u);
  EXPECT_EQ(ledger.snapshot().resources, 0u);
}

TEST(WorldGenerationMemoryBatchTest, ZeroBytePartsRemainSeparateResourcesAtHardLimit) {
  WorldGenerationMemoryLedger ledger(1000, Limits); auto full = ledger.reserve(Class::BaseTilesArray, 200);
  ASSERT_TRUE(full); auto batch = ledger.withDemand(Demand::Speculative).reserveBatch(requests(0, 0));
  ASSERT_TRUE(batch); EXPECT_EQ(ledger.snapshot().resources, 3u);
  EXPECT_EQ(ledger.snapshot().reservedBytes, 200u); EXPECT_FALSE((*batch)[1]->resizeEstimate(1));
}

TEST(WorldGenerationMemoryBatchTest, DuplicateClassesCountDistinctPhysicalResources) {
  WorldGenerationMemoryLedger ledger; auto input = requests(); input[1].kind = Class::BaseTilesArray;
  auto batch = ledger.reserveBatch(input); ASSERT_TRUE(batch);
  EXPECT_NE((*batch)[0], (*batch)[1]); EXPECT_EQ(ledger.snapshot().classResources[0], 2u);
  EXPECT_EQ(ledger.snapshot().reservedArrayBytes, 100u);
  (*batch)[0].reset(); EXPECT_EQ(ledger.snapshot().classBytes[0], 40u);
}

TEST(WorldGenerationMemoryBatchTest, PartsTransferAndReleaseIndependentlyWithoutRecharging) {
  WorldGenerationMemoryLedger ledger; auto batch = ledger.reserveBatch(requests()); ASSERT_TRUE(batch);
  auto output = std::move((*batch)[0]); batch.reset();
  EXPECT_EQ(ledger.snapshot().reservedBytes, 60u); EXPECT_EQ(ledger.snapshot().resources, 1u);
  output.reset(); EXPECT_EQ(ledger.snapshot().resources, 0u);
}

TEST(WorldGenerationMemoryBatchTest, EstimatePartsKeepTheirOriginalDemand) {
  WorldGenerationMemoryLedger ledger(1000, Limits); auto spec = ledger.withDemand(Demand::Speculative);
  auto batch = spec.reserveBatch(requests()); ASSERT_TRUE(batch);
  EXPECT_FALSE((*batch)[1]->resizeEstimate(41)); EXPECT_TRUE((*batch)[1]->resizeEstimate(20));
  EXPECT_TRUE((*batch)[1]->resizeEstimate(21)); EXPECT_EQ(ledger.snapshot().reservedBytes, 81u);
}

TEST(WorldGenerationMemoryBatchTest, FacadeDestructionDoesNotForgetOutstandingParts) {
  auto ledger = std::make_unique<WorldGenerationMemoryLedger>(1000, Limits); auto observer = *ledger;
  auto batch = ledger->reserveBatch(requests()); ASSERT_TRUE(batch); ledger.reset();
  EXPECT_EQ(observer.snapshot().reservedBytes, 100u); batch.reset();
  EXPECT_EQ(observer.snapshot().reservedBytes, 0u);
}

TEST(WorldGenerationMemoryBatchTest, EightContendersCannotPartiallyConsumeTheLastBatch) {
  WorldGenerationMemoryLedger ledger(1000, Limits); std::array<std::optional<Pair>, 8> held;
  std::atomic<unsigned> accepted{0};
  simultaneous([&](unsigned i) { held[i] = ledger.reserveBatch(requests(90, 60)); if (held[i]) ++accepted; });
  EXPECT_EQ(accepted.load(), 1u); EXPECT_EQ(ledger.snapshot().resources, 2u);
  EXPECT_EQ(ledger.snapshot().reservedBytes, 150u); EXPECT_EQ(ledger.snapshot().admission.pressureRefusals, 7u);
}

TEST(WorldGenerationMemoryBatchTest, BatchAndSingleReservationsShareOneAtomicBudget) {
  WorldGenerationMemoryLedger ledger(1000, Limits); std::array<std::optional<Pair>, 8> batches;
  std::array<Token, 8> singles; std::atomic<unsigned> accepted{0};
  simultaneous([&](unsigned i) {
    if (i % 2) { singles[i] = ledger.reserve(Class::MicroDungeonHalo, 150); if (singles[i]) ++accepted; }
    else { batches[i] = ledger.reserveBatch(requests(90, 60)); if (batches[i]) ++accepted; }
  });
  EXPECT_EQ(accepted.load(), 1u); EXPECT_EQ(ledger.snapshot().reservedBytes, 150u);
  EXPECT_TRUE(worldGenerationMemoryEnvelope(ledger.snapshot()).coherent);
}

TEST(WorldGenerationMemoryBatchTest, ConcurrentAcquireReleaseAndSnapshotsRemainCoherent) {
  WorldGenerationMemoryLedger ledger; std::atomic<unsigned> failures{0};
  simultaneous([&](unsigned) {
    for (unsigned i = 0; i < 100; ++i) {
      auto batch = ledger.reserveBatch(requests());
      if (!batch || !worldGenerationMemoryEnvelope(ledger.snapshot()).coherent) ++failures;
    }
  });
  EXPECT_EQ(failures.load(), 0u); EXPECT_EQ(ledger.snapshot().resources, 0u);
}

TEST(WorldGenerationMemoryBatchTest, ReservedCacheMissCanCompleteAtHardWithoutSecondAdmission) {
  WorldGenerationMemoryLedger ledger(1000, Limits); Cache cache; auto source = std::make_shared<unsigned>(1);
  auto batch = ledger.reserveBatch(requests(136, 64)); ASSERT_TRUE(batch);
  auto checks = ledger.snapshot().admission.pressureChecks;
  auto use = cache.getReserved(source, 1, ledger, 64, std::move((*batch)[1]), measure, produce);
  ASSERT_TRUE(use); EXPECT_EQ(ledger.snapshot().reservedBytes, 200u);
  EXPECT_EQ(ledger.snapshot().admission.pressureChecks, checks); EXPECT_TRUE(use.estimateComplete());
}

TEST(WorldGenerationMemoryBatchTest, ReservedCacheHitReturnsUnusedBytesWithoutRebuilding) {
  WorldGenerationMemoryLedger ledger; Cache cache; auto source = std::make_shared<unsigned>(1);
  auto old = cache.get(source, 1, ledger, 64, measure, produce);
  auto batch = ledger.reserveBatch(requests(60, 64)); ASSERT_TRUE(batch);
  EXPECT_EQ(ledger.snapshot().reservedBytes, 188u);
  auto fail = []() -> std::shared_ptr<Payload> { throw std::runtime_error("must not rebuild"); };
  auto use = cache.getReserved(source, 1, ledger, 64, std::move((*batch)[1]), measure, fail);
  ASSERT_TRUE(use); EXPECT_EQ(use.value(), old.value()); EXPECT_EQ(ledger.snapshot().reservedBytes, 124u);
}

TEST(WorldGenerationMemoryBatchTest, ReplacementOverlapsOldCacheAndPreservesItsActiveUse) {
  WorldGenerationMemoryLedger ledger(1000, Limits); Cache cache; auto source = std::make_shared<unsigned>(1);
  auto old = cache.get(source, 1, ledger, 64, measure, produce);
  auto batch = ledger.reserveBatch(requests(72, 64)); ASSERT_TRUE(batch);
  auto next = cache.getReserved(source, 2, ledger, 64, std::move((*batch)[1]), measure, produce);
  ASSERT_TRUE(next); EXPECT_NE(old.value(), next.value()); EXPECT_EQ(ledger.snapshot().reservedBytes, 200u);
  EXPECT_FALSE(ledger.reserveBatch(requests(0, 1)));
}

TEST(WorldGenerationMemoryBatchTest, ThrowingReplacementKeepsOldCacheAndRollsBackOnlyNewPart) {
  WorldGenerationMemoryLedger ledger; Cache cache; auto source = std::make_shared<unsigned>(1);
  auto old = cache.get(source, 1, ledger, 64, measure, produce);
  auto batch = ledger.reserveBatch(requests(60, 64)); ASSERT_TRUE(batch);
  auto fail = []() -> std::shared_ptr<Payload> { throw std::runtime_error("controlled replacement"); };
  EXPECT_THROW(cache.getReserved(source, 2, ledger, 64, std::move((*batch)[1]), measure, fail), std::runtime_error);
  EXPECT_EQ(ledger.snapshot().reservedBytes, 124u);
  auto same = cache.get(source, 1, ledger, 64, measure, fail); EXPECT_EQ(same.value(), old.value());
}

TEST(WorldGenerationMemoryBatchTest, InvalidCacheReservationsNeverBuildOrBypassEvenACacheHit) {
  WorldGenerationMemoryLedger ledger, foreign; Cache cache; auto source = std::make_shared<unsigned>(1);
  unsigned calls = 0; auto create = [&] { ++calls; return produce(); };
  { auto use = cache.get(source, 1, ledger, 64, measure, create); }
  std::array<Token, 5> invalid{{{}, foreign.reserve(Class::WorkerTemplateEstimate, 64),
      ledger.reserve(Class::BaseTilesArray, 64), ledger.reserve(Class::WorkerTemplateEstimate, 63),
      ledger.withDemand(Demand::Speculative).reserve(Class::WorkerTemplateEstimate, 64)}};
  for (auto& token : invalid) {
    EXPECT_FALSE(cache.getReserved(source, 1, ledger, 64, token, measure, create));
    EXPECT_FALSE(cache.getReserved(source, 2, ledger, 64, token, measure, create));
  }
  EXPECT_EQ(calls, 1u);
}

TEST(WorldGenerationMemoryBatchTest, SharedJsonTokenCannotMasqueradeAsWorkerStorage) {
  WorldGenerationMemoryLedger ledger; Cache cache; auto source = std::make_shared<unsigned>(1);
  auto token = ledger.reserveSharedJson(source, 64); unsigned calls = 0;
  EXPECT_FALSE(cache.getReserved(source, 1, ledger, 64, token, measure, [&] { ++calls; return produce(); }));
  EXPECT_EQ(calls, 0u); EXPECT_EQ(ledger.snapshot().sharedResources, 1u);
}

TEST(WorldGenerationMemoryBatchTest, CacheIdentityIncludesLedgerEvenWhenSourceAndRevisionMatch) {
  WorldGenerationMemoryLedger first, second; Cache cache; auto source = std::make_shared<unsigned>(1);
  auto old = cache.get(source, 1, first, 64, measure, produce);
  auto batch = second.reserveBatch(requests(60, 64)); ASSERT_TRUE(batch);
  auto next = cache.getReserved(source, 1, second, 64, std::move((*batch)[1]), measure, produce);
  ASSERT_TRUE(next); EXPECT_NE(next.value(), old.value());
  EXPECT_EQ(first.snapshot().reservedBytes, 64u); EXPECT_EQ(second.snapshot().reservedBytes, 124u);
  auto again = cache.get(source, 1, first, 64, measure, produce);
  EXPECT_NE(again.value(), next.value()); EXPECT_EQ(first.snapshot().reservedBytes, 128u);
}

TEST(WorldGenerationMemoryBatchTest, ChangingDemandCannotReuseAnEntryChargedUnderAnotherDemand) {
  WorldGenerationMemoryLedger ledger(1000, Limits); auto spec = ledger.withDemand(Demand::Speculative);
  Cache cache; auto source = std::make_shared<unsigned>(1);
  auto old = cache.get(source, 1, spec, 64, measure, produce);
  auto batch = ledger.reserveBatch(requests(72, 64)); ASSERT_TRUE(batch);
  auto next = cache.getReserved(source, 1, ledger, 64, std::move((*batch)[1]), measure, produce);
  ASSERT_TRUE(next); EXPECT_NE(next.value(), old.value()); EXPECT_EQ(ledger.snapshot().reservedBytes, 200u);
}

TEST(WorldGenerationMemoryBatchTest, BoundedContractDoesNotPretendUnpredictedGrowthWillFit) {
  WorldGenerationMemoryLedger ledger(1000, Limits); Cache cache; auto source = std::make_shared<unsigned>(1);
  auto batch = ledger.reserveBatch(requests(136, 64)); ASSERT_TRUE(batch);
  auto larger = [] { auto value = produce(); value->bytes = 65; return value; };
  auto use = cache.getReserved(source, 1, ledger, 64, std::move((*batch)[1]), measure, larger);
  ASSERT_TRUE(use); EXPECT_FALSE(use.estimateComplete());
  EXPECT_EQ(ledger.snapshot().reservedBytes, 200u); EXPECT_EQ(ledger.snapshot().incompleteEstimateUpdates, 1u);
}

TEST(WorldGenerationMemoryBatchTest, RefusedBatchRollsBackTaskBeforeAnyPrivateCopy) {
  WorldGenerationTasks tasks(1, partition(), 1, 1000); WorldGenerationMemoryLedger ledger(99);
  unsigned copies = 0;
  {
    WorldGenerationTaskPreparation preparation(tasks, {0, 0}, WorldGenerationTaskStage::BaseTiles, 60);
    ASSERT_TRUE(preparation.task()); auto batch = ledger.reserveBatch(requests());
    if (batch) ++copies;
  }
  EXPECT_EQ(copies, 0u); EXPECT_EQ(tasks.size(), 0u); EXPECT_EQ(ledger.snapshot().resources, 0u);
}

TEST(WorldGenerationMemoryBatchTest, PartialConstructionFailureDestroysPayloadBeforeReleasingItsPart) {
  WorldGenerationMemoryLedger ledger; WorldGenerationTasks tasks(1, partition(), 1, 1000);
  struct Probe {
    WorldGenerationMemoryLedger* ledger = nullptr;
    ~Probe() { if (ledger) { EXPECT_GE(ledger->snapshot().reservedBytes, 60u); } }
  };
  EXPECT_THROW({
    WorldGenerationTaskPreparation preparation(tasks, {0, 0}, WorldGenerationTaskStage::BaseTiles, 60);
    auto batch = ledger.reserveBatch(requests()); ASSERT_TRUE(batch);
    auto input = std::make_shared<WorldGenerationTrackedResource<Probe>>(std::move((*batch)[0]));
    input->value.ledger = &ledger;
    throw std::runtime_error("controlled partial snapshot");
  }, std::runtime_error);
  EXPECT_EQ(tasks.size(), 0u); EXPECT_EQ(ledger.snapshot().resources, 0u);
}

TEST(WorldGenerationMemoryBatchTest, WorkerCompletesReservedInPlaceOutputAtHardAndReleasesOnDrain) {
  WorldGenerationMemoryLedger ledger(1000, Limits); WorkerPool pool("Compound BaseTiles contract", 1);
  auto source = std::make_shared<unsigned>(1); auto batch = ledger.reserveBatch(requests(136, 64)); ASSERT_TRUE(batch);
  auto input = std::make_shared<WorldGenerationTrackedResource<std::array<unsigned, 34>>>(std::move((*batch)[0]));
  EXPECT_FALSE(ledger.reserve(Class::MicroDungeonHalo, 1));
  {
    auto promise = pool.addProducer<decltype(input)>([ledger, source, input, memory = std::move((*batch)[1])]() mutable {
      static thread_local Cache cache;
      auto use = cache.getReserved(source, 1, ledger, 64, std::move(memory), measure, produce);
      EXPECT_TRUE(use); input->value[0] = 42;
      return input;
    });
    input.reset(); batch.reset();
    auto output = promise.get(); EXPECT_EQ(output->value[0], 42u);
    EXPECT_EQ(ledger.snapshot().reservedBytes, 200u);
    pool.finish(); EXPECT_EQ(ledger.snapshot().reservedBytes, 136u);
    EXPECT_EQ(ledger.snapshot().admission.pressureChecks, 2u);
  }
  EXPECT_EQ(ledger.snapshot().resources, 0u);
}

TEST(WorldGenerationMemoryBatchTest, SeparateInputOutputAndOverlapPartsTransferWithoutNewAdmission) {
  WorldGenerationMemoryLedger ledger(1000, Limits);
  auto batch = ledger.reserveBatch(std::array<Request, 3>{{
      {Class::MicroDungeonHalo, 80}, {Class::BaseTilesArray, 70}, {Class::WorkerTemplateEstimate, 50}}});
  ASSERT_TRUE(batch);
  auto input = std::make_shared<WorldGenerationTrackedResource<std::array<char, 80>>>(std::move((*batch)[0]));
  auto output = std::make_shared<WorldGenerationTrackedResource<std::array<char, 70>>>(std::move((*batch)[1]));
  input->value[0] = 'x'; output->value[0] = input->value[0];
  EXPECT_EQ(ledger.snapshot().reservedBytes, 200u); EXPECT_EQ(ledger.snapshot().admission.pressureChecks, 1u);
  batch.reset(); input.reset(); EXPECT_EQ(ledger.snapshot().reservedBytes, 70u);
  EXPECT_EQ(output->value[0], 'x');
}

TEST(WorldGenerationMemoryBatchTest, SubmissionFailureRollsBackTaskAndEveryUnsubmittedPart) {
  WorldGenerationMemoryLedger ledger; WorldGenerationTasks tasks(1, partition(), 1, 1000);
  using Input = std::array<char, 60>;
  EXPECT_THROW({
    WorldGenerationTaskPreparation preparation(tasks, {0, 0}, WorldGenerationTaskStage::BaseTiles, 60);
    auto batch = ledger.reserveBatch(requests()); ASSERT_TRUE(batch);
    auto input = std::make_shared<WorldGenerationTrackedResource<Input>>(std::move((*batch)[0]));
    // Same unwind as addProducer throwing before it accepts the closure.
    auto closure = ([input, memory = std::move((*batch)[1])] {});
    (void)closure;
    throw std::runtime_error("controlled pre-acceptance submission failure");
  }, std::runtime_error);
  EXPECT_EQ(tasks.size(), 0u); EXPECT_EQ(ledger.snapshot().resources, 0u);
  EXPECT_EQ(ledger.snapshot().reservedBytes, 0u);
}

TEST(WorldGenerationMemoryBatchTest, LogicalCancellationCannotReclaimPhysicalCompoundParts) {
  WorldGenerationMemoryLedger ledger; WorldGenerationTasks tasks(1, partition(), 1, 1000);
  shared_ptr<WorldGenerationTask> task;
  std::shared_ptr<WorldGenerationTrackedResource<std::array<char, 60>>> input;
  Token worker;
  {
    WorldGenerationTaskPreparation preparation(tasks, {0, 0}, WorldGenerationTaskStage::BaseTiles, 60);
    task = preparation.task(); ASSERT_TRUE(task);
    auto batch = ledger.reserveBatch(requests()); ASSERT_TRUE(batch);
    input = std::make_shared<WorldGenerationTrackedResource<std::array<char, 60>>>(std::move((*batch)[0]));
    worker = std::move((*batch)[1]); preparation.handoff();
  }
  task->invalidate(); EXPECT_FALSE(tasks.retire(task, WorldGenerationTaskOutcome::Cancelled));
  EXPECT_EQ(ledger.snapshot().reservedBytes, 100u);
  ASSERT_TRUE(task->start()); ASSERT_TRUE(task->ready());
  EXPECT_TRUE(tasks.retire(task, WorldGenerationTaskOutcome::Cancelled));
  // Registry drain does not free physical owners (including a retained result).
  EXPECT_EQ(ledger.snapshot().reservedBytes, 100u);
  worker.reset(); EXPECT_EQ(ledger.snapshot().reservedBytes, 60u);
  input.reset(); EXPECT_EQ(ledger.snapshot().reservedBytes, 0u);
}
