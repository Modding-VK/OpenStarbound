#pragma once

#include "StarList.hpp"
#include "StarSet.hpp"
#include "StarWorldGenerationShadowDb.hpp"

#include <cstdint>
#include <utility>

namespace Star {

// IO-03 first slice: coherent checkpoint harness over the shadow DB. Confirms
// only complete logical units, journals sealed intentions for idempotent
// replay, and rejects partial/incompatible steps. No .world / SQLite.
enum class WorldGenerationShadowCheckpointStatus : uint8_t {
  Ok = 0,
  Closed = 1,
  Busy = 2,
  IncompleteUnit = 3,
  IncompatibleStep = 4,
  NotSealed = 5,
  MissingIntention = 6,
  DuplicateKey = 7,
  Rejected = 8,
  Recovered = 9,
  Error = 10
};

struct WorldGenerationShadowCheckpointPut {
  WorldGenerationShadowDbKey key;
  uint64_t contentVersion = 0;
  ByteArray bytes;
};

struct WorldGenerationShadowCheckpointIntention {
  uint64_t intentionId = 0;
  uint64_t stepEpoch = 0;
  List<WorldGenerationShadowDbKey> requiredKeys;
  HashMap<WorldGenerationShadowDbKey, WorldGenerationShadowCheckpointPut> pending;
  bool sealed = false;

  bool complete() const {
    if (requiredKeys.empty())
      return false;
    for (auto const& key : requiredKeys) {
      if (!pending.contains(key))
        return false;
    }
    return true;
  }
};

class WorldGenerationShadowCheckpointHarness {
public:
  explicit WorldGenerationShadowCheckpointHarness(String worldId)
    : m_db(std::move(worldId)) {}

  WorldGenerationShadowDbService const& db() const { return m_db; }
  WorldGenerationShadowDbService& db() { return m_db; }
  uint64_t confirmedStepEpoch() const { return m_confirmedStepEpoch; }
  bool hasIntention() const { return (bool)m_intention; }
  Maybe<WorldGenerationShadowCheckpointIntention> const& intention() const {
    return m_intention;
  }

  WorldGenerationShadowCheckpointStatus beginIntention(uint64_t stepEpoch,
      List<WorldGenerationShadowDbKey> requiredKeys) {
    if (!m_db.open())
      return WorldGenerationShadowCheckpointStatus::Closed;
    if (m_intention)
      return WorldGenerationShadowCheckpointStatus::Busy;
    if (requiredKeys.empty() || stepEpoch == 0)
      return WorldGenerationShadowCheckpointStatus::Error;
    // Only the next step may open; recovering a sealed intention uses recover().
    if (stepEpoch != m_confirmedStepEpoch + 1)
      return WorldGenerationShadowCheckpointStatus::IncompatibleStep;
    HashSet<WorldGenerationShadowDbKey> unique;
    for (auto const& key : requiredKeys) {
      if (!key.valid() || !unique.add(key))
        return WorldGenerationShadowCheckpointStatus::DuplicateKey;
    }
    WorldGenerationShadowCheckpointIntention intention;
    intention.intentionId = ++m_intentionSequence;
    intention.stepEpoch = stepEpoch;
    intention.requiredKeys = std::move(requiredKeys);
    m_intention = std::move(intention);
    return WorldGenerationShadowCheckpointStatus::Ok;
  }

  WorldGenerationShadowCheckpointStatus put(WorldGenerationShadowDbKey const& key,
      uint64_t contentVersion, ByteArray bytes) {
    if (!m_db.open())
      return WorldGenerationShadowCheckpointStatus::Closed;
    if (!m_intention)
      return WorldGenerationShadowCheckpointStatus::MissingIntention;
    if (m_intention->sealed)
      return WorldGenerationShadowCheckpointStatus::Busy;
    bool required = false;
    for (auto const& requiredKey : m_intention->requiredKeys) {
      if (requiredKey == key) {
        required = true;
        break;
      }
    }
    if (!required)
      return WorldGenerationShadowCheckpointStatus::Error;
    if (contentVersion == 0 || bytes.empty())
      return WorldGenerationShadowCheckpointStatus::Error;
    m_intention->pending[key] = {key, contentVersion, std::move(bytes)};
    return WorldGenerationShadowCheckpointStatus::Ok;
  }

