#pragma once

#include "StarWorldGenerationShadowCheckpoint.hpp"

#include <cstdint>
#include <utility>

namespace Star {

// IO-05 first slice: shadowed packet confirmation harness. Builds packages only
// from confirmed checkpoint bytes, enforces TileArrayUpdate-before-delta order,
// tracks interest→apply milestones, and keeps server/client clocks distinct.
// Packet ready on the server is not sector visibility on the client.
// No .world / SQLite / live network.
enum class WorldGenerationShadowPacketStatus : uint8_t {
  Ok = 0,
  Closed = 1,
  MissingConfirmed = 2,
  ProvisionalBlocked = 3,
  OrderViolation = 4,
  MissingInitial = 5,
  NotActive = 6,
  MissingPacket = 7,
  Busy = 8,
  ClockMismatch = 9,
  Error = 10
};

enum class WorldGenerationShadowPacketKind : uint8_t {
  TileArrayInitial = 0,
  SectorDelta = 1
};

enum class WorldGenerationShadowPacketPhase : uint8_t {
  Interest = 0,
  Prepared = 1,
  Published = 2,
  Enqueued = 3,
  Sent = 4,
  Received = 5,
  Applied = 6
};

// Physical sector format preserved by the first rework (32×32).
uint32_t constexpr WorldGenerationShadowPacketSectorExtent = 32;

struct WorldGenerationShadowPacketIdentity {
  Vec2U sector{};
  uint64_t contentVersion = 0;
  uint64_t serverClock = 0;
  String worldId;

  bool valid() const {
    return contentVersion != 0 && serverClock != 0 && !worldId.empty();
  }
};

struct WorldGenerationShadowPacketView {
  uint64_t packetId = 0;
  WorldGenerationShadowPacketKind kind = WorldGenerationShadowPacketKind::TileArrayInitial;
  WorldGenerationShadowPacketIdentity identity;
  WorldGenerationShadowPacketPhase phase = WorldGenerationShadowPacketPhase::Interest;
  uint64_t clientClock = 0;
  uint32_t payloadBytes = 0;
  bool sectorFormat32 = false;
};

struct WorldGenerationShadowPacketRecord {
  WorldGenerationShadowPacketView view;
  ByteArray payload;
};

struct WorldGenerationShadowSectorClientState {
  bool initialApplied = false;
  bool sectorActive = false;
  uint64_t appliedContentVersion = 0;
  uint64_t lastClientClock = 0;
};

class WorldGenerationShadowPacketHarness {
public:
  explicit WorldGenerationShadowPacketHarness(String worldId)
    : m_checkpoint(std::move(worldId)) {}

  WorldGenerationShadowCheckpointHarness const& checkpoint() const { return m_checkpoint; }
  WorldGenerationShadowCheckpointHarness& checkpoint() { return m_checkpoint; }
  WorldGenerationShadowDbService const& db() const { return m_checkpoint.db(); }
  String const& worldId() const { return db().worldId(); }
  uint64_t serverClock() const { return m_serverClock; }

  Maybe<WorldGenerationShadowPacketView> packet(uint64_t packetId) const {
    if (auto const* entry = m_packets.ptr(packetId))
      return entry->view;
    return {};
  }

  Maybe<WorldGenerationShadowSectorClientState> clientSector(Vec2U sector) const {
    if (auto const* entry = m_clientSectors.ptr(sector))
      return *entry;
    return {};
  }

  bool sectorActive(Vec2U sector) const {
    if (auto const* entry = m_clientSectors.ptr(sector))
      return entry->sectorActive;
    return false;
  }

