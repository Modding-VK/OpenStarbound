#pragma once

#include "StarWorldGenerationPublicationPass.hpp"
#include "StarWorldGenerationShadowPacket.hpp"

#include <cstdint>
#include <utility>

namespace Star {

// H3 cross-shadow integration (post IO-02..05 first slices): connect a confirmed
// WT-04 publication frontier into IO-03 checkpoint and IO-05 packets without
// touching the live world / network / .world.
enum class WorldGenerationShadowIntegrationStatus : uint8_t {
  Ok = 0,
  Closed = 1,
  FrontierMissing = 2,
  FrontierNotConfirmed = 3,
  CheckpointFailed = 4,
  PacketFailed = 5,
  Busy = 6,
  NotCommitted = 7,
  Error = 8
};

struct WorldGenerationShadowIntegrationFrontier {
  Vec2U sector{};
  uint64_t structureEpoch = 0;
  uint64_t bindingSeed = 0;
  uint32_t dirtyCellCount = 0;
  uint64_t tilesContentVersion = 0;
  ByteArray tilesBytes;
  bool confirmed = false;

  bool valid() const {
    return confirmed && structureEpoch != 0 && bindingSeed != 0 && tilesContentVersion != 0
        && !tilesBytes.empty();
  }
};

class WorldGenerationShadowIntegrationHarness {
public:
  explicit WorldGenerationShadowIntegrationHarness(String worldId)
    : m_packets(std::move(worldId)) {}

  WorldGenerationShadowPacketHarness const& packets() const { return m_packets; }
  WorldGenerationShadowPacketHarness& packets() { return m_packets; }
  WorldGenerationShadowCheckpointHarness const& checkpoint() const {
    return m_packets.checkpoint();
  }
  WorldGenerationShadowDbService const& db() const { return m_packets.db(); }
  bool unitCommitted() const { return m_unitCommitted; }
  uint64_t lastInitialPacketId() const { return m_lastInitialPacketId; }

  // Capture a confirmed WT-04 pass frontier into the integration unit.
  WorldGenerationShadowIntegrationStatus ingestPublicationFrontier(
      WorldGenerationPublicationPass const& pass, Vec2U sector, uint64_t tilesContentVersion,
      ByteArray tilesBytes) {
    if (!pass.frontierConfirmed() || !pass.staging().committed())
      return WorldGenerationShadowIntegrationStatus::FrontierNotConfirmed;
    if (tilesContentVersion == 0 || tilesBytes.empty())
      return WorldGenerationShadowIntegrationStatus::Error;
    if (m_frontier.confirmed && !m_unitCommitted)
      return WorldGenerationShadowIntegrationStatus::Busy;

    auto const& identity = pass.staging().identity();
    WorldGenerationShadowIntegrationFrontier frontier;
    frontier.sector = sector;
    frontier.structureEpoch = identity.structureEpoch;
    frontier.bindingSeed = identity.bindingIdentity.bindingSeed;
    frontier.dirtyCellCount = pass.stats().deferredDirtyCells;
    frontier.tilesContentVersion = tilesContentVersion;
    frontier.tilesBytes = std::move(tilesBytes);
    frontier.confirmed = true;
    m_frontier = std::move(frontier);
    m_unitCommitted = false;
    m_lastInitialPacketId = 0;
    return WorldGenerationShadowIntegrationStatus::Ok;
  }

  // Alternate intake when the caller already holds frontier metadata.
  WorldGenerationShadowIntegrationStatus ingestConfirmedFrontier(
      WorldGenerationShadowIntegrationFrontier frontier) {
    if (!frontier.valid())
      return WorldGenerationShadowIntegrationStatus::FrontierNotConfirmed;
    if (m_frontier.confirmed && !m_unitCommitted)
      return WorldGenerationShadowIntegrationStatus::Busy;
    m_frontier = std::move(frontier);
    m_unitCommitted = false;
    m_lastInitialPacketId = 0;
    return WorldGenerationShadowIntegrationStatus::Ok;
  }

