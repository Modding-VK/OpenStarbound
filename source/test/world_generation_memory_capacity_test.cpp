#include "StarWorldGenerationMemoryRetry.hpp"
#include "StarWorldGenerationMemoryEnvelope.hpp"
#include "StarWorkerPool.hpp"

#include <atomic>
#include <thread>
#include <vector>

#include "gtest/gtest.h"

using namespace Star;

namespace {
using Class = WorldGenerationMemoryClass;
using Demand = WorldGenerationMemoryDemand;
using Status = WorldGenerationMemoryRetryStatus;
using BatchStatus = WorldGenerationMemoryBatchStatus;
using Retry = WorldGenerationMemoryRetry<1>;
WorldGenerationMemoryPressureLimits const Limits{true, 80, 100, 200};
Retry::Requests requests(size_t bytes = 80) { return {{{Class::BaseTilesArray, bytes}}}; }
uint64_t revision(WorldGenerationMemoryLedger const& ledger) { return ledger.capacityRevision().sequence; }
}

TEST(WorldGenerationMemoryCapacityTest, NewLedgersAndSnapshotStartAtZero) {
  WorldGenerationMemoryLedger ledger;
  EXPECT_EQ(revision(ledger), 0u); EXPECT_EQ(ledger.snapshot().capacity.sequence, 0u);
  EXPECT_TRUE(ledger.capacityRevision().canSuppress(ledger.snapshot().capacity));
}

TEST(WorldGenerationMemoryCapacityTest, ReservationAndGrowthAreNotReleaseEvents) {
  WorldGenerationMemoryLedger ledger; auto token = ledger.reserve(Class::WorkerTemplateEstimate, 10);
  ASSERT_TRUE(token); EXPECT_TRUE(token->resizeEstimate(20)); EXPECT_EQ(revision(ledger), 0u);
  auto batch = ledger.tryReserveBatch(requests()); ASSERT_TRUE(batch.memory); EXPECT_EQ(revision(ledger), 0u);
}

TEST(WorldGenerationMemoryCapacityTest, OnlyPositiveShrinkChangesTheRevision) {
  WorldGenerationMemoryLedger ledger; auto token = ledger.reserve(Class::WorkerTemplateEstimate, 10);
  ASSERT_TRUE(token); EXPECT_TRUE(token->resizeEstimate(10)); EXPECT_EQ(revision(ledger), 0u);
  EXPECT_TRUE(token->resizeEstimate(5)); EXPECT_EQ(revision(ledger), 1u);
  EXPECT_TRUE(token->resizeEstimate(0)); EXPECT_EQ(revision(ledger), 2u);
  EXPECT_TRUE(token->resizeEstimate(0)); EXPECT_EQ(revision(ledger), 2u);
}

TEST(WorldGenerationMemoryCapacityTest, OnlyTheLastPhysicalReservationOwnerSignalsRelease) {
  WorldGenerationMemoryLedger ledger; auto first = ledger.reserve(Class::BaseTilesArray, 80); auto last = first;
  ASSERT_TRUE(first); first.reset(); EXPECT_EQ(revision(ledger), 0u);
  last.reset(); EXPECT_EQ(revision(ledger), 1u); EXPECT_EQ(ledger.snapshot().reservedBytes, 0u);
}

TEST(WorldGenerationMemoryCapacityTest, ZeroByteResourceDestructionAlsoSignalsCapacity) {
  WorldGenerationMemoryLedger ledger; auto zero = ledger.reserve(Class::WorkerTemplateEstimate, 0);
  ASSERT_TRUE(zero); EXPECT_EQ(revision(ledger), 0u); zero.reset();
  EXPECT_EQ(revision(ledger), 1u); EXPECT_EQ(ledger.snapshot().resources, 0u);
}

TEST(WorldGenerationMemoryCapacityTest, SharedJsonAliasesSignalOnlyOnceAtLastRelease) {
  WorldGenerationMemoryLedger ledger; auto owner = std::make_shared<unsigned>(1);
  auto one = ledger.reserveSharedJson(owner, 80), two = ledger.reserveSharedJson(owner, 80);
  ASSERT_TRUE(one); EXPECT_EQ(one, two); one.reset(); EXPECT_EQ(revision(ledger), 0u);
  two.reset(); EXPECT_EQ(revision(ledger), 1u); EXPECT_EQ(ledger.snapshot().sharedRegistryEntries, 0u);
}

TEST(WorldGenerationMemoryCapacityTest, RefusingInactiveBatchTokensDoesNotInventARelease) {
  WorldGenerationMemoryLedger ledger(1000, Limits); auto held = ledger.reserve(Class::BaseTilesArray, 150);
  auto refused = ledger.tryReserveBatch(requests()); EXPECT_EQ(refused.status, BatchStatus::Deferred);
  EXPECT_FALSE(refused.memory); ASSERT_TRUE(refused.deferredCapacity);
  EXPECT_EQ(refused.deferredCapacity->sequence, 0u); EXPECT_EQ(revision(ledger), 0u);
}