  // Seal only when every required key is present. Partial units cannot advance.
  WorldGenerationShadowCheckpointStatus seal() {
    if (!m_db.open())
      return WorldGenerationShadowCheckpointStatus::Closed;
    if (!m_intention)
      return WorldGenerationShadowCheckpointStatus::MissingIntention;
    if (m_intention->sealed)
      return WorldGenerationShadowCheckpointStatus::Ok;
    if (!m_intention->complete())
      return WorldGenerationShadowCheckpointStatus::IncompleteUnit;
    m_intention->sealed = true;
    return WorldGenerationShadowCheckpointStatus::Ok;
  }

  // Persist the sealed unit into the shadow DB checkpoint. Durability marker
  // (sync) is separate from visibility (commit).
  WorldGenerationShadowCheckpointStatus commitCheckpoint() {
    if (!m_db.open())
      return WorldGenerationShadowCheckpointStatus::Closed;
    if (!m_intention)
      return WorldGenerationShadowCheckpointStatus::MissingIntention;
    if (!m_intention->sealed)
      return WorldGenerationShadowCheckpointStatus::NotSealed;
    if (!m_intention->complete())
      return WorldGenerationShadowCheckpointStatus::IncompleteUnit;

    auto status = m_db.beginWrite(m_intention->intentionId);
    if (status != WorldGenerationShadowDbStatus::Ok)
      return mapDbStatus(status);
    for (auto const& requiredKey : m_intention->requiredKeys) {
      auto const& put = m_intention->pending.get(requiredKey);
      status = m_db.put(put.key, put.contentVersion, ByteArray(put.bytes));
      if (status != WorldGenerationShadowDbStatus::Ok) {
        m_db.rollback();
        return mapDbStatus(status);
      }
    }
    status = m_db.commit();
    if (status != WorldGenerationShadowDbStatus::Ok)
      return mapDbStatus(status);
    status = m_db.sync();
    if (status != WorldGenerationShadowDbStatus::Ok)
      return mapDbStatus(status);

    m_confirmedStepEpoch = m_intention->stepEpoch;
    m_intention.reset();
    return WorldGenerationShadowCheckpointStatus::Ok;
  }

  // Simulate crash after seal but before durable commit: intention remains.
  WorldGenerationShadowCheckpointStatus simulateCrashAfterSeal() {
    if (!m_intention || !m_intention->sealed)
      return WorldGenerationShadowCheckpointStatus::NotSealed;
    if (m_db.hasOpenTransaction())
      m_db.rollback();
    // Intention journal retained for recover(); checkpoint unchanged.
    return WorldGenerationShadowCheckpointStatus::Ok;
  }

  // Idempotent replay of a sealed intention, or reject an unsealed partial unit.
  WorldGenerationShadowCheckpointStatus recover() {
    if (!m_db.open())
      return WorldGenerationShadowCheckpointStatus::Closed;
    if (!m_intention)
      return WorldGenerationShadowCheckpointStatus::MissingIntention;
    if (!m_intention->sealed) {
      m_intention.reset();
      return WorldGenerationShadowCheckpointStatus::Rejected;
    }
    auto status = commitCheckpoint();
    if (status == WorldGenerationShadowCheckpointStatus::Ok)
      return WorldGenerationShadowCheckpointStatus::Recovered;
    return status;
  }

  WorldGenerationShadowCheckpointStatus reject() {
    if (!m_intention)
      return WorldGenerationShadowCheckpointStatus::MissingIntention;
    if (m_db.hasOpenTransaction())
      m_db.rollback();
    m_intention.reset();
    return WorldGenerationShadowCheckpointStatus::Rejected;
  }

private:
  static WorldGenerationShadowCheckpointStatus mapDbStatus(WorldGenerationShadowDbStatus status) {
    switch (status) {
      case WorldGenerationShadowDbStatus::Ok:
        return WorldGenerationShadowCheckpointStatus::Ok;
      case WorldGenerationShadowDbStatus::Closed:
        return WorldGenerationShadowCheckpointStatus::Closed;
      case WorldGenerationShadowDbStatus::BusyTransaction:
        return WorldGenerationShadowCheckpointStatus::Busy;
      case WorldGenerationShadowDbStatus::StaleContent:
      case WorldGenerationShadowDbStatus::Conflict:
        return WorldGenerationShadowCheckpointStatus::Error;
      default:
        return WorldGenerationShadowCheckpointStatus::Error;
    }
  }

  WorldGenerationShadowDbService m_db;
  Maybe<WorldGenerationShadowCheckpointIntention> m_intention;
  uint64_t m_confirmedStepEpoch = 0;
  uint64_t m_intentionSequence = 0;
};

}
