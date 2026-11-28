#pragma once

#include "StarByteArray.hpp"
#include "StarHash.hpp"
#include "StarMap.hpp"
#include "StarString.hpp"
#include "StarVector.hpp"

#include <cstdint>
#include <cstring>
#include <utility>

namespace Star {

// IO-02 first slice: in-memory shadow DB service harness. Serializes ops for
// one logical world, separates content versions from call tickets, and exposes
// checkpoint-only reads that never see uncommitted writes. No SQLite/.world.
enum class WorldGenerationShadowDbStatus : uint8_t {
  Ok = 0,
  Closed = 1,
  NoTransaction = 2,
  BusyTransaction = 3,
  StaleContent = 4,
  MissingKey = 5,
  Conflict = 6,
  Error = 7
};

// Logical product identity at a sector. contentVersion travels with the write
// payload / entry, not as part of the map key, so readers can observe the
// latest committed generation for that product.
struct WorldGenerationShadowDbKey {
  Vec2U sector{};
  String productKind;

  bool valid() const { return !productKind.empty(); }

  bool operator==(WorldGenerationShadowDbKey const& other) const {
    return sector == other.sector && productKind == other.productKind;
  }
};

template <>
struct hash<WorldGenerationShadowDbKey> {
  size_t operator()(WorldGenerationShadowDbKey const& key) const {
    size_t h = hash<Vec2U>()(key.sector);
    hashCombine(h, hash<String>()(key.productKind));
    return h;
  }
};

struct WorldGenerationShadowDbEntry {
  ByteArray bytes;
  uint64_t contentVersion = 0;
  uint64_t writeTicket = 0;
};

struct WorldGenerationShadowDbReadResult {
  WorldGenerationShadowDbStatus status = WorldGenerationShadowDbStatus::MissingKey;
  Maybe<WorldGenerationShadowDbEntry> entry;
};

class WorldGenerationShadowDbService {
public:
  explicit WorldGenerationShadowDbService(String worldId)
    : m_worldId(std::move(worldId)) {}

  String const& worldId() const { return m_worldId; }
  bool open() const { return m_open; }
  uint64_t checkpointEpoch() const { return m_checkpointEpoch; }
  uint64_t opSequence() const { return m_opSequence; }
  bool hasOpenTransaction() const { return m_txnActive; }

  WorldGenerationShadowDbStatus close() {
    if (!m_open)
      return WorldGenerationShadowDbStatus::Closed;
    if (m_txnActive)
      rollback();
    m_open = false;
    m_checkpoint.clear();
    return WorldGenerationShadowDbStatus::Ok;
  }

  // Independent readers only observe the last committed checkpoint.
  WorldGenerationShadowDbReadResult readCheckpoint(WorldGenerationShadowDbKey const& key) const {
    WorldGenerationShadowDbReadResult out;
    if (!m_open) {
      out.status = WorldGenerationShadowDbStatus::Closed;
      return out;
    }
    if (!key.valid()) {
      out.status = WorldGenerationShadowDbStatus::Error;
      return out;
    }
    if (auto const* entry = m_checkpoint.ptr(key)) {
      out.status = WorldGenerationShadowDbStatus::Ok;
      out.entry = *entry;
      return out;
    }
    out.status = WorldGenerationShadowDbStatus::MissingKey;
    return out;
  }

  WorldGenerationShadowDbStatus beginWrite(uint64_t writeTicket) {
    if (!m_open)
      return WorldGenerationShadowDbStatus::Closed;
    if (m_txnActive)
      return WorldGenerationShadowDbStatus::BusyTransaction;
    if (writeTicket == 0)
      return WorldGenerationShadowDbStatus::Error;
    m_txnActive = true;
    m_txnTicket = writeTicket;
    m_txnScratch.clear();
    ++m_opSequence;
    return WorldGenerationShadowDbStatus::Ok;
  }

