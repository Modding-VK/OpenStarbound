#include "StarWorldGenerationMemoryRetry.hpp"
#include "StarWorldGenerationMemoryEnvelope.hpp"
#include "StarWorkerPool.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <future>
#include <thread>
#include <vector>

#include "gtest/gtest.h"

using namespace Star;

namespace {
using Class = WorldGenerationMemoryClass;
using Demand = WorldGenerationMemoryDemand;
using BatchStatus = WorldGenerationMemoryBatchStatus;
using Retry = WorldGenerationMemoryRetry<2>;
using Status = WorldGenerationMemoryRetryStatus;
using Requests = Retry::Requests;
using Resource = WorldGenerationTrackedResource<std::unique_ptr<unsigned char[]>>;
WorldGenerationMemoryPressureLimits const Limits{true, 80, 100, 200};
Requests requests(size_t input = 60, size_t output = 20) {
  return {{{Class::MicroDungeonHalo, input}, {Class::BaseTilesArray, output}}};
}
struct VariableResult {
  size_t attempt;
  Requests required;
  std::shared_ptr<Resource> output;
};
// Test-only pure producer. Never allocate a discovered oversized output: return
// just a descriptor, unwinding all input/unused prediction owners before retry.
VariableResult produce(Retry::Attempt attempt, Requests predicted, size_t needed, unsigned seed,
    std::atomic<unsigned>& outputAllocations) {
  auto input = std::make_shared<Resource>(std::move((*attempt.memory)[0]));
  input->value = std::make_unique<unsigned char[]>(predicted[0].bytes);
  for (size_t i = 0; i < predicted[0].bytes; ++i) input->value[i] = static_cast<unsigned char>(seed + i);
  if (needed > predicted[1].bytes) return {attempt.attempt, requests(predicted[0].bytes, needed), {}};
  auto output = std::make_shared<Resource>(std::move((*attempt.memory)[1]));
  ++outputAllocations;
  output->value = std::make_unique<unsigned char[]>(predicted[1].bytes);
  for (size_t i = 0; i < needed; ++i) output->value[i] = input->value[i % predicted[0].bytes] ^ (i % 251);
  return {attempt.attempt, predicted, std::move(output)};
}
void checkOutput(VariableResult const& result, unsigned seed) {
  ASSERT_TRUE(result.output);
  for (size_t i = 0; i < result.required[1].bytes; ++i)
    EXPECT_EQ(result.output->value[i], static_cast<unsigned char>(seed + i % result.required[0].bytes) ^ (i % 251));
}
struct OpenGate {
  explicit OpenGate(WorkerPool& pool) : pool(pool) {}
  WorkerPool& pool;
  std::promise<void> promise;
  bool opened = false;
  void open() { if (!opened) { opened = true; promise.set_value(); } }
  ~OpenGate() { open(); pool.finish(); }
};
}

TEST(WorldGenerationMemoryRetryTest, DetailedAdmissionReturnsItsOwnAtomicStatusAndParts) {
  WorldGenerationMemoryLedger ledger(1000, Limits);
  auto admitted = ledger.tryReserveBatch(requests());
  EXPECT_EQ(admitted.status, BatchStatus::Admitted); ASSERT_TRUE(admitted.memory);
  EXPECT_EQ(admitted.requestedBytes, 80u); EXPECT_EQ(ledger.snapshot().resources, 2u);
  EXPECT_EQ(ledger.snapshot().admission.pressureChecks, 1u);
}

TEST(WorldGenerationMemoryRetryTest, TemporaryPressureDoesNotLeakInactiveTokens) {
  WorldGenerationMemoryLedger ledger(1000, Limits); auto held = ledger.reserve(Class::BaseTilesArray, 150);
  auto refused = ledger.tryReserveBatch(requests());
  EXPECT_EQ(refused.status, BatchStatus::Deferred); EXPECT_FALSE(refused.memory);
  EXPECT_EQ(refused.requestedBytes, 80u); EXPECT_EQ(ledger.snapshot().resources, 1u);
  held.reset(); EXPECT_EQ(ledger.tryReserveBatch(requests()).status, BatchStatus::Admitted);
}

