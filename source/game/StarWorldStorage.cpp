#include "StarWorldStorage.hpp"
#include "StarWorldGenerationDependency.hpp"
#include "StarTrace.hpp"
#include "StarFile.hpp"
#include "StarCompression.hpp"
#include "StarJsonExtra.hpp"
#include "StarDataStreamExtra.hpp"
#include "StarIterator.hpp"
#include "StarLogging.hpp"
#include "StarRoot.hpp"
#include "StarEntityMap.hpp"
#include "StarEntityFactory.hpp"
#include "StarAssets.hpp"
#include "StarMaterialDatabase.hpp"
#include "StarLiquidsDatabase.hpp"
#include "StarTime.hpp"

#include <condition_variable>
#include <algorithm>
#include <exception>
#include <map>
#include <mutex>
#include <thread>

namespace Star {

// Experimental sector mailbox. The worker receives only copied metadata and
// never dereferences WorldStorage or tiles; simulation ownership is separate.
class WorldSectorThreads {
public:
  using Sector = WorldStorage::Sector;

  explicit WorldSectorThreads(bool mailboxEnabled) {
    m_metrics.mailboxEnabled = mailboxEnabled;
  }

  ~WorldSectorThreads() {
    for (auto& entry : m_threads)
      entry.second->requestStop();
  }

  void loaded(Sector const& sector) {
    if (m_metrics.disabledAfterFailure)
      return;
    auto key = std::make_pair(sector[0], sector[1]);
    if (m_threads.find(key) != m_threads.end())
      return;
    STAR_TRACE_ZONE("Sector thread start");
    auto start = Time::monotonicMicroseconds();
    try {
      m_threads.emplace(key, make_unique<Worker>(sector, ++m_nextIncarnation));
    } catch (std::exception const& e) {
      ++m_metrics.failedCreations;
      m_metrics.disabledAfterFailure = true;
      Logger::warn("Sector thread lifecycle preview disabled after thread creation failed: {}", e.what());
      for (auto& entry : m_threads)
        entry.second->requestStop();
      while (!m_threads.empty()) {
        auto key = m_threads.begin()->first;
        unloaded(Sector(key.first, key.second));
      }
      return;
    }
    auto elapsed = static_cast<uint64_t>(Time::monotonicMicroseconds() - start);
    m_startSamples.add(elapsed);
    m_metrics.startTotalMicroseconds += elapsed;
    m_metrics.startMaximumMicroseconds = std::max(m_metrics.startMaximumMicroseconds, elapsed);
    ++m_metrics.created;
    m_metrics.peak = std::max(m_metrics.peak, m_threads.size());
  }

  void unloaded(Sector const& sector) {
    auto it = m_threads.find(std::make_pair(sector[0], sector[1]));
    if (it == m_threads.end())
      return;
    if (it->second->hasOutstanding())
      ++m_metrics.cancelled;
    STAR_TRACE_ZONE("Sector thread stop");
    auto start = Time::monotonicMicroseconds();
    m_threads.erase(it);
    auto elapsed = static_cast<uint64_t>(Time::monotonicMicroseconds() - start);
    m_stopSamples.add(elapsed);
    m_metrics.stopTotalMicroseconds += elapsed;
    m_metrics.stopMaximumMicroseconds = std::max(m_metrics.stopMaximumMicroseconds, elapsed);
    ++m_metrics.destroyed;
  }

  size_t count() const {
    return m_threads.size();
  }

  bool mailboxEnabled() const {
    return m_metrics.mailboxEnabled && !m_metrics.disabledAfterFailure;
  }

  void sample(Sector const& sector, uint8_t loadLevel, uint8_t generationLevel,
      uint64_t contentRevision) {
    if (!m_metrics.mailboxEnabled || m_metrics.disabledAfterFailure)
      return;
    auto it = m_threads.find(std::make_pair(sector[0], sector[1]));
    if (it == m_threads.end())
      return;
    Message result;
    if (it->second->takeResult(result)) {
      ++m_metrics.completed;
      m_queueWaitSamples.add(static_cast<uint64_t>(result.startedAt - result.sentAt));
      m_computeSamples.add(static_cast<uint64_t>(result.doneAt - result.startedAt));
      if (result.sector == sector && result.sequence == it->second->latestSequence
          && result.incarnation == it->second->incarnation && result.contentRevision == contentRevision
          && result.loadLevel == loadLevel && result.generationLevel == generationLevel
          && result.signature == ticketSignature(result))
        ++m_metrics.accepted;
      else
        ++m_metrics.stale;
    }
    Message message;
    message.sector = sector;
    message.sequence = ++m_nextSequence;
    message.incarnation = it->second->incarnation;
    message.contentRevision = contentRevision;
    message.loadLevel = loadLevel;
    message.generationLevel = generationLevel;
    message.sentAt = Time::monotonicMicroseconds();
    if (it->second->post(message))
      ++m_metrics.submitted;
    else
      ++m_metrics.skippedBusy;
  }

  WorldSectorThreadMetrics metrics() const {
    auto result = m_metrics;
    result.enabled = true;
    result.current = m_threads.size();
    result.startP95Microseconds = m_startSamples.percentile(95);
    result.startP99Microseconds = m_startSamples.percentile(99);
    result.stopP95Microseconds = m_stopSamples.percentile(95);
    result.stopP99Microseconds = m_stopSamples.percentile(99);
    result.queueWaitP95Microseconds = m_queueWaitSamples.percentile(95);
    result.queueWaitP99Microseconds = m_queueWaitSamples.percentile(99);
    result.computeP95Microseconds = m_computeSamples.percentile(95);
    result.computeP99Microseconds = m_computeSamples.percentile(99);
    return result;
  }

private:
  struct Message {
    Sector sector;
    uint64_t sequence = 0;
    uint64_t incarnation = 0;
    uint64_t contentRevision = 0;
    uint8_t loadLevel = 0;
    uint8_t generationLevel = 0;
    int64_t sentAt = 0;
    int64_t startedAt = 0;
    int64_t doneAt = 0;
    uint64_t signature = 0;
  };

  static uint64_t ticketSignature(Message const& message) {
    // A transport check for immutable metadata, not a hash of world tiles.
    return (message.sequence * 33u) ^ (message.incarnation * 65599u)
        ^ (uint64_t(message.sector[0]) << 32) ^ uint64_t(message.sector[1])
        ^ message.contentRevision ^ (uint64_t(message.loadLevel) << 16)
        ^ (uint64_t(message.generationLevel) << 24);
  }

  struct Samples {
    static constexpr size_t Capacity = 256;
    std::array<uint64_t, Capacity> values{};
    size_t count = 0;
    size_t next = 0;

    void add(uint64_t value) {
      values[next] = value;
      next = (next + 1) % Capacity;
      count = std::min(count + 1, Capacity);
    }

    uint64_t percentile(unsigned p) const {
      if (!count)
        return 0;
      auto sorted = values;
      std::sort(sorted.begin(), sorted.begin() + count);
      return sorted[((count - 1) * p + 99) / 100];
    }
  };

  struct Worker {
    explicit Worker(Sector sector, uint64_t incarnation) : sector(sector), incarnation(incarnation) {
      thread = std::thread([this] {
#ifdef STAR_ENABLE_TRACY
        tracy::SetThreadName("Sector mailbox preview");
#endif
        std::unique_lock<std::mutex> lock(mutex);
        ready = true;
        wake.notify_one();
        while (true) {
          wake.wait(lock, [this] { return stopping || hasPending; });
          if (stopping)
            break;
          Message work = pending;
          hasPending = false;
          processing = true;
          lock.unlock();
          work.startedAt = Time::monotonicMicroseconds();
          {
            STAR_TRACE_ZONE("Sector immutable metadata task");
            work.signature = WorldSectorThreads::ticketSignature(work);
          }
          work.doneAt = Time::monotonicMicroseconds();
          lock.lock();
          processing = false;
          if (!stopping) {
            result = work;
            hasResult = true;
          }
        }
      });
      try {
        std::unique_lock<std::mutex> lock(mutex);
        wake.wait(lock, [this] { return ready; });
      } catch (...) {
        requestStop();
        thread.join();
        throw;
      }
    }

    ~Worker() {
      requestStop();
      thread.join();
    }

    void requestStop() {
      {
        std::lock_guard<std::mutex> lock(mutex);
        stopping = true;
      }
      wake.notify_one();
    }

    bool post(Message const& message) {
      {
        std::lock_guard<std::mutex> lock(mutex);
        if (stopping || hasPending || processing || hasResult || message.sector != sector
            || message.incarnation != incarnation || message.sequence <= latestSequence)
          return false;
        pending = message;
        latestSequence = message.sequence;
        hasPending = true;
      }
      wake.notify_one();
      return true;
    }

    bool takeResult(Message& out) {
      std::lock_guard<std::mutex> lock(mutex);
      if (!hasResult)
        return false;
      out = result;
      hasResult = false;
      return true;
    }

    bool hasOutstanding() {
      std::lock_guard<std::mutex> lock(mutex);
      return hasPending || processing || hasResult;
    }

    Worker(Worker const&) = delete;
    Worker& operator=(Worker const&) = delete;

    std::mutex mutex;
    std::condition_variable wake;
    bool stopping = false;
    bool ready = false;
    bool hasPending = false;
    bool processing = false;
    bool hasResult = false;
    Message pending;
    Message result;
    Sector const sector;
    uint64_t const incarnation;
    uint64_t latestSequence = 0;
    std::thread thread;
  };

