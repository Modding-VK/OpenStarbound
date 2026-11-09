#pragma once

#include <algorithm>
#include <cstddef>
#include <set>

namespace Star {

struct WorldGenerationPassDecision {
  bool continueVisible;
  bool stopForContinuation;
};

// Owner-thread policy only. Recursive generation, prerequisites, publication,
// deadlines and queue ownership stay with WorldStorage. The key identifies an
// outer-loop candidate, not every sector reached recursively through it.
template <typename Candidate>
class WorldGenerationPass {
public:
  WorldGenerationPass(bool advancePreview, size_t candidateLimit)
    : m_advancePreview(advancePreview), m_candidateLimit(candidateLimit) {}

  bool tryVisit(Candidate const& candidate) {
    // No allocation or change to legacy traversal while the preview is off.
    return !m_advancePreview || m_visited.insert(candidate).second;
  }

  WorldGenerationPassDecision afterBlockedVisible(bool completeTarget, bool continuation) {
    bool advance = m_advancePreview && completeTarget
        && m_blockedCandidates++ < m_candidateLimit;
    // Saving a follow-up does not prevent independent work in this pass.
    // Once visible advancement is exhausted (or off), retain the old stop.
    return {advance, continuation && !advance};
  }

  bool reserveContinuation() {
    if (m_continuationReserved)
      return false;
    m_continuationReserved = true;
    return true;
  }

private:
  bool m_advancePreview;
  size_t m_candidateLimit;
  size_t m_blockedCandidates = 0;
  bool m_continuationReserved = false;
  std::set<Candidate> m_visited;
};

inline double worldGenerationReadyResultDeadline(double now, double deadline,
    bool reservedAttempt, unsigned reserveMilliseconds) {
  return reservedAttempt && reserveMilliseconds > 0
      ? std::max(now, deadline - reserveMilliseconds / 1000.0) : deadline;
}

}
