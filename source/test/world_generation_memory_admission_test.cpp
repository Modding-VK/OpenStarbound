#include "StarWorldGenerationMemory.hpp"
#include "StarWorldGenerationMemoryEnvelope.hpp"

#include <array>
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
using Reason = WorldGenerationMemoryAdviceReason;
using Token = std::shared_ptr<WorldGenerationMemoryReservation>;
WorldGenerationMemoryPressureLimits const Limits{true, 80, 100, 200};
struct ThrowingPayload { ThrowingPayload() { throw std::runtime_error("controlled payload construction"); } };
template <typename F> void simultaneous(F work) {
  std::promise<void> start; auto gate = start.get_future().share();
  std::vector<std::thread> threads;
  for (unsigned i = 0; i < 8; ++i) threads.emplace_back([&, i, gate] { gate.wait(); work(i); });
  start.set_value();
  for (auto& thread : threads) thread.join();
}
}

TEST(WorldGenerationMemoryAdmissionTest, LegacyFiniteAndRuntimeLedgersKeepPressureDisabled) {
  WorldGenerationMemoryLedger ledger(64); auto spec = ledger.withDemand(Demand::Speculative);
  auto token = spec.reserve(Class::BaseTilesArray, 64); ASSERT_TRUE(token);
  EXPECT_FALSE(spec.reserve(Class::MicroDungeonHalo, 1));
  EXPECT_FALSE(ledger.snapshot().admission.enabled); EXPECT_EQ(ledger.snapshot().admission.pressureChecks, 0u);
  EXPECT_FALSE(worldGenerationMemoryLedger().snapshot().admission.enabled);
}

TEST(WorldGenerationMemoryAdmissionTest, InvalidEnabledThresholdsCannotPublishALedger) {
  EXPECT_THROW((WorldGenerationMemoryLedger(200, {true, 80, 100, 201})), std::invalid_argument);
  EXPECT_THROW((WorldGenerationMemoryLedger(200, {true, 100, 100, 200})), std::invalid_argument);
  EXPECT_THROW((WorldGenerationMemoryLedger(200, {true, 80, 200, 100})), std::invalid_argument);
  WorldGenerationMemoryLedger disabled(200, {false, 100, 0, 0});
  EXPECT_FALSE(disabled.snapshot().admission.enabled);
}

TEST(WorldGenerationMemoryAdmissionTest, InvalidDemandCannotCreateABypassFacade) {
  WorldGenerationMemoryLedger ledger(1000, Limits);
  EXPECT_THROW(ledger.withDemand(static_cast<Demand>(255)), std::invalid_argument);
  EXPECT_EQ(ledger.snapshot().reservedBytes, 0u);
}

TEST(WorldGenerationMemoryAdmissionTest, FacadesShareBytesAndOnePressureHistory) {
  WorldGenerationMemoryLedger ledger(1000, Limits);
  auto spec = ledger.withDemand(Demand::Speculative), visible = ledger.withDemand(Demand::Visible);
  auto one = spec.reserve(Class::BaseTilesArray, 60), two = visible.reserve(Class::MicroDungeonHalo, 20);
  ASSERT_TRUE(one); ASSERT_TRUE(two);
  EXPECT_EQ(ledger.snapshot().reservedBytes, 80u); EXPECT_EQ(spec.snapshot().reservedBytes, 80u);
  EXPECT_EQ(visible.snapshot().admission.pressureChecks, 2u);
}

TEST(WorldGenerationMemoryAdmissionTest, SpeculationLeavesTheHardHeadroomForDependencies) {
  WorldGenerationMemoryLedger ledger(1000, Limits);
  auto spec = ledger.withDemand(Demand::Speculative), dependency = ledger.withDemand(Demand::Dependency);
  auto speculative = spec.reserve(Class::BaseTilesArray, 100); ASSERT_TRUE(speculative);
  EXPECT_FALSE(spec.reserve(Class::MicroDungeonHalo, 1));
  EXPECT_EQ(ledger.snapshot().admission.lastAdvice.reason, Reason::SpeculationPaused);
  auto urgent = dependency.reserve(Class::MicroDungeonHalo, 100); ASSERT_TRUE(urgent);
  EXPECT_EQ(ledger.snapshot().reservedBytes, 200u); EXPECT_FALSE(ledger.reserve(Class::BaseTilesArray, 1));
  EXPECT_EQ(ledger.snapshot().admission.lastAdvice.reason, Reason::AtHardLimit);
}