  // Seed confirmed checkpoint products used as the only legal packet source.
  WorldGenerationShadowPacketStatus confirmProduct(WorldGenerationShadowDbKey const& key,
      uint64_t contentVersion, ByteArray bytes) {
    if (!db().open())
      return WorldGenerationShadowPacketStatus::Closed;
    if (!key.valid() || contentVersion == 0 || bytes.empty())
      return WorldGenerationShadowPacketStatus::Error;
    List<WorldGenerationShadowDbKey> required = {key};
    uint64_t nextStep = m_checkpoint.confirmedStepEpoch() + 1;
    auto status = m_checkpoint.beginIntention(nextStep, required);
    if (status != WorldGenerationShadowCheckpointStatus::Ok)
      return WorldGenerationShadowPacketStatus::Error;
    status = m_checkpoint.put(key, contentVersion, std::move(bytes));
    if (status != WorldGenerationShadowCheckpointStatus::Ok) {
      m_checkpoint.reject();
      return WorldGenerationShadowPacketStatus::Error;
    }
    status = m_checkpoint.seal();
    if (status != WorldGenerationShadowCheckpointStatus::Ok) {
      m_checkpoint.reject();
      return WorldGenerationShadowPacketStatus::Error;
    }
    status = m_checkpoint.commitCheckpoint();
    if (status != WorldGenerationShadowCheckpointStatus::Ok)
      return WorldGenerationShadowPacketStatus::Error;
    return WorldGenerationShadowPacketStatus::Ok;
  }

  // Refuse building an "initial" packet from provisional (non-checkpoint) bytes.
  WorldGenerationShadowPacketStatus tryBuildFromProvisional(Vec2U sector, ByteArray provisional) {
    if (provisional.empty())
      return WorldGenerationShadowPacketStatus::Error;
    // Explicit gate: provisional payloads may never become the initial package.
    return WorldGenerationShadowPacketStatus::ProvisionalBlocked;
  }

  WorldGenerationShadowPacketStatus markInterest(Vec2U sector) {
    auto& client = m_clientSectors[sector];
    client.lastClientClock = 0;
    // Interest does not activate the sector.
    client.sectorActive = client.initialApplied;
    return WorldGenerationShadowPacketStatus::Ok;
  }

  // Build TileArrayInitial only from confirmed checkpoint tiles for the sector.
  WorldGenerationShadowPacketStatus tryBuildInitial(Vec2U sector, uint64_t& outPacketId) {
    if (!db().open())
      return WorldGenerationShadowPacketStatus::Closed;
    WorldGenerationShadowDbKey tilesKey{sector, String("Tiles")};
    auto read = db().readCheckpoint(tilesKey);
    if (read.status != WorldGenerationShadowDbStatus::Ok || !read.entry)
      return WorldGenerationShadowPacketStatus::MissingConfirmed;
    if (read.entry->bytes.empty())
      return WorldGenerationShadowPacketStatus::MissingConfirmed;

    WorldGenerationShadowPacketRecord record;
    record.view.packetId = ++m_packetSequence;
    record.view.kind = WorldGenerationShadowPacketKind::TileArrayInitial;
    record.view.identity.sector = sector;
    record.view.identity.contentVersion = read.entry->contentVersion;
    record.view.identity.serverClock = ++m_serverClock;
    record.view.identity.worldId = worldId();
    record.view.phase = WorldGenerationShadowPacketPhase::Prepared;
    record.view.sectorFormat32 = true;
    record.payload = ByteArray(read.entry->bytes.ptr(), read.entry->bytes.size());
    record.view.payloadBytes = (uint32_t)record.payload.size();
    // Prefix records physical extent for the harness (no live encode).
    // Payload body remains the confirmed tile bytes.
    outPacketId = record.view.packetId;
    m_packets[outPacketId] = std::move(record);
    return WorldGenerationShadowPacketStatus::Ok;
  }

  // Delta requires a prior applied initial for that sector (protocol order).
  WorldGenerationShadowPacketStatus tryBuildDelta(Vec2U sector, String productKind,
      uint64_t& outPacketId) {
    if (!db().open())
      return WorldGenerationShadowPacketStatus::Closed;
    if (productKind.empty() || productKind == "Tiles")
      return WorldGenerationShadowPacketStatus::Error;
    auto const* client = m_clientSectors.ptr(sector);
    if (!client || !client->initialApplied)
      return WorldGenerationShadowPacketStatus::MissingInitial;

    WorldGenerationShadowDbKey key{sector, productKind};
    auto read = db().readCheckpoint(key);
    if (read.status != WorldGenerationShadowDbStatus::Ok || !read.entry)
      return WorldGenerationShadowPacketStatus::MissingConfirmed;

    WorldGenerationShadowPacketRecord record;
    record.view.packetId = ++m_packetSequence;
    record.view.kind = WorldGenerationShadowPacketKind::SectorDelta;
    record.view.identity.sector = sector;
    record.view.identity.contentVersion = read.entry->contentVersion;
    record.view.identity.serverClock = ++m_serverClock;
    record.view.identity.worldId = worldId();
    record.view.phase = WorldGenerationShadowPacketPhase::Prepared;
    record.view.sectorFormat32 = true;
    record.payload = ByteArray(read.entry->bytes.ptr(), read.entry->bytes.size());
    record.view.payloadBytes = (uint32_t)record.payload.size();
    outPacketId = record.view.packetId;
    m_packets[outPacketId] = std::move(record);
    return WorldGenerationShadowPacketStatus::Ok;
  }