TEST(WorldGenerationMemoryRetryTest, ImpossibleUsesTheDeclaredHardLimitNotCurrentFreeBytes) {
  WorldGenerationMemoryLedger ledger(1000, Limits);
  auto refused = ledger.tryReserveBatch(requests(60, 141));
  EXPECT_EQ(refused.status, BatchStatus::Impossible); EXPECT_FALSE(refused.memory);
  EXPECT_EQ(refused.requestedBytes, 201u); EXPECT_EQ(ledger.snapshot().reservedBytes, 0u);
}

TEST(WorldGenerationMemoryRetryTest, ImpossibleSpeculationCanStillBePossibleForADependency) {
  WorldGenerationMemoryLedger ledger(1000, Limits);
  EXPECT_EQ(ledger.withDemand(Demand::Speculative).tryReserveBatch(requests(60, 41)).status, BatchStatus::Impossible);
  EXPECT_EQ(ledger.withDemand(Demand::Dependency).tryReserveBatch(requests(60, 41)).status, BatchStatus::Admitted);
}

TEST(WorldGenerationMemoryRetryTest, LegacyFiniteMaximumAlsoDistinguishesImpossibleFromDeferred) {
  WorldGenerationMemoryLedger ledger(100);
  EXPECT_EQ(ledger.tryReserveBatch(requests(60, 41)).status, BatchStatus::Impossible);
  auto held = ledger.reserve(Class::BaseTilesArray, 30);
  EXPECT_EQ(ledger.tryReserveBatch(requests()).status, BatchStatus::Deferred);
  EXPECT_FALSE(ledger.snapshot().admission.enabled);
}

TEST(WorldGenerationMemoryRetryTest, InvalidClassesJsonAndOverflowAreNotRetryablePressure) {
  WorldGenerationMemoryLedger ledger;
  auto invalid = requests(); invalid[1].kind = Class::Count;
  EXPECT_EQ(ledger.tryReserveBatch(invalid).status, BatchStatus::Invalid);
  invalid[1].kind = Class::TemplateJsonEstimate;
  EXPECT_EQ(ledger.tryReserveBatch(invalid).status, BatchStatus::Invalid);
  auto overflow = ledger.tryReserveBatch(requests(std::numeric_limits<size_t>::max(), 1));
  EXPECT_EQ(overflow.status, BatchStatus::Invalid); EXPECT_EQ(overflow.requestedBytes, 0u);
  EXPECT_FALSE(overflow.memory); EXPECT_EQ(ledger.snapshot().resources, 0u);
}

TEST(WorldGenerationMemoryRetryTest, EmptyAndZeroByteBatchesKeepTheirExistingSemantics) {
  WorldGenerationMemoryLedger ledger(1000, Limits); auto full = ledger.reserve(Class::BaseTilesArray, 200);
  auto empty = ledger.tryReserveBatch(std::array<WorldGenerationMemoryRequest, 0>{});
  EXPECT_EQ(empty.status, BatchStatus::Admitted); ASSERT_TRUE(empty.memory); EXPECT_TRUE(empty.memory->empty());
  auto zero = ledger.tryReserveBatch(requests(0, 0)); EXPECT_EQ(zero.status, BatchStatus::Admitted);
  EXPECT_EQ(ledger.snapshot().resources, 3u);
}

TEST(WorldGenerationMemoryRetryTest, InvalidRetryContractsCannotPublishAPolicy) {
  WorldGenerationMemoryLedger ledger; auto invalid = requests(); invalid[1].kind = Class::TemplateJsonEstimate;
  EXPECT_THROW((Retry(ledger, Demand::Visible, requests(), 0)), std::invalid_argument);
  EXPECT_THROW((Retry(ledger, Demand::Visible, invalid, 2)), std::invalid_argument);
  EXPECT_THROW((Retry(ledger, static_cast<Demand>(99), requests(), 2)), std::invalid_argument);
  EXPECT_THROW((Retry(ledger, Demand::Visible, requests(std::numeric_limits<size_t>::max(), 1), 2)), std::invalid_argument);
  EXPECT_EQ(ledger.snapshot().resources, 0u);
}