  // The world owner thread alone changes this map. Individual workers only
  // wait for shutdown, so they cannot race with world state or the registry.
  std::map<std::pair<unsigned, unsigned>, unique_ptr<Worker>> m_threads;
  WorldSectorThreadMetrics m_metrics;
  Samples m_startSamples;
  Samples m_stopSamples;
  Samples m_queueWaitSamples;
  Samples m_computeSamples;
  uint64_t m_nextIncarnation = 0;
  uint64_t m_nextSequence = 0;
};

WorldChunks WorldStorage::getWorldChunksUpdate(WorldChunks const& oldChunks, WorldChunks const& newChunks) {
  WorldChunks update;
  for (auto const& p : oldChunks) {
    if (!newChunks.contains(p.first))
      update[p.first] = {};
  }

  for (auto const& p : newChunks) {
    if (oldChunks.value(p.first) != p.second)
      update[p.first] = p.second;
  }
  return update;
}

void WorldStorage::applyWorldChunksUpdateToFile(String const& file, WorldChunks const& update) {
  BTreeDatabase db;
  openDatabase(db, File::open(file, IOMode::ReadWrite));

  for (auto const& p : update) {
    if (p.second)
      db.insert(p.first, *p.second);
    else
      db.remove(p.first);
  }
}

WorldChunks WorldStorage::getWorldChunksFromFile(String const& file) {
  BTreeDatabase db;
  openDatabase(db, File::open(file, IOMode::Read));

  WorldChunks chunks;
  db.forAll([&chunks](ByteArray key, ByteArray value) { chunks.add(std::move(key), std::move(value)); });
  return chunks;
}

WorldStorage::WorldStorage(Vec2U const& worldSize, IODevicePtr const& device, WorldGeneratorFacadePtr const& generatorFacade)
  : WorldStorage() {
  m_tileArray = make_shared<ServerTileSectorArray>(worldSize);
  m_entityMap = make_shared<EntityMap>(worldSize, MinServerEntityId, MaxServerEntityId);
  m_generatorFacade = generatorFacade;
  m_floatingDungeonWorld = false;

  // Creating a new world, clear any existing data.
  device->resize(0);

  openDatabase(m_db, device);

  m_db.insert(metadataKey(), writeWorldMetadata(WorldMetadataStore{worldSize, VersionedJson()}));
  m_db.commit();
}

WorldStorage::WorldStorage(IODevicePtr const& device, WorldGeneratorFacadePtr const& generatorFacade) : WorldStorage() {
  m_generatorFacade = generatorFacade;
  m_floatingDungeonWorld = false;

  openDatabase(m_db, device);

  Vec2U worldSize = readWorldMetadata(*m_db.find(metadataKey())).worldSize;
  m_tileArray = make_shared<ServerTileSectorArray>(worldSize);
  m_entityMap = make_shared<EntityMap>(worldSize, MinServerEntityId, MaxServerEntityId);
}

WorldStorage::WorldStorage(WorldChunks const& chunks, WorldGeneratorFacadePtr const& generatorFacade) : WorldStorage() {
  m_generatorFacade = generatorFacade;
  m_floatingDungeonWorld = false;

  openDatabase(m_db, File::ephemeralFile());

  for (auto const& p : chunks) {
    if (p.second)
      m_db.insert(p.first, *p.second);
  }

  Vec2U worldSize = readWorldMetadata(*m_db.find(metadataKey())).worldSize;
  m_tileArray = make_shared<ServerTileSectorArray>(worldSize);
  m_entityMap = make_shared<EntityMap>(worldSize, MinServerEntityId, MaxServerEntityId);
}

WorldStorage::~WorldStorage() {
  if (m_db.isOpen()) {
    unloadAll(true);
    m_db.close();
  }
}

VersionedJson WorldStorage::worldMetadata() {
  return readWorldMetadata(*m_db.find(metadataKey())).userMetadata;
}

void WorldStorage::setWorldMetadata(VersionedJson const& metadata) {
  m_db.insert(metadataKey(), writeWorldMetadata({Vec2U(m_tileArray->size()), metadata}));
}

Maybe<List<WorldRegionSnapshot>> WorldStorage::captureRegionalSnapshots(uint64_t step) {
  if (!m_regionObservation)
    m_regionObservation = make_unique<WorldRegionObservation>();
  m_regionObservation->reset();
  // loadedSectors() allocates the complete list, so enforce the cap first.
  if (m_tileArray->loadedSectorCount() > WorldRegionObservation::MaxSectors)
    return {};
  WorldRegionPartition partition{WorldGeometry(m_tileArray->size())};
  HashSet<Vec2U> regions;
  for (auto const& sector : m_tileArray->loadedSectors()) {
    auto region = partition.regionForSector(Vec2U(sector));
    if (!region)
      return {};
    regions.add(*region);
    if (regions.size() > WorldRegionCoordinator::MaxRegions)
      return {};
  }
  List<WorldRegionSnapshot> snapshots;
  for (auto const& region : regions) {
    auto bounds = *partition.regionBounds(region);
    for (int64_t x = bounds.xMin(); x < bounds.xMax(); x += WorldSectorSize) {
      for (int64_t y = bounds.yMin(); y < bounds.yMax(); y += WorldSectorSize) {
        if (!m_regionObservation->watch(Vec2U(x / WorldSectorSize, y / WorldSectorSize))) {
          m_regionObservation->reset();
          return {};
        }
      }
    }
    auto snapshot = captureRegionalSnapshot(region, step);
    if (!snapshot) {
      m_regionObservation->reset();
      return {};
    }
    snapshots.append(std::move(*snapshot));
  }
  return snapshots;
}

Maybe<WorldRegionSnapshot> WorldStorage::captureRegionalSnapshot(Vec2U region, uint64_t step) const {
  if (!m_regionObservation)
    return {};
  WorldRegionPartition partition{WorldGeometry(m_tileArray->size())};
  auto bounds = partition.regionBounds(region);
  if (!bounds)
    return {};
  WorldRegionSnapshot snapshot;
  snapshot.region = region;
  snapshot.captureStep = step;
  for (int64_t x = bounds->xMin(); x < bounds->xMax(); x += WorldSectorSize) {
    for (int64_t y = bounds->yMin(); y < bounds->yMax(); y += WorldSectorSize) {
      WorldRegionSectorState state;
      state.sector = Vec2U(x / WorldSectorSize, y / WorldSectorSize);
      auto revision = m_regionObservation->revision(state.sector);
      if (!revision)
        return {};
      state.incarnation = *revision;
      Sector sector(state.sector);
      state.loaded = m_tileArray->sectorLoaded(sector);
      state.contentRevision = m_tileArray->sectorRevision(sector);
      if (state.loaded) {
        auto metadata = m_sectorMetadata.ptr(sector);
        if (!metadata || metadata->loadLevel == SectorLoadLevel::None)
          return {};
        state.loadLevel = static_cast<uint8_t>(metadata->loadLevel);
        state.generationLevel = static_cast<uint8_t>(metadata->generationLevel);
      }
      snapshot.sectors.append(state);
    }
  }
  return snapshot;
}

void WorldStorage::clearRegionalObservations() {
  if (m_regionObservation)
    m_regionObservation->reset();
}

ServerTileSectorArrayPtr const& WorldStorage::tileArray() const {
  return m_tileArray;
}

EntityMapPtr const& WorldStorage::entityMap() const {
  return m_entityMap;
}

Maybe<WorldStorage::Sector> WorldStorage::sectorForPosition(Vec2I const& position) const {
  auto s = m_tileArray->sectorFor(position);
  if (m_tileArray->sectorValid(s))
    return s;
  return {};
}

List<WorldStorage::Sector> WorldStorage::sectorsForRegion(RectI const& region) const {
  return m_tileArray->validSectorsFor(region);
}

Maybe<RectI> WorldStorage::regionForSector(Sector sector) const {
  if (m_tileArray->sectorValid(sector))
    return m_tileArray->sectorRegion(sector);
  return {};
}

SectorLoadLevel WorldStorage::sectorLoadLevel(Sector sector) const {
  return m_sectorMetadata.value(sector).loadLevel;
}

Maybe<SectorGenerationLevel> WorldStorage::sectorGenerationLevel(Sector sector) const {
  if (auto p = m_sectorMetadata.ptr(sector))
    return p->generationLevel;
  return {};
}

bool WorldStorage::sectorActive(Sector sector) const {
  if (auto p = m_sectorMetadata.ptr(sector)) {
    if (p->loadLevel == SectorLoadLevel::Loaded && p->generationLevel == SectorGenerationLevel::Complete)
      return true;
  }
  return false;
}

size_t WorldStorage::sectorThreadCount() const {
  return m_sectorThreads ? m_sectorThreads->count() : 0;
}

bool WorldStorage::sectorThreadPreviewEnabled() const {
  return static_cast<bool>(m_sectorThreads);
}

WorldSectorThreadMetrics WorldStorage::sectorThreadMetrics() const {
  auto result = m_sectorThreads ? m_sectorThreads->metrics() : WorldSectorThreadMetrics{};
  result.loadedSectors = m_tileArray->loadedSectorCount();
  if (m_sectorThreads) {
    for (auto const& entry : m_sectorMetadata) {
      if (entry.second.loadLevel == SectorLoadLevel::Tiles)
        ++result.tilesOnly;
      else if (entry.second.loadLevel == SectorLoadLevel::Entities)
        ++result.withEntities;
    }
  }
  return result;
}

void WorldStorage::sampleSectorThreadMailbox() {
  if (!m_sectorThreads || !m_sectorThreads->mailboxEnabled())
    return;
  for (auto const& entry : m_sectorMetadata) {
    if (entry.second.loadLevel == SectorLoadLevel::None)
      continue;
    m_sectorThreads->sample(entry.first, static_cast<uint8_t>(entry.second.loadLevel),
        static_cast<uint8_t>(entry.second.generationLevel), m_tileArray->sectorRevision(entry.first));
  }
}

void WorldStorage::loadSector(Sector sector) {
  try {
    loadSectorToLevel(sector, SectorLoadLevel::Loaded);
    setSectorTimeToLive(sector, randomizedSectorTTL());
  } catch (std::exception const& e) {
    m_db.rollback();
    m_db.close();
    throw WorldStorageException(strf("Failed to load sector {}", sector), e);
  }
}

void WorldStorage::activateSector(Sector sector) {
  try {
    generateSectorToLevel(sector, SectorGenerationLevel::Complete, NPos, false, highest<double>());
    setSectorTimeToLive(sector, randomizedSectorTTL());
  } catch (std::exception const& e) {
    m_db.rollback();
    m_db.close();
    throw WorldStorageException(strf("Failed to load sector {}", sector), e);
  }
}

void WorldStorage::queueSectorActivation(Sector sector) {
  m_prefetchQueue.remove(sector);
  if (auto p = m_sectorMetadata.ptr(sector)) {
    p->timeToLive = randomizedSectorTTL();
    // Don't bother queueing the sector if it is already fully loaded
    if (p->loadLevel == SectorLoadLevel::Loaded && p->generationLevel == SectorGenerationLevel::Complete)
      return;
  }

  auto p = m_generationQueue.insert(sector, m_generationQueueTimeToLive);
  if (p.second && (m_explorationProfile || m_performanceSamplingEnabled))
    m_generationQueuedAt[sector] = Time::monotonicTime();
  if (m_performanceSamplingEnabled)
    m_performancePeakQueuedSectors = max(m_performancePeakQueuedSectors, m_generationQueue.size());
  p.first->second = m_generationQueueTimeToLive;
  m_generationQueue.toFront(p.first);
}

void WorldStorage::queueSectorPrefetch(Sector sector) {
  if (!m_tileArray->sectorValid(sector) || m_generationQueue.contains(sector))
    return;
  if (auto metadata = m_sectorMetadata.ptr(sector)) {
    if (metadata->generationLevel >= SectorGenerationLevel::BaseTiles)
      return;
  }
  auto inserted = m_prefetchQueue.insert(sector, m_generationQueueTimeToLive);
  inserted.first->second = m_generationQueueTimeToLive;
}

void WorldStorage::triggerTerraformSector(Sector sector) {
  try {
    loadSectorToLevel(sector, SectorLoadLevel::Loaded);
    if (auto p = m_sectorMetadata.ptr(sector)) {
      if (p->generationLevel < SectorGenerationLevel::Complete)
        generateSectorToLevel(sector, SectorGenerationLevel::Complete, NPos, false, highest<double>());

      p->generationLevel = SectorGenerationLevel::Terraform;
      m_tileArray->markSectorDirty(sector);
    } else {
      throw WorldStorageException(strf("Couldn't flag sector {} for terraforming; metadata unavailable", sector));
    }
  } catch (std::exception const& e) {
    m_db.rollback();
    m_db.close();
    throw WorldStorageException(strf("Failed to terraform sector {}", sector), e);
  }
}

RpcPromise<Vec2I> WorldStorage::enqueuePlacement(List<BiomeItemDistribution> distributions, Maybe<DungeonId> id) {
  return m_generatorFacade->enqueuePlacement(std::move(distributions), id);
}

void WorldStorage::setGenerationProfilingEnabled(bool enabled) {
  m_generationProfilingEnabled = enabled;
  m_generationProfile = {};
}

void WorldStorage::setExplorationProfile(ExplorationProfilePtr profile) {
  m_explorationProfile = std::move(profile);
}

void WorldStorage::setPerformanceSamplingEnabled(bool enabled) {
  m_performanceSamplingEnabled = enabled;
  m_performancePeakQueuedSectors = enabled ? m_generationQueue.size() : 0;
  m_performanceLateCompletedSectors = 0;
  m_performanceMaximumQueuedSeconds = 0;
  if (enabled) {
    double now = Time::monotonicTime();
    for (auto const& entry : m_generationQueue) {
      if (!m_generationQueuedAt.contains(entry.first))
        m_generationQueuedAt[entry.first] = now;
    }
  } else if (!m_explorationProfile) {
    m_generationQueuedAt.clear();
  }
}

Json WorldStorage::generationProfilingData() const {
  static constexpr std::array<char const*, GenerationProfileStageCount> stageNames{{
    "baseTiles",
    "microDungeons",
    "caveLiquid",
    "finalize",
    "terraform"
  }};

  JsonObject profile;
  for (size_t i = 0; i < stageNames.size(); ++i) {
    auto const& timing = m_generationProfile[i];
    JsonObject stage;
    stage.set("samples", timing.samples);
    stage.set("totalMilliseconds", timing.totalSeconds * 1000.0);
    stage.set("meanMilliseconds", timing.samples ? timing.totalSeconds * 1000.0 / timing.samples : 0.0);
    stage.set("maximumMilliseconds", timing.maximumSeconds * 1000.0);
    profile.set(stageNames[i], std::move(stage));
  }
  return profile;
}

WorldGenerationQueueSnapshot WorldStorage::generationQueueSnapshot(double overdueSeconds) {
  WorldGenerationQueueSnapshot snapshot;
  snapshot.queuedSectors = m_generationQueue.size();
  snapshot.prefetchedSectors = m_prefetchQueue.size();
  snapshot.peakQueuedSectors = m_performancePeakQueuedSectors;
  snapshot.lateCompletedSectors = m_performanceLateCompletedSectors;
  double now = Time::monotonicTime();
  for (auto const& entry : m_generationQueuedAt) {
    if (!m_generationQueue.contains(entry.first))
      continue;
    double age = max(0.0, now - entry.second);
    snapshot.oldestQueuedSeconds = max(snapshot.oldestQueuedSeconds, age);
    if (age >= overdueSeconds)
      ++snapshot.overdueSectors;
  }
  snapshot.maximumQueuedSeconds = max(m_performanceMaximumQueuedSeconds, snapshot.oldestQueuedSeconds);
  m_performancePeakQueuedSectors = m_generationQueue.size();
  m_performanceLateCompletedSectors = 0;
  m_performanceMaximumQueuedSeconds = snapshot.oldestQueuedSeconds;
  return snapshot;
}

void WorldStorage::recordGenerationProfile(size_t stage, double elapsedSeconds) {
  if (!m_generationProfilingEnabled)
    return;

  auto& timing = m_generationProfile[stage];
  ++timing.samples;
  timing.totalSeconds += elapsedSeconds;
  timing.maximumSeconds = max(timing.maximumSeconds, elapsedSeconds);
}

Maybe<float> WorldStorage::sectorTimeToLive(Sector sector) const {
  if (auto p = m_sectorMetadata.ptr(sector))
    return p->timeToLive;
  return {};
}

bool WorldStorage::setSectorTimeToLive(Sector sector, float newTimeToLive) {
  if (auto p = m_sectorMetadata.ptr(sector)) {
    p->timeToLive = newTimeToLive;
    return true;
  }
  return false;
}

Maybe<Vec2F> WorldStorage::findUniqueEntity(String const& uniqueId) {
  if (auto entity = m_entityMap->entity(m_entityMap->uniqueEntityId(uniqueId)))
    return entity->position();

  // Only return the unique index entry for the entity IF that stored sector is
  // not loaded, if the stored sector is loaded then the entity ought to have
  // been in the live entity map.
  if (auto sectorAndPosition = getUniqueIndexEntry(uniqueId)) {
    if (m_sectorMetadata.value(sectorAndPosition->first).loadLevel < SectorLoadLevel::Entities)
      return sectorAndPosition->second;
  }

  return {};
}

EntityId WorldStorage::loadUniqueEntity(String const& uniqueId) {
  EntityId entityId = m_entityMap->uniqueEntityId(uniqueId);
  if (entityId != NullEntityId)
    return entityId;

  if (auto sectorAndPosition = getUniqueIndexEntry(uniqueId)) {
    loadSector(sectorAndPosition->first);
    return m_entityMap->uniqueEntityId(uniqueId);
  }

  return {};
}

void WorldStorage::generateQueue(Maybe<size_t> sectorGenerationLevelLimit, double timeLimit,
    function<bool(Sector, Sector)> sectorOrdering) {
  STAR_TRACE_ZONE("WorldStorage generate queue");
  try {
    double visiblePassStart = Time::monotonicTime();
    size_t queueDepthAtStart = m_generationQueue.size();
    size_t readyCommittedThisPass = 0;
    size_t completedThisPass = 0;
    size_t levelsAdvancedThisPass = 0;
    double generationDeadline = Time::monotonicTime() + max(0.0, timeLimit);
    if (m_explorationProfile && !m_generationQueue.empty()) {
      m_explorationProfile->count("generation.queueDepthSamples");
      m_explorationProfile->count("generation.queueDepthTotal", m_generationQueue.size());
      double now = Time::monotonicTime();
      double oldestQueuedAt = now;
      Maybe<Sector> oldestSector;
      for (auto const& entry : m_generationQueuedAt) {
        if (!m_generationQueue.contains(entry.first))
          continue;
        if (entry.second < oldestQueuedAt) {
          oldestQueuedAt = entry.second;
          oldestSector = entry.first;
        }
      }
      m_explorationProfile->record("generation.oldestQueuedAge", now - oldestQueuedAt);
      if (oldestSector) {
        m_explorationProfile->recordQueuedSector(Vec2I((int)(*oldestSector)[0], (int)(*oldestSector)[1]),
            now - oldestQueuedAt);
        if (!m_oldestQueueDiagnosticSector || *m_oldestQueueDiagnosticSector != *oldestSector) {
          m_oldestQueueDiagnosticSector = *oldestSector;
          m_lastOldestQueueDiagnosticBucket = 0;
          m_oldestQueueAttempts = 0;
          m_oldestQueueFullAttempts = 0;
          m_oldestQueueAdvances = 0;
          m_oldestQueueBlockReason = "notSelected";
          m_oldestQueueBlockingNeighbor.reset();
        }
        // One diagnostic per ten-second age band, including the exact sector
        // and its current generation level. The queue may contain other
        // sectors that continue to make progress meanwhile.
        size_t bucket = static_cast<size_t>((now - oldestQueuedAt) / 10.0);
        if (bucket >= 1 && bucket > m_lastOldestQueueDiagnosticBucket) {
          auto metadata = m_sectorMetadata.ptr(*oldestSector);
          auto blockerMetadata = m_oldestQueueBlockingNeighbor
              ? m_sectorMetadata.ptr(*m_oldestQueueBlockingNeighbor) : nullptr;
          Logger::info("[SectorQueue] oldest={} age={:.1f}s generationLevel={} loadLevel={} queued={} attempts={} fullAttempts={} advances={} lastBlock={} blocker={} blockerLevel={}",
              *oldestSector, now - oldestQueuedAt,
              metadata ? static_cast<int>(metadata->generationLevel) : -1,
              metadata ? static_cast<int>(metadata->loadLevel) : -1,
              m_generationQueue.size(), m_oldestQueueAttempts, m_oldestQueueFullAttempts,
              m_oldestQueueAdvances, m_oldestQueueBlockReason,
              m_oldestQueueBlockingNeighbor,
              blockerMetadata ? static_cast<int>(blockerMetadata->generationLevel) : -1);
          m_oldestQueueAttempts = 0;
          m_oldestQueueFullAttempts = 0;
          m_oldestQueueAdvances = 0;
        }
        m_lastOldestQueueDiagnosticBucket = bucket;
      } else {
        m_lastOldestQueueDiagnosticBucket = 0;
        m_oldestQueueDiagnosticSector.reset();
      }
    }
    if (sectorOrdering) {
      ExplorationProfile::Scope timing(m_explorationProfile.get(), "generation.queueSort");
      m_generationQueue.sort([&sectorOrdering](auto const& a, auto const& b) {
          return sectorOrdering(a.first, b.first);
        });
    }

    // This is a bounded shadow of the recursive prerequisite rule below.  It
    // observes a bounded prefix of queued sectors only and never changes queue order,
    // loading, generation or worker admission.  The data identifies whether a
    // future regional scheduler is blocked by stage prerequisites rather than
    // by its own policy.
    if (m_explorationProfile && m_explorationProfile->regionalDetailsEnabled() && !m_generationQueue.empty()) {
      ExplorationProfile::Scope timing(m_explorationProfile.get(), "generation.dependencies.observe");
      static constexpr size_t MaxObservedRoots = 8;
      List<WorldGenerationDependencyNode> roots;
      for (auto const& entry : m_generationQueue) {
        roots.append(WorldGenerationDependencyNode{Vec2U(entry.first), static_cast<uint8_t>(SectorGenerationLevel::Complete)});
        if (roots.size() == MaxObservedRoots)
          break;
      }
      auto rootCount = roots.size();
      auto plan = WorldGenerationDependencyGraph::build(std::move(roots),
          [this](Vec2U const& observed) {
            List<Vec2U> result;
            auto neighbors = adjacentSectors(Sector(observed[0], observed[1]));
            for (auto const& neighbor : neighbors)
              result.append(Vec2U(neighbor[0], neighbor[1]));
            return result;
          });
      auto analysis = WorldGenerationDependencyGraph::analyze(plan, [this](Vec2U const& observed) {
          if (auto metadata = m_sectorMetadata.ptr(Sector(observed[0], observed[1])))
            return static_cast<uint8_t>(metadata->generationLevel);
          return uint8_t(0);
        });
      m_explorationProfile->count("generation.dependencies.samples");
      m_explorationProfile->count("generation.dependencies.roots", rootCount);
      m_explorationProfile->count("generation.dependencies.nodes", plan.nodes.size());
      m_explorationProfile->count("generation.dependencies.edges", plan.edges.size());
      m_explorationProfile->count("generation.dependencies.satisfied", analysis.satisfiedNodes);
      m_explorationProfile->count("generation.dependencies.runnable", analysis.runnableNodes);
      m_explorationProfile->count("generation.dependencies.blocked", analysis.blockedNodes);
      m_explorationProfile->count("generation.dependencies.directBlockers", analysis.directBlockers);
      m_explorationProfile->count("generation.dependencies.frontier", analysis.frontier.size());
      if (plan.truncated)
        m_explorationProfile->count("generation.dependencies.truncated");
    }

    // Drain already loaded BaseTiles before admitting more work. These jobs
    // have no neighbor-generation prerequisites; routing them through
    // generateSectorToLevel would also load entities in prefetched sectors.
    auto readySectors = m_generatorFacade->readyBaseTileSectors(this);
    if (sectorOrdering)
      std::sort(readySectors.begin(), readySectors.end(), sectorOrdering);
    for (auto const& sector : readySectors) {
      if ((sectorGenerationLevelLimit && *sectorGenerationLevelLimit == 0)
          || Time::monotonicTime() >= generationDeadline)
        break;
      auto metadata = m_sectorMetadata.ptr(sector);
      if (!metadata || metadata->loadLevel < SectorLoadLevel::Tiles
          || metadata->generationLevel >= SectorGenerationLevel::BaseTiles)
        continue;
      ExplorationProfile::Scope timing(m_explorationProfile.get(), "generation.baseTiles");
      auto result = m_generatorFacade->generateSectorLevel(this, sector, SectorGenerationLevel::BaseTiles, true);
      timing.stop();
      if (!result.complete)
        continue;
      metadata->generationLevel = SectorGenerationLevel::BaseTiles;
      m_tileArray->markSectorDirty(sector);
      m_prefetchQueue.remove(sector);
      if (sectorGenerationLevelLimit)
        --*sectorGenerationLevelLimit;
      if (result.workerElapsedSeconds) {
        recordGenerationProfile(0, *result.workerElapsedSeconds);
        if (m_explorationProfile)
          m_explorationProfile->record("generation.baseTiles.worker", *result.workerElapsedSeconds);
      }
      if (m_explorationProfile)
        m_explorationProfile->count("generation.baseTiles.readyResultsCommitted");
      ++readyCommittedThisPass;
      ++levelsAdvancedThisPass;
    }

    bool schedulingOnly = false;
    size_t visitedSectors = 0;
    while (!m_generationQueue.empty()) {
      if ((sectorGenerationLevelLimit && *sectorGenerationLevelLimit == 0) || Time::monotonicTime() >= generationDeadline)
        break;

      auto sector = m_generationQueue.firstKey();
      auto targetLevel = schedulingOnly ? SectorGenerationLevel::BaseTiles : SectorGenerationLevel::Complete;
      m_traceOldestQueueAttempt = m_explorationProfile && m_oldestQueueDiagnosticSector
          && sector == *m_oldestQueueDiagnosticSector;
      if (m_traceOldestQueueAttempt) {
        ++m_oldestQueueAttempts;
        if (!schedulingOnly)
          ++m_oldestQueueFullAttempts;
        m_oldestQueueBlockReason = "none";
        m_oldestQueueBlockRecorded = false;
        m_oldestQueueBlockingNeighbor.reset();
      }
      auto p = generateSectorToLevel(sector, targetLevel,
          sectorGenerationLevelLimit.value(NPos), true, generationDeadline);
      if (m_traceOldestQueueAttempt && p.second)
        ++m_oldestQueueAdvances;
      m_traceOldestQueueAttempt = false;
      if (!schedulingOnly && p.first) {
        ++completedThisPass;
        m_generationQueue.removeFirst();
        if (auto queuedAt = m_generationQueuedAt.ptr(sector)) {
          double wait = Time::monotonicTime() - *queuedAt;
          if (m_explorationProfile) {
            m_explorationProfile->record("generation.sectorWait", wait);
            m_explorationProfile->recordCompletedSector(Vec2I((int)sector[0], (int)sector[1]), wait);
            m_explorationProfile->count("generation.sectorsCompleted");
          }
          if (m_performanceSamplingEnabled) {
            m_performanceMaximumQueuedSeconds = max(m_performanceMaximumQueuedSeconds, wait);
            if (wait >= 0.1)
              ++m_performanceLateCompletedSectors;
          }
          m_generationQueuedAt.remove(sector);
        }
      } else if (!schedulingOnly) {
        // Preserve ordered processing of higher generation levels. Once the
        // first requested sector blocks on asynchronous base tiles, later
        // sectors may only schedule or commit their independent BaseTiles
        // stage during this pass.
        schedulingOnly = true;
        m_generationQueue.toBack(sector);
      } else {
        m_generationQueue.toBack(sector);
      }
      if (sectorGenerationLevelLimit)
        *sectorGenerationLevelLimit -= p.second;
      levelsAdvancedThisPass += p.second;
      if (schedulingOnly && ++visitedSectors >= m_generationQueue.size())
        break;
    }
    if (!m_generationQueue.empty() && Time::monotonicTime() >= generationDeadline && m_explorationProfile)
      m_explorationProfile->count("generation.timeBudgetExhausted");
    if (m_explorationProfile && queueDepthAtStart) {
      m_explorationProfile->count("generation.visiblePasses");
      m_explorationProfile->count("generation.visiblePassQueueDepthEndTotal", m_generationQueue.size());
      m_explorationProfile->count("generation.visiblePassSectorsCompleted", completedThisPass);
      m_explorationProfile->count("generation.visiblePassReadyCommitted", readyCommittedThisPass);
      m_explorationProfile->count("generation.visiblePassLevelsAdvanced", levelsAdvancedThisPass);
      m_explorationProfile->record("generation.visiblePass", Time::monotonicTime() - visiblePassStart);
      if (!completedThisPass && !levelsAdvancedThisPass)
        m_explorationProfile->count("generation.visiblePassesWithoutProgress");
      else if (!completedThisPass)
        m_explorationProfile->count("generation.visiblePassesWithPartialProgress");
    }
    // Background preparation uses only time left after visible sectors. It
    // never initializes entities or runs the shared microdungeon stages.
    size_t examined = 0;
    size_t pending = m_prefetchQueue.size();
    while (m_generationQueue.empty() && !m_prefetchQueue.empty()
        && examined++ < pending && Time::monotonicTime() < generationDeadline) {
      auto sector = m_prefetchQueue.firstKey();
      if (m_generationQueue.contains(sector)) {
        m_prefetchQueue.removeFirst();
        continue;
      }
      loadSectorToLevel(sector, SectorLoadLevel::Tiles);
      auto& metadata = m_sectorMetadata[sector];
      if (metadata.generationLevel < SectorGenerationLevel::BaseTiles) {
        auto result = m_generatorFacade->generateSectorLevel(this, sector, SectorGenerationLevel::BaseTiles, true);
        if (result.complete) {
          metadata.generationLevel = SectorGenerationLevel::BaseTiles;
          m_tileArray->markSectorDirty(sector);
          if (m_explorationProfile)
            m_explorationProfile->count("generation.prefetchCompleted");
        }
      }
      if (metadata.generationLevel >= SectorGenerationLevel::BaseTiles)
        m_prefetchQueue.removeFirst();
      else
        m_prefetchQueue.toBack(sector);
    }
  } catch (std::exception const& e) {
    m_db.rollback();
    m_db.close();
    throw WorldStorageException("WorldStorage generation failed while generating from queue", e);
  }
}

void WorldStorage::tick(float dt, String const* worldId) {
  try {
    // Tick down generation queue entries, and erase any that are expired.
    eraseWhere(m_generationQueue, [this, dt](auto& p) {
        p.second -= dt;
        if (p.second <= 0.0f) {
          if (m_performanceSamplingEnabled) {
            if (auto queuedAt = m_generationQueuedAt.ptr(p.first))
              m_performanceMaximumQueuedSeconds = max(m_performanceMaximumQueuedSeconds,
                  Time::monotonicTime() - *queuedAt);
          }
          m_generationQueuedAt.remove(p.first);
          if (m_explorationProfile)
            m_explorationProfile->count("generation.sectorsExpired");
          return true;
        }
        return false;
      });
    eraseWhere(m_prefetchQueue, [dt](auto& p) {
        p.second -= dt;
        return p.second <= 0.0f;
      });

    // Tick down sector TTL values
    for (auto& p : m_sectorMetadata)
      p.second.timeToLive -= dt;

    // Loop over every loaded sector, figure out whether the sector needs to be
    // unloaded, kept alive by a keep-alive entity, or has any entities that need
    // to be stored because they moved into an entity-unloaded sector (zombies).
    auto entityFactory = Root::singleton().entityFactory();
    unsigned unloaded = 0, skipped = 0;
    for (auto const& p : m_sectorMetadata.pairs()) {
      auto const& sector = p.first;
      auto const& metadata = p.second;

      bool needsUnload = metadata.timeToLive <= 0.0f;

      // If it is not time to unload the sector, then we don't need to scan for
      // keep-alive entities.  If the sector is fully loaded, it can not have any
      // zombie entities.  If both of these are true, there is no work to do.
      if (!needsUnload && metadata.loadLevel == SectorLoadLevel::Entities)
        continue;

      bool keepAlive = false;
      List<EntityPtr> zombieEntities;
      m_entityMap->forEachEntity(RectF(m_tileArray->sectorRegion(sector)), [&](EntityPtr const& entity) {
          if (belongsInSector(sector, entity->position())) {
            if (!keepAlive && m_generatorFacade->entityKeepAlive(this, entity))
              keepAlive = true;
            else if (metadata.loadLevel < SectorLoadLevel::Entities)
              zombieEntities.append(entity);
          }
        });

      if (keepAlive) {
        setSectorTimeToLive(sector, randomizedSectorTTL());
      } else if (needsUnload) {
        (unloadSectorToLevel(sector, SectorLoadLevel::None) ? unloaded : skipped)++;
      } else if (!zombieEntities.empty()) {
        List<EntityPtr> zombiesToStore;
        List<EntityPtr> zombiesToRemove;
        for (auto const& entity : zombieEntities) {
          if (m_generatorFacade->entityPersistent(this, entity))
            zombiesToStore.append(entity);
          else
            zombiesToRemove.append(entity);
        }

        for (auto const& entity : zombiesToRemove) {
          m_entityMap->removeEntity(entity->entityId());
          m_generatorFacade->destructEntity(this, entity);
        }

        if (!zombiesToStore.empty()) {
          EntitySectorStore sectorStore;
          if (auto res = m_db.find(entitySectorKey(sector)))
            sectorStore = readEntitySector(*res);

          UniqueIndexStore storedUniques;
          for (auto const& entity : zombiesToStore) {
            m_entityMap->removeEntity(entity->entityId());
            m_generatorFacade->destructEntity(this, entity);
            if (auto uniqueId = entity->uniqueId())
              storedUniques.add(*uniqueId, {sector, entity->position()});
            sectorStore.append(entityFactory->storeVersionedEntity(entity));
          }
          m_db.insert(entitySectorKey(sector), writeEntitySector(sectorStore));
          mergeSectorUniques(sector, storedUniques);
        }
      }
    }
    if (worldId) {
      LogMap::set(strf("server_{}_storage", *worldId),
        strf("{} active, {}/{} unloaded ({} held)", m_sectorMetadata.size(), unloaded, skipped + unloaded, skipped));
    }
  } catch (std::exception const& e) {
    m_db.rollback();
    m_db.close();
    throw WorldStorageException("WorldStorage exception during tick", e);
  }
}

void WorldStorage::unloadAll(bool force) {
  try {
    auto storageConfig = Root::singleton().assets()->json("/worldstorage.config");
    auto sectors = m_sectorMetadata.keys();

    // Entities can do some strange things during unload, such as repeatedly
    // creating new entities during uninit, or setting their bounding box null
    // or being entirely outside of the world geometry.  This limits the number
    // of tries to completely uninit and store all entities before giving up
    // and just letting some entities not be stored.
    if (m_entityMap) {
      unsigned forceUnloadTries = storageConfig.getUInt("forceUnloadTries");
      for (unsigned i = 0; i < forceUnloadTries; ++i) {
        for (auto& sector : sectors)
          unloadSectorToLevel(sector, SectorLoadLevel::Tiles, force);

        if (!force || m_entityMap->size() == 0)
          break;
      }
    }
    for (auto& sector : sectors)
      unloadSectorToLevel(sector, SectorLoadLevel::None, force);

  } catch (std::exception const& e) {
    m_db.rollback();
    m_db.close();
    throw WorldStorageException("WorldStorage exception during unload", e);
  }
}

void WorldStorage::sync() {
  STAR_TRACE_ZONE("WorldStorage sync");
  ExplorationProfile::Scope timing(m_explorationProfile.get(), "storage.sync.total");
  try {
    for (auto const& pair : m_sectorMetadata)
      syncSector(pair.first);
    ExplorationProfile::Scope commitTiming(m_explorationProfile.get(), "storage.sync.commit");
    m_db.commit();
  } catch (std::exception const& e) {
    m_db.rollback();
    m_db.close();
    throw WorldStorageException("WorldStorage exception during sync", e);
  }
}

WorldChunks WorldStorage::readChunks() {
  try {
    for (auto const& pair : m_sectorMetadata)
      syncSector(pair.first);

    WorldChunks chunks;
    m_db.forAll([&chunks](ByteArray k, ByteArray v) {
        chunks.add(std::move(k), std::move(v));
      });

    return WorldChunks(chunks);

  } catch (std::exception const& e) {
    m_db.rollback();
    m_db.close();
    throw WorldStorageException("WorldStorage exception during readChunks", e);
  }
}

bool WorldStorage::floatingDungeonWorld() const {
  return m_floatingDungeonWorld;
}

void WorldStorage::setFloatingDungeonWorld(bool floatingDungeonWorld) {
  m_floatingDungeonWorld = floatingDungeonWorld;
}

WorldStorage::TileSectorStore::TileSectorStore()
  : tileSerializationVersion(ServerTile::CurrentSerializationVersion) {}

WorldStorage::SectorMetadata::SectorMetadata()
  : loadLevel(SectorLoadLevel::None), generationLevel(SectorGenerationLevel::None), timeToLive(0.0f) {}

ByteArray WorldStorage::metadataKey() {
  DataStreamBuffer metadata(5);
  metadata.write(StoreType::Metadata);
  return metadata.takeData();
}

WorldStorage::WorldMetadataStore WorldStorage::readWorldMetadata(ByteArray const& data) {
  DataStreamBuffer ds(uncompressData(data));

  WorldMetadataStore metadata;
  ds.read(metadata.worldSize);
  ds.read(metadata.userMetadata);
  VersionedJson::readSubVersioning(ds, metadata.userMetadata);
  return metadata;
}

ByteArray WorldStorage::writeWorldMetadata(WorldMetadataStore const& metadata) {
  DataStreamBuffer ds;

  ds.write(metadata.worldSize);
  ds.write(metadata.userMetadata);
  VersionedJson::writeSubVersioning(ds, metadata.userMetadata);
  return compressData(ds.data());
}

ByteArray WorldStorage::entitySectorKey(Sector const& sector) {
  DataStreamBuffer ds(5);
  ds.write(StoreType::EntitySector);
  ds.cwrite<uint16_t>(sector[0]);
  ds.cwrite<uint16_t>(sector[1]);
  return ds.takeData();
}

WorldStorage::EntitySectorStore WorldStorage::readEntitySector(ByteArray const& data, ExplorationProfile* profile) {
  ExplorationProfile::Scope timing(profile, "storage.entities.decompress");
  DataStreamBuffer ds(uncompressData(data));
  timing.next("storage.entities.deserialize");
  auto store = ds.read<EntitySectorStore>();
  for (auto& entity : store) {
    VersionedJson::readSubVersioning(ds, entity);
  }
  return store;
}

ByteArray WorldStorage::writeEntitySector(EntitySectorStore const& store) {
  DataStreamBuffer ds;
  ds.write(store);
  for (auto& entity : store) {
    VersionedJson::writeSubVersioning(ds, entity);
  }
  return compressData(ds.data());
}

ByteArray WorldStorage::tileSectorKey(Sector const& sector) {
  DataStreamBuffer ds(5);
  ds.write(StoreType::TileSector);
  ds.cwrite<uint16_t>(sector[0]);
  ds.cwrite<uint16_t>(sector[1]);
  return ds.takeData();
}

WorldStorage::TileSectorStore WorldStorage::readTileSector(ByteArray const& data, ExplorationProfile* profile) {
  auto& root = Root::singleton();
  auto matDatabase = root.materialDatabase();
  auto liqDatabase = root.liquidsDatabase();
  auto storageConfig = root.assets()->json("/worldstorage.config");

  ExplorationProfile::Scope timing(profile, "storage.tiles.decompress");
  DataStreamBuffer ds(uncompressData(data));
  timing.next("storage.tiles.deserialize");
  TileSectorStore store;
  ds.vuread(store.generationLevel);
  ds.vuread(store.tileSerializationVersion);

  store.tiles.reset(new TileArray());
  for (size_t y = 0; y < WorldSectorSize; ++y) {
    for (size_t x = 0; x < WorldSectorSize; ++x) {
      ServerTile tile;
      tile.read(ds, store.tileSerializationVersion);

      if (!matDatabase->isValidMaterialId(tile.foreground))
        tile.foreground = storageConfig.getUInt("replacementMaterialId");
      if (!matDatabase->isValidMaterialId(tile.background))
        tile.background = storageConfig.getUInt("replacementMaterialId");
      if (!matDatabase->isValidModId(tile.foregroundMod))
        tile.foregroundMod = storageConfig.getUInt("replacementModId");
      if (!matDatabase->isValidModId(tile.backgroundMod))
        tile.backgroundMod = storageConfig.getUInt("replacementModId");
      if (!liqDatabase->isValidLiquidId(tile.liquid.liquid)) {
        LiquidId replacementLiquid = storageConfig.getUInt("replacementLiquidId");
        if (replacementLiquid == EmptyLiquidId)
          tile.liquid = LiquidStore();
        else
          tile.liquid.liquid = replacementLiquid;
      }

      (*store.tiles)(x, y) = tile;
    }
  }

  return store;
}

ByteArray WorldStorage::writeTileSector(TileSectorStore const& store) {
  DataStreamBuffer ds;
  ds.vuwrite(store.generationLevel);
  ds.vuwrite(store.tileSerializationVersion);
  starAssert(store.tiles);
  for (size_t y = 0; y < WorldSectorSize; ++y) {
    for (size_t x = 0; x < WorldSectorSize; ++x)
      (*store.tiles)(x, y).write(ds);
  }
  return compressData(ds.takeData());
}

ByteArray WorldStorage::uniqueIndexKey(String const& uniqueId) {
  DataStreamBuffer ds(5);
  ds.write(StoreType::UniqueIndex);
  ds.write(xxHash32(uniqueId));
  return ds.takeData();
}

WorldStorage::UniqueIndexStore WorldStorage::readUniqueIndexStore(ByteArray const& data) {
  return DataStreamBuffer::deserializeMapContainer<UniqueIndexStore>(uncompressData(data),
      [](DataStream& ds, String& key, SectorAndPosition& value) {
        ds.read(key);
        ds.cread<uint16_t>(value.first[0]);
        ds.cread<uint16_t>(value.first[1]);
        ds.read(value.second);
      });
}

ByteArray WorldStorage::writeUniqueIndexStore(UniqueIndexStore const& store) {
  return compressData(DataStreamBuffer::serializeMapContainer(store,
      [](DataStream& ds, String const& key, SectorAndPosition const& value) {
        ds.write(key);
        ds.cwrite<uint16_t>(value.first[0]);
        ds.cwrite<uint16_t>(value.first[1]);
        ds.write(value.second);
      }));
}

ByteArray WorldStorage::sectorUniqueKey(Sector const& sector) {
  DataStreamBuffer ds(5);
  ds.write(StoreType::SectorUniques);
  ds.cwrite<uint16_t>(sector[0]);
  ds.cwrite<uint16_t>(sector[1]);
  return ds.takeData();
}

WorldStorage::SectorUniqueStore WorldStorage::readSectorUniqueStore(ByteArray const& data) {
  return DataStreamBuffer::deserialize<SectorUniqueStore>(uncompressData(data));
}

ByteArray WorldStorage::writeSectorUniqueStore(SectorUniqueStore const& store) {
  return compressData(DataStreamBuffer::serialize(store));
}

void WorldStorage::openDatabase(BTreeDatabase& db, IODevicePtr device) {
  db.setContentIdentifier("World4");
  db.setKeySize(5);
  db.setIODevice(std::move(device));
  db.setBlockSize(2048);
  db.setAutoCommit(false);
  db.open();

  if (db.contentIdentifier() != "World4" || db.keySize() != 5)
    throw WorldStorageException::format("World database format is too old or unrecognized!");
}

WorldStorage::WorldStorage() {
  auto storageConfig = Root::singleton().assets()->json("/worldstorage.config");
  m_sectorTimeToLive = jsonToVec2F(storageConfig.get("sectorTimeToLive"));
  m_generationQueueTimeToLive = storageConfig.getFloat("generationQueueTimeToLive");
  auto threading = Root::singleton().configuration()->get("worldThreading", JsonObject());
  if (threading.getBool("sectorThreadLifecyclePreview", false))
    m_sectorThreads = make_unique<WorldSectorThreads>(threading.getBool("sectorThreadMailboxPreview", false));
}

bool WorldStorage::belongsInSector(Sector const& sector, Vec2F const& position) const {
  WorldGeometry geometry(m_tileArray->size());
  return RectF(m_tileArray->sectorRegion(sector)).belongs(geometry.limit(position));
}

float WorldStorage::randomizedSectorTTL() const {
  return Random::randf(m_sectorTimeToLive[0], m_sectorTimeToLive[1]);
}

pair<bool, size_t> WorldStorage::generateSectorToLevel(Sector const& sector, SectorGenerationLevel targetGenerationLevel,
    size_t sectorGenerationLevelLimit, bool allowAsync, double generationDeadline) {
  STAR_TRACE_ZONE("WorldStorage generate sector");
  TileRevisionScope revisionScope(TileRevisionCause::WorldGeneration, m_tileArray->revisionTrackingEnabled());
  if (Time::monotonicTime() >= generationDeadline) {
    if (m_traceOldestQueueAttempt && !m_oldestQueueBlockRecorded) {
      m_oldestQueueBlockReason = "deadline";
      m_oldestQueueBlockRecorded = true;
    }
    if (m_explorationProfile)
      m_explorationProfile->count("generation.blocked.deadline");
    return {false, 0};
  }

  if (!m_tileArray->sectorValid(sector)) {
    if (m_traceOldestQueueAttempt && !m_oldestQueueBlockRecorded) {
      m_oldestQueueBlockReason = "invalidSector";
      m_oldestQueueBlockRecorded = true;
    }
    if (m_explorationProfile)
      m_explorationProfile->count("generation.blocked.invalidSector");
    return {false, 0};
  }

  loadSectorToLevel(sector, SectorLoadLevel::Loaded);

  auto& metadata = m_sectorMetadata[sector];

  if (targetGenerationLevel == SectorGenerationLevel::Complete && metadata.generationLevel == SectorGenerationLevel::Terraform) {
    ExplorationProfile::Scope timing(m_explorationProfile.get(), "generation.terraform");
    double profileStart = m_generationProfilingEnabled ? Time::monotonicTime() : 0.0;
    m_generatorFacade->terraformSector(this, sector);
    if (m_generationProfilingEnabled)
      recordGenerationProfile(4, Time::monotonicTime() - profileStart);
    metadata.generationLevel = SectorGenerationLevel::Complete;
    m_tileArray->markSectorDirty(sector);
    metadata.timeToLive = randomizedSectorTTL();
    return {true, 1};
  }

  if (metadata.generationLevel >= targetGenerationLevel)
    return {true, 0};

  metadata.timeToLive = randomizedSectorTTL();

  size_t totalGeneratedLevels = 0;
  for (uint8_t i = (uint8_t)metadata.generationLevel + 1; i <= (uint8_t)targetGenerationLevel; ++i) {
    SectorGenerationLevel currentGeneration = (SectorGenerationLevel)i;
    SectorGenerationLevel stepDownGeneration = (SectorGenerationLevel)(i - 1);

    if (stepDownGeneration != SectorGenerationLevel::None) {
      for (auto adjacentSector : adjacentSectors(sector)) {
        auto p = generateSectorToLevel(adjacentSector, stepDownGeneration,
            sectorGenerationLevelLimit - totalGeneratedLevels, allowAsync, generationDeadline);
        totalGeneratedLevels += p.second;
        if (!p.first || totalGeneratedLevels >= sectorGenerationLevelLimit) {
          if (m_traceOldestQueueAttempt && !m_oldestQueueBlockRecorded) {
            m_oldestQueueBlockReason = !p.first ? "neighbor" : "levelLimit";
            m_oldestQueueBlockingNeighbor = adjacentSector;
            m_oldestQueueBlockRecorded = true;
          }
          if (m_explorationProfile)
            m_explorationProfile->count(!p.first ? "generation.blocked.neighbor" : "generation.blocked.levelLimit");
          return {false, totalGeneratedLevels};
        }
      }
    }

    static char const* const generationStages[] = {
      "generation.baseTiles", "generation.microDungeons", "generation.caveLiquid", "generation.finalize", "generation.terraform"
    };
    static char const* const workerStages[] = {
      "generation.baseTiles.worker", "generation.microDungeons.worker",
      "generation.caveLiquid.worker", "generation.finalize.worker", "generation.terraform.worker"
    };
    size_t stageIndex = (size_t)currentGeneration - 1;
    ExplorationProfile::Scope timing(m_explorationProfile.get(), generationStages[stageIndex]);
    double profileStart = m_generationProfilingEnabled ? Time::monotonicTime() : 0.0;
    auto generationResult = m_generatorFacade->generateSectorLevel(this, sector, currentGeneration, allowAsync);
    if (!generationResult.complete) {
      if (m_traceOldestQueueAttempt && !m_oldestQueueBlockRecorded) {
        m_oldestQueueBlockReason = generationStages[stageIndex];
        m_oldestQueueBlockingNeighbor = sector;
        m_oldestQueueBlockRecorded = true;
      }
      if (m_explorationProfile) {
        static char const* const blockedStages[] = {
          "generation.blocked.baseTiles", "generation.blocked.microDungeons",
          "generation.blocked.caveLiquid", "generation.blocked.finalize", "generation.blocked.terraform"
        };
        m_explorationProfile->count(blockedStages[stageIndex]);
      }
      timing.stop();
      return {false, totalGeneratedLevels};
    }
    timing.stop();
    if (generationResult.workerElapsedSeconds) {
      if (m_explorationProfile)
        m_explorationProfile->record(workerStages[stageIndex], *generationResult.workerElapsedSeconds);
      recordGenerationProfile(stageIndex, *generationResult.workerElapsedSeconds);
    } else if (m_generationProfilingEnabled) {
      recordGenerationProfile(stageIndex, Time::monotonicTime() - profileStart);
    }
    metadata.generationLevel = currentGeneration;
    // A generation stage can alter an array directly or update its persisted
    // generation level without passing through modifyTile().
    m_tileArray->markSectorDirty(sector);

    ++totalGeneratedLevels;
    if (totalGeneratedLevels >= sectorGenerationLevelLimit || Time::monotonicTime() >= generationDeadline) {
      if (m_traceOldestQueueAttempt && !m_oldestQueueBlockRecorded) {
        m_oldestQueueBlockReason = totalGeneratedLevels >= sectorGenerationLevelLimit ? "levelLimit" : "deadline";
        m_oldestQueueBlockRecorded = true;
      }
      if (m_explorationProfile)
        m_explorationProfile->count(totalGeneratedLevels >= sectorGenerationLevelLimit
            ? "generation.blocked.levelLimit" : "generation.blocked.deadline");
      return {metadata.generationLevel == targetGenerationLevel, totalGeneratedLevels};
    }
  }

  return {true, totalGeneratedLevels};
}

void WorldStorage::loadSectorToLevel(Sector const& sector, SectorLoadLevel targetLoadLevel) {
  STAR_TRACE_ZONE("WorldStorage load sector");
  TileRevisionScope revisionScope(TileRevisionCause::Storage, m_tileArray->revisionTrackingEnabled());
  if (!m_tileArray->sectorValid(sector))
    return;

  auto entityFactory = Root::singleton().entityFactory();

  auto& metadata = m_sectorMetadata[sector];
  if (metadata.loadLevel >= targetLoadLevel)
    return;

  metadata.timeToLive = randomizedSectorTTL();

  for (uint8_t i = (uint8_t)metadata.loadLevel + 1; i <= (uint8_t)targetLoadLevel; ++i) {
    SectorLoadLevel currentLoad = (SectorLoadLevel)i;
    SectorLoadLevel stepDownLoad = (SectorLoadLevel)(i - 1);

    if (stepDownLoad != SectorLoadLevel::None) {
      for (auto adjacentSector : adjacentSectors(sector))
        loadSectorToLevel(adjacentSector, stepDownLoad);
    }

    if (currentLoad == SectorLoadLevel::Tiles) {
      if (m_sectorThreads)
        m_sectorThreads->loaded(sector);
      ExplorationProfile::Scope total(m_explorationProfile.get(), "storage.tiles.total");
      ExplorationProfile::Scope timing(m_explorationProfile.get(), "storage.tiles.read");
      auto res = m_db.find(tileSectorKey(sector));
      timing.stop();
      if (res) {
        if (m_explorationProfile) {
          m_explorationProfile->count("tileSectorsFromStorage");
          m_explorationProfile->count("tileBytesFromStorage", res->size());
        }
        TileSectorStore sectorStore = readTileSector(*res, m_explorationProfile.get());
        timing.next("storage.tiles.install");
        m_tileArray->loadSector(sector, std::move(sectorStore.tiles));
        markTileSectorSynced(sector);

        metadata.generationLevel = sectorStore.generationLevel;
      } else {
        if (m_explorationProfile)
          m_explorationProfile->count("tileSectorsDefault");
        timing.next("storage.tiles.installDefault");
        if (!m_tileArray->sectorLoaded(sector))
          m_tileArray->loadDefaultSector(sector);
        // New sectors have no database record yet. Persist the default state
        // on the next sync so a crash cannot change their generation history.
        m_tileArray->markSectorDirty(sector);
        m_syncedTileRevisions.remove(sector);
      }

      metadata.loadLevel = currentLoad;
      if (m_regionObservation)
        m_regionObservation->changed(Vec2U(sector));
      timing.next("storage.tiles.callbacks");
      m_generatorFacade->sectorLoadLevelChanged(this, sector, currentLoad);

    } else if (currentLoad == SectorLoadLevel::Entities) {
      ExplorationProfile::Scope total(m_explorationProfile.get(), "storage.entities.total");
      List<EntityPtr> addedEntities;
      ExplorationProfile::Scope timing(m_explorationProfile.get(), "storage.entities.read");
      auto res = m_db.find(entitySectorKey(sector));
      timing.stop();
      if (res) {
        if (m_explorationProfile) {
          m_explorationProfile->count("entitySectorsFromStorage");
          m_explorationProfile->count("entityBytesFromStorage", res->size());
        }
        EntitySectorStore sectorStore = readEntitySector(*res, m_explorationProfile.get());
        timing.next("storage.entities.create");
        for (auto const& entityStore : sectorStore) {
          try {
            addedEntities.append(entityFactory->loadVersionedEntity(entityStore));
          } catch (std::exception const& e) {
            Logger::warn("Failed to deserialize entity '{}'. {}", entityStore.toJson(), outputException(e, true));
          }
        }
      }

      timing.next("storage.entities.init");
      if (m_explorationProfile)
        m_explorationProfile->count("entitiesLoaded", addedEntities.size());
      UniqueIndexStore readUniques;
      for (auto const& entity : addedEntities) {
        m_generatorFacade->initEntity(this, m_entityMap->reserveEntityId(), entity);
        m_entityMap->addEntity(entity);
        if (auto uniqueId = entity->uniqueId())
          readUniques.add(*uniqueId, {sector, entity->position()});
      }

      // Update the stored unique ids on load, in case a desync has happened
      // and there are stale entries in the index.
      timing.next("storage.entities.uniqueIndex");
      updateSectorUniques(sector, readUniques);

      metadata.loadLevel = currentLoad;
      if (m_regionObservation)
        m_regionObservation->changed(Vec2U(sector));
      timing.next("storage.entities.callbacks");
      m_generatorFacade->sectorLoadLevelChanged(this, sector, currentLoad);
    }
  }
}

bool WorldStorage::unloadSectorToLevel(Sector const& sector, SectorLoadLevel targetLoadLevel, bool force) {
  ExplorationProfile::Scope timing(m_explorationProfile.get(), "storage.unloadAttempt");
  if (!m_tileArray->sectorValid(sector) || targetLoadLevel == SectorLoadLevel::Loaded)
    return true;

  auto& metadata = m_sectorMetadata[sector];
  bool entitiesOverlap = false;
  if (m_entityMap) {
    auto entityFactory = Root::singleton().entityFactory();
    List<EntityPtr> entitiesToStore;
    List<EntityPtr> entitiesToRemove;

    for (auto& entity : m_entityMap->entityQuery(RectF(m_tileArray->sectorRegion(sector)))) {
      // Only store / remove entities who belong to this sector.  If an entity
      // overlaps with this sector but does not belong to it, we may not want to
      // completely unload it.
      auto position = entity->position();
      if (!belongsInSector(sector, position)) {
        if (auto entitySector = sectorForPosition(Vec2I(position))) {
          if (auto p = m_sectorMetadata.ptr(*entitySector))
            entitiesOverlap |= p->timeToLive > 0.0f;
        }
        continue;
      }

      bool keepAlive = m_generatorFacade->entityKeepAlive(this, entity);
      if (keepAlive && !force)
        return false;

      if (m_generatorFacade->entityPersistent(this, entity))
        entitiesToStore.append(std::move(entity));
      else
        entitiesToRemove.append(std::move(entity));
    }

    for (auto const& entity : entitiesToRemove) {
      m_entityMap->removeEntity(entity->entityId());
      m_generatorFacade->destructEntity(this, entity);
    }

    if (metadata.loadLevel == SectorLoadLevel::Entities || !entitiesToStore.empty()) {
      EntitySectorStore sectorStore;

      // If our current load level indicates that we might have entities that are
      // not loaded, we need to load and merge with them, otherwise we should be
      // overwriting them.
      if (metadata.loadLevel < SectorLoadLevel::Entities) {
        if (auto res = m_db.find(entitySectorKey(sector)))
          sectorStore = readEntitySector(*res);
      }

      UniqueIndexStore storedUniques;
      for (auto const& entity : entitiesToStore) {
        m_entityMap->removeEntity(entity->entityId());
        m_generatorFacade->destructEntity(this, entity);
        auto position = entity->position();
        if (auto uniqueId = entity->uniqueId())
          storedUniques.add(*uniqueId, {sector, position});
        sectorStore.append(entityFactory->storeVersionedEntity(entity));
      }
      m_db.insert(entitySectorKey(sector), writeEntitySector(sectorStore));
      if (metadata.loadLevel < SectorLoadLevel::Entities)
        mergeSectorUniques(sector, storedUniques);
      else
        updateSectorUniques(sector, storedUniques);

      if (metadata.loadLevel == SectorLoadLevel::Entities) {
        metadata.loadLevel = SectorLoadLevel::Tiles;
        if (m_regionObservation)
          m_regionObservation->changed(Vec2U(sector));
        m_generatorFacade->sectorLoadLevelChanged(this, sector, SectorLoadLevel::Tiles);
      }
    }
  }

  if (targetLoadLevel == SectorLoadLevel::None) {
    if (metadata.loadLevel > SectorLoadLevel::None && !entitiesOverlap) {
      TileSectorStore sectorStore;
      sectorStore.tiles = m_tileArray->unloadSector(sector);
      sectorStore.generationLevel = metadata.generationLevel;
      m_db.insert(tileSectorKey(sector), writeTileSector(sectorStore));
      m_syncedTileRevisions.remove(sector);
      m_sectorMetadata.remove(sector);
      if (m_regionObservation)
        m_regionObservation->changed(Vec2U(sector));
      if (m_sectorThreads)
        m_sectorThreads->unloaded(sector);
      m_generatorFacade->sectorLoadLevelChanged(this, sector, SectorLoadLevel::None);
      return true;
    }
    return false;
  }
  return true;
}

void WorldStorage::syncSector(Sector const& sector) {
  STAR_TRACE_ZONE("WorldStorage sync sector");
  ExplorationProfile::Scope timing(m_explorationProfile.get(), "storage.sync.sector");
  if (!m_tileArray->sectorValid(sector))
    return;

  auto entityFactory = Root::singleton().entityFactory();
  auto& metadata = m_sectorMetadata[sector];

  // Only sync the levels that we know are loaded.  It is possible that this
  // sector is at load level < Entities but has zombie entities in it,  but
  // storing those without unloading them will lead to duplication.  Zombie
  // entities will be unloaded in update eventually anyway.

  if (metadata.loadLevel >= SectorLoadLevel::Entities) {
    EntitySectorStore sectorStore;
    UniqueIndexStore storedUniques;
    for (auto const& entity : m_entityMap->entityQuery(RectF(m_tileArray->sectorRegion(sector)))) {
      if (!belongsInSector(sector, entity->position()))
        continue;

      if (m_generatorFacade->entityPersistent(this, entity)) {
        if (auto uniqueId = entity->uniqueId())
          storedUniques.add(*uniqueId, {sector, entity->position()});
        sectorStore.append(entityFactory->storeVersionedEntity(entity));
      }
    }
    m_db.insert(entitySectorKey(sector), writeEntitySector(sectorStore));
    updateSectorUniques(sector, storedUniques);
  }

  if (metadata.loadLevel >= SectorLoadLevel::Tiles && tileSectorDirty(sector)) {
    ExplorationProfile::Scope tileTiming(m_explorationProfile.get(), "storage.sync.tiles");
    TileSectorStore sectorStore;
    sectorStore.tiles = m_tileArray->copySector(sector);
    sectorStore.generationLevel = metadata.generationLevel;
    m_db.insert(tileSectorKey(sector), writeTileSector(sectorStore));
    markTileSectorSynced(sector);
    if (m_explorationProfile)
      m_explorationProfile->count("storage.sync.dirtyTiles");
  } else if (metadata.loadLevel >= SectorLoadLevel::Tiles && m_explorationProfile) {
    m_explorationProfile->count("storage.sync.cleanTiles");
  }
}

bool WorldStorage::tileSectorDirty(Sector const& sector) const {
  return m_tileArray->sectorRevision(sector) != m_syncedTileRevisions.value(sector, 0);
}

void WorldStorage::markTileSectorSynced(Sector const& sector) {
  m_syncedTileRevisions[sector] = m_tileArray->sectorRevision(sector);
}

List<WorldStorage::Sector> WorldStorage::adjacentSectors(Sector const& sector) const {
  auto tiles = m_tileArray->sectorRegion(sector);
  return m_tileArray->validSectorsFor(tiles.padded(WorldSectorSize));
}

void WorldStorage::updateSectorUniques(Sector const& sector, UniqueIndexStore const& sectorUniques) {
  // If there was an old unique sector store here, then we need to remove all
  // the unique index entries for uniques that used to be in this sector but
  // now aren't, in case they are now gone.
  if (auto oldSectorUniques = m_db.find(sectorUniqueKey(sector)).apply(readSectorUniqueStore)) {
    for (auto const& uniqueId : *oldSectorUniques) {
      if (!sectorUniques.contains(uniqueId))
        removeUniqueIndexEntry(uniqueId, sector);
    }
  }

  for (auto const& p : sectorUniques)
    setUniqueIndexEntry(p.first, p.second);

  if (sectorUniques.empty())
    m_db.remove(sectorUniqueKey(sector));
  else
    m_db.insert(sectorUniqueKey(sector), writeSectorUniqueStore(HashSet<String>::from(sectorUniques.keys())));
}

void WorldStorage::mergeSectorUniques(Sector const& sector, UniqueIndexStore const& sectorUniques) {
  auto sectorUniqueStore = m_db.find(sectorUniqueKey(sector)).apply(readSectorUniqueStore).value();
  for (auto const& p : sectorUniques) {
    setUniqueIndexEntry(p.first, p.second);
    sectorUniqueStore.add(p.first);
  }

  if (sectorUniqueStore.empty())
    m_db.remove(sectorUniqueKey(sector));
  else
    m_db.insert(sectorUniqueKey(sector), writeSectorUniqueStore(sectorUniqueStore));
}

auto WorldStorage::getUniqueIndexEntry(String const& uniqueId) -> Maybe<SectorAndPosition> {
  if (auto uniqueIndex = m_db.find(uniqueIndexKey(uniqueId)).apply(readUniqueIndexStore))
    return uniqueIndex->maybe(uniqueId);
  return {};
}

void WorldStorage::setUniqueIndexEntry(String const& uniqueId, SectorAndPosition const& sectorAndPosition) {
  UniqueIndexStore uniqueIndex = m_db.find(uniqueIndexKey(uniqueId)).apply(readUniqueIndexStore).value();
  auto p = uniqueIndex.insert(uniqueId, sectorAndPosition);
  if (!p.second) {
    // Don't need to update the index if the entry was already there and the
    // sector and position haven't changed
    if (p.first->second == sectorAndPosition)
      return;
    p.first->second = sectorAndPosition;
  }
  m_db.insert(uniqueIndexKey(uniqueId), writeUniqueIndexStore(uniqueIndex));
}

void WorldStorage::removeUniqueIndexEntry(String const& uniqueId, Sector const& sector) {
  if (auto uniqueIndex = m_db.find(uniqueIndexKey(uniqueId)).apply(readUniqueIndexStore)) {
    if (auto sectorAndPosition = uniqueIndex->maybe(uniqueId)) {
      if (sectorAndPosition->first == sector) {
        uniqueIndex->remove(uniqueId);
        if (uniqueIndex->empty())
          m_db.remove(uniqueIndexKey(uniqueId));
        else
          m_db.insert(uniqueIndexKey(uniqueId), writeUniqueIndexStore(*uniqueIndex));
      }
    }
  }
}

}
