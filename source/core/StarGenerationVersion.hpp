#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <mutex>

namespace Star {

namespace GenerationVersionDetail {
// Process-local identities, not persisted hashes. Saturation fails closed;
// no wrap, addresses, RNG or recycled identities. Separate atomic for tests.
inline uint64_t allocate(std::atomic<uint64_t>& last) noexcept {
  uint64_t previous = last.load(std::memory_order_relaxed);
  while (previous != std::numeric_limits<uint64_t>::max()) {
    if (last.compare_exchange_weak(previous, previous + 1, std::memory_order_relaxed)) return previous + 1;
  }
  return 0;
}
}

inline uint64_t nextGenerationVersion() noexcept {
  static std::atomic<uint64_t> last{0};
  return GenerationVersionDetail::allocate(last);
}

// Metadata only: does NOT lock source contents, stop workers or authorize a
// live-world commit. Source mutations retain their existing synchronization.
class GenerationVersionClock {
public:
  GenerationVersionClock() : m_version(nextGenerationVersion()) {}
  GenerationVersionClock(GenerationVersionClock const&) = delete;
  GenerationVersionClock& operator=(GenerationVersionClock const&) = delete;
  uint64_t current() const noexcept { return m_version.load(std::memory_order_acquire); }

  class Change {
  public:
    explicit Change(GenerationVersionClock& owner) : m_owner(owner), m_admitted(owner.begin()) {}
    Change(Change const&) = delete;
    Change& operator=(Change const&) = delete;
    ~Change() { if (m_admitted) m_owner.end(m_completed); }
    // Explicit success, not "destructor ran without an exception". A caller
    // returning early or swallowing a partial failure still stays unavailable.
    void complete() noexcept { m_completed = true; }
  private:
    GenerationVersionClock& m_owner;
    bool m_admitted;
    bool m_completed = false;
  };
  Change change() { return Change(*this); }
  void retire() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_retired = true;
    m_version.store(0, std::memory_order_release);
  }

private:
  bool begin() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_retired) return false;
    if (m_active == std::numeric_limits<size_t>::max()) {
      m_retired = true;
      m_version.store(0, std::memory_order_release);
      return false;
    }
    if (!m_active) m_failed = false;
    ++m_active;
    m_version.store(0, std::memory_order_release);
    return true;
  }
  void end(bool success) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_failed = m_failed || !success;
    --m_active;
    if (!m_active && !m_failed && !m_retired) m_version.store(nextGenerationVersion(), std::memory_order_release);
  }
  std::mutex m_mutex;
  std::atomic<uint64_t> m_version;
  size_t m_active = 0;
  bool m_failed = false;
  bool m_retired = false;
};

}