TEST(WorldGenerationMemoryAdmissionTest, DifferentClassesCannotMultiplyTheScopedBudget) {
  WorldGenerationMemoryLedger ledger(1000, Limits); auto spec = ledger.withDemand(Demand::Speculative);
  auto array = spec.reserve(Class::BaseTilesArray, 60), estimate = spec.reserve(Class::MicroDungeonNameEstimate, 40);
  ASSERT_TRUE(array); ASSERT_TRUE(estimate); EXPECT_FALSE(spec.reserve(Class::MicroDungeonPlanBuffer, 1));
  auto dependency = ledger.withDemand(Demand::Dependency).reserve(Class::MicroDungeonPlanBuffer, 80);
  ASSERT_TRUE(dependency); EXPECT_EQ(ledger.snapshot().reservedBytes, 180u);
  EXPECT_TRUE(worldGenerationMemoryEnvelope(ledger.snapshot()).coherent);
}

TEST(WorldGenerationMemoryAdmissionTest, ReleaseAndShrinkRecoverOnlyAtTheResumeWatermark) {
  WorldGenerationMemoryLedger ledger(1000, Limits); auto spec = ledger.withDemand(Demand::Speculative);
  auto token = spec.reserve(Class::MicroDungeonNameEstimate, 100); ASSERT_TRUE(token);
  EXPECT_FALSE(spec.reserve(Class::BaseTilesArray, 1));
  EXPECT_TRUE(token->resizeEstimate(90)); EXPECT_FALSE(spec.reserve(Class::BaseTilesArray, 1));
  EXPECT_TRUE(token->resizeEstimate(80));
  auto resumed = spec.reserve(Class::BaseTilesArray, 1); EXPECT_TRUE(resumed);
}

TEST(WorldGenerationMemoryAdmissionTest, EstimateGrowthChecksOnlyItsDeltaAndRefusalKeepsTheOldCharge) {
  WorldGenerationMemoryLedger ledger(1000, Limits); auto spec = ledger.withDemand(Demand::Speculative);
  auto token = spec.reserve(Class::MicroDungeonNameEstimate, 20), other = spec.reserve(Class::BaseTilesArray, 60);
  ASSERT_TRUE(token); ASSERT_TRUE(other); EXPECT_TRUE(token->resizeEstimate(40));
  EXPECT_EQ(ledger.snapshot().reservedBytes, 100u);
  EXPECT_FALSE(token->resizeEstimate(41)); EXPECT_EQ(ledger.snapshot().reservedBytes, 100u);
  EXPECT_EQ(ledger.snapshot().classBytes[static_cast<size_t>(Class::MicroDungeonNameEstimate)], 40u);
}

TEST(WorldGenerationMemoryAdmissionTest, ShrinkingAndUnchangedEstimatesNeverNeedNewAdmission) {
  WorldGenerationMemoryLedger ledger(1000, Limits); auto token = ledger.reserve(Class::MicroDungeonNameEstimate, 200);
  ASSERT_TRUE(token); auto checks = ledger.snapshot().admission.pressureChecks;
  EXPECT_TRUE(token->resizeEstimate(200)); EXPECT_TRUE(token->resizeEstimate(100)); EXPECT_TRUE(token->resizeEstimate(0));
  EXPECT_EQ(ledger.snapshot().admission.pressureChecks, checks); EXPECT_EQ(ledger.snapshot().reservedBytes, 0u);
}

TEST(WorldGenerationMemoryAdmissionTest, ConcurrentResizeUsesTheCurrentTokenChargeUnderTheSameLock) {
  WorldGenerationMemoryLedger ledger(1000, Limits); auto token = ledger.reserve(Class::MicroDungeonNameEstimate, 20);
  ASSERT_TRUE(token); std::atomic<unsigned> accepted{0};
  simultaneous([&](unsigned) { if (token->resizeEstimate(100)) ++accepted; });
  EXPECT_EQ(accepted.load(), 8u); EXPECT_EQ(ledger.snapshot().reservedBytes, 100u);
  EXPECT_EQ(ledger.snapshot().admission.pressureChecks, 2u);
}

