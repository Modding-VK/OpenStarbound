#include "StarWorldGenerationMemoryEnvelope.hpp"
#include "StarWorldGenerationBufferAllocator.hpp"

#include <atomic>
#include <thread>
#include <vector>

#include "gtest/gtest.h"

using namespace Star;

namespace {
using Class = WorldGenerationMemoryClass;
using Quantity = WorldGenerationMemoryQuantity;
using Collection = WorldGenerationMemoryCollection;
using Frontier = WorldGenerationMemoryFrontier;
using Pressure = WorldGenerationMemoryPressure;
using Demand = WorldGenerationMemoryDemand;
using Reason = WorldGenerationMemoryAdviceReason;
using Policy = WorldGenerationMemoryPressurePolicy;
WorldGenerationMemoryPressureLimits const Limits{true, 80, 100, 200};

WorldGenerationMemorySnapshot sample(size_t bytes) {
  WorldGenerationMemorySnapshot result;
  result.reservedBytes = result.reservedArrayBytes = result.classBytes[0] = bytes;
  result.resources = result.classResources[0] = 1;
  return result;
}
WorldGenerationMemoryEnvelope envelope(size_t bytes) { return worldGenerationMemoryEnvelope(sample(bytes)); }
}

TEST(WorldGenerationMemoryEnvelopeTest, ClassSchemaCoversEveryEnumAndExistingBufferClassification) {
  unsigned arrays = 0, buffers = 0, estimates = 0, profiling = 0;
  for (size_t i = 0; i < static_cast<size_t>(Class::Count); ++i) {
    auto info = worldGenerationMemoryClassInfo(static_cast<Class>(i)); ASSERT_NE(info, nullptr);
    EXPECT_EQ(static_cast<size_t>(info->kind), i);
    EXPECT_EQ(info->quantity == Quantity::AllocatorRequest, worldGenerationBufferClass(info->kind));
    if (info->quantity == Quantity::ArrayPayload) { ++arrays; EXPECT_LT(i, 2u); }
    if (info->quantity == Quantity::AllocatorRequest) ++buffers;
    if (info->quantity == Quantity::Estimate) ++estimates;
    if (info->collection == Collection::RegionalProfiling) { ++profiling; EXPECT_GE(i, 2u); EXPECT_LE(i, 4u); }
    if (info->quantity != Quantity::Estimate) {
      EXPECT_EQ(info->frontier, Frontier::BeforeAllocation);
    }
  }
  EXPECT_EQ(arrays, 2u); EXPECT_EQ(buffers, 4u); EXPECT_EQ(estimates, 8u); EXPECT_EQ(profiling, 3u);
  EXPECT_EQ(worldGenerationMemoryClassInfo(Class::WorkerTemplateEstimate)->frontier, Frontier::JobBoundary);
}

TEST(WorldGenerationMemoryEnvelopeTest, InvalidClassIdsHaveNoInventedCoverage) {
  EXPECT_EQ(worldGenerationMemoryClassInfo(Class::Count), nullptr);
  EXPECT_EQ(worldGenerationMemoryClassInfo(static_cast<Class>(std::numeric_limits<size_t>::max())), nullptr);
}

TEST(WorldGenerationMemoryEnvelopeTest, EmptyEnvelopeIsCoherentButNeverACompleteHeapMeasurement) {
  auto value = worldGenerationMemoryEnvelope({});
  EXPECT_TRUE(value.coherent); EXPECT_EQ(value.trackedBytes, 0u); EXPECT_EQ(value.resources, 0u);
  EXPECT_FALSE(WorldGenerationMemoryEnvelope::CompleteHeap);
  EXPECT_EQ(WorldGenerationMemoryEnvelope::ModelVersion, 1u);
  static_assert(noexcept(worldGenerationMemoryEnvelope({})), "Envelope calculation must not throw");
}

TEST(WorldGenerationMemoryEnvelopeTest, NativeLedgerSeparatesAllFourteenClassesWithoutDoubleCounting) {
  WorldGenerationMemoryLedger ledger; auto owner = std::make_shared<unsigned>(1);
  std::vector<std::shared_ptr<WorldGenerationMemoryReservation>> tokens;
  for (size_t i = 0; i < static_cast<size_t>(Class::Count); ++i) {
    auto kind = static_cast<Class>(i);
    auto token = kind == Class::TemplateJsonEstimate ? ledger.reserveSharedJson(owner, 17) : ledger.reserve(kind, 17);
    ASSERT_TRUE(token); tokens.push_back(std::move(token));
  }
  auto value = worldGenerationMemoryEnvelope(ledger.snapshot());
  EXPECT_TRUE(value.coherent); EXPECT_EQ(value.trackedBytes, 238u); EXPECT_EQ(value.resources, 14u);
  EXPECT_EQ(value.arrayPayloadBytes, 34u); EXPECT_EQ(value.allocatorRequestBytes, 68u);
  EXPECT_EQ(value.estimatedBytes, 136u); EXPECT_EQ(value.alwaysEstimatedBytes, 85u);
  EXPECT_EQ(value.profilingEstimatedBytes, 51u);
}

