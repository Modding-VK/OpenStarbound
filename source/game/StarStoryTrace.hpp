#pragma once

#include <atomic>
#include <cstdint>

namespace Star {

// Diagnostic context for in-process story messages. It is never serialized or
// used to decide gameplay. A zero id means that profiling is inactive.
class StoryTrace {
public:
  static uint64_t current() { return currentId(); }
  static uint64_t currentOrNext() {
    auto id = currentId();
    return id ? id : nextId().fetch_add(1, std::memory_order_relaxed);
  }

  explicit StoryTrace(uint64_t id) : m_active(id != 0), m_previous(m_active ? currentId() : 0) {
    if (m_active)
      currentId() = id;
  }
  ~StoryTrace() {
    if (m_active)
      currentId() = m_previous;
  }
  StoryTrace(StoryTrace const&) = delete;
  StoryTrace& operator=(StoryTrace const&) = delete;

private:
  static uint64_t& currentId() {
    static thread_local uint64_t id = 0;
    return id;
  }
  static std::atomic<uint64_t>& nextId() {
    static std::atomic<uint64_t> id{1};
    return id;
  }
  bool m_active;
  uint64_t m_previous;
};

}