TEST(WorldGenerationMemoryCapacityTest, RefusalKeepsTheRevisionOfItsDecisionNotALaterSnapshot) {
  WorldGenerationMemoryLedger ledger(1000, Limits); auto held = ledger.reserve(Class::BaseTilesArray, 150);
  auto refused = ledger.tryReserveBatch(requests()); ASSERT_TRUE(refused.deferredCapacity);
  std::thread worker([&] { held.reset(); }); worker.join();
  EXPECT_EQ(refused.deferredCapacity->sequence, 0u); EXPECT_EQ(revision(ledger), 1u);
  EXPECT_FALSE(ledger.capacityRevision().canSuppress(*refused.deferredCapacity));
  // Negative control: replacing that marker with a later observation swallows
  // the release and would suppress the next retry despite newly free capacity.
  auto late = ledger.capacityRevision(); EXPECT_TRUE(late.canSuppress(late));
}

TEST(WorldGenerationMemoryCapacityTest, AForeignLedgerCannotWakeTheBoundDescriptor) {
  WorldGenerationMemoryLedger ledger(1000, Limits), foreign;
  auto held = ledger.reserve(Class::BaseTilesArray, 150); Retry retry(ledger, Demand::Visible, requests(), 2);
  EXPECT_EQ(retry.admit().status, Status::Deferred);
  auto unrelated = foreign.reserve(Class::BaseTilesArray, 50); unrelated.reset();
  EXPECT_EQ(retry.admit().status, Status::AwaitProgress); EXPECT_EQ(revision(ledger), 0u);
}

TEST(WorldGenerationMemoryCapacityTest, WorkerReleaseAutomaticallyReadmitsOnTheNextOwnerPoll) {
  WorldGenerationMemoryLedger ledger(1000, Limits); WorkerPool pool("Capacity release contract", 1);
  auto held = ledger.reserve(Class::BaseTilesArray, 150); Retry retry(ledger, Demand::Visible, requests(), 2);
  EXPECT_EQ(retry.admit().status, Status::Deferred);
  auto done = pool.addProducer<bool>([memory = std::move(held)]() mutable { memory.reset(); return true; });
  EXPECT_TRUE(done.get()); pool.finish();
  auto admitted = retry.admit(); EXPECT_EQ(admitted.status, Status::Admitted);
  EXPECT_EQ(ledger.snapshot().reservedBytes, 80u);
}

TEST(WorldGenerationMemoryCapacityTest, CapacitySignalsCannotBypassOldAttemptPhysicalFences) {
  WorldGenerationMemoryLedger ledger; Retry retry(ledger, Demand::Visible, requests(), 2);
  auto attempt = retry.admit(); ASSERT_TRUE(attempt.memory); EXPECT_TRUE(retry.retry(1, requests(90)));
  auto unrelated = ledger.reserve(Class::MicroDungeonHalo, 1); unrelated.reset();
  EXPECT_EQ(retry.admit().status, Status::AwaitRelease); attempt.memory.reset();
  EXPECT_EQ(retry.admit().status, Status::Admitted);
}

TEST(WorldGenerationMemoryCapacityTest, ConcurrentShrinkAndReleasePreserveEveryCapacityEvent) {
  WorldGenerationMemoryLedger ledger; std::array<std::shared_ptr<WorldGenerationMemoryReservation>, 8> tokens;
  for (auto& token : tokens) token = ledger.reserve(Class::WorkerTemplateEstimate, 64);
  std::vector<std::thread> threads; std::atomic<unsigned> failures{0};
  for (unsigned i = 0; i < tokens.size(); ++i) threads.emplace_back([&, i] {
    if (!tokens[i]->resizeEstimate(32)) ++failures;
    tokens[i].reset();
    if (!worldGenerationMemoryEnvelope(ledger.snapshot()).coherent) ++failures;
  });
  for (auto& thread : threads) thread.join();
  EXPECT_EQ(failures.load(), 0u); EXPECT_EQ(revision(ledger), 16u); EXPECT_EQ(ledger.snapshot().resources, 0u);
}

TEST(WorldGenerationMemoryCapacityTest, SaturationNeverWrapsOrSuppressesFurtherReleaseChecks) {
  WorldGenerationMemoryCapacityRevision value{std::numeric_limits<uint64_t>::max() - 1}; auto before = value;
  EXPECT_TRUE(value.canSuppress(before)); value.advance(); EXPECT_FALSE(value.canSuppress(before));
  EXPECT_FALSE(value.canSuppress(value)); value.advance();
  EXPECT_EQ(value.sequence, std::numeric_limits<uint64_t>::max()); EXPECT_FALSE(value.canSuppress(value));
}