TEST(WorldGenerationMemoryEnvelopeTest, NativeAllocatorRequestIncludesItsHeaderNotOnlyVectorElements) {
  WorldGenerationMemoryLedger ledger;
  using Allocator = WorldGenerationBufferAllocator<unsigned>;
  std::vector<unsigned, Allocator> values(Allocator(ledger, Class::MicroDungeonCandidateBuffer));
  values.reserve(16);
  auto value = worldGenerationMemoryEnvelope(ledger.snapshot());
  EXPECT_TRUE(value.coherent);
  EXPECT_EQ(value.allocatorRequestBytes, Allocator::requestBytes(values.capacity()));
  EXPECT_GT(value.allocatorRequestBytes, values.capacity() * sizeof(unsigned));
  EXPECT_EQ(value.estimatedBytes, 0u);
}

TEST(WorldGenerationMemoryEnvelopeTest, SharedFacadesAndHandlesDoNotMultiplyTheEnvelope) {
  WorldGenerationMemoryLedger ledger; auto view = ledger;
  auto token = ledger.reserve(Class::BaseTilesArray, 64); auto alias = token;
  EXPECT_EQ(worldGenerationMemoryEnvelope(view.snapshot()).trackedBytes, 64u);
  token.reset(); EXPECT_EQ(worldGenerationMemoryEnvelope(ledger.snapshot()).trackedBytes, 64u);
  alias.reset(); EXPECT_EQ(worldGenerationMemoryEnvelope(view.snapshot()).trackedBytes, 0u);
}

TEST(WorldGenerationMemoryEnvelopeTest, ReleasedResourcesAndHistoricalPeaksDoNotRemainCurrentPressure) {
  WorldGenerationMemoryLedger ledger;
  { auto token = ledger.reserve(Class::BaseTilesArray, 64); }
  auto usage = ledger.snapshot(); EXPECT_EQ(usage.peakReservedBytes, 64u);
  usage.peakEstimatedBytes = std::numeric_limits<size_t>::max();
  auto value = worldGenerationMemoryEnvelope(usage);
  EXPECT_TRUE(value.coherent); EXPECT_EQ(value.trackedBytes, 0u);
}

TEST(WorldGenerationMemoryEnvelopeTest, WrongTrackedTotalIsRejected) {
  auto usage = sample(64); ++usage.reservedBytes;
  EXPECT_FALSE(worldGenerationMemoryEnvelope(usage).coherent);
}

TEST(WorldGenerationMemoryEnvelopeTest, WrongArrayTotalIsRejected) {
  auto usage = sample(64); ++usage.reservedArrayBytes;
  EXPECT_FALSE(worldGenerationMemoryEnvelope(usage).coherent);
}

TEST(WorldGenerationMemoryEnvelopeTest, WrongBufferTotalIsRejected) {
  auto usage = sample(64); ++usage.reservedBufferBytes;
  EXPECT_FALSE(worldGenerationMemoryEnvelope(usage).coherent);
}

TEST(WorldGenerationMemoryEnvelopeTest, WrongEstimateTotalIsRejected) {
  auto usage = sample(64); ++usage.estimatedBytes;
  EXPECT_FALSE(worldGenerationMemoryEnvelope(usage).coherent);
}

TEST(WorldGenerationMemoryEnvelopeTest, WrongResourceTotalIsRejected) {
  auto usage = sample(64); ++usage.resources;
  EXPECT_FALSE(worldGenerationMemoryEnvelope(usage).coherent);
}

TEST(WorldGenerationMemoryEnvelopeTest, BytesWithoutAnOwnerResourceAreRejected) {
  auto usage = sample(64); usage.resources = usage.classResources[0] = 0;
  EXPECT_FALSE(worldGenerationMemoryEnvelope(usage).coherent);
}

