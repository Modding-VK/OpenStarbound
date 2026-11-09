#pragma once

#include "StarWorldGenerationMemoryEstimate.hpp"

#include <array>
#include <stdexcept>
#include <type_traits>

namespace Star {

enum class WorldGenerationTemporarySlot : size_t { Ambient, Queued, Validation, Output, Count };

// Job-local aggregate. It must outlive its scopes; neither it nor the owning
// values can move. Only the ledger facade is shared, never world state.
class WorldGenerationTemporaryMemory {
  static constexpr size_t Count = static_cast<size_t>(WorldGenerationTemporarySlot::Count);
public:
  class Scope {
  public:
    Scope(Scope const&) = delete;
    Scope& operator=(Scope const&) = delete;
    ~Scope() { m_owner.release(m_slot); }
    void predict(WorldGenerationBackingEstimate const& value) { m_owner.predict(m_slot, value); }
    void observe(WorldGenerationBackingEstimate const& value) { m_owner.observe(m_slot, value); }
  private:
    friend class WorldGenerationTemporaryMemory;
    Scope(WorldGenerationTemporaryMemory& owner, WorldGenerationTemporarySlot slot) : m_owner(owner), m_slot(slot) {}
    WorldGenerationTemporaryMemory& m_owner;
    WorldGenerationTemporarySlot m_slot;
  };

  explicit WorldGenerationTemporaryMemory(WorldGenerationMemoryLedger& ledger) : m_ledger(ledger) {}
  WorldGenerationTemporaryMemory(WorldGenerationTemporaryMemory const&) = delete;
  WorldGenerationTemporaryMemory& operator=(WorldGenerationTemporaryMemory const&) = delete;

  Scope acquire(WorldGenerationTemporarySlot slot, WorldGenerationBackingEstimate const& prediction) {
    auto index = checkedIndex(slot);
    if (m_active[index]) throw std::logic_error("Temporary slot already owned");
    auto total = totalReplacing(index, prediction);
    if (!total.storage.complete || !charge(total.storage.bytes)) throw std::bad_alloc();
    m_slots[index] = prediction;
    m_active[index] = true;
    return Scope(*this, slot);
  }
  bool estimateComplete() const { return m_complete; }

private:
  static size_t checkedIndex(WorldGenerationTemporarySlot slot) {
    auto index = static_cast<size_t>(slot);
    if (index >= Count) throw std::out_of_range("Invalid temporary slot");
    return index;
  }
  WorldGenerationBackingEstimate totalReplacing(size_t index, WorldGenerationBackingEstimate const& value) const {
    WorldGenerationBackingEstimate total;
    for (size_t i = 0; i < Count; ++i) total.add(i == index ? value : m_slots[i]);
    return total;
  }
  bool charge(size_t bytes) {
    if (bytes == m_charged) return true; // Empty sky paths do not allocate a token or lock.
    if (m_memory) {
      if (!m_memory->resizeEstimate(bytes)) return false;
    } else {
      m_memory = m_ledger.reserve(WorldGenerationMemoryClass::MicroDungeonTemporaryEstimate, bytes);
      if (!m_memory) return false;
    }
    m_charged = bytes;
    return true;
  }
  void incomplete() {
    m_complete = false;
    if (!m_memory) m_memory = m_ledger.reserve(WorldGenerationMemoryClass::MicroDungeonTemporaryEstimate, 0);
    if (m_memory) m_memory->noteIncompleteEstimate();
  }
  void predict(WorldGenerationTemporarySlot slot, WorldGenerationBackingEstimate const& prediction) {
    auto index = checkedIndex(slot);
    auto total = totalReplacing(index, prediction);
    if (!total.storage.complete || !charge(total.storage.bytes)) throw std::bad_alloc();
    m_slots[index] = prediction;
  }
  void observe(WorldGenerationTemporarySlot slot, WorldGenerationBackingEstimate const& value) {
    auto index = checkedIndex(slot);
    m_slots[index] = value; // The physical value already exists; never reject it on a finite refusal.
    auto total = totalReplacing(index, value);
    if (!total.storage.complete || !charge(total.storage.bytes)) incomplete();
  }
  void release(WorldGenerationTemporarySlot slot) noexcept {
    auto index = static_cast<size_t>(slot);
    m_slots[index] = {};
    m_active[index] = false;
    bool active = false;
    for (bool value : m_active) active = active || value;
    if (!active) { m_memory.reset(); m_charged = 0; return; }
    auto total = totalReplacing(index, {});
    // Destruction may shrink, but must not allocate/admit more to repair an
    // earlier incomplete observation. Other live scopes retain their charge.
    if (total.storage.complete && total.storage.bytes <= m_charged) {
      if (m_memory) m_memory->resizeEstimate(total.storage.bytes);
      m_charged = total.storage.bytes;
    } else m_complete = false;
  }

