#pragma once

#include "StarWorldGenerationShadowCheckpoint.hpp"

#include <cstdint>
#include <utility>

namespace Star {

// IO-04 first slice: shadowed unload/drain harness. Revokes publish authority,
// preserves reservations, drains or discards producer conclusions, blocks
// partial handoff to a central owner without a valid checkpoint, and frees
// late private buffers only after drain. No .world / SQLite / live WorldThread.
enum class WorldGenerationShadowUnloadStatus : uint8_t {
  Ok = 0,
  Closed = 1,
  MissingLease = 2,
  Busy = 3,
  PublishRevoked = 4,
  KeepAliveProtected = 5,
  CheckpointRequired = 6,
  PartialBlocked = 7,
  NotDraining = 8,
  BufferHeld = 9,
  Error = 10
};

enum class WorldGenerationShadowLeaseState : uint8_t {
  Producing = 0,
  Ready = 1,
  Published = 2,
  Unloading = 3,
  Unloaded = 4,
  Discarded = 5,
  KeepAlive = 6
};

struct WorldGenerationShadowLeaseView {
  WorldGenerationShadowDbKey key;
  WorldGenerationShadowLeaseState state = WorldGenerationShadowLeaseState::Producing;
  bool reservationPreserved = false;
  bool privateBufferHeld = false;
  bool publishAuthorized = true;
  uint64_t contentVersion = 0;
};

struct WorldGenerationShadowLease {
  WorldGenerationShadowLeaseView view;
  ByteArray privateBytes;
};

class WorldGenerationShadowUnloadHarness {
public:
  explicit WorldGenerationShadowUnloadHarness(String worldId)
    : m_checkpoint(std::move(worldId)) {}

  WorldGenerationShadowCheckpointHarness const& checkpoint() const { return m_checkpoint; }
  WorldGenerationShadowCheckpointHarness& checkpoint() { return m_checkpoint; }
  WorldGenerationShadowDbService const& db() const { return m_checkpoint.db(); }
  uint64_t confirmedStepEpoch() const { return m_checkpoint.confirmedStepEpoch(); }
  size_t leaseCount() const { return m_leases.size(); }
  bool modeIsLegacy() const { return m_legacyMode; }

  // Metadata-only view; avoids copying private ByteArray payloads.
  Maybe<WorldGenerationShadowLeaseView> lease(WorldGenerationShadowDbKey const& key) const {
    if (auto const* entry = m_leases.ptr(key))
      return entry->view;
    return {};
  }

  WorldGenerationShadowUnloadStatus beginProduce(WorldGenerationShadowDbKey const& key) {
    if (!db().open())
      return WorldGenerationShadowUnloadStatus::Closed;
    if (!key.valid())
      return WorldGenerationShadowUnloadStatus::Error;
    if (m_leases.contains(key))
      return WorldGenerationShadowUnloadStatus::Busy;
    WorldGenerationShadowLease lease;
    lease.view.key = key;
    lease.view.state = WorldGenerationShadowLeaseState::Producing;
    lease.view.publishAuthorized = true;
    m_leases[key] = std::move(lease);
    return WorldGenerationShadowUnloadStatus::Ok;
  }

  WorldGenerationShadowUnloadStatus completePrivate(WorldGenerationShadowDbKey const& key,
      uint64_t contentVersion, ByteArray bytes) {
    auto* lease = m_leases.ptr(key);
    if (!lease)
      return WorldGenerationShadowUnloadStatus::MissingLease;
    if (lease->view.state != WorldGenerationShadowLeaseState::Producing
        && lease->view.state != WorldGenerationShadowLeaseState::Unloading)
      return WorldGenerationShadowUnloadStatus::Busy;
    if (contentVersion == 0 || bytes.empty())
      return WorldGenerationShadowUnloadStatus::Error;
    lease->view.contentVersion = contentVersion;
    lease->privateBytes = std::move(bytes);
    lease->view.privateBufferHeld = true;
    if (lease->view.state == WorldGenerationShadowLeaseState::Producing)
      lease->view.state = WorldGenerationShadowLeaseState::Ready;
    // Late completion while unloading stays Unloading with a held buffer.
    return WorldGenerationShadowUnloadStatus::Ok;
  }