TEST(WorldGenerationMemoryEnvelopeTest, SharedOwnerRegistryMustMatchSharedClassResources) {
  auto usage = sample(64); usage.sharedRegistryEntries = 1;
  EXPECT_FALSE(worldGenerationMemoryEnvelope(usage).coherent);
  usage.sharedResources = 1; EXPECT_FALSE(worldGenerationMemoryEnvelope(usage).coherent);
}

TEST(WorldGenerationMemoryEnvelopeTest, ZeroByteNativeReservationStillHasOneCoherentResource) {
  WorldGenerationMemoryLedger ledger; auto token = ledger.reserve(Class::BaseTilesArray, 0);
  auto value = worldGenerationMemoryEnvelope(ledger.snapshot());
  EXPECT_TRUE(value.coherent); EXPECT_EQ(value.resources, 1u); EXPECT_EQ(value.trackedBytes, 0u);
}

TEST(WorldGenerationMemoryEnvelopeTest, ClassByteOverflowDoesNotWrapIntoAUsableEnvelope) {
  auto usage = sample(std::numeric_limits<size_t>::max());
  usage.classBytes[1] = 1; usage.classResources[1] = 1; usage.resources = 2;
  auto value = worldGenerationMemoryEnvelope(usage);
  EXPECT_FALSE(value.coherent); EXPECT_EQ(value.trackedBytes, std::numeric_limits<size_t>::max());
}

TEST(WorldGenerationMemoryEnvelopeTest, ClassResourceOverflowDoesNotWrapIntoAUsableEnvelope) {
  auto usage = sample(0); usage.classResources[0] = usage.resources = std::numeric_limits<size_t>::max();
  usage.classResources[1] = 1;
  EXPECT_FALSE(worldGenerationMemoryEnvelope(usage).coherent);
}

TEST(WorldGenerationMemoryEnvelopeTest, HistoricalIncompleteUpdatesDoNotInventLiveOwnerCompleteness) {
  auto usage = sample(64); usage.incompleteEstimateUpdates = std::numeric_limits<size_t>::max();
  auto value = worldGenerationMemoryEnvelope(usage);
  EXPECT_TRUE(value.coherent); EXPECT_EQ(value.historicalIncompleteUpdates, usage.incompleteEstimateUpdates);
  EXPECT_FALSE(WorldGenerationMemoryEnvelope::CompleteHeap);
}

TEST(WorldGenerationMemoryEnvelopeTest, AllocatorRetentionSurvivesFacadeCloseAndVectorClearUntilDestruction) {
  auto facade = std::make_unique<WorldGenerationMemoryLedger>(); auto observer = *facade;
  {
    using Allocator = WorldGenerationBufferAllocator<unsigned>;
    std::vector<unsigned, Allocator> values(Allocator(*facade, Class::MicroDungeonPlanBuffer));
    values.resize(16); facade.reset(); auto before = worldGenerationMemoryEnvelope(observer.snapshot());
    values.clear(); auto after = worldGenerationMemoryEnvelope(observer.snapshot());
    EXPECT_TRUE(before.coherent); EXPECT_TRUE(after.coherent);
    EXPECT_EQ(after.allocatorRequestBytes, before.allocatorRequestBytes); EXPECT_GT(after.trackedBytes, 0u);
  }
  EXPECT_EQ(worldGenerationMemoryEnvelope(observer.snapshot()).trackedBytes, 0u);
}

TEST(WorldGenerationMemoryEnvelopeTest, ConcurrentLedgerUpdatesKeepSnapshotEnvelopesCoherent) {
  WorldGenerationMemoryLedger ledger; auto owner = std::make_shared<unsigned>(1);
  std::atomic<unsigned> failures{0}; std::vector<std::thread> threads;
  for (unsigned i = 0; i < 8; ++i) threads.emplace_back([&, i] {
    for (unsigned j = 0; j < 200; ++j) {
      auto array = ledger.reserve(i % 2 ? Class::BaseTilesArray : Class::MicroDungeonHalo, 64);
      auto json = ledger.reserveSharedJson(owner, 32);
      if (!worldGenerationMemoryEnvelope(ledger.snapshot()).coherent) ++failures;
    }
  });
  for (auto& thread : threads) thread.join();
  EXPECT_EQ(failures.load(), 0u); EXPECT_EQ(ledger.snapshot().reservedBytes, 0u);
}

