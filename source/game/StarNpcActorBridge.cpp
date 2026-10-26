#include "StarNpcActorBridge.hpp"
#include "StarTime.hpp"
#include "StarWorkerPool.hpp"
#include "StarTrace.hpp"

#include <algorithm>

namespace Star {

NpcActorBridge::NpcActorBridge(String worldName, uint64_t worldEpoch)
  : Thread("NpcActorBridge: " + worldName),
    m_monitorName("NPC actor bridge " + std::move(worldName)),
    m_worldEpoch(worldEpoch) {}

NpcActorBridge::~NpcActorBridge() {
  stop();
}

bool NpcActorBridge::start() {
  if (!isJoined())
    return false;
  {
    MutexLocker locker(m_mutex);
    m_stopping = false;
    m_invalidateFrame = true;
    m_pendingFrame.reset();
    m_pendingMovement.clear();
    m_pendingEvents.clear();
    m_nextSignalSequence = 0;
    m_metrics = {};
  }
  return Thread::start();
}

void NpcActorBridge::stop() {
  {
    MutexLocker locker(m_mutex);
    m_stopping = true;
    m_pendingFrame.reset();
    m_pendingMovement.clear();
    m_pendingEvents.clear();
    m_condition.broadcast();
  }
  join();
}

bool NpcActorBridge::publishFrame(WorldFrame frame) {
  MutexLocker locker(m_mutex);
  if (m_stopping)
    return false;
  if (frame.worldEpoch != m_worldEpoch || frame.npcs.size() > MaxNpcs
      || frame.players.size() > MaxPlayers) {
    m_pendingFrame.reset();
    m_invalidateFrame = true;
    ++m_metrics.framesRejected;
    m_condition.signal();
    return false;
  }
  if (m_pendingFrame)
    ++m_metrics.framesCoalesced;
  m_pendingFrame = std::move(frame);
  ++m_metrics.framesPublished;
  m_condition.signal();
  return true;
}

void NpcActorBridge::invalidateFrame() {
  MutexLocker locker(m_mutex);
  m_pendingFrame.reset();
  m_invalidateFrame = true;
  m_condition.signal();
}

void NpcActorBridge::publishPlayerSignal(ConnectionId clientId, PlayerSignalKind kind, EntityId target) {
  MutexLocker locker(m_mutex);
  if (m_stopping)
    return;

  PlayerSignal signal{clientId, kind, target, ++m_nextSignalSequence, Time::monotonicTime()};
  if (kind == PlayerSignalKind::EntityUpdate) {
    if (m_pendingMovement.contains(clientId)) {
      ++m_metrics.signalsCoalesced;
    } else if (m_pendingMovement.size() >= MaxPlayers) {
      ++m_metrics.signalsDropped;
      return;
    }
    m_pendingMovement[clientId] = signal;
  } else {
    if (m_pendingEvents.size() >= 128) {
      ++m_metrics.signalsDropped;
      return;
    }
    m_pendingEvents.append(signal);
  }
  ++m_metrics.signalsPublished;
  m_condition.signal();
}

NpcActorBridge::Metrics NpcActorBridge::takeMetrics() {
  MutexLocker locker(m_mutex);
  return take(m_metrics);
}

void NpcActorBridge::run() {
  Maybe<WorldFrame> activeFrame;
  while (true) {
    Maybe<WorldFrame> frame;
    Map<ConnectionId, PlayerSignal> movement;
    Deque<PlayerSignal> events;
    bool invalidate = false;
    {
      MutexLocker locker(m_mutex);
      while (!m_stopping && !m_pendingFrame && !m_invalidateFrame
          && m_pendingMovement.empty() && m_pendingEvents.empty())
        m_condition.wait(m_mutex);
      if (m_stopping)
        return;
      frame = take(m_pendingFrame);
      movement = take(m_pendingMovement);
      events = take(m_pendingEvents);
      invalidate = m_invalidateFrame;
      m_invalidateFrame = false;
    }

    STAR_TRACE_ZONE("NPC actor bridge observations");
    int64_t workStart = WorkerPool::monitoring() ? Time::monotonicMicroseconds() : 0;
    Metrics delta;
    if (invalidate)
      activeFrame.reset();
    if (frame) {
      activeFrame = frame.take();
      ++delta.framesProcessed;
      double age = max(0.0, Time::monotonicTime() - activeFrame->capturedAt);
      delta.frameAgeSecondsTotal += age;
      delta.frameAgeSecondsMax = max(delta.frameAgeSecondsMax, age);
      // NPC-to-NPC perception is calculated here, away from the world thread.
      for (size_t i = 0; i < activeFrame->npcs.size(); ++i) {
        for (size_t j = 0; j < activeFrame->npcs.size(); ++j) {
          if (i != j && activeFrame->geometry.diff(activeFrame->npcs[i].second,
                  activeFrame->npcs[j].second).magnitudeSquared() <= 32.0f * 32.0f) {
            ++delta.npcPeerObservations;
            break;
          }
        }
      }
    }

    auto observe = [&](PlayerSignal const& signal) {
      ++delta.signalsProcessed;
      double now = Time::monotonicTime();
      double latency = max(0.0, now - signal.receivedAt);
      delta.signalLatencySecondsTotal += latency;
      delta.signalLatencySecondsMax = max(delta.signalLatencySecondsMax, latency);
      if (!activeFrame) {
        ++delta.signalsWithoutView;
        return;
      }
      Maybe<Vec2F> playerPosition;
      for (auto const& player : activeFrame->players) {
        if (player.first == signal.clientId) {
          playerPosition = player.second;
          break;
        }
      }
      if (!playerPosition) {
        ++delta.signalsWithoutView;
        return;
      }
      double viewAge = max(0.0, now - activeFrame->capturedAt);
      delta.signalViewAgeSecondsTotal += viewAge;
      delta.signalViewAgeSecondsMax = max(delta.signalViewAgeSecondsMax, viewAge);
      if (signal.target != NullEntityId) {
        for (auto const& npc : activeFrame->npcs) {
          if (npc.first == signal.target) {
            ++delta.targetedNpcObservations;
            return;
          }
        }
      } else if (signal.kind == PlayerSignalKind::EntityUpdate) {
        for (auto const& npc : activeFrame->npcs) {
          if (activeFrame->geometry.diff(npc.second, *playerPosition).magnitudeSquared() <= 32.0f * 32.0f)
            ++delta.playerNpcObservations;
        }
      }
    };
    List<PlayerSignal> orderedSignals;
    orderedSignals.reserve(movement.size() + events.size());
    for (auto const& signal : movement)
      orderedSignals.append(signal.second);
    for (auto const& signal : events)
      orderedSignals.append(signal);
    std::sort(orderedSignals.begin(), orderedSignals.end(), [](PlayerSignal const& a, PlayerSignal const& b) {
      return a.sequence < b.sequence;
    });
    for (auto const& signal : orderedSignals)
      observe(signal);

    if (workStart)
      WorkerPool::recordDedicatedWork(m_monitorName, Time::monotonicMicroseconds() - workStart);
    {
      MutexLocker locker(m_mutex);
      m_metrics.framesProcessed += delta.framesProcessed;
      m_metrics.signalsProcessed += delta.signalsProcessed;
      m_metrics.signalsWithoutView += delta.signalsWithoutView;
      m_metrics.playerNpcObservations += delta.playerNpcObservations;
      m_metrics.npcPeerObservations += delta.npcPeerObservations;
      m_metrics.targetedNpcObservations += delta.targetedNpcObservations;
      m_metrics.frameAgeSecondsTotal += delta.frameAgeSecondsTotal;
      m_metrics.frameAgeSecondsMax = max(m_metrics.frameAgeSecondsMax, delta.frameAgeSecondsMax);
      m_metrics.signalLatencySecondsTotal += delta.signalLatencySecondsTotal;
      m_metrics.signalLatencySecondsMax = max(m_metrics.signalLatencySecondsMax, delta.signalLatencySecondsMax);
      m_metrics.signalViewAgeSecondsTotal += delta.signalViewAgeSecondsTotal;
      m_metrics.signalViewAgeSecondsMax = max(m_metrics.signalViewAgeSecondsMax, delta.signalViewAgeSecondsMax);
    }
  }
}

}