  WorldGenerationShadowUnloadStatus markKeepAlive(WorldGenerationShadowDbKey const& key) {
    auto* lease = m_leases.ptr(key);
    if (!lease)
      return WorldGenerationShadowUnloadStatus::MissingLease;
    lease->view.reservationPreserved = true;
    if (lease->view.state == WorldGenerationShadowLeaseState::Ready
        || lease->view.state == WorldGenerationShadowLeaseState::Producing
        || lease->view.state == WorldGenerationShadowLeaseState::Published)
      lease->view.state = WorldGenerationShadowLeaseState::KeepAlive;
    return WorldGenerationShadowUnloadStatus::Ok;
  }

  WorldGenerationShadowUnloadStatus preserveReservation(WorldGenerationShadowDbKey const& key) {
    auto* lease = m_leases.ptr(key);
    if (!lease)
      return WorldGenerationShadowUnloadStatus::MissingLease;
    lease->view.reservationPreserved = true;
    return WorldGenerationShadowUnloadStatus::Ok;
  }

  // Publish into the shadow checkpoint path only while authorized.
  WorldGenerationShadowUnloadStatus tryPublish(WorldGenerationShadowDbKey const& key) {
    auto* lease = m_leases.ptr(key);
    if (!lease)
      return WorldGenerationShadowUnloadStatus::MissingLease;
    if (!lease->view.publishAuthorized
        || lease->view.state == WorldGenerationShadowLeaseState::Unloading
        || lease->view.state == WorldGenerationShadowLeaseState::Unloaded
        || lease->view.state == WorldGenerationShadowLeaseState::Discarded)
      return WorldGenerationShadowUnloadStatus::PublishRevoked;
    if (lease->view.state != WorldGenerationShadowLeaseState::Ready
        && lease->view.state != WorldGenerationShadowLeaseState::KeepAlive)
      return WorldGenerationShadowUnloadStatus::Busy;
    if (!lease->view.privateBufferHeld || lease->privateBytes.empty())
      return WorldGenerationShadowUnloadStatus::Error;

    List<WorldGenerationShadowDbKey> required = {key};
    uint64_t nextStep = m_checkpoint.confirmedStepEpoch() + 1;
    auto status = m_checkpoint.beginIntention(nextStep, required);
    if (status != WorldGenerationShadowCheckpointStatus::Ok)
      return WorldGenerationShadowUnloadStatus::Error;
    // Copy before put: empty-source ByteArray append is sanitizer-noisy.
    ByteArray payload(lease->privateBytes.ptr(), lease->privateBytes.size());
    status = m_checkpoint.put(key, lease->view.contentVersion, std::move(payload));
    if (status != WorldGenerationShadowCheckpointStatus::Ok) {
      m_checkpoint.reject();
      return WorldGenerationShadowUnloadStatus::Error;
    }
    status = m_checkpoint.seal();
    if (status != WorldGenerationShadowCheckpointStatus::Ok) {
      m_checkpoint.reject();
      return WorldGenerationShadowUnloadStatus::Error;
    }
    status = m_checkpoint.commitCheckpoint();
    if (status != WorldGenerationShadowCheckpointStatus::Ok)
      return WorldGenerationShadowUnloadStatus::Error;

    lease->view.state = WorldGenerationShadowLeaseState::Published;
    lease->privateBytes.reset();
    lease->view.privateBufferHeld = false;
    return WorldGenerationShadowUnloadStatus::Ok;
  }

  // Unload one sector: revoke publish authority for every lease in that sector.
  WorldGenerationShadowUnloadStatus requestUnload(Vec2U sector) {
    if (!db().open())
      return WorldGenerationShadowUnloadStatus::Closed;
    bool found = false;
    for (auto& entry : m_leases) {
      if (entry.first.sector != sector)
        continue;
      found = true;
      auto& lease = entry.second;
      lease.view.publishAuthorized = false;
      if (lease.view.state == WorldGenerationShadowLeaseState::KeepAlive) {
        // keepAlive stays protected; still cannot publish after unload.
        continue;
      }
      if (lease.view.state == WorldGenerationShadowLeaseState::Unloaded
          || lease.view.state == WorldGenerationShadowLeaseState::Discarded)
        continue;
      lease.view.state = WorldGenerationShadowLeaseState::Unloading;
    }
    if (!found)
      return WorldGenerationShadowUnloadStatus::MissingLease;
    return WorldGenerationShadowUnloadStatus::Ok;
  }

