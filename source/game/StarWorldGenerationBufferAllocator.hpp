#pragma once

#include "StarWorldGenerationMemory.hpp"

#include <new>
#include <stdexcept>
#include <type_traits>

namespace Star {

// Fixed reservation per allocation, owned by an in-band header. Account for
// the entire request (header/padding + inline elements), not nested heaps or
// malloc overhead. Old/new buffers stay charged together during reallocation.
template <typename T>
class WorldGenerationBufferAllocator {
  struct Header {
    explicit Header(std::shared_ptr<WorldGenerationMemoryReservation> token) noexcept
      : memory(std::move(token)) {}
    std::shared_ptr<WorldGenerationMemoryReservation> memory;
  };
  static constexpr size_t Alignment = alignof(T) > alignof(Header) ? alignof(T) : alignof(Header);
  static constexpr size_t Prefix = (sizeof(Header) + alignof(T) - 1) / alignof(T) * alignof(T);

  template <typename U> friend class WorldGenerationBufferAllocator;
  friend struct WorldGenerationBufferAllocatorTestAccess;

  static void* allocateStorage(size_t bytes) {
    if constexpr (Alignment > __STDCPP_DEFAULT_NEW_ALIGNMENT__)
      return ::operator new(bytes, std::align_val_t(Alignment));
    else
      return ::operator new(bytes);
  }
  static void freeStorage(void* pointer) noexcept {
    if constexpr (Alignment > __STDCPP_DEFAULT_NEW_ALIGNMENT__)
      ::operator delete(pointer, std::align_val_t(Alignment));
    else
      ::operator delete(pointer);
  }

  template <typename Allocate>
  T* allocateWith(size_t count, Allocate rawAllocate) {
    auto bytes = requestBytes(count);
    WorldGenerationMemoryLedger view(m_state);
    auto memory = view.reserve(m_kind, bytes);
    if (!memory) throw std::bad_alloc();
    auto raw = static_cast<unsigned char*>(rawAllocate(bytes));
    ::new (static_cast<void*>(raw)) Header(std::move(memory));
    return reinterpret_cast<T*>(raw + Prefix);
  }

  static void validate(WorldGenerationMemoryClass kind) {
    if (!worldGenerationBufferClass(kind)) throw std::invalid_argument("Not a generation buffer class");
  }

public:
  using value_type = T;
  using propagate_on_container_copy_assignment = std::true_type;
  using propagate_on_container_move_assignment = std::true_type;
  using propagate_on_container_swap = std::true_type;
  using is_always_equal = std::false_type;
  template <typename U> struct rebind { using other = WorldGenerationBufferAllocator<U>; };

  // Require an explicit ledger. Never silently create an unobserved ledger or
  // access a process singleton on a worker's destruction path.
  WorldGenerationBufferAllocator() = delete;
  WorldGenerationBufferAllocator(WorldGenerationMemoryLedger const& ledger, WorldGenerationMemoryClass kind)
    : m_state(ledger.m_state), m_kind(kind) { validate(kind); }
  WorldGenerationBufferAllocator(WorldGenerationBufferAllocator const&) = default;
  WorldGenerationBufferAllocator& operator=(WorldGenerationBufferAllocator const&) = default;
  // Preserve a usable allocator in moved-from containers as well.
  WorldGenerationBufferAllocator(WorldGenerationBufferAllocator&& other) noexcept
    : m_state(other.m_state), m_kind(other.m_kind) {}
  WorldGenerationBufferAllocator& operator=(WorldGenerationBufferAllocator&& other) noexcept {
    m_state = other.m_state; m_kind = other.m_kind; return *this;
  }
  template <typename U>
  WorldGenerationBufferAllocator(WorldGenerationBufferAllocator<U> const& other) noexcept
    : m_state(other.m_state), m_kind(other.m_kind) {}
  template <typename U>
  WorldGenerationBufferAllocator(WorldGenerationBufferAllocator<U> const& other, WorldGenerationMemoryClass kind)
    : m_state(other.m_state), m_kind(kind) { validate(kind); }

  static constexpr size_t max_size() noexcept {
    return (std::numeric_limits<size_t>::max() - Prefix) / sizeof(T);
  }
  static size_t requestBytes(size_t count) {
    if (count > max_size()) throw std::bad_array_new_length();
    return Prefix + count * sizeof(T);
  }
  T* allocate(size_t count) {
    return allocateWith(count, [](size_t bytes) {
      return allocateStorage(bytes);
    });
  }
  void deallocate(T* pointer, size_t) noexcept {
    auto raw = reinterpret_cast<unsigned char*>(pointer) - Prefix;
    auto header = std::launder(reinterpret_cast<Header*>(raw));
    auto memory = std::move(header->memory);
    header->~Header();
    freeStorage(static_cast<void*>(raw));
    // The local token releases only after physical storage has been freed.
  }
  template <typename U>
  bool operator==(WorldGenerationBufferAllocator<U> const& other) const noexcept {
    return m_state == other.m_state && m_kind == other.m_kind;
  }
  template <typename U>
  bool operator!=(WorldGenerationBufferAllocator<U> const& other) const noexcept { return !(*this == other); }

private:
  std::shared_ptr<WorldGenerationMemoryState> m_state;
  WorldGenerationMemoryClass m_kind;
};

}
