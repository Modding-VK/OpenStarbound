#include "StarWorldRegionCoordinator.hpp"
#include "StarGameTypes.hpp"

#include <algorithm>
#include <limits>

#include "gtest/gtest.h"

using namespace Star;

namespace {

WorldRegionPartition partition() {
  return WorldRegionPartition(WorldGeometry(256, 128));
}

WorldRegionSnapshot snapshot(WorldRegionPartition const& grid, Vec2U region, uint64_t step) {
  WorldRegionSnapshot result{region, step, {}};
  auto bounds = *grid.regionBounds(region);
  for (unsigned x = bounds.xMin() / WorldSectorSize; x <= unsigned(bounds.xMax() - 1) / WorldSectorSize; ++x) {
    for (unsigned y = bounds.yMin() / WorldSectorSize; y <= unsigned(bounds.yMax() - 1) / WorldSectorSize; ++y)
      result.sectors.append({{x, y}, true, 1, 9, 2, 4});
  }
  return result;
}

List<WorldRegionSnapshot> frame(uint64_t step) {
  auto grid = partition();
  return {snapshot(grid, {0, 0}, step), snapshot(grid, {1, 0}, step),
      snapshot(grid, {2, 0}, step), snapshot(grid, {3, 0}, step)};
}

}

TEST(WorldRegionCoordinatorTest, ReservesUniqueRegionsAndLanesAcrossSteps) {
  WorldRegionCoordinator coordinator(17, partition(), 2);
  auto first = coordinator.schedule(1, frame(1));
  ASSERT_EQ(first.size(), 2u);
  EXPECT_EQ(first[0]->worldEpoch, 17u);
  EXPECT_NE(first[0]->lane, first[1]->lane);
  EXPECT_NE(first[0]->snapshot.region, first[1]->snapshot.region);
  EXPECT_EQ(coordinator.activeCount(), 2u);
  EXPECT_TRUE(coordinator.schedule(2, frame(2)).empty());
  EXPECT_EQ(coordinator.finish(first[0], snapshot(partition(), first[0]->snapshot.region, 3)),
      WorldRegionTurnOutcome::Accepted);
  auto next = coordinator.schedule(3, frame(3));
  ASSERT_EQ(next.size(), 1u);
  EXPECT_NE(next[0]->snapshot.region, first[1]->snapshot.region);
  EXPECT_EQ(next[0]->lane, first[0]->lane);
  EXPECT_GT(next[0]->sequence, first[1]->sequence);
}

TEST(WorldRegionCoordinatorTest, FinishesOutOfOrderWithoutReleasingAnotherTurn) {
  WorldRegionCoordinator coordinator(17, partition(), 2);
  auto turns = coordinator.schedule(1, frame(1));
  ASSERT_EQ(turns.size(), 2u);
  EXPECT_EQ(coordinator.finish(turns[1], turns[1]->snapshot), WorldRegionTurnOutcome::Accepted);
  EXPECT_EQ(coordinator.activeCount(), 1u);
  EXPECT_EQ(coordinator.finish(turns[1], turns[1]->snapshot), WorldRegionTurnOutcome::Foreign);
  EXPECT_EQ(coordinator.activeCount(), 1u);
  EXPECT_EQ(coordinator.finish(turns[0], turns[0]->snapshot), WorldRegionTurnOutcome::Accepted);
  EXPECT_EQ(coordinator.activeCount(), 0u);
}

TEST(WorldRegionCoordinatorTest, RotatesBoundedCapacityWithoutStarvation) {
  WorldRegionCoordinator coordinator(17, partition(), 1);
  for (uint64_t step = 1; step <= 8; ++step) {
    auto turns = coordinator.schedule(step, frame(step));
    ASSERT_EQ(turns.size(), 1u);
    EXPECT_EQ(turns[0]->snapshot.region, Vec2U(unsigned((step - 1) % 4), 0));
    EXPECT_EQ(coordinator.finish(turns[0], turns[0]->snapshot), WorldRegionTurnOutcome::Accepted);
  }
}

