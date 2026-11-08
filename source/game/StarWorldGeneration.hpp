#pragma once

#include "StarDungeonGenerator.hpp"
#include "StarFallingBlocksAgent.hpp"
#include "StarSpawner.hpp"
#include "StarWorldStorage.hpp"
#include "StarMicroDungeon.hpp"
#include "StarCellularLiquid.hpp"
#include "StarBiomePlacement.hpp"
#include "StarWorkerPool.hpp"
#include "StarWorldGenerationTasks.hpp"

namespace Star {

STAR_CLASS(WorldServer);
WorkerPool& worldSectorWorkerPool();
STAR_CLASS(WorldTemplate);
STAR_CLASS(FallingBlocksWorld);
STAR_CLASS(DungeonGeneratorWorld);
STAR_CLASS(SpawnerWorld);
STAR_CLASS(WorldGenerator);
STAR_CLASS(Plant);
STAR_CLASS(LiquidsDatabase);

class LiquidWorld : public CellularLiquidWorld<LiquidId> {
public:
  LiquidWorld(WorldServer* world);

  Vec2I uniqueLocation(Vec2I const& location) const override;
  CellularLiquidCell<LiquidId> cell(Vec2I const& location) const override;
  float drainLevel(Vec2I const& location) const override;
  void setFlow(Vec2I const& location, CellularLiquidFlowCell<LiquidId> const& flow) override;
  void liquidInteraction(Vec2I const& a, LiquidId aLiquid, Vec2I const& b, LiquidId bLiquid) override;
  void liquidCollision(Vec2I const& pos, LiquidId liquid, Vec2I const& blockPos) override;

private:
  WorldServer* m_worldServer;
  LiquidsDatabaseConstPtr m_liquidsDatabase;
  MaterialDatabaseConstPtr m_materialDatabase;
};

class FallingBlocksWorld : public FallingBlocksFacade {
public:
  FallingBlocksWorld(WorldServer* world);

  FallingBlockType blockType(Vec2I const& pos, bool const& withSignal = true) override;
  void moveBlock(Vec2I const& from, Vec2I const& to) override;

private:
  WorldServer* m_worldServer;
  MaterialDatabaseConstPtr m_materialDatabase;
};

class DungeonGeneratorWorld : public DungeonGeneratorWorldFacade {
public:
  DungeonGeneratorWorld(WorldServer* worldServer, bool markForActivation);

  void markRegion(RectI const& region) override;
  void markTerrain(PolyF const& region) override;
  void markSpace(PolyF const& region) override;

  void setForegroundMaterial(Vec2I const& position, MaterialId material, MaterialHue hueshift, MaterialColorVariant colorVariant) override;
  void setBackgroundMaterial(Vec2I const& position, MaterialId material, MaterialHue hueshift, MaterialColorVariant colorVariant) override;
  void setForegroundMod(Vec2I const& position, ModId mod, MaterialHue hueshift) override;
  void setBackgroundMod(Vec2I const& position, ModId mod, MaterialHue hueshift) override;
  void placeObject(
      Vec2I const& pos, String const& objectName, Star::Direction direction, Json const& parameters) override;
  void placeVehicle(Vec2F const& pos, String const& vehicleName, Json const& parameters) override;
  void placeSurfaceBiomeItems(Vec2I const& pos) override;
  void placeBiomeTree(Vec2I const& pos) override;
  void addDrop(Vec2F const& position, ItemDescriptor const& item) override;
  void spawnNpc(Vec2F const& position, Json const& parameters) override;
  void spawnStagehand(Vec2F const& position, Json const& definition) override;
  void setLiquid(Vec2I const& pos, LiquidStore const& liquid) override;
  void setPlayerStart(Vec2F const& startPosition) override;
  void connectWireGroup(List<Vec2I> const& wireGroup) override;
  void setTileProtection(DungeonId dungeonId, bool isProtected) override;
  bool checkSolid(Vec2I const& position, TileLayer layer) override;
  bool checkOpen(Vec2I const& position, TileLayer open) override;
  bool checkOceanLiquid(Vec2I const& position) override;
  DungeonId getDungeonIdAt(Vec2I const& position) override;
  void setDungeonIdAt(Vec2I const& position, DungeonId dungeonId) override;
  void clearTileEntities(RectI const& bounds, Set<Vec2I> const& positions, bool clearAnchoredObjects) override;

