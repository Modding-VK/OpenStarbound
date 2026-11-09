#pragma once

#include "StarWorldGenerationBufferAllocator.hpp"
#include "StarWorldGenerationMemoryEstimate.hpp"

#include <algorithm>
#include <functional>
#include <vector>

namespace Star {

// Candidate storage with a separate estimate for known nested heaps. Keep the
// original vector growth and copy history: no reserve, extra element move, or
// filtering. Estimates are not a complete heap bound or a new runtime cap.
template <typename T, typename Sizer>
class WorldGenerationPayloadBuffer {
  using Values = std::vector<T, WorldGenerationBufferAllocator<T>>;
public:
  WorldGenerationPayloadBuffer(WorldGenerationMemoryLedger& ledger, Sizer sizer)
    : m_ledger(ledger), m_sizer(std::move(sizer)),
      m_values(WorldGenerationBufferAllocator<T>(ledger, WorldGenerationMemoryClass::MicroDungeonCandidateBuffer)) {
    static_assert(std::is_copy_constructible<T>::value || std::is_nothrow_move_constructible<T>::value,
        "Vector growth must preserve the original values on construction failure");
    static_assert(std::is_nothrow_invocable_r<WorldGenerationBackingEstimate, Sizer&, T const&>::value,
        "Payload sizing after insertion must not throw");
  }
  WorldGenerationPayloadBuffer(WorldGenerationPayloadBuffer const&) = delete;
  WorldGenerationPayloadBuffer& operator=(WorldGenerationPayloadBuffer const&) = delete;

  template <typename Factory>
  void append(WorldGenerationBackingEstimate const& source, Factory&& factory) {
    static_assert(std::is_same<std::invoke_result_t<Factory&>, T>::value,
        "Factory must return a value for C++17 guaranteed copy elision");
    bool grows = m_values.size() == m_values.capacity();
    WorldGenerationSizeEstimate peak;
    peak.add(m_current.storage.bytes, grows ? 2 : 1);
    peak.add(source.storage.bytes, 2); // Temporary plus destination before transfer.
    if (!peak.complete || !source.storage.complete || !m_current.storage.complete)
      throw std::bad_alloc();
    bool hadMemory = bool(m_memory);
    size_t before = m_charged;
    setCharge(std::max(before, peak.bytes));
    try {
      // A prvalue factory adds no copy/move relative to push_back(T{...}).
      // Its temporary is destroyed while the predicted overlap is still held.
      T value = std::forward<Factory>(factory)();
      m_values.push_back(std::move(value));
    } catch (...) {
      if (!hadMemory) { m_memory.reset(); m_charged = 0; }
      else setCharge(before); // Shrink only; never needs new admission.
      throw;
    }
    reconcile(grows);
  }

  void sort() {
    std::sort(m_values.begin(), m_values.end(), std::less<T>{});
    if (!m_values.empty()) reconcile(true);
  }
  auto begin() const { return m_values.begin(); }
  auto end() const { return m_values.end(); }
  size_t size() const { return m_values.size(); }
  size_t capacity() const { return m_values.capacity(); }
  bool empty() const { return m_values.empty(); }
  WorldGenerationBackingEstimate const& knownBacking() const { return m_current; }
  bool estimateComplete() const { return m_complete; }

private:
  void setCharge(size_t bytes) {
    if (!m_memory) {
      m_memory = m_ledger.reserve(WorldGenerationMemoryClass::MicroDungeonCandidateEstimate, bytes);
      if (!m_memory) throw std::bad_alloc();
    } else if (!m_memory->resizeEstimate(bytes)) throw std::bad_alloc();
    m_charged = bytes;
  }
  void reconcile(bool all) {
    if (all) {
      m_current = {};
      for (auto const& value : m_values) m_current.add(m_sizer(value));
    } else m_current.add(m_sizer(m_values.back()));
    if (m_current.storage.complete && m_memory->resizeEstimate(m_current.storage.bytes)) {
      m_charged = m_current.storage.bytes;
    } else {
      // Do not discard an accepted payload on diagnostic reconciliation failure.
      // Keep the prior reservation and expose incomplete coverage explicitly.
      m_complete = false;
      m_memory->noteIncompleteEstimate();
    }
  }

  WorldGenerationMemoryLedger m_ledger;
  std::shared_ptr<WorldGenerationMemoryReservation> m_memory;
  size_t m_charged = 0;
  WorldGenerationBackingEstimate m_current;
  bool m_complete = true;
  Sizer m_sizer;
  // Destroy elements and their buffer before releasing the backing estimate.
  Values m_values;
};

}
