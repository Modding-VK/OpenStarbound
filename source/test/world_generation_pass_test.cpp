#include "StarWorldGenerationPass.hpp"
#include "StarWorldGenerationPassHarness.hpp"

#include "gtest/gtest.h"

using namespace Star;

TEST(WorldGenerationPassTest, ContinuationDoesNotCancelAnAllowedVisibleAdvance) {
  WorldGenerationPass<unsigned> pass(true, 4);
  ASSERT_TRUE(pass.tryVisit(1));
  auto decision = pass.afterBlockedVisible(true, true);
  EXPECT_TRUE(decision.continueVisible);
  EXPECT_FALSE(decision.stopForContinuation);
}

TEST(WorldGenerationPassTest, LegacyContinuationStillStopsThePass) {
  WorldGenerationPass<unsigned> pass(false, 4);
  auto decision = pass.afterBlockedVisible(true, true);
  EXPECT_FALSE(decision.continueVisible);
  EXPECT_TRUE(decision.stopForContinuation);
  EXPECT_TRUE(pass.tryVisit(1));
  EXPECT_TRUE(pass.tryVisit(1));
}

TEST(WorldGenerationPassTest, CandidateLimitStillStopsAContinuation) {
  WorldGenerationPass<unsigned> pass(true, 1);
  auto first = pass.afterBlockedVisible(true, true);
  EXPECT_TRUE(first.continueVisible);
  EXPECT_FALSE(first.stopForContinuation);
  auto second = pass.afterBlockedVisible(true, true);
  EXPECT_FALSE(second.continueVisible);
  EXPECT_TRUE(second.stopForContinuation);
}

TEST(WorldGenerationPassTest, CandidateLimitWithoutContinuationFallsBackToBaseTiles) {
  WorldGenerationPass<unsigned> pass(true, 1);
  EXPECT_TRUE(pass.afterBlockedVisible(true, false).continueVisible);
  auto decision = pass.afterBlockedVisible(true, false);
  EXPECT_FALSE(decision.continueVisible);
  EXPECT_FALSE(decision.stopForContinuation);
}

TEST(WorldGenerationPassTest, NonCompleteTargetCannotAdvanceVisibleWork) {
  WorldGenerationPass<unsigned> pass(true, 4);
  EXPECT_FALSE(pass.afterBlockedVisible(false, false).continueVisible);
}

TEST(WorldGenerationPassTest, PreviewVisitsEachOuterCandidateOnlyOnce) {
  WorldGenerationPass<unsigned> pass(true, 4);
  EXPECT_TRUE(pass.tryVisit(1));
  EXPECT_TRUE(pass.tryVisit(2));
  EXPECT_FALSE(pass.tryVisit(1));
  EXPECT_FALSE(pass.tryVisit(2));
  EXPECT_TRUE(pass.tryVisit(3));
}

TEST(WorldGenerationPassTest, FirstContinuationWinsWithinEachPass) {
  WorldGenerationPass<unsigned> first(true, 4);
  EXPECT_TRUE(first.reserveContinuation());
  EXPECT_FALSE(first.reserveContinuation());
  WorldGenerationPass<unsigned> next(true, 4);
  EXPECT_TRUE(next.reserveContinuation());
}

TEST(WorldGenerationPassTest, ReadyDrainLeavesReservedTailOfTheSameDeadline) {
  EXPECT_DOUBLE_EQ(worldGenerationReadyResultDeadline(1.0, 1.040, true, 2), 1.038);
  EXPECT_DOUBLE_EQ(worldGenerationReadyResultDeadline(1.039, 1.040, true, 2), 1.039);
  EXPECT_DOUBLE_EQ(worldGenerationReadyResultDeadline(1.0, 1.040, false, 2), 1.040);
  EXPECT_DOUBLE_EQ(worldGenerationReadyResultDeadline(1.0, 1.040, true, 0), 1.040);
  EXPECT_DOUBLE_EQ(worldGenerationReadyResultDeadline(1.0, 1.001, true, 8), 1.0);
}