  WorldGenerationShadowDbStatus put(WorldGenerationShadowDbKey const& key, uint64_t contentVersion,
      ByteArray bytes) {
    if (!m_open)
      return WorldGenerationShadowDbStatus::Closed;
    if (!m_txnActive)
      return WorldGenerationShadowDbStatus::NoTransaction;
    if (!key.valid() || contentVersion == 0 || bytes.empty())
      return WorldGenerationShadowDbStatus::Error;
    if (auto const* committed = m_checkpoint.ptr(key)) {
      if (contentVersion < committed->contentVersion)
        return WorldGenerationShadowDbStatus::StaleContent;
      if (contentVersion == committed->contentVersion && bytes != committed->bytes)
        return WorldGenerationShadowDbStatus::Conflict;
    }
    WorldGenerationShadowDbEntry entry;
    entry.bytes = std::move(bytes);
    entry.contentVersion = contentVersion;
    entry.writeTicket = m_txnTicket;
    m_txnScratch[key] = std::move(entry);
    ++m_opSequence;
    return WorldGenerationShadowDbStatus::Ok;
  }

  WorldGenerationShadowDbStatus commit() {
    if (!m_open)
      return WorldGenerationShadowDbStatus::Closed;
    if (!m_txnActive)
      return WorldGenerationShadowDbStatus::NoTransaction;
    for (auto& entry : m_txnScratch)
      m_checkpoint[entry.first] = std::move(entry.second);
    m_txnScratch.clear();
    m_txnActive = false;
    m_txnTicket = 0;
    ++m_checkpointEpoch;
    ++m_opSequence;
    return WorldGenerationShadowDbStatus::Ok;
  }

  WorldGenerationShadowDbStatus rollback() {
    if (!m_open)
      return WorldGenerationShadowDbStatus::Closed;
    if (!m_txnActive)
      return WorldGenerationShadowDbStatus::NoTransaction;
    m_txnScratch.clear();
    m_txnActive = false;
    m_txnTicket = 0;
    ++m_opSequence;
    return WorldGenerationShadowDbStatus::Ok;
  }

  // Durable frontier marker for the harness (no disk). Sync fails if a write
  // transaction is still open so callers cannot pretend uncommitted bytes are
  // durable.
  WorldGenerationShadowDbStatus sync() {
    if (!m_open)
      return WorldGenerationShadowDbStatus::Closed;
    if (m_txnActive)
      return WorldGenerationShadowDbStatus::BusyTransaction;
    ++m_opSequence;
    return WorldGenerationShadowDbStatus::Ok;
  }

  // Serialize a confirmed publication frontier (WT-04 deferred dirty metadata)
  // as product bytes. Still in-memory only.
  static ByteArray encodeFrontierBlob(uint64_t structureEpoch, uint64_t bindingSeed,
      uint32_t dirtyCellCount) {
    ByteArray out;
    out.append((char const*)&structureEpoch, sizeof(structureEpoch));
    out.append((char const*)&bindingSeed, sizeof(bindingSeed));
    out.append((char const*)&dirtyCellCount, sizeof(dirtyCellCount));
    return out;
  }

  static bool decodeFrontierBlob(ByteArray const& bytes, uint64_t& structureEpoch,
      uint64_t& bindingSeed, uint32_t& dirtyCellCount) {
    size_t need = sizeof(uint64_t) * 2 + sizeof(uint32_t);
    if (bytes.size() < need)
      return false;
    char const* data = bytes.ptr();
    memcpy(&structureEpoch, data, sizeof(structureEpoch));
    memcpy(&bindingSeed, data + sizeof(structureEpoch), sizeof(bindingSeed));
    memcpy(&dirtyCellCount, data + sizeof(structureEpoch) + sizeof(bindingSeed), sizeof(dirtyCellCount));
    return true;
  }

private:
  String m_worldId;
  bool m_open = true;
  bool m_txnActive = false;
  uint64_t m_txnTicket = 0;
  uint64_t m_checkpointEpoch = 0;
  uint64_t m_opSequence = 0;
  HashMap<WorldGenerationShadowDbKey, WorldGenerationShadowDbEntry> m_checkpoint;
  HashMap<WorldGenerationShadowDbKey, WorldGenerationShadowDbEntry> m_txnScratch;
};

}
