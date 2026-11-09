#pragma once

#include "StarWorldGenerationPass.hpp"

#include <array>
#include <cstdint>
#include <deque>
#include <limits>
#include <map>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace Star {

// Controlled streaming of outer-loop requests, not a fake implementation of
// terrain or the 3x3 recursive generator. Uses the production pass policy;
// synthetic per-root stages expose queue progress, deadlines and outputs.
// All times are integer virtual milliseconds, never wall-clock benchmarks.
class WorldGenerationPassHarness {
public:
  struct Options {
    bool advancePreview = true;
    bool continuationPreview = true;
    unsigned candidateLimit = 4;
    uint64_t budgetMilliseconds = 16;
    size_t levelLimit = std::numeric_limits<size_t>::max();
    std::optional<unsigned> promoted;
    // Test-only counterfactual: reinsert the removed unconditional break.
    // This is not a game configuration key or execution of an older binary.
    bool originalContinuationBreak = false;
  };

  struct Result {
    std::vector<std::pair<unsigned, bool>> attempts; // true = Complete target
    std::vector<unsigned> completed;
    unsigned newSectorsCompleted = 0;
    unsigned levelsAdvanced = 0;
    unsigned continuedWithContinuation = 0;
    uint64_t elapsedMilliseconds = 0;
  };

  void add(unsigned key, std::array<uint64_t, 4> readyAt = {}, unsigned initialLevel = 0) {
    if (initialLevel > 4 || m_roots.count(key))
      throw std::invalid_argument("Invalid streaming fixture root");
    m_roots.emplace(key, Root{initialLevel, initialLevel, readyAt});
    m_queue.push_back(key);
  }

  void advanceTo(uint64_t milliseconds) {
    if (milliseconds < m_now)
      throw std::invalid_argument("Streaming fixture clock cannot go backwards");
    m_now = milliseconds;
  }

  Result run(Options const& options) {
    Result result;
    uint64_t started = m_now;
    uint64_t deadline = m_now + options.budgetMilliseconds;
    size_t remainingLevels = options.levelLimit;
    WorldGenerationPass<unsigned> pass(options.advancePreview, options.candidateLimit);
    bool schedulingOnly = false;
    size_t tailVisits = 0;
    size_t repeatedCandidates = 0;
    auto resumed = m_continuation;
    if (options.continuationPreview && resumed)
      moveToFront(*resumed);
    else if (options.promoted)
      moveToFront(*options.promoted);

    while (!m_queue.empty() && m_now < deadline && remainingLevels > 0) {
      unsigned key = m_queue.front();
      if (!pass.tryVisit(key)) {
        moveToBack(key);
        if (++repeatedCandidates >= m_queue.size())
          break;
        continue;
      }
      repeatedCandidates = 0;
      bool full = !schedulingOnly;
      result.attempts.emplace_back(key, full);
      if (options.continuationPreview && resumed && key == *resumed)
        m_continuation.reset();
      auto attempt = generate(key, full ? 4 : 1, remainingLevels, deadline);
      bool continuation = options.continuationPreview && full
          && !attempt.first && attempt.second > 0;
      if (continuation && pass.reserveContinuation())
        m_continuation = key;

      bool stop = false;
      if (full && attempt.first) {
        remove(key);
        result.completed.push_back(key);
        if (m_roots.at(key).initialLevel == 0)
          ++result.newSectorsCompleted;
      } else if (full) {
        auto decision = pass.afterBlockedVisible(true, continuation);
        stop = decision.stopForContinuation;
        if (decision.continueVisible) {
          moveToBack(key);
          if (continuation && !stop)
            ++result.continuedWithContinuation;
        } else {
          schedulingOnly = true;
          if (continuation)
            moveToFront(key);
          else
            moveToBack(key);
        }
      } else {
        moveToBack(key);
      }
      remainingLevels -= attempt.second;
      result.levelsAdvanced += attempt.second;
      if (stop || (options.originalContinuationBreak && continuation))
        break;
      if (schedulingOnly && ++tailVisits >= m_queue.size())
        break;
    }
    result.elapsedMilliseconds = m_now - started;
    return result;
  }

  std::optional<unsigned> continuation() const { return m_continuation; }
  bool empty() const { return m_queue.empty(); }

  // Compare final products by (root, stage), not global cross-root order:
  // selecting an independent root earlier is the opt-in behavior under test.
  auto products() const {
    auto result = m_products;
    std::sort(result.begin(), result.end());
    return result;
  }

  std::vector<std::array<uint64_t, 3>> const& publicationOrder() const { return m_products; }

private:
  struct Root {
    unsigned initialLevel;
    unsigned level;
    std::array<uint64_t, 4> readyAt;
  };

  std::pair<bool, size_t> generate(unsigned key, unsigned target, size_t limit, uint64_t deadline) {
    auto& root = m_roots.at(key);
    size_t advanced = 0;
    while (root.level < target) {
      if (advanced == limit || m_now >= deadline || m_now < root.readyAt[root.level])
        return {false, advanced};
      ++m_now; // One virtual millisecond per completed synthetic stage.
      ++root.level;
      ++advanced;
      uint64_t product = 0x1a2b3c4dULL ^ (uint64_t(key) << 8) ^ root.level;
      m_products.push_back({key, root.level, product});
    }
    return {true, advanced};
  }

  void remove(unsigned key) {
    auto entry = std::find(m_queue.begin(), m_queue.end(), key);
    if (entry == m_queue.end())
      throw std::logic_error("Streaming fixture lost a queued request");
    m_queue.erase(entry);
  }

  void moveToBack(unsigned key) { remove(key); m_queue.push_back(key); }
  void moveToFront(unsigned key) { remove(key); m_queue.push_front(key); }

  uint64_t m_now = 0;
  std::map<unsigned, Root> m_roots;
  std::deque<unsigned> m_queue;
  std::optional<unsigned> m_continuation;
  std::vector<std::array<uint64_t, 3>> m_products;
};

}