TEST(WorldGenerationMemoryAdmissionTest, SizeMaxReservationsCannotWrapScopedHeadroom) {
  auto max = std::numeric_limits<size_t>::max();
  WorldGenerationMemoryLedger ledger(max, {true, max - 3, max - 2, max});
  auto one = ledger.withDemand(Demand::Speculative).reserve(Class::BaseTilesArray, max - 2);
  ASSERT_TRUE(one); auto two = ledger.withDemand(Demand::Dependency).reserve(Class::MicroDungeonHalo, 2);
  ASSERT_TRUE(two); EXPECT_EQ(ledger.snapshot().reservedBytes, max);
  EXPECT_FALSE(ledger.reserve(Class::BaseTilesArray, 1));
}

TEST(WorldGenerationMemoryAdmissionTest, ALooserLegacyMaximumDoesNotBypassTheScopedHardLimit) {
  WorldGenerationMemoryLedger ledger(1000, Limits); auto token = ledger.reserve(Class::BaseTilesArray, 200);
  ASSERT_TRUE(token); EXPECT_FALSE(ledger.reserve(Class::MicroDungeonHalo, 1));
  EXPECT_EQ(ledger.snapshot().peakReservedBytes, 200u);
}

TEST(WorldGenerationMemoryAdmissionTest, EstimateTokensKeepTheirCreationDemandOnLaterWorkerGrowth) {
  WorldGenerationMemoryLedger ledger(1000, Limits); auto spec = ledger.withDemand(Demand::Speculative);
  auto token = spec.reserve(Class::MicroDungeonNameEstimate, 90), visible = ledger.reserve(Class::BaseTilesArray, 20);
  ASSERT_TRUE(token); ASSERT_TRUE(visible); EXPECT_FALSE(token->resizeEstimate(91));
  auto urgent = ledger.reserve(Class::MicroDungeonCandidateEstimate, 0); ASSERT_TRUE(urgent);
  EXPECT_TRUE(urgent->resizeEstimate(10)); EXPECT_EQ(ledger.snapshot().reservedBytes, 120u);
}

TEST(WorldGenerationMemoryAdmissionTest, SharedJsonReuseAtHardLimitIsNotNewGrowth) {
  WorldGenerationMemoryLedger ledger(1000, Limits); auto owner = std::make_shared<unsigned>(1);
  auto json = ledger.reserveSharedJson(owner, 80), other = ledger.reserve(Class::BaseTilesArray, 120);
  ASSERT_TRUE(json); ASSERT_TRUE(other); auto checks = ledger.snapshot().admission.pressureChecks;
  auto alias = ledger.withDemand(Demand::Speculative).reserveSharedJson(owner, 80);
  EXPECT_EQ(alias, json); EXPECT_EQ(ledger.snapshot().admission.pressureChecks, checks);
  EXPECT_FALSE(ledger.reserveSharedJson(std::make_shared<unsigned>(2), 1));
  EXPECT_EQ(ledger.snapshot().reservedBytes, 200u);
}

TEST(WorldGenerationMemoryAdmissionTest, ConflictingSharedModelsAreRefusedWithoutASecondPressureCharge) {
  WorldGenerationMemoryLedger ledger(1000, Limits); auto owner = std::make_shared<unsigned>(1);
  auto json = ledger.reserveSharedJson(owner, 64); ASSERT_TRUE(json);
  auto checks = ledger.snapshot().admission.pressureChecks;
  EXPECT_FALSE(ledger.reserveSharedJson(owner, 32));
  EXPECT_EQ(ledger.snapshot().reservedBytes, 64u); EXPECT_EQ(ledger.snapshot().admission.pressureChecks, checks);
}