  WorldGeometry getWorldGeometry() const override;

private:
  // Microdungeon-only, fixed-cardinality diagnostics. This never retains
  // world data or mod payloads and is disabled unless regional details are on.
  void recordMicroDungeonFacade(char const* counter, uint64_t amount = 1) const;
  void placePlant(PlantPtr const& plant, Vec2I const& position);
  void placeBiomeItems(Vec2I const& pos, List<BiomeItemPlacement>& potentialItems);

  WorldServer* m_worldServer;
  bool m_markForActivation;
};

class SpawnerWorld : public SpawnerFacade {
public:
  SpawnerWorld(WorldServer* worldServer);

  WorldGeometry geometry() const override;
  List<RectF> clientWindows() const override;
  bool signalRegion(RectF const& region) const override;
  CollisionKind collision(Vec2I const& position) const override;
  bool isFreeSpace(RectF const& area) const override;
  bool isBackgroundEmpty(Vec2I const& position) const override;
  LiquidLevel liquidLevel(Vec2I const& position) const override;
  bool spawningProhibited(RectF const& area) const override;
  uint64_t spawnSeed() const override;
  SpawnProfile spawnProfile(Vec2F const& position) const override;
  float dayLevel() const override;
  float threatLevel() const override;
  EntityId spawnEntity(EntityPtr entity) const override;
  void despawnEntity(EntityId entityId) override;
  EntityPtr getEntity(EntityId entityId) const override;

private:
  WorldServer* m_worldServer;
};

class WorldGenerator : public WorldGeneratorFacade {
public:
  WorldGenerator(WorldServer* server);

  SectorGenerationResult generateSectorLevel(WorldStorage* worldStorage, Sector const& sector,
      SectorGenerationLevel generationLevel, bool allowAsync) override;
  void sectorLoadLevelChanged(WorldStorage* worldStorage, Sector const& sector, SectorLoadLevel loadLevel) override;
  List<Sector> readyBaseTileSectors(WorldStorage* worldStorage) override;
  List<Sector> readyMicroDungeonPlanSectors(WorldStorage* worldStorage) override;
  bool admitBaseTilesPreparation(WorldStorage* worldStorage, Sector const& sector) override;
  bool admitMicroDungeonPlanPreparation(WorldStorage* worldStorage, Sector const& sector) override;
  void terraformSector(WorldStorage* worldStorage, Sector const& sector) override;
  void initEntity(WorldStorage* worldStorage, EntityId entityId, EntityPtr const& entity) override;
  void destructEntity(WorldStorage* worldStorage, EntityPtr const& entity) override;
  bool entityKeepAlive(WorldStorage* worldStorage, EntityPtr const& entity) const override;
  bool entityPersistent(WorldStorage* worldStorage, EntityPtr const& entity) const override;
  RpcPromise<Vec2I> enqueuePlacement(List<BiomeItemDistribution> distributions, Maybe<DungeonId> id) override;

  void replaceBiomeBlocks(ServerTile* tile);

private:
  struct QueuedPlacement {
    List<BiomeItemDistribution> distributions;
    Maybe<DungeonId> dungeonId;
    RpcPromiseKeeper<Vec2I> promise;
    bool fulfilled;
  };

  // Immutable selection descriptor. It intentionally contains no world,
  // entity, Lua or facade reference, so a later planning worker can receive a
  // value snapshot without gaining write access to the live world.
  struct MicroDungeonPlan {
    RectI bounds;
    Vec2I position;
    uint64_t seed;
    String dungeonName;
    Maybe<size_t> queuedPlacement;
  };

  // Copy-only input for the future MicroDungeonPlan producer. It deliberately
  // excludes WorldServer, a live WorldTemplate, entities, Lua state and
  // promises: a worker receives only this data and its own WorldTemplate
  // reconstructed from templateStore.
  struct QueuedMicroDungeonPlacementSnapshot {
    List<BiomeItemDistribution> distributions;
    size_t queuedPlacement;
  };

  struct MicroDungeonPlanSnapshot {
    Json templateStore;
    uint64_t templateRevision;
    Sector sector;
    RectI sectorTiles;
    RectI tileHalo;
    HashMap<Sector, uint64_t> sectorRevisions;
    List<pair<Sector, ServerTileSectorArray::ArrayPtr>> haloTiles;
    List<QueuedMicroDungeonPlacementSnapshot> queuedPlacements;
  };

  struct PreparedMicroDungeonPlan {
    List<MicroDungeonPlan> plans;
    double elapsedSeconds;
    double queueSeconds;
    double completedAt;
  };