  // Wait-drain: Ready conclusion becomes Unloaded without installing; reservation kept.
  WorldGenerationShadowUnloadStatus drainWait(WorldGenerationShadowDbKey const& key) {
    auto* lease = m_leases.ptr(key);
    if (!lease)
      return WorldGenerationShadowUnloadStatus::MissingLease;
    if (lease->view.state == WorldGenerationShadowLeaseState::KeepAlive)
      return WorldGenerationShadowUnloadStatus::KeepAliveProtected;
    if (lease->view.state != WorldGenerationShadowLeaseState::Unloading)
      return WorldGenerationShadowUnloadStatus::NotDraining;
    lease->view.state = WorldGenerationShadowLeaseState::Unloaded;
    lease->view.reservationPreserved = true;
    // Private buffer may still be held until explicit release.
    return WorldGenerationShadowUnloadStatus::Ok;
  }

  // Discard-drain: drop Ready/Unloading conclusion; keepAlive protected.
  WorldGenerationShadowUnloadStatus drainDiscard(WorldGenerationShadowDbKey const& key) {
    auto* lease = m_leases.ptr(key);
    if (!lease)
      return WorldGenerationShadowUnloadStatus::MissingLease;
    if (lease->view.state == WorldGenerationShadowLeaseState::KeepAlive)
      return WorldGenerationShadowUnloadStatus::KeepAliveProtected;
    if (lease->view.state != WorldGenerationShadowLeaseState::Unloading
        && lease->view.state != WorldGenerationShadowLeaseState::Ready)
      return WorldGenerationShadowUnloadStatus::NotDraining;
    lease->view.state = WorldGenerationShadowLeaseState::Discarded;
    lease->privateBytes.reset();
    lease->view.privateBufferHeld = false;
    // Used reservations remain marked when previously preserved.
    return WorldGenerationShadowUnloadStatus::Ok;
  }

  // Regional-owner failure must not push partial unloading state to "central".
  WorldGenerationShadowUnloadStatus tryHandoffPartialToCentral(Vec2U sector) {
    bool unloadingOrPartial = false;
    for (auto const& entry : m_leases) {
      if (entry.first.sector != sector)
        continue;
      auto const& lease = entry.second;
      if (lease.view.state == WorldGenerationShadowLeaseState::Unloading
          || lease.view.state == WorldGenerationShadowLeaseState::Producing
          || lease.view.state == WorldGenerationShadowLeaseState::Ready) {
        unloadingOrPartial = true;
        break;
      }
    }
    if (unloadingOrPartial)
      return WorldGenerationShadowUnloadStatus::PartialBlocked;
    if (m_checkpoint.confirmedStepEpoch() == 0)
      return WorldGenerationShadowUnloadStatus::CheckpointRequired;
    return WorldGenerationShadowUnloadStatus::Ok;
  }

  // Mode switch back to legacy requires a confirmed checkpoint/reload frontier.
  WorldGenerationShadowUnloadStatus switchToLegacyMode() {
    if (m_checkpoint.confirmedStepEpoch() == 0)
      return WorldGenerationShadowUnloadStatus::CheckpointRequired;
    for (auto const& entry : m_leases) {
      auto const& lease = entry.second;
      if (lease.view.state == WorldGenerationShadowLeaseState::Unloading
          || lease.view.state == WorldGenerationShadowLeaseState::Producing
          || lease.view.state == WorldGenerationShadowLeaseState::Ready)
        return WorldGenerationShadowUnloadStatus::Busy;
    }
    m_legacyMode = true;
    return WorldGenerationShadowUnloadStatus::Ok;
  }

  // Late private buffers free only after unload/discard; not while producing/ready.
  WorldGenerationShadowUnloadStatus releasePrivateBuffers(WorldGenerationShadowDbKey const& key) {
    auto* lease = m_leases.ptr(key);
    if (!lease)
      return WorldGenerationShadowUnloadStatus::MissingLease;
    if (lease->view.state != WorldGenerationShadowLeaseState::Unloaded
        && lease->view.state != WorldGenerationShadowLeaseState::Discarded
        && lease->view.state != WorldGenerationShadowLeaseState::Published)
      return WorldGenerationShadowUnloadStatus::BufferHeld;
    lease->privateBytes.reset();
    lease->view.privateBufferHeld = false;
    return WorldGenerationShadowUnloadStatus::Ok;
  }

private:
  WorldGenerationShadowCheckpointHarness m_checkpoint;
  HashMap<WorldGenerationShadowDbKey, WorldGenerationShadowLease> m_leases;
  bool m_legacyMode = false;
};

}