TEST(WorldGenerationMemoryRetryTest, ActiveAttemptCannotBeDispatchedTwiceEvenIfItsPartsAreReleased) {
  WorldGenerationMemoryLedger ledger; Retry retry(ledger, Demand::Visible, requests(), 3);
  auto attempt = retry.admit(0); EXPECT_EQ(attempt.status, Status::Admitted); EXPECT_EQ(attempt.attempt, 1u);
  attempt.memory.reset(); EXPECT_EQ(retry.admit(1).status, Status::Active);
  EXPECT_EQ(retry.attempts(), 1u); EXPECT_TRUE(retry.complete(1));
}

TEST(WorldGenerationMemoryRetryTest, CompletionIsTerminalButDoesNotReleaseARetainedResult) {
  WorldGenerationMemoryLedger ledger; Retry retry(ledger, Demand::Visible, requests(), 2);
  auto attempt = retry.admit(0); ASSERT_TRUE(attempt.memory);
  EXPECT_TRUE(retry.complete(attempt.attempt)); EXPECT_FALSE(retry.complete(attempt.attempt));
  EXPECT_EQ(retry.admit(99).status, Status::Completed); EXPECT_EQ(ledger.snapshot().reservedBytes, 80u);
  EXPECT_FALSE(retry.retry(attempt.attempt, requests(60, 60)));
  attempt.memory.reset(); EXPECT_EQ(ledger.snapshot().resources, 0u);
}

TEST(WorldGenerationMemoryRetryTest, GrowthWaitsForEveryPhysicalTokenBeforeReadmitting) {
  WorldGenerationMemoryLedger ledger; Retry retry(ledger, Demand::Visible, requests(), 3);
  auto attempt = retry.admit(0); ASSERT_TRUE(attempt.memory);
  auto input = std::make_shared<Resource>(std::move((*attempt.memory)[0]));
  EXPECT_TRUE(retry.retry(attempt.attempt, requests(60, 60)));
  EXPECT_EQ(retry.admit(1).status, Status::AwaitRelease);
  attempt.memory.reset(); EXPECT_EQ(retry.admit(2).status, Status::AwaitRelease);
  input.reset(); auto next = retry.admit(2); EXPECT_EQ(next.status, Status::Admitted); EXPECT_EQ(next.attempt, 2u);
  EXPECT_EQ(ledger.snapshot().reservedBytes, 120u);
}

TEST(WorldGenerationMemoryRetryTest, RetryPolicyItselfDoesNotKeepTheOldPhysicalPartsAlive) {
  WorldGenerationMemoryLedger ledger; Retry retry(ledger, Demand::Visible, requests(), 3);
  auto attempt = retry.admit(0); ASSERT_TRUE(attempt.memory);
  std::weak_ptr<WorldGenerationMemoryReservation> weak = (*attempt.memory)[0];
  EXPECT_TRUE(retry.retry(1, requests(60, 60))); attempt.memory.reset();
  EXPECT_TRUE(weak.expired()); EXPECT_EQ(ledger.snapshot().resources, 0u);
  EXPECT_EQ(retry.admit(0).status, Status::Admitted);
}

