#pragma once

#include "StarWorldGenerationBufferAllocator.hpp"
#include "StarWorldGenerationMemoryEstimate.hpp"

#include <unordered_map>

namespace Star {

// One immutable string per distinct output name in a planning invocation.
// This is not an asset cache: no definition, world, or process-global index.
class WorldGenerationRetainedName {
public:
  WorldGenerationRetainedName(std::shared_ptr<WorldGenerationMemoryReservation> memory, String const& value)
    : m_memory(std::move(memory)), m_value(value) {
    WorldGenerationSizeEstimate estimate;
    estimate.stringCapacity(m_value);
    m_complete = estimate.complete && m_memory->resizeEstimate(estimate.bytes);
    if (!m_complete) m_memory->noteIncompleteEstimate();
  }
  WorldGenerationRetainedName(WorldGenerationRetainedName const&) = delete;
  WorldGenerationRetainedName& operator=(WorldGenerationRetainedName const&) = delete;

  String const& value() const { return m_value; }
  bool estimateComplete() const { return m_complete; }

private:
  // Release the String before its backing estimate.
  std::shared_ptr<WorldGenerationMemoryReservation> m_memory;
  String m_value;
  bool m_complete = false;
};

using WorldGenerationName = std::shared_ptr<WorldGenerationRetainedName const>;

class WorldGenerationNamePool {
  struct Hash {
    size_t operator()(String const* value) const { return hash<String>{}(*value); }
  };
  struct Equal {
    bool operator()(String const* a, String const* b) const { return *a == *b; }
  };
  using Entry = std::pair<String const* const, WorldGenerationName>;
  using Allocator = WorldGenerationBufferAllocator<Entry>;
  using Index = std::unordered_map<String const*, WorldGenerationName, Hash, Equal, Allocator>;

public:
  explicit WorldGenerationNamePool(WorldGenerationMemoryLedger& ledger)
    : m_ledger(ledger), m_names(0, Hash{}, Equal{},
        Allocator(ledger, WorldGenerationMemoryClass::MicroDungeonNameBuffer)) {}
  WorldGenerationNamePool(WorldGenerationNamePool const&) = delete;
  WorldGenerationNamePool& operator=(WorldGenerationNamePool const&) = delete;

  WorldGenerationName intern(String const& value) {
    auto found = m_names.find(&value);
    if (found != m_names.end()) return found->second;
    WorldGenerationSizeEstimate estimate;
    // The copy's capacity is implementation-dependent: reconcile afterwards.
    // This is a pre-copy estimate, not a hard bound on string allocation.
    estimate.add(value.utf8Size());
    estimate.add(1);
    if (!estimate.complete) throw std::bad_alloc();
    auto memory = m_ledger.reserve(WorldGenerationMemoryClass::MicroDungeonNameEstimate, estimate.bytes);
    if (!memory) throw std::bad_alloc();
    auto name = std::allocate_shared<WorldGenerationRetainedName>(
        WorldGenerationBufferAllocator<WorldGenerationRetainedName>(m_ledger,
            WorldGenerationMemoryClass::MicroDungeonNameBuffer), std::move(memory), value);
    // Keys point into immutable owned entries, never into the caller's string.
    // A failed insertion releases the new name; existing entries stay valid.
    m_names.emplace(&name->value(), name);
    return name;
  }

  size_t size() const { return m_names.size(); }

private:
  WorldGenerationMemoryLedger m_ledger;
  Index m_names;
};

}