  struct PreparedBaseTiles {
    ServerTileSectorArray::ArrayPtr tiles;
    double elapsedSeconds;
    double queueSeconds;
    double completedAt;
  };

  struct PendingBaseTiles {
    shared_ptr<WorldGenerationTask> task;
    ServerTileSectorArray::Array const* sourceTiles;
    uint64_t sourceTileRevision;
    WorldTemplate const* sourceTemplate;
    uint64_t sourceTemplateRevision;
    double scheduledAt;
    Maybe<WorkerPoolPromise<PreparedBaseTiles>> promise;
  };

  // Owner-thread queue entry. The completed payload remains in the promise
  // until the normal commit path consumes it; this record makes the ready
  // state, identity and wait time explicit without giving workers world access.
  struct ReadyBaseTiles {
    shared_ptr<WorldGenerationTask> task;
    double readyAt;
  };

  // Like ReadyBaseTiles, this is an owner-thread index over an already
  // completed private producer. The payload remains in its promise until the
  // regular MicroDungeons apply path revalidates and consumes it.
  struct ReadyMicroDungeonPlan {
    shared_ptr<WorldGenerationTask> task;
    double readyAt;
  };

  // The owning region keeps this record until the producer has returned and
  // its template and sector revisions have been revalidated.
  struct PendingMicroDungeonPlan {
    shared_ptr<WorldGenerationTask> task;
    HashMap<Sector, uint64_t> sourceSectorRevisions;
    WorldTemplate const* sourceTemplate;
    uint64_t sourceTemplateRevision;
    double scheduledAt;
    Maybe<WorkerPoolPromise<PreparedMicroDungeonPlan>> promise;
    bool dependencyPrepared = false;
  };

  void prepareTiles(WorldStorage* worldStorage, Sector const& sector);
  void retireBaseTileTask(shared_ptr<WorldGenerationTask> const& task, WorldGenerationTaskOutcome outcome);
  SectorGenerationResult prepareTilesAsync(WorldStorage* worldStorage, Sector const& sector, bool allowAsync);
  SectorGenerationResult generateMicroDungeons(WorldStorage* worldStorage, Sector const& sector, bool allowAsync);
  static List<MicroDungeonPlan> planMicroDungeons(WorldTemplatePtr const& templateSource,
      MicroDungeonPlanSnapshot const& snapshot);
  void applyMicroDungeonPlan(WorldStorage* worldStorage, MicroDungeonPlan const& plan,
      DungeonGeneratorWorldPtr const& facade, ExplorationProfile* profile, bool planPreview);
  void generateCaveLiquid(WorldStorage* worldStorage, Sector const& sector);
  void prepareSector(WorldStorage* worldStorage, Sector const& sector);
  void prepareSectorBiomeBlocks(WorldStorage* worldStorage, Sector const& sector);

  void placeBiomeGrass(WorldStorage* worldStorage, ServerTile* tile, Vec2I const& position);

  void reapplyBiome(WorldStorage* worldStorage, Sector const& sector);

  Set<Vec2I> caveLiquidSeeds(WorldStorage* worldStorage, Sector const& sector);
  Map<Vec2I, float> determineLiquidLevel(Set<Vec2I> const& spots, Set<Vec2I> const& filled);
  void levelCluster(Set<Vec2I>& cluster, Set<Vec2I> const& filled, Map<Vec2I, float>& results);

  // Special plant placement routine that does slight terrain adjustments to
  // fit plants.
  bool placePlant(WorldStorage* worldStorage, PlantPtr const& plant, Vec2I const& position);

  WorldServer* m_worldServer;
  MicroDungeonFactoryPtr m_microDungeonFactory;
  List<QueuedPlacement> m_queuedPlacements;
  HashMap<Sector, PendingBaseTiles> m_pendingBaseTiles;
  HashMap<Sector, ReadyBaseTiles> m_readyBaseTiles;
  HashMap<Sector, PendingMicroDungeonPlan> m_pendingMicroDungeonPlans;
  HashMap<Sector, ReadyMicroDungeonPlan> m_readyMicroDungeonPlans;
  unique_ptr<WorldGenerationTasks> m_generationTasks;
  double m_lastPoolProfileTime = 0;
  WorldTemplatePtr m_baseTileTemplateSource;
  uint64_t m_baseTileTemplateRevision = 0;
  Json m_baseTileTemplateStore;
  WorldTemplatePtr m_microDungeonTemplateSource;
  uint64_t m_microDungeonTemplateRevision = 0;
  Json m_microDungeonTemplateStore;
};

}