TEST(WorldGenerationMemoryRetryTest, RetryNeedsStrictGrowthAndPreservesClassesAndAllPriorPredictions) {
  WorldGenerationMemoryLedger ledger; Retry retry(ledger, Demand::Visible, requests(), 3);
  auto attempt = retry.admit(0); auto wrongClass = requests(60, 60); wrongClass[1].kind = Class::MicroDungeonHalo;
  EXPECT_FALSE(retry.retry(1, requests())); EXPECT_FALSE(retry.retry(1, requests(59, 60)));
  EXPECT_FALSE(retry.retry(1, wrongClass));
  EXPECT_FALSE(retry.retry(1, requests(std::numeric_limits<size_t>::max(), 1)));
  EXPECT_EQ(retry.requests()[1].bytes, 20u); EXPECT_EQ(retry.admit(0).status, Status::Active);
}

TEST(WorldGenerationMemoryRetryTest, StaleAttemptDescriptorsCannotChangeANewerAttempt) {
  WorldGenerationMemoryLedger ledger; Retry retry(ledger, Demand::Visible, requests(), 3);
  EXPECT_FALSE(retry.retry(0, requests(60, 60))); EXPECT_FALSE(retry.complete(0));
  auto first = retry.admit(0); EXPECT_TRUE(retry.retry(1, requests(60, 60))); first.memory.reset();
  auto second = retry.admit(1); ASSERT_TRUE(second.memory);
  EXPECT_FALSE(retry.retry(1, requests(60, 80))); EXPECT_FALSE(retry.complete(1));
  EXPECT_EQ(retry.requests()[1].bytes, 60u); EXPECT_TRUE(retry.complete(2));
}

TEST(WorldGenerationMemoryRetryTest, RepeatedPressurePollsWaitForAnOwnerProgressEvent) {
  WorldGenerationMemoryLedger ledger(1000, Limits); auto held = ledger.reserve(Class::BaseTilesArray, 150);
  Retry retry(ledger, Demand::Visible, requests(), 2);
  EXPECT_EQ(retry.admit(7).status, Status::Deferred); auto checks = ledger.snapshot().admission.pressureChecks;
  for (unsigned i = 0; i < 50; ++i) EXPECT_EQ(retry.admit(7).status, Status::AwaitProgress);
  EXPECT_EQ(ledger.snapshot().admission.pressureChecks, checks); EXPECT_EQ(retry.attempts(), 0u);
  held.reset(); EXPECT_EQ(retry.admit(7).status, Status::AwaitProgress);
  EXPECT_EQ(retry.admit(8).status, Status::Admitted);
}

TEST(WorldGenerationMemoryRetryTest, AnEventWithoutCapacityDoesNotGrantAnythingOrConsumeAnAttempt) {
  WorldGenerationMemoryLedger ledger(1000, Limits); auto held = ledger.reserve(Class::BaseTilesArray, 150);
  Retry retry(ledger, Demand::Visible, requests(), 2);
  EXPECT_EQ(retry.admit(0).status, Status::Deferred); EXPECT_EQ(retry.admit(1).status, Status::Deferred);
  EXPECT_EQ(retry.attempts(), 0u); EXPECT_EQ(ledger.snapshot().reservedBytes, 150u);
}

TEST(WorldGenerationMemoryRetryTest, ImpossiblePredictionTerminatesWithoutPollingForever) {
  WorldGenerationMemoryLedger ledger(1000, Limits); Retry retry(ledger, Demand::Visible, requests(60, 141), 3);
  EXPECT_EQ(retry.admit(0).status, Status::Impossible); auto checks = ledger.snapshot().admission.pressureChecks;
  EXPECT_EQ(retry.admit(99).status, Status::Impossible); EXPECT_EQ(retry.attempts(), 0u);
  EXPECT_EQ(ledger.snapshot().admission.pressureChecks, checks);
}

TEST(WorldGenerationMemoryRetryTest, DiscoveredOversizedOutputTerminatesAfterReleasingItsAttempt) {
  WorldGenerationMemoryLedger ledger(1000, Limits); Retry retry(ledger, Demand::Visible, requests(), 3);
  auto attempt = retry.admit(0); EXPECT_TRUE(retry.retry(1, requests(60, 141)));
  EXPECT_EQ(retry.admit(0).status, Status::AwaitRelease); attempt.memory.reset();
  EXPECT_EQ(retry.admit(0).status, Status::Impossible); EXPECT_EQ(ledger.snapshot().resources, 0u);
}