  WorldGenerationMemoryLedger m_ledger;
  std::shared_ptr<WorldGenerationMemoryReservation> m_memory;
  std::array<WorldGenerationBackingEstimate, Count> m_slots{};
  std::array<bool, Count> m_active{};
  size_t m_charged = 0;
  bool m_complete = true;
};

template <typename T, typename Sizer>
class WorldGenerationScopedTemporary {
public:
  template <typename Factory>
  WorldGenerationScopedTemporary(WorldGenerationTemporaryMemory& memory, WorldGenerationTemporarySlot slot,
      WorldGenerationBackingEstimate const& prediction, Factory&& factory, Sizer sizer)
    : m_scope(memory.acquire(slot, prediction)), m_sizer(std::move(sizer)),
      m_value(std::forward<Factory>(factory)()) {
    static_assert(std::is_same<std::invoke_result_t<Factory&&>, T>::value,
        "Factory must return a prvalue for C++17 guaranteed copy elision");
    static_assert(std::is_nothrow_invocable_r<WorldGenerationBackingEstimate, Sizer&, T const&>::value,
        "Temporary sizing must not throw");
    reconcile();
  }
  WorldGenerationScopedTemporary(WorldGenerationScopedTemporary const&) = delete;
  WorldGenerationScopedTemporary& operator=(WorldGenerationScopedTemporary const&) = delete;
  T const& value() const { return m_value; }
  WorldGenerationBackingEstimate const& knownBacking() const { return m_known; }
  Sizer const& sizer() const { return m_sizer; }
  void reconcile() { m_known = m_sizer(m_value); m_scope.observe(m_known); }

  template <typename Operation>
  void mutate(Operation&& operation) {
    try { std::forward<Operation>(operation)(m_value); }
    catch (...) { reconcile(); throw; }
    reconcile();
  }
  template <typename Operation>
  void mutatePredicted(WorldGenerationBackingEstimate const& prediction, Operation&& operation) {
    m_scope.predict(prediction);
    mutate(std::forward<Operation>(operation));
  }
  // Transfer only to an operation whose lifetime stays inside this scope.
  // Its input reservation remains until the moved parameter has been destroyed.
  T&& transfer() { return std::move(m_value); }

private:
  WorldGenerationTemporaryMemory::Scope m_scope;
  Sizer m_sizer;
  T m_value; // Destroy physical value before the scope releases its charge.
  WorldGenerationBackingEstimate m_known;
};

// Keep exactly one value copy, then move its containers into the existing
// by-value validator. The output forecast bridges transfer and parameter death.
template <typename Input, typename InputSizer, typename Output, typename OutputSizer, typename Operation>
void validateWorldGenerationTemporaryCopy(WorldGenerationTemporaryMemory& memory,
    WorldGenerationScopedTemporary<Input, InputSizer> const& source,
    WorldGenerationScopedTemporary<Output, OutputSizer>& output, Operation&& operation) {
  static_assert(std::is_nothrow_move_constructible<Input>::value,
      "Validation must transfer containers without another element copy");
  WorldGenerationBackingEstimate prediction;
  prediction.add(output.knownBacking()); prediction.add(output.knownBacking());
  prediction.add(source.knownBacking()); prediction.add(source.knownBacking());
  output.mutatePredicted(prediction, [&](Output& destination) {
    WorldGenerationScopedTemporary<Input, InputSizer> copy(memory, WorldGenerationTemporarySlot::Validation,
        source.knownBacking(), [&]() -> Input { return source.value(); }, source.sizer());
    std::forward<Operation>(operation)(copy.transfer(), destination);
  });
}

}
