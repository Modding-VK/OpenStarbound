#include "StarWorldPerformance.hpp"
#include "StarTime.hpp"

#include <atomic>
#include <map>
#include <mutex>
#include <utility>

namespace Star {

namespace {
std::atomic<bool> overlayEnabled{false};
std::atomic<uint64_t> overlayGeneration{0};
std::mutex snapshotMutex;
std::map<String, WorldPerformanceSnapshot> snapshots;
}

bool WorldPerformanceOverlay::enabled() {
  return overlayEnabled.load(std::memory_order_relaxed);
}

uint64_t WorldPerformanceOverlay::generation() {
  return overlayGeneration.load(std::memory_order_relaxed);
}

void WorldPerformanceOverlay::setEnabled(bool enabled) {
  if (overlayEnabled.exchange(enabled, std::memory_order_relaxed) != enabled) {
    overlayGeneration.fetch_add(1, std::memory_order_relaxed);
    std::lock_guard<std::mutex> lock(snapshotMutex);
    snapshots.clear();
  }
}

void WorldPerformanceOverlay::publish(WorldPerformanceSnapshot snapshot) {
  if (!enabled())
    return;

  std::lock_guard<std::mutex> lock(snapshotMutex);
  if (!enabled())
    return;

  for (auto it = snapshots.begin(); it != snapshots.end();) {
    if (snapshot.capturedAt - it->second.capturedAt > 3.0)
      it = snapshots.erase(it);
    else
      ++it;
  }
  snapshots[snapshot.worldId] = std::move(snapshot);
  while (snapshots.size() > 16) {
    auto oldest = snapshots.begin();
    for (auto it = snapshots.begin(); it != snapshots.end(); ++it) {
      if (it->second.capturedAt < oldest->second.capturedAt)
        oldest = it;
    }
    snapshots.erase(oldest);
  }
}

Maybe<WorldPerformanceSnapshot> WorldPerformanceOverlay::latest(String const& worldId) {
  if (!enabled())
    return {};

  std::lock_guard<std::mutex> lock(snapshotMutex);
  auto it = snapshots.find(worldId);
  if (it == snapshots.end() || Time::monotonicTime() - it->second.capturedAt > 3.0)
    return {};
  return it->second;
}

}