TEST(WorldGenerationMemoryRetryTest, FiniteAttemptCountBoundsAProducerWhichKeepsUnderpredicting) {
  WorldGenerationMemoryLedger ledger; Retry retry(ledger, Demand::Visible, requests(), 2);
  auto first = retry.admit(0); EXPECT_TRUE(retry.retry(1, requests(60, 21))); first.memory.reset();
  auto second = retry.admit(1); EXPECT_TRUE(retry.retry(2, requests(60, 22)));
  EXPECT_EQ(retry.admit(2).status, Status::Exhausted);
  EXPECT_EQ(ledger.snapshot().reservedBytes, 81u); second.memory.reset();
  EXPECT_EQ(retry.admit(3).status, Status::Exhausted); EXPECT_EQ(ledger.snapshot().resources, 0u);
}

TEST(WorldGenerationMemoryRetryTest, StopDoesNotPretendToCancelOrFreeAPhysicalProducer) {
  WorldGenerationMemoryLedger ledger; Retry retry(ledger, Demand::Visible, requests(), 3);
  auto attempt = retry.admit(0); retry.stop();
  EXPECT_EQ(retry.admit(1).status, Status::Stopped); EXPECT_FALSE(retry.complete(1));
  EXPECT_FALSE(retry.retry(1, requests(60, 60))); EXPECT_EQ(ledger.snapshot().reservedBytes, 80u);
  attempt.memory.reset(); EXPECT_EQ(ledger.snapshot().resources, 0u);
}

TEST(WorldGenerationMemoryRetryTest, LedgerAndDemandAreBoundOnceNotChangedByRetrying) {
  auto ledger = std::make_unique<WorldGenerationMemoryLedger>(1000, Limits); auto observer = *ledger;
  Retry retry(*ledger, Demand::Speculative, requests(), 3); ledger.reset();
  auto first = retry.admit(0); EXPECT_TRUE(retry.retry(1, requests(60, 41))); first.memory.reset();
  EXPECT_EQ(retry.admit(1).status, Status::Impossible); EXPECT_EQ(observer.snapshot().resources, 0u);
  EXPECT_EQ(observer.tryReserveBatch(requests(60, 41)).status, BatchStatus::Admitted);
}

TEST(WorldGenerationMemoryRetryTest, ReturnedStatusDoesNotChangeWhenAnotherThreadAdmitsOrReleases) {
  WorldGenerationMemoryLedger ledger(1000, Limits); auto held = ledger.reserve(Class::BaseTilesArray, 150);
  auto result = ledger.tryReserveBatch(requests()); EXPECT_EQ(result.status, BatchStatus::Deferred);
  std::thread other([&] { held.reset(); auto granted = ledger.tryReserveBatch(requests()); EXPECT_TRUE(granted.memory); });
  other.join(); EXPECT_EQ(result.status, BatchStatus::Deferred); EXPECT_FALSE(result.memory);
}

TEST(WorldGenerationMemoryRetryTest, SingleWorkerCanRunOtherWorkAfterAbortingAnOversizedOutput) {
  WorldGenerationMemoryLedger ledger(1000, Limits); WorkerPool pool("Variable memory nonblocking contract", 1);
  Retry retry(ledger, Demand::Visible, requests(), 3); auto attempt = retry.admit(0);
  std::atomic<unsigned> allocations{0};
  auto promise = pool.addProducer<VariableResult>([attempt = std::move(attempt), &allocations]() mutable {
    return produce(std::move(attempt), requests(), 60, 3, allocations);
  });
  auto other = pool.addProducer<unsigned>([] { return 42u; });
  EXPECT_TRUE(other.wait(5000)); EXPECT_EQ(other.get(), 42u);
  auto const& result = promise.get(); EXPECT_FALSE(result.output); EXPECT_EQ(allocations.load(), 0u);
  EXPECT_TRUE(retry.retry(result.attempt, result.required)); pool.finish();
  EXPECT_EQ(ledger.snapshot().resources, 0u); EXPECT_EQ(retry.admit(1).status, Status::Admitted);
}