TEST(WorldGenerationMemoryAdmissionTest, DifferentJsonOwnersMustIndependentlyPassAdmission) {
  WorldGenerationMemoryLedger ledger(1000, Limits); auto spec = ledger.withDemand(Demand::Speculative);
  auto one = spec.reserveSharedJson(std::make_shared<unsigned>(1), 60); ASSERT_TRUE(one);
  EXPECT_FALSE(spec.reserveSharedJson(std::make_shared<unsigned>(2), 41));
  EXPECT_EQ(ledger.snapshot().admission.lastAdvice.reason, Reason::ExceedsSoftLimit);
  EXPECT_EQ(ledger.snapshot().sharedResources, 1u);
}

TEST(WorldGenerationMemoryAdmissionTest, InvalidClassesNeverEnterPressureAdmission) {
  WorldGenerationMemoryLedger ledger(1000, Limits);
  EXPECT_FALSE(ledger.reserve(Class::Count, 1)); EXPECT_FALSE(ledger.reserve(Class::TemplateJsonEstimate, 1));
  EXPECT_EQ(ledger.snapshot().admission.pressureChecks, 0u);
}

TEST(WorldGenerationMemoryAdmissionTest, ZeroByteResourcesCanExistAtHardLimitButCannotGrowThere) {
  WorldGenerationMemoryLedger ledger(1000, Limits); auto full = ledger.reserve(Class::BaseTilesArray, 200);
  ASSERT_TRUE(full); auto token = ledger.withDemand(Demand::Speculative).reserve(Class::MicroDungeonNameEstimate, 0);
  ASSERT_TRUE(token); EXPECT_FALSE(token->resizeEstimate(1)); EXPECT_EQ(ledger.snapshot().resources, 2u);
  full.reset(); EXPECT_TRUE(token->resizeEstimate(1));
}

TEST(WorldGenerationMemoryAdmissionTest, ClosingTheOriginalFacadeDoesNotResetBudgetOrPolicy) {
  auto facade = std::make_unique<WorldGenerationMemoryLedger>(1000, Limits);
  auto dependency = facade->withDemand(Demand::Dependency), spec = facade->withDemand(Demand::Speculative);
  auto old = spec.reserve(Class::BaseTilesArray, 100); ASSERT_TRUE(old); facade.reset();
  auto current = dependency.reserve(Class::MicroDungeonHalo, 100); ASSERT_TRUE(current);
  EXPECT_EQ(spec.snapshot().reservedBytes, 200u); EXPECT_FALSE(spec.reserve(Class::BaseTilesArray, 1));
  old.reset(); EXPECT_EQ(dependency.snapshot().reservedBytes, 100u);
}

TEST(WorldGenerationMemoryAdmissionTest, ConstructionFailureRollsBackAnAlreadyAdmittedResource) {
  WorldGenerationMemoryLedger ledger(1000, Limits); auto spec = ledger.withDemand(Demand::Speculative);
  EXPECT_THROW(makeWorldGenerationTrackedResource<ThrowingPayload>(spec, Class::BaseTilesArray, 90), std::runtime_error);
  EXPECT_EQ(ledger.snapshot().reservedBytes, 0u); EXPECT_EQ(ledger.snapshot().resources, 0u);
  EXPECT_TRUE(spec.reserve(Class::BaseTilesArray, 100));
}

TEST(WorldGenerationMemoryAdmissionTest, ConcurrentSpeculativeRequestsCannotOversubscribeSoftHeadroom) {
  WorldGenerationMemoryLedger ledger(1000, Limits); auto spec = ledger.withDemand(Demand::Speculative);
  std::array<Token, 8> held;
  simultaneous([&](unsigned i) { held[i] = spec.reserve(Class::BaseTilesArray, 20); });
  unsigned accepted = 0; for (auto const& token : held) accepted += bool(token);
  EXPECT_EQ(accepted, 5u); EXPECT_EQ(ledger.snapshot().reservedBytes, 100u);
  EXPECT_EQ(ledger.snapshot().admission.pressureChecks, 8u); EXPECT_EQ(ledger.snapshot().admission.pressureRefusals, 3u);
  auto dependency = ledger.withDemand(Demand::Dependency).reserve(Class::MicroDungeonHalo, 100);
  ASSERT_TRUE(dependency); EXPECT_EQ(ledger.snapshot().reservedBytes, 200u);
}