  // Commit Tiles + Frontier blob as one IO-03 logical unit.
  WorldGenerationShadowIntegrationStatus commitCheckpointUnit() {
    if (!m_frontier.valid())
      return WorldGenerationShadowIntegrationStatus::FrontierMissing;
    if (m_unitCommitted)
      return WorldGenerationShadowIntegrationStatus::Ok;

    WorldGenerationShadowDbKey tilesKey{m_frontier.sector, String("Tiles")};
    WorldGenerationShadowDbKey frontierKey{m_frontier.sector, String("Frontier")};
    auto frontierBytes = WorldGenerationShadowDbService::encodeFrontierBlob(
        m_frontier.structureEpoch, m_frontier.bindingSeed, m_frontier.dirtyCellCount);

    auto& cp = m_packets.checkpoint();
    uint64_t nextStep = cp.confirmedStepEpoch() + 1;
    List<WorldGenerationShadowDbKey> required = {tilesKey, frontierKey};
    auto status = cp.beginIntention(nextStep, required);
    if (status != WorldGenerationShadowCheckpointStatus::Ok)
      return WorldGenerationShadowIntegrationStatus::CheckpointFailed;
    status = cp.put(tilesKey, m_frontier.tilesContentVersion,
        ByteArray(m_frontier.tilesBytes.ptr(), m_frontier.tilesBytes.size()));
    if (status != WorldGenerationShadowCheckpointStatus::Ok) {
      cp.reject();
      return WorldGenerationShadowIntegrationStatus::CheckpointFailed;
    }
    status = cp.put(frontierKey, m_frontier.tilesContentVersion, std::move(frontierBytes));
    if (status != WorldGenerationShadowCheckpointStatus::Ok) {
      cp.reject();
      return WorldGenerationShadowIntegrationStatus::CheckpointFailed;
    }
    status = cp.seal();
    if (status != WorldGenerationShadowCheckpointStatus::Ok) {
      cp.reject();
      return WorldGenerationShadowIntegrationStatus::CheckpointFailed;
    }
    status = cp.commitCheckpoint();
    if (status != WorldGenerationShadowCheckpointStatus::Ok)
      return WorldGenerationShadowIntegrationStatus::CheckpointFailed;

    m_unitCommitted = true;
    return WorldGenerationShadowIntegrationStatus::Ok;
  }

  // Build/send/apply TileArrayInitial from confirmed checkpoint only.
  WorldGenerationShadowIntegrationStatus deliverInitialPacket() {
    if (!m_unitCommitted)
      return WorldGenerationShadowIntegrationStatus::NotCommitted;
    uint64_t packetId = 0;
    auto status = m_packets.tryBuildInitial(m_frontier.sector, packetId);
    if (status != WorldGenerationShadowPacketStatus::Ok)
      return WorldGenerationShadowIntegrationStatus::PacketFailed;
    status = m_packets.enqueueAndSend(packetId);
    if (status != WorldGenerationShadowPacketStatus::Ok)
      return WorldGenerationShadowIntegrationStatus::PacketFailed;
    // Server Sent is not client visibility — apply is a separate milestone.
    if (m_packets.sectorActive(m_frontier.sector))
      return WorldGenerationShadowIntegrationStatus::Error;
    status = m_packets.receiveAndApply(packetId);
    if (status != WorldGenerationShadowPacketStatus::Ok)
      return WorldGenerationShadowIntegrationStatus::PacketFailed;
    if (!m_packets.sectorActive(m_frontier.sector))
      return WorldGenerationShadowIntegrationStatus::PacketFailed;
    m_lastInitialPacketId = packetId;
    return WorldGenerationShadowIntegrationStatus::Ok;
  }

  WorldGenerationShadowIntegrationStatus refuseUnconfirmed(
      WorldGenerationPublicationPass const& pass) const {
    if (pass.frontierConfirmed())
      return WorldGenerationShadowIntegrationStatus::Error;
    return WorldGenerationShadowIntegrationStatus::FrontierNotConfirmed;
  }

private:
  WorldGenerationShadowPacketHarness m_packets;
  WorldGenerationShadowIntegrationFrontier m_frontier;
  bool m_unitCommitted = false;
  uint64_t m_lastInitialPacketId = 0;
};

}
