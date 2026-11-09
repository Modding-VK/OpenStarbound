#pragma once

#include "StarWorldGenerationMemory.hpp"

#include <cstdint>
#include <stdexcept>

namespace Star {

enum class WorldGenerationMemoryRetryStatus {
  Admitted, Active, AwaitRelease, AwaitProgress, Deferred,
  Impossible, Invalid, Exhausted, Completed, Stopped
};

template <size_t N>
struct WorldGenerationMemoryAttempt {
  WorldGenerationMemoryRetryStatus status;
  size_t attempt;
  std::optional<std::array<std::shared_ptr<WorldGenerationMemoryReservation>, N>> memory;
};

// Owner-thread protocol for restartable PURE producers, not a worker wait loop.
// Discovery of a larger output aborts the whole attempt before allocating it.
// Only declared resources are fenced: every physical owner must retain its
// corresponding token, and no tokens/backing may escape that ownership rule.
// Not wired into live generation: arbitrary Lua/world mutations cannot replay.
template <size_t N>
class WorldGenerationMemoryRetry {
public:
  static constexpr unsigned ModelVersion = 1;
  using Requests = std::array<WorldGenerationMemoryRequest, N>;
  using Status = WorldGenerationMemoryRetryStatus;
  using Attempt = WorldGenerationMemoryAttempt<N>;

  WorldGenerationMemoryRetry(WorldGenerationMemoryLedger const& ledger,
      WorldGenerationMemoryDemand demand, Requests requests, size_t maximumAttempts)
    : m_ledger(ledger.withDemand(demand)), m_requests(requests), m_maximumAttempts(maximumAttempts) {
    if (!maximumAttempts || !valid(requests)) throw std::invalid_argument("Invalid generation memory retry contract");
  }
  WorldGenerationMemoryRetry(WorldGenerationMemoryRetry const&) = delete;
  WorldGenerationMemoryRetry& operator=(WorldGenerationMemoryRetry const&) = delete;

  // Revision belongs to the owner and changes on useful drain/capacity events,
  // not on every poll/tick. Same-revision pressure refusals do not re-admit.
  // Exceptions (including physical bad_alloc) propagate; never become retries.
  Attempt admit(uint64_t progressRevision) {
    if (m_terminal) return result(*m_terminal);
    if (m_active) return result(Status::Active);
    if (m_attempts >= m_maximumAttempts) return finish(Status::Exhausted);
    for (auto const& owner : m_owners) if (!owner.expired()) return result(Status::AwaitRelease);
    if (m_rejectedRevision && *m_rejectedRevision == progressRevision) return result(Status::AwaitProgress);
    auto admission = m_ledger.tryReserveBatch(m_requests);
    switch (admission.status) {
      case WorldGenerationMemoryBatchStatus::Deferred:
        m_rejectedRevision = progressRevision;
        return result(Status::Deferred);
      case WorldGenerationMemoryBatchStatus::Impossible: return finish(Status::Impossible);
      case WorldGenerationMemoryBatchStatus::Invalid: return finish(Status::Invalid);
      case WorldGenerationMemoryBatchStatus::Admitted: break;
    }
    ++m_attempts;
    m_active = true;
    m_rejectedRevision.reset();
    for (size_t i = 0; i < N; ++i) m_owners[i] = (*admission.memory)[i];
    return {Status::Admitted, m_attempts, std::move(admission.memory)};
  }

  // Owner consumes a worker's growth descriptor, never its partial payload.
  // Strict monotonic growth avoids retrying an unchanged prediction forever.
  // Weak fences keep retries closed until ALL prior physical parts disappear.
  bool retry(size_t attempt, Requests requests) noexcept {
    if (m_terminal || !m_active || attempt != m_attempts || !valid(requests)) return false;
    bool grows = false;
    for (size_t i = 0; i < N; ++i) {
      if (requests[i].kind != m_requests[i].kind || requests[i].bytes < m_requests[i].bytes) return false;
      grows = grows || requests[i].bytes > m_requests[i].bytes;
    }
    if (!grows) return false;
    m_requests = requests;
    m_active = false;
    m_rejectedRevision.reset();
    return true;
  }

  bool complete(size_t attempt) noexcept {
    if (m_terminal || !m_active || attempt != m_attempts) return false;
    finish(Status::Completed);
    return true;
  }
  // Stopping the protocol does not cancel a producer or reclaim live memory.
  void stop() noexcept { if (!m_terminal) finish(Status::Stopped); }
  size_t attempts() const noexcept { return m_attempts; }
  Requests const& requests() const noexcept { return m_requests; }

private:
  static bool valid(Requests const& requests) noexcept {
    size_t bytes = 0;
    for (auto const& request : requests) {
      if (static_cast<size_t>(request.kind) >= static_cast<size_t>(WorldGenerationMemoryClass::Count)
          || request.kind == WorldGenerationMemoryClass::TemplateJsonEstimate
          || request.bytes > std::numeric_limits<size_t>::max() - bytes) return false;
      bytes += request.bytes;
    }
    return true;
  }
  Attempt result(Status status) const noexcept { return {status, m_attempts, {}}; }
  Attempt finish(Status status) noexcept {
    m_terminal = status;
    m_active = false;
    return result(status);
  }

  WorldGenerationMemoryLedger m_ledger;
  Requests m_requests;
  size_t const m_maximumAttempts;
  size_t m_attempts = 0;
  bool m_active = false;
  std::optional<Status> m_terminal;
  std::optional<uint64_t> m_rejectedRevision;
  std::array<std::weak_ptr<WorldGenerationMemoryReservation>, N> m_owners;
};

}