TEST(WorldGenerationPassTest, StreamingReproducesOriginalEarlyExitAndDeliversOtherRoots) {
  WorldGenerationPassHarness original, fixed;
  for (auto harness : {&original, &fixed}) {
    harness->add(1, {0, 100, 0, 0});
    harness->add(2);
    harness->add(3);
  }
  WorldGenerationPassHarness::Options control;
  control.originalContinuationBreak = true;
  auto before = original.run(control);
  auto after = fixed.run(WorldGenerationPassHarness::Options{});
  EXPECT_EQ(before.attempts.size(), 1u);
  EXPECT_EQ(before.levelsAdvanced, 1u);
  EXPECT_TRUE(before.completed.empty());
  EXPECT_EQ(after.completed, (std::vector<unsigned>{2, 3}));
  EXPECT_EQ(after.newSectorsCompleted, 2u);
  EXPECT_EQ(after.levelsAdvanced, 9u);
  EXPECT_EQ(after.elapsedMilliseconds, 9u);
  EXPECT_EQ(after.continuedWithContinuation, 1u);
  ASSERT_TRUE(fixed.continuation());
  EXPECT_EQ(*fixed.continuation(), 1u);
}

TEST(WorldGenerationPassTest, StreamingProductsMatchOriginalAfterControlledCompletions) {
  WorldGenerationPassHarness original, fixed;
  for (auto harness : {&original, &fixed}) {
    harness->add(1, {0, 100, 0, 0});
    harness->add(2);
    harness->add(3);
  }
  WorldGenerationPassHarness::Options control;
  control.originalContinuationBreak = true;
  for (uint64_t tick = 0; tick <= 160; tick += 16) {
    original.advanceTo(tick);
    fixed.advanceTo(tick);
    original.run(control);
    fixed.run(WorldGenerationPassHarness::Options{});
  }
  EXPECT_TRUE(original.empty());
  EXPECT_TRUE(fixed.empty());
  EXPECT_EQ(original.products(), fixed.products());
  EXPECT_EQ(fixed.products().size(), 12u);
  unsigned levels[4] = {};
  for (auto const& event : fixed.publicationOrder())
    EXPECT_EQ(event[1], ++levels[event[0]]);
}

TEST(WorldGenerationPassTest, StreamingLegacyDoesNotAdvancePastAContinuation) {
  WorldGenerationPassHarness harness;
  harness.add(1, {0, 100, 0, 0});
  harness.add(2);
  WorldGenerationPassHarness::Options options;
  options.advancePreview = false;
  auto result = harness.run(options);
  EXPECT_EQ(result.attempts.size(), 1u);
  EXPECT_EQ(result.levelsAdvanced, 1u);
  EXPECT_TRUE(result.completed.empty());
}

TEST(WorldGenerationPassTest, StreamingSingleBlockedRootDoesNotSpinOrRetry) {
  WorldGenerationPassHarness harness;
  harness.add(1, {100, 0, 0, 0});
  auto result = harness.run(WorldGenerationPassHarness::Options{});
  EXPECT_EQ(result.attempts.size(), 1u);
  EXPECT_EQ(result.levelsAdvanced, 0u);
  EXPECT_EQ(result.elapsedMilliseconds, 0u);
}

TEST(WorldGenerationPassTest, StreamingBaseTilesTailDoesNotRegenerateVisitedRoots) {
  WorldGenerationPassHarness harness;
  for (unsigned key = 1; key <= 6; ++key)
    harness.add(key, {0, 100, 0, 0});
  WorldGenerationPassHarness::Options options;
  options.candidateLimit = 1;
  auto result = harness.run(options);
  // Both first roots make progress: the second continuation stops the pass
  // at the limit. Their follow-ups cannot overwrite the first reservation.
  EXPECT_EQ(result.attempts.size(), 2u);
  EXPECT_EQ(result.levelsAdvanced, 2u);
  ASSERT_TRUE(harness.continuation());
  EXPECT_EQ(*harness.continuation(), 1u);

  WorldGenerationPassHarness tail;
  for (unsigned key = 1; key <= 6; ++key)
    tail.add(key, {0, 100, 0, 0});
  options.continuationPreview = false;
  auto tailResult = tail.run(options);
  ASSERT_EQ(tailResult.attempts.size(), 6u);
  for (unsigned i = 0; i < 6; ++i) {
    EXPECT_EQ(tailResult.attempts[i].first, i + 1);
    EXPECT_EQ(tailResult.attempts[i].second, i < 2);
  }
}