TEST(WorldGenerationMemoryAdmissionTest, ConcurrentVisibleRequestsCannotOversubscribeHardHeadroom) {
  WorldGenerationMemoryLedger ledger(1000, Limits); std::array<Token, 8> held;
  simultaneous([&](unsigned i) { held[i] = ledger.reserve(Class::BaseTilesArray, 50); });
  unsigned accepted = 0; for (auto const& token : held) accepted += bool(token);
  EXPECT_EQ(accepted, 4u); EXPECT_EQ(ledger.snapshot().reservedBytes, 200u);
  EXPECT_EQ(ledger.snapshot().admission.pressureRefusals, 4u);
}

TEST(WorldGenerationMemoryAdmissionTest, MixedConcurrentDemandsPreserveBothBudgetBounds) {
  WorldGenerationMemoryLedger ledger(1000, Limits); std::array<Token, 8> held;
  simultaneous([&](unsigned i) {
    auto view = ledger.withDemand(i % 2 ? Demand::Dependency : Demand::Speculative);
    held[i] = view.reserve(i % 2 ? Class::MicroDungeonHalo : Class::BaseTilesArray, i % 2 ? 60 : 40);
  });
  auto usage = ledger.snapshot(); EXPECT_LE(usage.reservedBytes, 200u); EXPECT_LE(usage.classBytes[0], 100u);
  EXPECT_TRUE(worldGenerationMemoryEnvelope(usage).coherent);
}

TEST(WorldGenerationMemoryAdmissionTest, StalePositiveAdviceCannotAuthorizeTwoPhysicalReservations) {
  WorldGenerationMemoryLedger ledger(1000, Limits); std::array<Token, 8> held; std::atomic<unsigned> advice{0};
  auto stale = worldGenerationMemoryEnvelope(ledger.snapshot());
  simultaneous([&](unsigned i) {
    WorldGenerationMemoryPressurePolicy policy(Limits);
    if (policy.evaluate(stale, Demand::Visible, 150).allowRequest) ++advice;
    held[i] = ledger.reserve(Class::BaseTilesArray, 150);
  });
  unsigned accepted = 0; for (auto const& token : held) accepted += bool(token);
  EXPECT_EQ(advice.load(), 8u); EXPECT_EQ(accepted, 1u); EXPECT_EQ(ledger.snapshot().reservedBytes, 150u);
}

TEST(WorldGenerationMemoryAdmissionTest, ConcurrentSharedOwnersCreateOnlyOneChargedReservation) {
  WorldGenerationMemoryLedger ledger(1000, Limits); auto spec = ledger.withDemand(Demand::Speculative);
  auto owner = std::make_shared<unsigned>(1); std::array<Token, 8> held;
  simultaneous([&](unsigned i) { held[i] = spec.reserveSharedJson(owner, 100); });
  for (auto const& token : held) EXPECT_EQ(token, held[0]);
  EXPECT_EQ(ledger.snapshot().sharedResources, 1u); EXPECT_EQ(ledger.snapshot().reservedBytes, 100u);
  EXPECT_EQ(ledger.snapshot().admission.pressureChecks, 1u);
}

TEST(WorldGenerationMemoryAdmissionTest, ConcurrentSharedAcquireAndLastReleaseKeepTheScopedEnvelopeCoherent) {
  WorldGenerationMemoryLedger ledger(128, {true, 64, 96, 128}); auto spec = ledger.withDemand(Demand::Speculative);
  auto owner = std::make_shared<unsigned>(1); std::atomic<unsigned> failed{0};
  simultaneous([&](unsigned) {
    for (unsigned i = 0; i < 100; ++i) {
      auto array = spec.reserve(Class::BaseTilesArray, 4), json = spec.reserveSharedJson(owner, 32);
      auto usage = ledger.snapshot();
      if (!array || !json || usage.reservedBytes > 128 || !worldGenerationMemoryEnvelope(usage).coherent) ++failed;
    }
  });
  EXPECT_EQ(failed.load(), 0u); EXPECT_EQ(ledger.snapshot().reservedBytes, 0u);
  EXPECT_EQ(ledger.snapshot().sharedRegistryEntries, 0u);
}