TEST(WorldRegionCoordinatorTest, RejectsChangedRevisionIncarnationAndAvailability) {
  for (unsigned mutation = 0; mutation < 5; ++mutation) {
    WorldRegionCoordinator coordinator(17, partition(), 1);
    auto turns = coordinator.schedule(1, frame(1));
    ASSERT_EQ(turns.size(), 1u);
    auto current = turns[0]->snapshot;
    current.captureStep = 2;
    switch (mutation) {
      case 0: ++current.sectors[0].contentRevision; break;
      case 1: ++current.sectors[0].incarnation; break;
      case 2: current.sectors[0].loaded = false; break;
      case 3: --current.sectors[0].loadLevel; break;
      case 4: --current.sectors[0].generationLevel; break;
    }
    EXPECT_EQ(coordinator.finish(turns[0], current), WorldRegionTurnOutcome::Stale);
    EXPECT_EQ(coordinator.activeCount(), 0u);
  }
}

TEST(WorldRegionCoordinatorTest, ValidatesCompletePartialEdgeIncludingUnloadedSectors) {
  WorldRegionPartition grid(WorldGeometry(70, 100));
  WorldRegionCoordinator coordinator(17, grid, 1);
  auto edge = snapshot(grid, {1, 1}, 1);
  ASSERT_EQ(edge.sectors.size(), 2u);
  edge.sectors[1].loaded = false;
  edge.sectors[1].loadLevel = edge.sectors[1].generationLevel = 0;
  auto turns = coordinator.schedule(1, {edge});
  ASSERT_EQ(turns.size(), 1u);
  EXPECT_EQ(coordinator.finish(turns[0], edge), WorldRegionTurnOutcome::Accepted);
  edge.captureStep = 2;
  edge.sectors.pop_back();
  EXPECT_TRUE(coordinator.schedule(2, {edge}).empty());
  edge = snapshot(grid, {1, 1}, 3);
  edge.sectors[1].sector = edge.sectors[0].sector;
  EXPECT_TRUE(coordinator.schedule(3, {edge}).empty());
}

TEST(WorldRegionCoordinatorTest, InputOrderDoesNotChooseSchedulingOrder) {
  WorldRegionCoordinator a(17, partition(), 3), b(17, partition(), 3);
  auto ordered = frame(1);
  auto reversed = ordered;
  std::reverse(reversed.begin(), reversed.end());
  for (auto& region : reversed)
    std::reverse(region.sectors.begin(), region.sectors.end());
  auto left = a.schedule(1, ordered);
  auto right = b.schedule(1, reversed);
  ASSERT_EQ(left.size(), right.size());
  for (size_t i = 0; i < left.size(); ++i) {
    EXPECT_EQ(left[i]->snapshot.region, right[i]->snapshot.region);
    EXPECT_EQ(left[i]->snapshot.sectors, right[i]->snapshot.sectors);
    EXPECT_EQ(left[i]->lane, right[i]->lane);
    auto current = left[i]->snapshot;
    std::reverse(current.sectors.begin(), current.sectors.end());
    EXPECT_EQ(a.finish(left[i], current), WorldRegionTurnOutcome::Accepted);
  }
}

TEST(WorldRegionCoordinatorTest, StepCannotRepeatOrGoBackwardsAfterCancellation) {
  WorldRegionCoordinator coordinator(17, partition(), 2);
  EXPECT_EQ(coordinator.schedule(5, frame(5)).size(), 2u);
  coordinator.cancelAll();
  EXPECT_TRUE(coordinator.schedule(5, frame(5)).empty());
  EXPECT_TRUE(coordinator.schedule(4, frame(4)).empty());
  EXPECT_EQ(coordinator.schedule(6, frame(6)).size(), 2u);
}