TEST(WorldGenerationMemoryRetryTest, TwoWorkersAndRetainedResultsProgressWithoutHoldingInputsForMemory) {
  WorldGenerationMemoryLedger ledger(1000, Limits); WorkerPool pool("Two variable memory jobs", 2);
  std::promise<void> bothEntered; auto enteredFuture = bothEntered.get_future(); std::atomic<unsigned> entered{0}, allocations{0};
  OpenGate gate(pool); auto release = gate.promise.get_future().share();
  auto retained = makeWorldGenerationTrackedResource<std::array<char, 40>>(ledger, Class::BaseTilesArray, 40);
  Retry a(ledger, Demand::Visible, requests(), 3), b(ledger, Demand::Dependency, requests(), 3);
  auto aa = a.admit(0), bb = b.admit(0); ASSERT_TRUE(aa.memory); ASSERT_TRUE(bb.memory);
  EXPECT_EQ(ledger.snapshot().reservedBytes, 200u);
  auto dispatch = [&](Retry::Attempt attempt, unsigned seed) {
    return pool.addProducer<VariableResult>([attempt = std::move(attempt), seed, release, &entered, &bothEntered, &allocations]() mutable {
      if (entered.fetch_add(1) == 1) bothEntered.set_value();
      release.wait(); // Test ordering barrier, never a wait for budget.
      return produce(std::move(attempt), requests(), 60, seed, allocations);
    });
  };
  auto ap = dispatch(std::move(aa), 7), bp = dispatch(std::move(bb), 19);
  ASSERT_EQ(enteredFuture.wait_for(std::chrono::seconds(5)), std::future_status::ready);
  EXPECT_EQ(ledger.snapshot().reservedBytes, 200u); gate.open();
  auto ar = ap.get(), br = bp.get(); EXPECT_FALSE(ar.output); EXPECT_FALSE(br.output);
  EXPECT_TRUE(a.retry(ar.attempt, ar.required)); EXPECT_TRUE(b.retry(br.attempt, br.required));
  pool.finish(); EXPECT_EQ(allocations.load(), 0u); EXPECT_EQ(ledger.snapshot().reservedBytes, 40u);
  auto nextA = a.admit(1); ASSERT_TRUE(nextA.memory);
  pool.start(1);
  {
    auto result = pool.addProducer<VariableResult>([attempt = std::move(nextA), predicted = a.requests(), &allocations]() mutable {
      return produce(std::move(attempt), predicted, 60, 7, allocations);
    });
    ar = result.get(); pool.finish();
  }
  ASSERT_TRUE(a.complete(ar.attempt));
  checkOutput(ar, 7); EXPECT_EQ(ledger.snapshot().reservedBytes, 100u);
  EXPECT_EQ(b.admit(1).status, Status::Deferred); EXPECT_EQ(ledger.snapshot().reservedBytes, 100u);
  retained.reset(); EXPECT_EQ(b.admit(1).status, Status::AwaitProgress);
  auto nextB = b.admit(2); ASSERT_TRUE(nextB.memory);
  pool.start(1);
  {
    auto result = pool.addProducer<VariableResult>([attempt = std::move(nextB), predicted = b.requests(), &allocations]() mutable {
      return produce(std::move(attempt), predicted, 60, 19, allocations);
    });
    br = result.get(); pool.finish();
  }
  EXPECT_TRUE(b.complete(br.attempt));
  checkOutput(br, 19); EXPECT_EQ(ledger.snapshot().reservedBytes, 120u);
  EXPECT_LE(ledger.snapshot().peakReservedBytes, 200u); EXPECT_EQ(allocations.load(), 2u);
  ar.output.reset(); br.output.reset(); EXPECT_EQ(ledger.snapshot().resources, 0u);
  EXPECT_TRUE(worldGenerationMemoryEnvelope(ledger.snapshot()).coherent);
}

