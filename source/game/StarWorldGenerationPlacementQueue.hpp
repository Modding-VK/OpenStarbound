#pragma once

#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <stdexcept>
#include <utility>

namespace Star {

using WorldGenerationPlacementId = uint64_t;

// Owner-thread registry. Workers only receive IDs and copied input values,
// never this queue or its promise-bearing entries. IDs are scoped to one
// WorldGenerator lifetime; the generation task validates the world epoch.
template <typename Placement>
class WorldGenerationPlacementQueue {
public:
  WorldGenerationPlacementId append(Placement placement) {
    if (m_sequence == std::numeric_limits<WorldGenerationPlacementId>::max())
      throw std::overflow_error("World generation placement IDs exhausted");
    auto id = m_sequence + 1;
    m_entries.emplace(id, std::move(placement));
    m_sequence = id;
    return id;
  }

  bool contains(WorldGenerationPlacementId id) const {
    return m_entries.find(id) != m_entries.end();
  }

  // Retire before fulfilling the promise so any later plan for this ID is a
  // no-op. A failed factory must leave the entry pending by not calling take.
  std::optional<Placement> take(WorldGenerationPlacementId id) {
    auto entry = m_entries.find(id);
    if (entry == m_entries.end())
      return {};
    std::optional<Placement> result(std::move(entry->second));
    m_entries.erase(entry);
    return result;
  }

  // Ascending IDs preserve the relative order of surviving insertions, just
  // as the old compacted list did, including candidate-sort tie breakers.
  std::map<WorldGenerationPlacementId, Placement> const& entries() const {
    return m_entries;
  }

private:
  WorldGenerationPlacementId m_sequence = 0;
  std::map<WorldGenerationPlacementId, Placement> m_entries;
};

}