TEST(WorldRegionCoordinatorTest, ForeignAndCancelledTicketsCannotFinishNewTurn) {
  WorldRegionCoordinator coordinator(17, partition(), 1), foreign(18, partition(), 1);
  auto old = coordinator.schedule(1, frame(1));
  auto other = foreign.schedule(1, frame(1));
  ASSERT_EQ(old.size(), 1u);
  ASSERT_EQ(other.size(), 1u);
  coordinator.cancelAll();
  auto fresh = coordinator.schedule(2, frame(2));
  ASSERT_EQ(fresh.size(), 1u);
  EXPECT_GT(fresh[0]->sequence, old[0]->sequence);
  EXPECT_EQ(coordinator.finish(old[0], old[0]->snapshot), WorldRegionTurnOutcome::Foreign);
  EXPECT_EQ(coordinator.finish(other[0], other[0]->snapshot), WorldRegionTurnOutcome::Foreign);
  EXPECT_EQ(coordinator.finish({}, fresh[0]->snapshot), WorldRegionTurnOutcome::Foreign);
  auto forged = make_shared<WorldRegionTurn const>(17, fresh[0]->sequence,
      fresh[0]->lane, fresh[0]->snapshot);
  EXPECT_EQ(coordinator.finish(forged, fresh[0]->snapshot), WorldRegionTurnOutcome::Foreign);
  auto invalidLane = make_shared<WorldRegionTurn const>(17, fresh[0]->sequence,
      std::numeric_limits<unsigned>::max(), fresh[0]->snapshot);
  EXPECT_EQ(coordinator.finish(invalidLane, fresh[0]->snapshot), WorldRegionTurnOutcome::Foreign);
  EXPECT_EQ(coordinator.activeCount(), 1u);
  EXPECT_EQ(coordinator.finish(fresh[0], fresh[0]->snapshot), WorldRegionTurnOutcome::Accepted);
}

TEST(WorldRegionCoordinatorTest, RetainsOwnedSnapshotAndRejectsBackwardView) {
  WorldRegionCoordinator coordinator(17, partition(), 1);
  auto input = frame(5);
  auto turns = coordinator.schedule(5, input);
  ASSERT_EQ(turns.size(), 1u);
  input[0].sectors[0].contentRevision = 999;
  EXPECT_EQ(turns[0]->snapshot.sectors[0].contentRevision, 9u);
  auto current = turns[0]->snapshot;
  current.captureStep = 4;
  EXPECT_EQ(coordinator.finish(turns[0], current), WorldRegionTurnOutcome::Stale);
}

TEST(WorldRegionCoordinatorTest, RejectsMalformedAndOversizedFramesAtomically) {
  WorldRegionCoordinator coordinator(17, partition(), 2);
  auto wrongStep = frame(1);
  wrongStep[2].captureStep = 0;
  EXPECT_TRUE(coordinator.schedule(1, wrongStep).empty());
  auto invalidRegion = frame(2);
  invalidRegion[3].region = Vec2U(4, 0);
  EXPECT_TRUE(coordinator.schedule(2, invalidRegion).empty());
  auto duplicateRegion = frame(3);
  duplicateRegion.append(duplicateRegion[0]);
  EXPECT_TRUE(coordinator.schedule(3, duplicateRegion).empty());
  List<WorldRegionSnapshot> oversized;
  for (size_t i = 0; i <= WorldRegionCoordinator::MaxRegions; ++i)
    oversized.append(snapshot(partition(), {0, 0}, 4));
  EXPECT_TRUE(coordinator.schedule(4, std::move(oversized)).empty());
  EXPECT_EQ(coordinator.activeCount(), 0u);
}

TEST(WorldRegionCoordinatorTest, ClampsLaneCapacity) {
  WorldRegionCoordinator zero(17, partition(), 0), huge(17, partition(), std::numeric_limits<size_t>::max());
  auto grid = partition();
  List<WorldRegionSnapshot> regions;
  for (unsigned x = 0; x < 4; ++x) {
    for (unsigned y = 0; y < 2; ++y)
      regions.append(snapshot(grid, {x, y}, 1));
  }
  EXPECT_EQ(zero.schedule(1, regions).size(), 1u);
  auto turns = huge.schedule(1, regions);
  EXPECT_EQ(turns.size(), WorldRegionCoordinator::MaxTurns);
  for (auto const& turn : turns)
    EXPECT_LT(turn->lane, WorldRegionCoordinator::MaxTurns);
}