TEST(WorldGenerationMemoryEnvelopeTest, PolicyIsDisabledByDefaultAndDoesNotChooseHardwareLimits) {
  Policy policy; auto usage = envelope(1000); usage.coherent = false;
  auto advice = policy.evaluate(usage, Demand::Visible, std::numeric_limits<size_t>::max());
  EXPECT_EQ(advice.pressure, Pressure::Unrestricted); EXPECT_EQ(advice.reason, Reason::Disabled);
  EXPECT_TRUE(advice.allowRequest); EXPECT_TRUE(advice.drainExisting);
}

TEST(WorldGenerationMemoryEnvelopeTest, InvalidLimitOrdersAreRejectedWithoutStoppingDrain) {
  for (auto limits : {WorldGenerationMemoryPressureLimits{true, 0, 0, 0}, {true, 100, 100, 200},
      {true, 80, 100, 100}, {true, 200, 100, 300}, {true, 80, 200, 100}}) {
    Policy policy(limits); auto advice = policy.evaluate(envelope(0), Demand::Visible, 0);
    EXPECT_EQ(advice.reason, Reason::InvalidLimits); EXPECT_EQ(advice.pressure, Pressure::Invalid);
    EXPECT_FALSE(advice.allowRequest); EXPECT_TRUE(advice.drainExisting);
  }
}

TEST(WorldGenerationMemoryEnvelopeTest, IncoherentEnvelopeCannotRecommendAdmissionEvenForZeroBytes) {
  Policy policy(Limits); auto usage = envelope(0); usage.coherent = false;
  for (auto bytes : {size_t(0), size_t(1)}) {
    auto advice = policy.evaluate(usage, Demand::Visible, bytes);
    EXPECT_EQ(advice.reason, Reason::InvalidEnvelope); EXPECT_FALSE(advice.allowRequest);
    EXPECT_TRUE(advice.drainExisting);
  }
}

TEST(WorldGenerationMemoryEnvelopeTest, InvalidDemandCannotSilentlyBecomeUrgentWork) {
  Policy policy(Limits); auto advice = policy.evaluate(envelope(0), static_cast<Demand>(255), 1);
  EXPECT_EQ(advice.reason, Reason::InvalidDemand); EXPECT_FALSE(advice.allowRequest);
}

TEST(WorldGenerationMemoryEnvelopeTest, VisibleAndDependencyWorkCanUseTheDeclaredHardHeadroom) {
  Policy policy(Limits);
  for (auto demand : {Demand::Visible, Demand::Dependency}) {
    auto advice = policy.evaluate(envelope(10), demand, 190);
    EXPECT_TRUE(advice.allowRequest); EXPECT_EQ(advice.pressure, Pressure::Normal);
    EXPECT_EQ(advice.reason, Reason::Fits);
  }
}

TEST(WorldGenerationMemoryEnvelopeTest, SpeculationCannotCrossSoftHeadroomOrLatchUnallocatedBytes) {
  Policy policy(Limits);
  EXPECT_TRUE(policy.evaluate(envelope(0), Demand::Speculative, 100).allowRequest);
  auto denied = policy.evaluate(envelope(0), Demand::Speculative, 101);
  EXPECT_FALSE(denied.allowRequest); EXPECT_EQ(denied.reason, Reason::ExceedsSoftLimit);
  EXPECT_EQ(denied.pressure, Pressure::Normal);
  EXPECT_TRUE(policy.evaluate(envelope(90), Demand::Speculative, 1).allowRequest);
}

TEST(WorldGenerationMemoryEnvelopeTest, SoftPressurePausesSpeculationButReservesUrgentHeadroom) {
  Policy policy(Limits);
  auto speculative = policy.evaluate(envelope(150), Demand::Speculative, 1);
  EXPECT_EQ(speculative.pressure, Pressure::Soft); EXPECT_EQ(speculative.reason, Reason::SpeculationPaused);
  for (auto demand : {Demand::Visible, Demand::Dependency}) {
    EXPECT_TRUE(policy.evaluate(envelope(150), demand, 50).allowRequest);
    EXPECT_EQ(policy.evaluate(envelope(150), demand, 51).reason, Reason::ExceedsHardLimit);
  }
}

TEST(WorldGenerationMemoryEnvelopeTest, HardPressureStopsGrowthButNotZeroGrowthReuseOrDrain) {
  Policy policy(Limits);
  for (auto demand : {Demand::Visible, Demand::Dependency, Demand::Speculative}) {
    auto growth = policy.evaluate(envelope(200), demand, 1);
    EXPECT_EQ(growth.reason, Reason::AtHardLimit); EXPECT_FALSE(growth.allowRequest);
    EXPECT_TRUE(growth.drainExisting);
    auto reuse = policy.evaluate(envelope(200), demand, 0);
    EXPECT_EQ(reuse.pressure, Pressure::Hard); EXPECT_EQ(reuse.reason, Reason::Reuse);
    EXPECT_TRUE(reuse.allowRequest); EXPECT_TRUE(reuse.drainExisting);
  }
}

