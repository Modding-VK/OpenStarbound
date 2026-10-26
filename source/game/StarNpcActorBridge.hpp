#pragma once

#include "StarGameTypes.hpp"
#include "StarThread.hpp"
#include "StarWorldGeometry.hpp"
#include "StarList.hpp"
#include "StarMap.hpp"

namespace Star {

// Experimental, read-only transport for a future per-world NPC actor runtime.
// It owns no World, Player, Npc, Lua state, or gameplay authority.
class NpcActorBridge : public Thread {
public:
  static constexpr size_t MaxNpcs = 256;
  static constexpr size_t MaxPlayers = 64;

  struct WorldFrame {
    uint64_t worldEpoch = 0;
    uint64_t worldStep = 0;
    double capturedAt = 0;
    WorldGeometry geometry;
    List<pair<EntityId, Vec2F>> npcs;
    List<pair<ConnectionId, Vec2F>> players;
  };

  enum class PlayerSignalKind { EntityUpdate, Interaction, Message };

  struct Metrics {
    uint64_t framesPublished = 0;
    uint64_t framesProcessed = 0;
    uint64_t framesCoalesced = 0;
    uint64_t framesRejected = 0;
    uint64_t signalsPublished = 0;
    uint64_t signalsProcessed = 0;
    uint64_t signalsCoalesced = 0;
    uint64_t signalsDropped = 0;
    uint64_t signalsWithoutView = 0;
    uint64_t playerNpcObservations = 0;
    uint64_t npcPeerObservations = 0;
    uint64_t targetedNpcObservations = 0;
    double frameAgeSecondsTotal = 0;
    double frameAgeSecondsMax = 0;
    double signalLatencySecondsTotal = 0;
    double signalLatencySecondsMax = 0;
    double signalViewAgeSecondsTotal = 0;
    double signalViewAgeSecondsMax = 0;
  };

  NpcActorBridge(String worldName, uint64_t worldEpoch);
  ~NpcActorBridge();

  bool start();
  // Call before this object is destroyed. No world lock is acquired.
  void stop();
  bool publishFrame(WorldFrame frame);
  void invalidateFrame();
  // Called during connection packet routing, before the original packet is
  // queued for world-side validation. Signals are never authenticated state.
  void publishPlayerSignal(ConnectionId clientId, PlayerSignalKind kind,
      EntityId target = NullEntityId);
  Metrics takeMetrics();

protected:
  void run() override;

private:
  struct PlayerSignal {
    ConnectionId clientId;
    PlayerSignalKind kind;
    EntityId target;
    uint64_t sequence;
    double receivedAt;
  };

  String m_monitorName;
  uint64_t m_worldEpoch;
  Mutex m_mutex;
  ConditionVariable m_condition;
  bool m_stopping = false;
  bool m_invalidateFrame = false;
  Maybe<WorldFrame> m_pendingFrame;
  Map<ConnectionId, PlayerSignal> m_pendingMovement;
  Deque<PlayerSignal> m_pendingEvents;
  uint64_t m_nextSignalSequence = 0;
  Metrics m_metrics;
};

}