TEST(WorldGenerationMemoryRetryTest, PhysicalBadAllocIsNotSilentlyConvertedIntoAPredictionRetry) {
  WorldGenerationMemoryLedger ledger; WorkerPool pool("Physical failure is not prediction pressure", 1);
  Retry retry(ledger, Demand::Visible, requests(), 3); auto attempt = retry.admit(0);
  auto promise = pool.addProducer<unsigned>([attempt = std::move(attempt)]() -> unsigned { throw std::bad_alloc(); });
  EXPECT_THROW(promise.get(), std::bad_alloc); pool.finish();
  EXPECT_EQ(retry.admit(1).status, Status::Active); EXPECT_EQ(retry.attempts(), 1u);
  retry.stop(); EXPECT_EQ(retry.admit(2).status, Status::Stopped); EXPECT_EQ(ledger.snapshot().resources, 0u);
}

TEST(WorldGenerationMemoryRetryTest, ProducerExceptionDoesNotAutomaticallyReplayOrPublishAnEmptyOutput) {
  WorldGenerationMemoryLedger ledger; WorkerPool pool("Producer failure contract", 1);
  Retry retry(ledger, Demand::Visible, requests(), 3); auto attempt = retry.admit(0); std::atomic<unsigned> calls{0};
  auto promise = pool.addProducer<unsigned>([attempt = std::move(attempt), &calls]() -> unsigned {
    ++calls; throw std::runtime_error("controlled producer failure");
  });
  EXPECT_THROW(promise.get(), std::runtime_error); pool.finish(); retry.stop();
  EXPECT_EQ(calls.load(), 1u); EXPECT_EQ(retry.admit(1).status, Status::Stopped);
  EXPECT_EQ(ledger.snapshot().resources, 0u);
}

TEST(WorldGenerationMemoryRetryTest, StopPreservesAnAlreadyRecordedTerminalOutcome) {
  WorldGenerationMemoryLedger ledger; Retry retry(ledger, Demand::Visible, requests(), 1);
  auto attempt = retry.admit(0); ASSERT_TRUE(retry.complete(attempt.attempt)); retry.stop();
  EXPECT_EQ(retry.admit(1).status, Status::Completed);
  WorldGenerationMemoryLedger small(10); Retry impossible(small, Demand::Visible, requests(), 1);
  EXPECT_EQ(impossible.admit(0).status, Status::Impossible); impossible.stop();
  EXPECT_EQ(impossible.admit(1).status, Status::Impossible);
}

TEST(WorldGenerationMemoryRetryTest, RetainedPartialOutputAlsoClosesThePhysicalReleaseFence) {
  WorldGenerationMemoryLedger ledger; Retry retry(ledger, Demand::Visible, requests(), 3);
  auto attempt = retry.admit(0); ASSERT_TRUE(attempt.memory);
  auto output = std::make_shared<Resource>(std::move((*attempt.memory)[1]));
  output->value = std::make_unique<unsigned char[]>(20);
  EXPECT_TRUE(retry.retry(attempt.attempt, requests(60, 60))); attempt.memory.reset();
  EXPECT_EQ(ledger.snapshot().reservedBytes, 20u);
  for (unsigned i = 0; i < 10; ++i) EXPECT_EQ(retry.admit(i).status, Status::AwaitRelease);
  output.reset(); EXPECT_EQ(retry.admit(10).status, Status::Admitted);
}

