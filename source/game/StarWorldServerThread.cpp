#include "StarWorldServerThread.hpp"
#include "StarTickRateMonitor.hpp"
#include "StarNpc.hpp"
#include "StarRoot.hpp"
#include "StarLogging.hpp"
#include "StarTrace.hpp"
#include "StarAssets.hpp"
#include "StarPlayer.hpp"
#include "StarWorkerPool.hpp"
#include "StarNpcActorBridge.hpp"
#include "StarTime.hpp"

namespace Star {

WorldServerThread::WorldServerThread(WorldServerPtr server, WorldId worldId)
  : Thread("WorldServerThread: " + printWorldId(worldId)),
    m_worldServer(std::move(server)),
    m_worldId(std::move(worldId)),
    m_stop(false),
    m_errorOccurred(false),
    m_shouldExpire(true) {
  if (m_worldServer)
    m_worldServer->setWorldId(printWorldId(m_worldId));
  if (m_worldServer && Root::singleton().configuration()->get("npcActorBridge", JsonObject()).getBool("enabled", false))
    m_npcActorBridge = make_unique<NpcActorBridge>(printWorldId(m_worldId), m_worldServer->instanceEpoch());
}

WorldServerThread::~WorldServerThread() {
  m_stop = true;
  join();
  if (m_npcActorBridge)
    m_npcActorBridge->stop();

  RecursiveMutexLocker locker(m_mutex);
  for (auto clientId : m_worldServer->clientIds())
    removeClient(clientId);
}

WorldId WorldServerThread::worldId() const {
  return m_worldId;
}

void WorldServerThread::start() {
  m_stop = false;
  m_errorOccurred = false;
  if (m_npcActorBridge)
    m_npcActorBridge->start();
  Thread::start();
}

void WorldServerThread::stop() {
  m_stop = true;
  Thread::join();
  if (m_npcActorBridge)
    m_npcActorBridge->stop();
}

void WorldServerThread::setPause(shared_ptr<const atomic<bool>> pause) {
  m_pause = pause;
}

bool WorldServerThread::serverErrorOccurred() {
  return m_errorOccurred;
}

bool WorldServerThread::shouldExpire() {
  return m_shouldExpire;
}

bool WorldServerThread::spawnTargetValid(SpawnTarget const& spawnTarget) {
  try {
    RecursiveMutexLocker locker(m_mutex);
    return m_worldServer->spawnTargetValid(spawnTarget);
  } catch (std::exception const& e) {
    Logger::error("WorldServerThread exception caught: {}", outputException(e, true));
    m_errorOccurred = true;
    return false;
  }
}

bool WorldServerThread::addClient(ConnectionId clientId, SpawnTarget const& spawnTarget, bool isLocal, bool isAdmin, NetCompatibilityRules netRules, ClientSubWorldId subWorldId) {
  try {
    RecursiveMutexLocker locker(m_mutex);
    if (m_worldServer->addClient(clientId, spawnTarget, isLocal, isAdmin, netRules, subWorldId)) {
      m_clients.add(clientId);
      return true;
    }

    return false;
  } catch (std::exception const& e) {
    Logger::error("WorldServerThread exception caught: {}", outputException(e, true));
    m_errorOccurred = true;
    return false;
  }
}

List<PacketPtr> WorldServerThread::removeClient(ConnectionId clientId) {
  RecursiveMutexLocker locker(m_mutex);
  if (!m_clients.contains(clientId))
    return {};

  RecursiveMutexLocker queueLocker(m_queueMutex);

  List<PacketPtr> outgoingPackets;
  try {
    auto incomingPackets = take(m_incomingPacketQueue[clientId]);
    if (m_worldServer->hasClient(clientId))
      m_worldServer->handleIncomingPackets(clientId, std::move(incomingPackets));

    outgoingPackets = take(m_outgoingPacketQueue[clientId]);
    if (m_worldServer->hasClient(clientId))
      outgoingPackets.appendAll(m_worldServer->removeClient(clientId));

  } catch (std::exception const& e) {
    Logger::error("WorldServerThread exception caught: {}", outputException(e, true));
    m_errorOccurred = true;
  }

  m_clients.remove(clientId);
  m_incomingPacketQueue.remove(clientId);
  m_outgoingPacketQueue.remove(clientId);
  if (m_npcActorBridge)
    m_npcActorBridge->invalidateFrame();
  return outgoingPackets;
}

List<ConnectionId> WorldServerThread::clients() const {
  RecursiveMutexLocker locker(m_mutex);
  return m_clients.values();
}

bool WorldServerThread::hasClient(ConnectionId clientId) const {
  RecursiveMutexLocker locker(m_mutex);
  return m_clients.contains(clientId);
}

bool WorldServerThread::noClients() const {
  RecursiveMutexLocker locker(m_mutex);
  return m_clients.empty();
}

ClientSubWorldId WorldServerThread::clientSubWorld(ConnectionId clientId) const {
  RecursiveMutexLocker locker(m_mutex);
  return m_worldServer->clientSubWorld(clientId);
}

List<ConnectionId> WorldServerThread::erroredClients() const {
  RecursiveMutexLocker locker(m_mutex);
  auto unerroredClients = HashSet<ConnectionId>::from(m_worldServer->clientIds());
  return m_clients.difference(unerroredClients).values();
}

void WorldServerThread::pushIncomingPackets(ConnectionId clientId, List<PacketPtr> packets) {
  if (m_npcActorBridge) {
    // Diagnostic hints reach the actor thread without taking the world lock.
    // The original packets still require normal world-side validation.
    for (auto const& packet : packets) {
      if (auto update = as<EntityUpdateSetPacket>(packet)) {
        if (!update->deltas.empty())
          m_npcActorBridge->publishPlayerSignal(clientId, NpcActorBridge::PlayerSignalKind::EntityUpdate);
      } else if (auto interact = as<EntityInteractPacket>(packet)) {
        m_npcActorBridge->publishPlayerSignal(clientId, NpcActorBridge::PlayerSignalKind::Interaction,
            interact->interactRequest.targetId);
      } else if (auto message = as<EntityMessagePacket>(packet)) {
        auto target = message->entityId.ptr<EntityId>();
        m_npcActorBridge->publishPlayerSignal(clientId, NpcActorBridge::PlayerSignalKind::Message,
            target ? *target : NullEntityId);
      }
    }
  }
  RecursiveMutexLocker queueLocker(m_queueMutex);
  m_incomingPacketQueue[clientId].appendAll(std::move(packets));
}

List<PacketPtr> WorldServerThread::pullOutgoingPackets(ConnectionId clientId) {
  RecursiveMutexLocker queueLocker(m_queueMutex);
  return take(m_outgoingPacketQueue[clientId]);
}

Maybe<Vec2F> WorldServerThread::playerRevivePosition(ConnectionId clientId) const {
  try {
    RecursiveMutexLocker locker(m_mutex);
    if (auto player = m_worldServer->clientPlayer(clientId))
      return player->position() + player->feetOffset();
    return {};
  } catch (std::exception const& e) {
    Logger::error("WorldServerThread exception caught: {}", outputException(e, true));
    m_errorOccurred = true;
    return {};
  }
}

Maybe<pair<String, String>> WorldServerThread::pullNewPlanetType() {
  try {
    RecursiveMutexLocker locker(m_mutex);
    return m_worldServer->pullNewPlanetType();
  } catch (std::exception const& e) {
    Logger::error("WorldServerThread exception caught: {}", outputException(e, true));
    m_errorOccurred = true;
    return {};
  }
}

void WorldServerThread::executeAction(WorldServerAction action) {
  RecursiveMutexLocker locker(m_mutex);
  action(this, m_worldServer.get());
}

void WorldServerThread::setUpdateAction(WorldServerAction updateAction) {
  RecursiveMutexLocker locker(m_mutex);
  m_updateAction = updateAction;
}

void WorldServerThread::passMessages(List<Message>&& messages) {
  RecursiveMutexLocker locker(m_messageMutex);
  m_messages.appendAll(std::move(messages));
}

void WorldServerThread::unloadAll(bool force) {
  try {
    RecursiveMutexLocker locker(m_mutex);
    m_worldServer->unloadAll(force);
    if (m_npcActorBridge)
      m_npcActorBridge->invalidateFrame();
  } catch (std::exception const& e) {
    Logger::error("WorldServerThread exception caught: {}", outputException(e, true));
    m_errorOccurred = true;
  }
}

WorldChunks WorldServerThread::readChunks() {
  try {
    RecursiveMutexLocker locker(m_mutex);
    return m_worldServer->readChunks();
  } catch (std::exception const& e) {
    Logger::error("WorldServerThread exception caught: {}", outputException(e, true));
    m_errorOccurred = true;
    return {};
  }
}

void WorldServerThread::run() {
  try {
    auto& root = Root::singleton();
    double updateMeasureWindow = root.assets()->json("/universe_server.config:updateMeasureWindow").toDouble();
    double fidelityDecrementScore = root.assets()->json("/universe_server.config:fidelityDecrementScore").toDouble();
    double fidelityIncrementScore = root.assets()->json("/universe_server.config:fidelityIncrementScore").toDouble();

    String serverFidelityMode = root.configuration()->get("serverFidelity").toString();
    Maybe<WorldServerFidelity> lockedFidelity;
    if (!serverFidelityMode.equalsIgnoreCase("automatic"))
      lockedFidelity = WorldServerFidelityNames.getLeft(serverFidelityMode);

    double storageInterval = root.assets()->json("/universe_server.config:worldStorageInterval").toDouble() / 1000.0;
    Timer storageTimer = Timer::withTime(storageInterval);

    TickRateApproacher tickApproacher(1.0f / ServerGlobalTimestep, updateMeasureWindow);
    double fidelityScore = 0.0;
    WorldServerFidelity automaticFidelity = WorldServerFidelity::Medium;

    while (!m_stop && !m_errorOccurred) {
      STAR_TRACE_ZONE("WorldServerThread tick");
      STAR_TRACE_FRAME("WorldServerThread");
      int64_t threadWorkStart = WorkerPool::monitoring() ? Time::monotonicMicroseconds() : 0;
      bool performanceEnabled = m_worldServer->performanceOverlayActive();
      double tickStart = performanceEnabled ? Time::monotonicTime() : 0;
      auto fidelity = lockedFidelity.value(automaticFidelity);
      LogMap::set(strf("server_{}_fidelity", m_worldId), WorldServerFidelityNames.getRight(fidelity));
      LogMap::set(strf("server_{}_update", m_worldId), strf("{:4.2f}Hz", tickApproacher.rate()));

      update(fidelity);
      tickApproacher.setTargetTickRate(1.0f / ServerGlobalTimestep);
      tickApproacher.tick();

      if (storageTimer.timeUp()) {
        sync();
        storageTimer.restart(storageInterval);
      }

      double spareTime = tickApproacher.spareTime();
      fidelityScore += spareTime;

      if (fidelityScore <= fidelityDecrementScore) {
        if (automaticFidelity > WorldServerFidelity::Minimum)
          automaticFidelity = (WorldServerFidelity)((int)automaticFidelity - 1);
        fidelityScore = 0.0;
      }

      if (fidelityScore >= fidelityIncrementScore) {
        if (automaticFidelity < WorldServerFidelity::High)
          automaticFidelity = (WorldServerFidelity)((int)automaticFidelity + 1);
        fidelityScore = 0.0;
      }

      int64_t spareMilliseconds = floor(spareTime * 1000);
      if (threadWorkStart)
        WorkerPool::recordDedicatedWork("World server " + printWorldId(m_worldId),
            Time::monotonicMicroseconds() - threadWorkStart);
      double busySeconds = performanceEnabled ? Time::monotonicTime() - tickStart : 0;
      double idleSeconds = 0;
      if (spareMilliseconds > 0) {
        STAR_TRACE_ZONE("WorldServerThread sleep");
        double sleepStart = performanceEnabled ? Time::monotonicTime() : 0;
        Thread::sleepPrecise(spareMilliseconds);
        if (performanceEnabled)
          idleSeconds = Time::monotonicTime() - sleepStart;
      }
      if (performanceEnabled) {
        m_worldServer->recordPerformanceTick(busySeconds, idleSeconds);
        if (m_worldServer->performanceSnapshotDue()) {
          RecursiveMutexLocker locker(m_mutex);
          m_worldServer->publishPerformanceSnapshot();
        }
      }
    }
  } catch (std::exception const& e) {
    Logger::error("WorldServerThread exception caught: {}", outputException(e, true));
    m_errorOccurred = true;
  }
  if (m_npcActorBridge)
    m_npcActorBridge->stop();
}

void WorldServerThread::update(WorldServerFidelity fidelity) {
  STAR_TRACE_ZONE("WorldServerThread update");
  RecursiveMutexLocker locker(m_mutex);
  auto unerroredClientIds = m_worldServer->clientIds();
  for (auto clientId : unerroredClientIds) {
    RecursiveMutexLocker queueLocker(m_queueMutex);
    auto incomingPackets = take(m_incomingPacketQueue[clientId]);
    queueLocker.unlock();
    try {
      m_worldServer->handleIncomingPackets(clientId, std::move(incomingPackets));
    } catch (std::exception const& e) {
      Logger::error("WorldServerThread exception caught handling incoming packets for client {}: {}",
          clientId, outputException(e, true));
      RecursiveMutexLocker queueLocker(m_queueMutex);
      m_outgoingPacketQueue[clientId].appendAll(m_worldServer->removeClient(clientId));
      unerroredClientIds.remove(clientId);
    }
  }

  float dt = ServerGlobalTimestep * GlobalTimescale;
  m_worldServer->setFidelity(fidelity);
  if (dt > 0.0f && (!m_pause || *m_pause == false))
    m_worldServer->update(dt);

  if (m_npcActorBridge) {
    NpcActorBridge::WorldFrame frame;
    frame.worldEpoch = m_worldServer->instanceEpoch();
    frame.worldStep = m_worldServer->currentStep();
    frame.capturedAt = Time::monotonicTime();
    frame.geometry = m_worldServer->geometry();
    for (auto clientId : unerroredClientIds) {
      if (auto player = m_worldServer->clientPlayer(clientId)) {
        frame.players.append({clientId, player->position()});
        if (frame.players.size() > NpcActorBridge::MaxPlayers)
          break;
      }
    }
    m_worldServer->forAllEntities([&](EntityPtr const& entity) {
      if (entity->entityType() == EntityType::Npc && frame.npcs.size() <= NpcActorBridge::MaxNpcs)
        frame.npcs.append({entity->entityId(), entity->position()});
    });
    m_npcActorBridge->publishFrame(std::move(frame));
    auto metrics = m_npcActorBridge->takeMetrics();
    if (auto profile = m_worldServer->explorationProfile()) {
      profile->count("npc.actorBridge.framesPublished", metrics.framesPublished);
      profile->count("npc.actorBridge.framesProcessed", metrics.framesProcessed);
      profile->count("npc.actorBridge.framesCoalesced", metrics.framesCoalesced);
      profile->count("npc.actorBridge.framesRejected", metrics.framesRejected);
      profile->count("npc.actorBridge.signalsPublished", metrics.signalsPublished);
      profile->count("npc.actorBridge.signalsProcessed", metrics.signalsProcessed);
      profile->count("npc.actorBridge.signalsCoalesced", metrics.signalsCoalesced);
      profile->count("npc.actorBridge.signalsDropped", metrics.signalsDropped);
      profile->count("npc.actorBridge.signalsWithoutView", metrics.signalsWithoutView);
      profile->count("npc.actorBridge.playerNpcObservations", metrics.playerNpcObservations);
      profile->count("npc.actorBridge.npcPeerObservations", metrics.npcPeerObservations);
      profile->count("npc.actorBridge.targetedNpcObservations", metrics.targetedNpcObservations);
      if (metrics.framesProcessed) {
        profile->record("npc.actorBridge.frameAgeMean", metrics.frameAgeSecondsTotal / metrics.framesProcessed);
        profile->record("npc.actorBridge.frameAgeMax", metrics.frameAgeSecondsMax);
      }
      if (metrics.signalsProcessed) {
        profile->record("npc.actorBridge.signalLatencyMean", metrics.signalLatencySecondsTotal / metrics.signalsProcessed);
        profile->record("npc.actorBridge.signalLatencyMax", metrics.signalLatencySecondsMax);
      }
      if (metrics.signalsProcessed > metrics.signalsWithoutView) {
        auto viewedSignals = metrics.signalsProcessed - metrics.signalsWithoutView;
        profile->record("npc.actorBridge.signalViewAgeMean", metrics.signalViewAgeSecondsTotal / viewedSignals);
        profile->record("npc.actorBridge.signalViewAgeMax", metrics.signalViewAgeSecondsMax);
      }
    }
  }

  List<Message> messages;
  {
    RecursiveMutexLocker locker(m_messageMutex);
    messages = std::move(m_messages);
  }
  for (auto& message : messages) {
    if (auto resp = m_worldServer->receiveMessage(ServerConnectionId, message.message, message.args))
      if (resp->is<RpcPromise<Json>>())
        message.promise.chain(resp->get<RpcPromise<Json>>());
      else
        message.promise.fulfill(resp->get<Json>());
    else
      message.promise.fail("Message not handled by world");
  }

  for (auto& clientId : unerroredClientIds) {
    auto outgoingPackets = m_worldServer->getOutgoingPackets(clientId);
    RecursiveMutexLocker queueLocker(m_queueMutex);
    m_outgoingPacketQueue[clientId].appendAll(std::move(outgoingPackets));
  }

  m_shouldExpire = m_worldServer->shouldExpire();

  if (m_updateAction)
    m_updateAction(this, m_worldServer.get());
}

void WorldServerThread::sync() {
  STAR_TRACE_ZONE("WorldServerThread sync");
  RecursiveMutexLocker locker(m_mutex);
  Logger::debug("WorldServer: periodic sync to disk of world {}", m_worldId);
  m_worldServer->sync();
}

}