TEST(WorldGenerationPassTest, StreamingDeadlineAndLevelQuotaBoundUsefulWork) {
  for (unsigned limit = 0; limit <= 4; ++limit) {
    WorldGenerationPassHarness deadline, quota;
    deadline.add(1);
    quota.add(1);
    WorldGenerationPassHarness::Options options;
    options.budgetMilliseconds = limit;
    auto timed = deadline.run(options);
    EXPECT_EQ(timed.levelsAdvanced, limit);
    EXPECT_EQ(timed.elapsedMilliseconds, limit);
    options.budgetMilliseconds = 16;
    options.levelLimit = limit;
    auto limited = quota.run(options);
    EXPECT_EQ(limited.levelsAdvanced, limit);
    EXPECT_EQ(limited.elapsedMilliseconds, limit);
  }
}

TEST(WorldGenerationPassTest, StreamingResumesTheFirstContinuationBeforeNearerWork) {
  WorldGenerationPassHarness harness;
  harness.add(1, {0, 100, 0, 0});
  harness.add(2, {0, 100, 0, 0});
  harness.run(WorldGenerationPassHarness::Options{});
  ASSERT_TRUE(harness.continuation());
  EXPECT_EQ(*harness.continuation(), 1u);
  harness.advanceTo(100);
  WorldGenerationPassHarness::Options options;
  options.promoted = 2;
  auto next = harness.run(options);
  ASSERT_FALSE(next.attempts.empty());
  EXPECT_EQ(next.attempts.front().first, 1u);
}

TEST(WorldGenerationPassTest, StreamingSavedCompletionIsNotNewTerrain) {
  WorldGenerationPassHarness harness;
  harness.add(1, {}, 4);
  auto result = harness.run(WorldGenerationPassHarness::Options{});
  EXPECT_EQ(result.completed.size(), 1u);
  EXPECT_EQ(result.newSectorsCompleted, 0u);
  EXPECT_EQ(result.levelsAdvanced, 0u);
  EXPECT_TRUE(harness.products().empty());
}

TEST(WorldGenerationPassTest, StreamingPromotionGetsFirstOwnerAttempt) {
  WorldGenerationPassHarness harness;
  harness.add(1);
  harness.add(2);
  WorldGenerationPassHarness::Options options;
  options.promoted = 2;
  options.levelLimit = 1;
  auto result = harness.run(options);
  ASSERT_EQ(result.attempts.size(), 1u);
  EXPECT_EQ(result.attempts.front().first, 2u);
}

TEST(WorldGenerationPassTest, StreamingControlledRoutesMatchAcrossLimitsAndLegacyMode) {
  for (bool preview : {false, true}) {
    for (bool continuation : {false, true}) {
      for (unsigned limit = 1; limit <= 16; ++limit) {
        for (unsigned roots : {1u, 3u, 17u}) {
          SCOPED_TRACE(testing::Message() << "preview=" << preview
              << " continuation=" << continuation << " limit=" << limit << " roots=" << roots);
          WorldGenerationPassHarness original, fixed;
          for (unsigned key = 1; key <= roots; ++key) {
            std::array<uint64_t, 4> ready{(key % 4) * 4u, (key % 5) * 7u, (key % 3) * 5u, 0};
            original.add(key, ready);
            fixed.add(key, ready);
          }
          WorldGenerationPassHarness::Options options;
          options.advancePreview = preview;
          options.continuationPreview = continuation;
          options.candidateLimit = limit;
          for (uint64_t tick = 0; tick <= 512; tick += 16) {
            original.advanceTo(tick);
            fixed.advanceTo(tick);
            options.originalContinuationBreak = true;
            auto before = original.run(options);
            options.originalContinuationBreak = false;
            auto after = fixed.run(options);
            EXPECT_LE(after.elapsedMilliseconds, options.budgetMilliseconds);
            if (preview) {
              std::set<unsigned> visited;
              for (auto const& attempt : after.attempts)
                EXPECT_TRUE(visited.insert(attempt.first).second);
            } else {
              // With advancement disabled, the removed guard was redundant:
              // the unchanged policy still stops on every continuation.
              EXPECT_EQ(before.attempts, after.attempts);
              EXPECT_EQ(before.completed, after.completed);
              EXPECT_EQ(before.levelsAdvanced, after.levelsAdvanced);
            }
          }
          EXPECT_TRUE(original.empty());
          EXPECT_TRUE(fixed.empty());
          EXPECT_EQ(original.products(), fixed.products());
          EXPECT_EQ(fixed.products().size(), roots * 4u);
        }
      }
    }
  }
}