TEST(WorldGenerationMemoryRetryTest, EmptyContractCanCompleteButCannotInventGrowthOrExtraAttempts) {
  WorldGenerationMemoryLedger ledger; WorldGenerationMemoryRetry<0> retry(ledger, Demand::Visible, {}, 1);
  auto attempt = retry.admit(0); EXPECT_EQ(attempt.status, Status::Admitted); ASSERT_TRUE(attempt.memory);
  EXPECT_FALSE(retry.retry(attempt.attempt, {})); EXPECT_TRUE(retry.complete(attempt.attempt));
  EXPECT_EQ(retry.admit(1).status, Status::Completed); EXPECT_EQ(ledger.snapshot().resources, 0u);
}

TEST(WorldGenerationMemoryRetryTest, EightIndependentDescriptorsCannotMultiplyTheSharedSoftBudget) {
  WorldGenerationMemoryLedger ledger(1000, Limits);
  std::array<std::unique_ptr<Retry>, 8> policies; std::array<std::optional<Retry::Attempt>, 8> attempts;
  for (auto& policy : policies) policy = std::make_unique<Retry>(ledger, Demand::Speculative, requests(), 3);
  std::promise<void> start; auto gate = start.get_future().share(); std::vector<std::thread> threads;
  std::atomic<unsigned> admitted{0}, deferred{0};
  for (unsigned i = 0; i < 8; ++i) threads.emplace_back([&, i, gate] {
    gate.wait(); attempts[i] = policies[i]->admit(0);
    if (attempts[i]->status == Status::Admitted) ++admitted;
    if (attempts[i]->status == Status::Deferred) ++deferred;
  });
  start.set_value(); for (auto& thread : threads) thread.join();
  EXPECT_EQ(admitted.load(), 1u); EXPECT_EQ(deferred.load(), 7u);
  EXPECT_EQ(ledger.snapshot().reservedBytes, 80u); EXPECT_EQ(ledger.snapshot().admission.pressureChecks, 8u);
  for (unsigned i = 0; i < 8; ++i)
    EXPECT_EQ(policies[i]->admit(0).status, attempts[i]->memory ? Status::Active : Status::AwaitProgress);
  EXPECT_EQ(ledger.snapshot().admission.pressureChecks, 8u);
}

TEST(WorldGenerationMemoryRetryTest, RetriedPureProducerMatchesTheAdequatelyPredictedSinglePassBytes) {
  WorldGenerationMemoryLedger ledger; std::atomic<unsigned> allocations{0};
  Retry retry(ledger, Demand::Visible, requests(), 3), baseline(ledger, Demand::Visible, requests(60, 60), 1);
  auto first = produce(retry.admit(0), requests(), 60, 17, allocations); EXPECT_FALSE(first.output);
  EXPECT_TRUE(retry.retry(first.attempt, first.required));
  auto retried = produce(retry.admit(1), retry.requests(), 60, 17, allocations);
  auto original = produce(baseline.admit(0), baseline.requests(), 60, 17, allocations);
  ASSERT_TRUE(retried.output); ASSERT_TRUE(original.output);
  EXPECT_TRUE(std::equal(retried.output->value.get(), retried.output->value.get() + 60, original.output->value.get()));
  EXPECT_EQ(allocations.load(), 2u); EXPECT_TRUE(retry.complete(retried.attempt)); EXPECT_TRUE(baseline.complete(original.attempt));
}

TEST(WorldGenerationMemoryRetryTest, DrainEventsDoNotBypassSoftHeadroomOrHysteresis) {
  WorldGenerationMemoryLedger ledger(1000, Limits);
  auto held = ledger.reserve(Class::WorkerTemplateEstimate, 100); ASSERT_TRUE(held);
  Retry retry(ledger, Demand::Speculative, requests(), 3);
  EXPECT_EQ(retry.admit(0).status, Status::Deferred);
  EXPECT_TRUE(held->resizeEstimate(90)); EXPECT_EQ(retry.admit(1).status, Status::Deferred);
  EXPECT_TRUE(held->resizeEstimate(20)); auto admitted = retry.admit(2);
  EXPECT_EQ(admitted.status, Status::Admitted);
  EXPECT_EQ(ledger.snapshot().reservedBytes, 100u);
}