TEST(WorldGenerationMemoryEnvelopeTest, HardPressureDowngradesBeforeHysteresisFullyRecovers) {
  Policy policy(Limits);
  EXPECT_EQ(policy.evaluate(envelope(210), Demand::Visible, 0).pressure, Pressure::Hard);
  EXPECT_EQ(policy.evaluate(envelope(150), Demand::Visible, 0).pressure, Pressure::Soft);
  EXPECT_EQ(policy.evaluate(envelope(90), Demand::Visible, 0).pressure, Pressure::Soft);
  EXPECT_EQ(policy.evaluate(envelope(80), Demand::Visible, 0).pressure, Pressure::Normal);
}

TEST(WorldGenerationMemoryEnvelopeTest, SoftHysteresisDoesNotOscillateAtTheEntryThreshold) {
  Policy policy(Limits);
  for (auto bytes : {size_t(100), size_t(99), size_t(81)})
    EXPECT_EQ(policy.evaluate(envelope(bytes), Demand::Visible, 0).pressure, Pressure::Soft);
  EXPECT_EQ(policy.evaluate(envelope(80), Demand::Visible, 0).pressure, Pressure::Normal);
  EXPECT_EQ(policy.evaluate(envelope(99), Demand::Visible, 0).pressure, Pressure::Normal);
}

TEST(WorldGenerationMemoryEnvelopeTest, AdviceDoesNotReserveBytesOrMutateTheNativeLedger) {
  WorldGenerationMemoryLedger ledger; auto token = ledger.reserve(Class::BaseTilesArray, 64);
  auto before = ledger.snapshot(); Policy policy(Limits);
  auto advice = policy.evaluate(worldGenerationMemoryEnvelope(before), Demand::Visible, 100);
  EXPECT_TRUE(advice.allowRequest); EXPECT_EQ(ledger.snapshot().reservedBytes, before.reservedBytes);
  EXPECT_EQ(ledger.snapshot().resources, before.resources);
  static_assert(noexcept(policy.evaluate(WorldGenerationMemoryEnvelope{}, Demand::Visible, 1)),
      "Pressure advice must not throw");
}

TEST(WorldGenerationMemoryEnvelopeTest, NearSizeMaxHeadroomChecksNeverOverflowOrUnderflow) {
  auto max = std::numeric_limits<size_t>::max(); Policy policy({true, max - 3, max - 2, max});
  EXPECT_TRUE(policy.evaluate(envelope(max - 1), Demand::Visible, 1).allowRequest);
  EXPECT_EQ(policy.evaluate(envelope(max - 1), Demand::Visible, 2).reason, Reason::ExceedsHardLimit);
  EXPECT_FALSE(policy.evaluate(envelope(max - 1), Demand::Visible, max).allowRequest);
  EXPECT_EQ(policy.evaluate(envelope(max), Demand::Visible, 0).pressure, Pressure::Hard);
}

TEST(WorldGenerationMemoryEnvelopeTest, InvalidObservationDoesNotPermanentlyLatchPressure) {
  Policy policy(Limits); auto bad = envelope(0); bad.coherent = false;
  EXPECT_EQ(policy.evaluate(bad, Demand::Visible, 1).pressure, Pressure::Invalid);
  EXPECT_EQ(policy.evaluate(envelope(0), Demand::Speculative, 1).pressure, Pressure::Normal);
}

TEST(WorldGenerationMemoryEnvelopeTest, AdviceGridPreservesSoftHardAndDrainInvariants) {
  Policy policy(Limits);
  for (size_t current = 0; current <= 240; ++current)
    for (auto demand : {Demand::Visible, Demand::Dependency, Demand::Speculative})
      for (size_t extra = 0; extra <= 260; extra += 10) {
        auto advice = policy.evaluate(envelope(current), demand, extra);
        ASSERT_TRUE(advice.drainExisting);
        if (advice.allowRequest && extra) {
          ASSERT_LT(current, Limits.hardBytes); EXPECT_LE(extra, Limits.hardBytes - current);
          if (demand == Demand::Speculative) {
            EXPECT_EQ(advice.pressure, Pressure::Normal);
            ASSERT_LT(current, Limits.softBytes); EXPECT_LE(extra, Limits.softBytes - current);
          }
        }
      }
}