  WorldGenerationShadowPacketStatus advancePhase(uint64_t packetId,
      WorldGenerationShadowPacketPhase next) {
    auto* record = m_packets.ptr(packetId);
    if (!record)
      return WorldGenerationShadowPacketStatus::MissingPacket;
    auto current = record->view.phase;
    if ((uint8_t)next != (uint8_t)current + 1)
      return WorldGenerationShadowPacketStatus::OrderViolation;
    if (next == WorldGenerationShadowPacketPhase::Published) {
      // Published milestone means confirmed identity is locked for enqueue.
      if (!record->view.identity.valid())
        return WorldGenerationShadowPacketStatus::Error;
    }
    if (next == WorldGenerationShadowPacketPhase::Received) {
      // Client clock is independent; never derived by subtracting server clocks.
      record->view.clientClock = ++m_clientClockSequence;
    }
    if (next == WorldGenerationShadowPacketPhase::Applied) {
      if (record->view.phase != WorldGenerationShadowPacketPhase::Received)
        return WorldGenerationShadowPacketStatus::OrderViolation;
      auto& client = m_clientSectors[record->view.identity.sector];
      if (record->view.kind == WorldGenerationShadowPacketKind::TileArrayInitial) {
        client.initialApplied = true;
        client.sectorActive = true;
        client.appliedContentVersion = record->view.identity.contentVersion;
      } else if (!client.initialApplied) {
        return WorldGenerationShadowPacketStatus::MissingInitial;
      } else {
        client.appliedContentVersion = record->view.identity.contentVersion;
      }
      client.lastClientClock = record->view.clientClock;
    }
    record->view.phase = next;
    return WorldGenerationShadowPacketStatus::Ok;
  }

  // Convenience: enqueue→send on server side. Does not imply client visibility.
  WorldGenerationShadowPacketStatus enqueueAndSend(uint64_t packetId) {
    auto status = advancePhase(packetId, WorldGenerationShadowPacketPhase::Published);
    if (status != WorldGenerationShadowPacketStatus::Ok)
      return status;
    status = advancePhase(packetId, WorldGenerationShadowPacketPhase::Enqueued);
    if (status != WorldGenerationShadowPacketStatus::Ok)
      return status;
    return advancePhase(packetId, WorldGenerationShadowPacketPhase::Sent);
  }

  WorldGenerationShadowPacketStatus receiveAndApply(uint64_t packetId) {
    auto status = advancePhase(packetId, WorldGenerationShadowPacketPhase::Received);
    if (status != WorldGenerationShadowPacketStatus::Ok)
      return status;
    return advancePhase(packetId, WorldGenerationShadowPacketPhase::Applied);
  }

  // Distinct process clocks must not be subtracted as a latency metric.
  WorldGenerationShadowPacketStatus compareClocks(uint64_t packetId, int64_t& /*outDelta*/) const {
    auto const* record = m_packets.ptr(packetId);
    if (!record)
      return WorldGenerationShadowPacketStatus::MissingPacket;
    if (record->view.clientClock == 0 || record->view.identity.serverClock == 0)
      return WorldGenerationShadowPacketStatus::ClockMismatch;
    // Reject naive subtraction across unsynchronized process clocks.
    return WorldGenerationShadowPacketStatus::ClockMismatch;
  }

private:
  WorldGenerationShadowCheckpointHarness m_checkpoint;
  HashMap<uint64_t, WorldGenerationShadowPacketRecord> m_packets;
  HashMap<Vec2U, WorldGenerationShadowSectorClientState> m_clientSectors;
  uint64_t m_packetSequence = 0;
  uint64_t m_serverClock = 0;
  uint64_t m_clientClockSequence = 0;
};

}