TEST(WorldGenerationMemoryCapacityTest, ConstructionRollbackSignalsTheCapacityItReallyReturns) {
  struct Throwing { Throwing() { throw std::runtime_error("controlled construction"); } };
  WorldGenerationMemoryLedger ledger;
  EXPECT_THROW(makeWorldGenerationTrackedResource<Throwing>(ledger, Class::BaseTilesArray, 80), std::runtime_error);
  EXPECT_EQ(revision(ledger), 1u); EXPECT_EQ(ledger.snapshot().resources, 0u);
}

TEST(WorldGenerationMemoryCapacityTest, AReleaseHintDoesNotPromiseThatTheWholeRequestFits) {
  WorldGenerationMemoryLedger ledger(1000, Limits); auto held = ledger.reserve(Class::WorkerTemplateEstimate, 150);
  Retry retry(ledger, Demand::Visible, requests(), 2); EXPECT_EQ(retry.admit().status, Status::Deferred);
  EXPECT_TRUE(held->resizeEstimate(149)); EXPECT_EQ(retry.admit().status, Status::Deferred);
  auto checks = ledger.snapshot().admission.pressureChecks;
  EXPECT_EQ(retry.admit().status, Status::AwaitProgress); EXPECT_EQ(ledger.snapshot().admission.pressureChecks, checks);
  EXPECT_TRUE(held->resizeEstimate(120)); auto next = retry.admit(); EXPECT_EQ(next.status, Status::Admitted);
}

TEST(WorldGenerationMemoryCapacityTest, SpeculationStillNeedsSoftHeadroomAndHysteresisRecovery) {
  WorldGenerationMemoryLedger ledger(1000, Limits); auto held = ledger.reserve(Class::WorkerTemplateEstimate, 100);
  Retry retry(ledger, Demand::Speculative, requests(), 2); EXPECT_EQ(retry.admit().status, Status::Deferred);
  EXPECT_TRUE(held->resizeEstimate(90)); EXPECT_EQ(retry.admit().status, Status::Deferred);
  EXPECT_TRUE(held->resizeEstimate(20)); auto next = retry.admit(); EXPECT_EQ(next.status, Status::Admitted);
  EXPECT_EQ(ledger.snapshot().reservedBytes, 100u);
}

TEST(WorldGenerationMemoryCapacityTest, FacadeClosureDoesNotResetTheRevisionOrLoseItsRelease) {
  auto facade = std::make_unique<WorldGenerationMemoryLedger>(1000, Limits); auto observer = *facade;
  auto held = facade->reserve(Class::BaseTilesArray, 150); Retry retry(*facade, Demand::Visible, requests(), 2);
  EXPECT_EQ(retry.admit().status, Status::Deferred); facade.reset(); held.reset();
  EXPECT_EQ(revision(observer), 1u); EXPECT_EQ(retry.admit().status, Status::Admitted);
}

TEST(WorldGenerationMemoryCapacityTest, RefusalSameSizeAndIncompleteNotesDoNotSignalCapacity) {
  WorldGenerationMemoryLedger ledger(100); auto held = ledger.reserve(Class::WorkerTemplateEstimate, 90);
  ASSERT_TRUE(held); EXPECT_FALSE(held->resizeEstimate(101)); EXPECT_TRUE(held->resizeEstimate(90));
  held->noteIncompleteEstimate(); EXPECT_FALSE(ledger.reserve(Class::BaseTilesArray, 11));
  EXPECT_EQ(ledger.tryReserveBatch(requests(101)).status, BatchStatus::Impossible);
  EXPECT_EQ(revision(ledger), 0u);
}

TEST(WorldGenerationMemoryCapacityTest, OnlyDeferredCarriesAnAtomicCapacityMarker) {
  WorldGenerationMemoryLedger ledger(100);
  auto admitted = ledger.tryReserveBatch(requests()); ASSERT_TRUE(admitted.memory); EXPECT_FALSE(admitted.deferredCapacity);
  auto deferred = ledger.tryReserveBatch(requests()); EXPECT_EQ(deferred.status, BatchStatus::Deferred); EXPECT_TRUE(deferred.deferredCapacity);
  auto impossible = ledger.tryReserveBatch(requests(101)); EXPECT_EQ(impossible.status, BatchStatus::Impossible); EXPECT_FALSE(impossible.deferredCapacity);
  auto invalidRequests = requests(); invalidRequests[0].kind = Class::Count;
  auto invalid = ledger.tryReserveBatch(invalidRequests); EXPECT_EQ(invalid.status, BatchStatus::Invalid); EXPECT_FALSE(invalid.deferredCapacity);
}

TEST(WorldGenerationMemoryCapacityTest, EstimatedReleaseIsDeclaredCapacityNotAnObservationOfRss) {
  WorldGenerationMemoryLedger ledger; auto estimate = ledger.reserve(Class::WorkerTemplateEstimate, 100);
  std::vector<char> physical(100, 'x'); ASSERT_TRUE(estimate); EXPECT_TRUE(estimate->resizeEstimate(50));
  EXPECT_EQ(revision(ledger), 1u); EXPECT_EQ(physical.size(), 100u); EXPECT_EQ(ledger.snapshot().reservedBytes, 50u);
}
