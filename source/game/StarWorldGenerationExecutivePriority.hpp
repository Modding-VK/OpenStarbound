#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>

#include "StarList.hpp"
#include "StarMap.hpp"
#include "StarMaybe.hpp"
#include "StarWorldGenerationDependency.hpp"
#include "StarWorldGenerationExecutiveShadow.hpp"

namespace Star {

// DAG-03 shadow ranking: interest/wrap/age, TTL/load, unlock benefit, and a
// bounded selection trace. Does not mutate WorldStorage or publish.

enum class WorldGenerationInterestClass : uint8_t {
  Background = 0,
  Nearby,
  Visible,
  Immediate
};

struct WorldGenerationExecutiveDemand {
  WorldGenerationExecutiveConsumerId consumer;
  WorldGenerationInterestClass interest = WorldGenerationInterestClass::Nearby;
  Vec2U focusSector;
  // Original demand age; never renewed by scoring/priority alone.
  uint64_t ageTick = 0;
  uint32_t worldWidthSectors = 0; // 0 disables horizontal wrap
  // Optional arrival estimate; 0 means unused. Lower eta is more urgent.
  uint64_t etaTick = 0;
};

struct WorldGenerationExecutivePriorityPolicy {
  // 0 disables TTL. Demands with (nowTick - ageTick) > ttlTicks are ignored.
  uint64_t ttlTicks = 0;
  uint64_t nowTick = 0;
  // Soft load caps applied during fair selection. Default is effectively open.
  size_t maxBackgroundSelections = std::numeric_limits<size_t>::max();
  size_t maxTotalSelections = 1;
  // Caps recorded blockers in the selection trace.
  size_t maxBlockers = 8;
};

enum class WorldGenerationExecutiveSelectionReason : uint8_t {
  None = 0,
  HighestInterest,
  FairAgeRetention,
  UnlockBenefit,
  OnlyEligible
};

enum class WorldGenerationExecutiveBlockerKind : uint8_t {
  TtlExpired = 0,
  LoadCapBackground,
  EmptyFrontier,
  NoActiveDemand
};

struct WorldGenerationExecutiveBlocker {
  WorldGenerationExecutiveBlockerKind kind = WorldGenerationExecutiveBlockerKind::TtlExpired;
  WorldGenerationExecutiveConsumerId consumer;
  WorldGenerationDependencyNode node;
};

struct WorldGenerationExecutivePriorityScore {
  WorldGenerationDependencyNode node;
  WorldGenerationExecutiveConsumerId consumer;
  WorldGenerationInterestClass interest = WorldGenerationInterestClass::Nearby;
  uint64_t ageTick = 0;
  uint64_t etaTick = 0;
  uint32_t distance = 0;
  size_t unlockBenefit = 0;
  uint64_t total = 0;
};

struct WorldGenerationExecutiveSelectionTrace {
  Maybe<WorldGenerationExecutivePriorityScore> chosen;
  WorldGenerationExecutiveSelectionReason reason = WorldGenerationExecutiveSelectionReason::None;
  List<WorldGenerationExecutiveBlocker> blockers;
  size_t expiredDemands = 0;
  size_t activeDemands = 0;
};

class WorldGenerationExecutivePriority {
public:
  static uint32_t sectorDistance(Vec2U const& a, Vec2U const& b, uint32_t worldWidthSectors);

  static bool demandActive(WorldGenerationExecutiveDemand const& demand,
      WorldGenerationExecutivePriorityPolicy const& policy);

  // Higher total is more urgent. Interest class dominates, then lower distance,
  // then earlier eta, then older age, then unlock benefit, then stable order.
  static WorldGenerationExecutivePriorityScore score(WorldGenerationDependencyNode const& node,
      WorldGenerationExecutiveDemand const& demand, size_t unlockBenefit = 0);

  static Maybe<WorldGenerationExecutivePriorityScore> selectFair(
      List<WorldGenerationDependencyNode> const& frontier,
      List<WorldGenerationExecutiveDemand> const& demands);

  // TTL/load-aware selection with bounded blocker/reason trace. Priority never
  // renews ageTick. Auxiliaries inherit consumer interest via shared demands.
  static WorldGenerationExecutiveSelectionTrace selectFairTraced(
      List<WorldGenerationDependencyNode> const& frontier,
      List<WorldGenerationExecutiveDemand> const& demands,
      WorldGenerationExecutivePriorityPolicy const& policy,
      Map<WorldGenerationDependencyNode, size_t> const& unlockBenefits = {});
};

}
