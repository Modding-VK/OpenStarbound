#pragma once

#include "StarHash.hpp"
#include "StarMap.hpp"
#include "StarVector.hpp"

#include <cstdint>

namespace Star {

// WT-03: admit a whole-region demand set before waiting on any producer.
// Pure bookkeeping — no Thread::yield loop, no live-world publish.
enum class WorldGenerationRegionDemandState : uint8_t {
  Planned = 0,
  Admitted = 1,
  Ready = 2,
  Failed = 3,
  Cancelled = 4
};

struct WorldGenerationRegionDemandKey {
  Vec2U sector{};
  uint64_t templateRevision = 0;

  bool operator==(WorldGenerationRegionDemandKey const& other) const {
    return sector == other.sector && templateRevision == other.templateRevision;
  }
};

template <>
struct hash<WorldGenerationRegionDemandKey> {
  size_t operator()(WorldGenerationRegionDemandKey const& key) const {
    size_t h = hash<Vec2U>()(key.sector);
    hashCombine(h, hash<uint64_t>()(key.templateRevision));
    return h;
  }
};

class WorldGenerationRegionDemandBatch {
public:
  explicit WorldGenerationRegionDemandBatch(Vec2U regionOrigin, Vec2U regionSize,
      uint64_t templateRevision)
    : m_regionOrigin(regionOrigin), m_regionSize(regionSize), m_templateRevision(templateRevision) {}

  Vec2U regionOrigin() const { return m_regionOrigin; }
  Vec2U regionSize() const { return m_regionSize; }
  uint64_t templateRevision() const { return m_templateRevision; }
  size_t size() const { return m_entries.size(); }

  // Plan every sector in the region rectangle before any wait.
  size_t planRegion() {
    size_t added = 0;
    for (unsigned y = 0; y < m_regionSize[1]; ++y) {
      for (unsigned x = 0; x < m_regionSize[0]; ++x) {
        WorldGenerationRegionDemandKey key{{m_regionOrigin[0] + x, m_regionOrigin[1] + y}, m_templateRevision};
        if (!m_entries.contains(key)) {
          m_entries.add(key, WorldGenerationRegionDemandState::Planned);
          ++added;
        }
      }
    }
    return added;
  }

  bool admit(Vec2U sector) {
    WorldGenerationRegionDemandKey key{sector, m_templateRevision};
    auto* state = m_entries.ptr(key);
    if (!state || *state != WorldGenerationRegionDemandState::Planned)
      return false;
    *state = WorldGenerationRegionDemandState::Admitted;
    return true;
  }

  size_t admitAllPlanned() {
    size_t count = 0;
    for (auto& pair : m_entries) {
      if (pair.second == WorldGenerationRegionDemandState::Planned) {
        pair.second = WorldGenerationRegionDemandState::Admitted;
        ++count;
      }
    }
    return count;
  }

  bool markReady(Vec2U sector) {
    return transition(sector, WorldGenerationRegionDemandState::Admitted, WorldGenerationRegionDemandState::Ready);
  }

  bool markFailed(Vec2U sector) {
    auto* state = m_entries.ptr({sector, m_templateRevision});
    if (!state)
      return false;
    if (*state == WorldGenerationRegionDemandState::Ready || *state == WorldGenerationRegionDemandState::Cancelled)
      return false;
    *state = WorldGenerationRegionDemandState::Failed;
    return true;
  }

  bool cancel(Vec2U sector) {
    auto* state = m_entries.ptr({sector, m_templateRevision});
    if (!state || *state == WorldGenerationRegionDemandState::Ready)
      return false;
    *state = WorldGenerationRegionDemandState::Cancelled;
    return true;
  }

  WorldGenerationRegionDemandState state(Vec2U sector) const {
    if (auto* s = m_entries.ptr({sector, m_templateRevision}))
      return *s;
    return WorldGenerationRegionDemandState::Cancelled;
  }

  bool allTerminal() const {
    if (m_entries.empty())
      return false;
    for (auto const& pair : m_entries) {
      if (pair.second == WorldGenerationRegionDemandState::Planned
          || pair.second == WorldGenerationRegionDemandState::Admitted)
        return false;
    }
    return true;
  }

  bool allReady() const {
    if (m_entries.empty())
      return false;
    for (auto const& pair : m_entries) {
      if (pair.second != WorldGenerationRegionDemandState::Ready)
        return false;
    }
    return true;
  }

  size_t count(WorldGenerationRegionDemandState wanted) const {
    size_t n = 0;
    for (auto const& pair : m_entries) {
      if (pair.second == wanted)
        ++n;
    }
    return n;
  }

private:
  bool transition(Vec2U sector, WorldGenerationRegionDemandState from, WorldGenerationRegionDemandState to) {
    auto* state = m_entries.ptr({sector, m_templateRevision});
    if (!state || *state != from)
      return false;
    *state = to;
    return true;
  }

  Vec2U m_regionOrigin{};
  Vec2U m_regionSize{};
  uint64_t m_templateRevision = 0;
  HashMap<WorldGenerationRegionDemandKey, WorldGenerationRegionDemandState> m_entries;
};

}
