#pragma once

#include "StarWorldGenerationBufferAllocator.hpp"
#include "StarWorldGenerationMemoryEstimate.hpp"

#include <vector>

namespace Star {

// Snapshot-normalized once, then shared only through const references. Do not
// move source elements: a copy may intentionally normalize nested containers.
template <typename T>
class WorldGenerationImmutableArray {
public:
  using Values = std::vector<T, WorldGenerationBufferAllocator<T>>;
  template <typename Source, typename Sizer>
  WorldGenerationImmutableArray(std::shared_ptr<WorldGenerationMemoryReservation> backing,
      WorldGenerationBufferAllocator<T> allocator, Source const& source, Sizer sizer)
    : m_backing(std::move(backing)), m_values(allocator) {
    m_values.reserve(source.size());
    for (auto const& value : source) m_values.emplace_back(value);
    auto actual = sizer(m_values);
    m_opaqueItems = actual.opaqueItems;
    m_complete = actual.storage.complete && m_backing->resizeEstimate(actual.storage.bytes);
    if (!m_complete) m_backing->noteIncompleteEstimate();
  }
  WorldGenerationImmutableArray(WorldGenerationImmutableArray const&) = delete;
  WorldGenerationImmutableArray& operator=(WorldGenerationImmutableArray const&) = delete;

  Values const& values() const { return m_values; }
  size_t opaqueItems() const { return m_opaqueItems; }
  bool estimateComplete() const { return m_complete; }

private:
  // Destroy elements before releasing their nested-backing estimate.
  std::shared_ptr<WorldGenerationMemoryReservation> m_backing;
  Values m_values;
  size_t m_opaqueItems = 0;
  bool m_complete = false;
};

template <typename T, typename Source, typename Sizer>
std::shared_ptr<WorldGenerationImmutableArray<T> const> makeWorldGenerationImmutableArray(
    WorldGenerationMemoryLedger& ledger, Source const& source, Sizer sizer) {
  auto estimate = sizer(source);
  if (!estimate.storage.complete) throw std::bad_alloc();
  auto backing = ledger.reserve(WorldGenerationMemoryClass::MicroDungeonDistributionEstimate, estimate.storage.bytes);
  if (!backing) throw std::bad_alloc();
  using Array = WorldGenerationImmutableArray<T>;
  WorldGenerationBufferAllocator<T> allocator(ledger, WorldGenerationMemoryClass::MicroDungeonDistributionBuffer);
  return std::allocate_shared<Array>(WorldGenerationBufferAllocator<Array>(allocator),
      std::move(backing), allocator, source, sizer);
}

// Owner-only lazy capture. The original input is retained until the first
// successful snapshot-equivalent copy; later snapshots share that copy.
template <typename T, typename Source>
class WorldGenerationSnapshotCache {
public:
  template <typename Sizer>
  WorldGenerationSnapshotCache(WorldGenerationMemoryLedger& ledger, Source source, Sizer sizer)
    : m_originalBacking(reserveOriginal(ledger, source, sizer)),
      m_source(std::move(source)), m_ledger(ledger) {
    static_assert(std::is_nothrow_move_constructible<Source>::value && std::is_nothrow_move_assignable<Source>::value,
        "Original input transfer must not introduce another element copy");
  }
  WorldGenerationSnapshotCache(WorldGenerationSnapshotCache const&) = delete;
  WorldGenerationSnapshotCache& operator=(WorldGenerationSnapshotCache const&) = delete;
  WorldGenerationSnapshotCache(WorldGenerationSnapshotCache&&) = default;

  bool frozen() const { return bool(m_frozen); }
  Source const& source() const { return m_source; }
  template <typename Sizer>
  std::shared_ptr<WorldGenerationImmutableArray<T> const> freeze(Sizer sizer) {
    if (!m_frozen) {
      auto frozen = makeWorldGenerationImmutableArray<T>(m_ledger, m_source, sizer);
      m_frozen = std::move(frozen);
      // Destroy the original physical storage before releasing its estimate.
      m_source = Source{};
      m_originalBacking.reset();
    }
    return m_frozen;
  }

private:
  template <typename Sizer>
  static std::shared_ptr<WorldGenerationMemoryReservation> reserveOriginal(
      WorldGenerationMemoryLedger& ledger, Source const& source, Sizer sizer) {
    auto estimate = sizer(source);
    estimate.storage.add(source.capacity(), sizeof(T));
    if (!estimate.storage.complete) throw std::bad_alloc();
    auto result = ledger.reserve(WorldGenerationMemoryClass::MicroDungeonDistributionEstimate, estimate.storage.bytes);
    if (!result) throw std::bad_alloc();
    return result;
  }

  std::shared_ptr<WorldGenerationMemoryReservation> m_originalBacking;
  Source m_source;
  WorldGenerationMemoryLedger m_ledger;
  std::shared_ptr<WorldGenerationImmutableArray<T> const> m_frozen;
};

}
