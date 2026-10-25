#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "StarMaybe.hpp"
#include "StarString.hpp"

namespace Star {

enum class WorldPerformanceCategory : size_t {
  Npc,
  Object,
  OtherEntities,
  WorldScripts,
  Generation,
  Storage,
  Other,
  Idle,
  Count
};

constexpr size_t WorldPerformanceCategoryCount = static_cast<size_t>(WorldPerformanceCategory::Count);
constexpr size_t WorldPerformanceTickHistory = 64;

// Wall-clock diagnostics for one world and one bounded reporting window.
struct WorldPerformanceSnapshot {
  std::array<double, WorldPerformanceCategoryCount> seconds{};
  std::array<float, WorldPerformanceTickHistory> tickBusyMilliseconds{};
  String worldId;
  double capturedAt = 0;
  double windowSeconds = 0;
  double busySeconds = 0;
  double idleSeconds = 0;
  double maximumTickMilliseconds = 0;
  double excessTickMilliseconds = 0;
  double oldestQueuedMilliseconds = 0;
  double maximumQueuedMilliseconds = 0;
  bool detailed = false;
  uint64_t ticks = 0;
  uint64_t overBudgetTicks = 0;
  uint64_t slowNpcUpdates = 0;
  uint64_t slowObjectUpdates = 0;
  size_t queuedSectors = 0;
  size_t prefetchedSectors = 0;
  size_t overdueSectors = 0;
  size_t peakQueuedSectors = 0;
  size_t lateCompletedSectors = 0;
};

// Only the local client requests sampling. Publishing once per window and
// periodic UI reads take a short lock; entity and sector work never takes it.
class WorldPerformanceOverlay {
public:
  static bool enabled();
  static bool detailed();
  static uint64_t generation();
  static void setEnabled(bool enabled);
  static void setDetailed(bool detailed);
  static void publish(WorldPerformanceSnapshot snapshot);
  static Maybe<WorldPerformanceSnapshot> latest(String const& worldId);
};

}