TEST(WorldRegionObservationTest, DetectsLoadUnloadEvenWhenPresenceReturnsToAbsent) {
  WorldRegionObservation observation;
  auto absent = observation.watch({2, 3});
  ASSERT_TRUE(absent);
  EXPECT_EQ(observation.watch({2, 3}), absent);
  observation.changed({2, 3}); // loaded
  observation.changed({2, 3}); // unloaded before next snapshot
  auto returnedToAbsent = observation.revision({2, 3});
  ASSERT_TRUE(returnedToAbsent);
  EXPECT_GT(*returnedToAbsent, *absent);
}

TEST(WorldRegionObservationTest, UnrelatedChangesDoNotInvalidateLocalObservation) {
  WorldRegionObservation observation;
  auto local = observation.watch({0, 0});
  auto remote = observation.watch({100, 100});
  ASSERT_TRUE(local);
  ASSERT_TRUE(remote);
  observation.changed({100, 100});
  observation.changed({200, 200}); // not watched; consumes no entry
  EXPECT_EQ(observation.revision({0, 0}), local);
  EXPECT_GT(*observation.revision({100, 100}), *remote);
  EXPECT_FALSE(observation.revision({200, 200}));
}

TEST(WorldRegionObservationTest, CapsWatchesAndNeverReusesStampAfterReset) {
  WorldRegionObservation observation;
  auto original = observation.watch({0, 0});
  ASSERT_TRUE(original);
  for (unsigned x = 1; x < WorldRegionObservation::MaxSectors; ++x)
    ASSERT_TRUE(observation.watch({x, 0}));
  EXPECT_FALSE(observation.watch({WorldRegionObservation::MaxSectors, 0}));
  EXPECT_EQ(observation.watch({0, 0}), original);
  observation.reset();
  EXPECT_FALSE(observation.revision({0, 0}));
  auto fresh = observation.watch({0, 0});
  ASSERT_TRUE(fresh);
  EXPECT_GT(*fresh, *original);
}

TEST(WorldRegionCoordinatorTest, LifecycleChangeRejectsOnlyTheObservedRegion) {
  auto grid = partition();
  WorldRegionObservation observation;
  auto observed = [&](Vec2U region, uint64_t step) {
    auto view = snapshot(grid, region, step);
    for (auto& sector : view.sectors) {
      // Model an absent slot as metadata, never as an empty tile array.
      sector.loaded = false;
      sector.loadLevel = sector.generationLevel = 0;
      sector.incarnation = *observation.watch(sector.sector);
    }
    return view;
  };
  WorldRegionCoordinator coordinator(44, grid, 2);
  auto turns = coordinator.schedule(1, {observed({0, 0}, 1), observed({1, 0}, 1)});
  ASSERT_EQ(turns.size(), 2u);
  observation.changed({0, 0}); // absent -> loaded
  observation.changed({0, 0}); // loaded -> absent before next capture
  EXPECT_EQ(coordinator.finish(turns[1], observed({1, 0}, 2)), WorldRegionTurnOutcome::Accepted);
  EXPECT_EQ(coordinator.finish(turns[0], observed({0, 0}, 2)), WorldRegionTurnOutcome::Stale);
  auto retry = coordinator.schedule(2, {observed({0, 0}, 2)});
  ASSERT_EQ(retry.size(), 1u);
  EXPECT_EQ(coordinator.finish(turns[0], observed({0, 0}, 3)), WorldRegionTurnOutcome::Foreign);
  EXPECT_EQ(coordinator.finish(retry[0], observed({0, 0}, 3)), WorldRegionTurnOutcome::Accepted);
}
