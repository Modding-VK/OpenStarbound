#include "StarWorldGeneration.hpp"
#include "StarWorldGenerationWorkerCache.hpp"
#include "StarWorldServer.hpp"
#include "StarExplorationProfile.hpp"
#include "StarMaterialItem.hpp"
#include "StarMaterialDatabase.hpp"
#include "StarNpcDatabase.hpp"
#include "StarMonsterDatabase.hpp"
#include "StarNpc.hpp"
#include "StarBiome.hpp"
#include "StarSky.hpp"
#include "StarWorldTemplate.hpp"
#include "StarBiomePlacement.hpp"
#include "StarWireEntity.hpp"
#include "StarItemDrop.hpp"
#include "StarLogging.hpp"
#include "StarRoot.hpp"
#include "StarItemDatabase.hpp"
#include "StarProjectileDatabase.hpp"
#include "StarProjectile.hpp"
#include "StarObjectDatabase.hpp"
#include "StarObject.hpp"
#include "StarContainerObject.hpp"
#include "StarMonster.hpp"
#include "StarEntityMap.hpp"
#include "StarPlant.hpp"
#include "StarLiquidsDatabase.hpp"
#include "StarStagehand.hpp"
#include "StarVehicleDatabase.hpp"
#include "StarTime.hpp"
#include "StarTrace.hpp"
#include "StarXXHash.hpp"

#include <thread>
#include <type_traits>

namespace Star {

static int const PlantAdjustmentLimit = 2;

namespace {

bool crossesLiquidSector(WorldGeometry const& geometry, Vec2I a, Vec2I b) {
  a = geometry.xwrap(a);
  b = geometry.xwrap(b);
  if (a[1] < 0 || b[1] < 0 || static_cast<unsigned>(a[1]) >= geometry.height()
      || static_cast<unsigned>(b[1]) >= geometry.height())
    return false;
  return a[0] / WorldSectorSize != b[0] / WorldSectorSize
      || a[1] / WorldSectorSize != b[1] / WorldSectorSize;
}

bool isPreservedTileCollisionOverride(CollisionKind collision) {
  return collision == CollisionKind::None
      || collision == CollisionKind::Platform
      || collision == CollisionKind::Block;
}

CollisionKind biomeForegroundCollision(MaterialDatabaseConstPtr const& materialDatabase, MaterialId oldForeground, MaterialId newForeground, CollisionKind currentCollision) {
  CollisionKind oldCollision = materialDatabase->materialCollisionKind(oldForeground);
  CollisionKind newCollision = materialDatabase->materialCollisionKind(newForeground);

  if (currentCollision != oldCollision && isPreservedTileCollisionOverride(currentCollision))
    return currentCollision;

  return newCollision;
}

void prepareBaseTileArray(WorldTemplatePtr const& planet, MaterialDatabaseConstPtr const& materialDatabase,
    RectI const& sectorRegion, ServerTileSectorArray::Array& tiles) {
  STAR_TRACE_ZONE("Base tiles worker prepare");
  for (int x = sectorRegion.xMin(); x < sectorRegion.xMax(); ++x) {
    for (int y = sectorRegion.yMin(); y < sectorRegion.yMax(); ++y) {
      ServerTile& tile = tiles(x - sectorRegion.xMin(), y - sectorRegion.yMin());
      auto blockInfo = planet->blockInfo(x, y);

      tile.blockBiomeIndex = blockInfo.blockBiomeIndex;
      tile.environmentBiomeIndex = blockInfo.environmentBiomeIndex;
      tile.biomeTransition = blockInfo.biomeTransition;

      if (tile.foreground == NullMaterialId) {
        tile.foreground = blockInfo.foreground;
        tile.foregroundColorVariant = DefaultMaterialColorVariant;
        tile.foregroundHueShift = planet->biomeMaterialHueShift(tile.blockBiomeIndex, tile.foreground);

        if (materialDatabase->supportsMod(tile.foreground, blockInfo.foregroundMod)) {
          tile.foregroundMod = blockInfo.foregroundMod;
          tile.foregroundModHueShift = planet->biomeModHueShift(tile.blockBiomeIndex, tile.foregroundMod);
        }
      }

      if (tile.background == NullMaterialId) {
        tile.background = blockInfo.background;
        tile.backgroundColorVariant = DefaultMaterialColorVariant;
        tile.backgroundHueShift = planet->biomeMaterialHueShift(tile.blockBiomeIndex, tile.background);

        if (materialDatabase->supportsMod(tile.background, blockInfo.backgroundMod)) {
          tile.backgroundMod = blockInfo.backgroundMod;
          tile.backgroundModHueShift = planet->biomeModHueShift(tile.blockBiomeIndex, tile.backgroundMod);
        }
      }

      if (tile.foreground != EmptyMaterialId) {
        tile.liquid = LiquidStore();
      } else if (blockInfo.oceanLiquid != EmptyLiquidId && y < blockInfo.oceanLiquidLevel) {
        float pressure = blockInfo.oceanLiquidLevel - y;
        if (tile.background == EmptyMaterialId)
          tile.liquid = LiquidStore::endless(blockInfo.oceanLiquid, pressure);
        else if (blockInfo.encloseLiquids)
          tile.liquid = LiquidStore::filled(blockInfo.oceanLiquid, 1.0f, pressure);
      }
    }
  }
}

auto workerWorldTemplate(WorldTemplatePtr const& sourceTemplate,
    uint64_t sourceRevision, WorldGenerationJsonSnapshotPtr const& templateStore,
    WorldGenerationMemoryLedger& ledger) {
  static thread_local WorldGenerationWorkerCache<WorldTemplate, WorldTemplate> cache;
  auto use = cache.get(sourceTemplate, sourceRevision, ledger, sizeof(WorldTemplate),
      +[](WorldTemplate const& value) { return value.generationCacheStorageBytes(); },
      [&] { return make_shared<WorldTemplate>(templateStore->value); });
  if (!use) throw StarException("Could not track worker template storage");
  return use;
}

WorkerPool& sectorGenerationWorkerPool() {
  static WorkerPool pool = [] {
    auto threadingConfig = Root::singleton().configuration()->get("worldThreading", JsonObject());
    unsigned sectorWorkers = threadingConfig.getUInt("sectorWorkers", 0);
    auto performanceMode = threadingConfig.getString("performanceMode", "legacy");
    if (sectorWorkers == 0 && (performanceMode == "adaptive" || performanceMode == "aggressive" || performanceMode == "dynamic")) {
      unsigned hardwareWorkers = max(1u, std::thread::hardware_concurrency());
      unsigned reservedWorkers = min<unsigned>(threadingConfig.getUInt("reservedSystemWorkers", 1), hardwareWorkers - 1);
      unsigned automaticWorkers = max(1u, hardwareWorkers - reservedWorkers);
      unsigned maximumWorkers = threadingConfig.getUInt("maxSectorWorkers", 0);
      if (maximumWorkers != 0)
        automaticWorkers = min(automaticWorkers, maximumWorkers);
      sectorWorkers = automaticWorkers;
    }
    // The pool retains snapshots and results while work is in flight.  Keep a
    // hard process-local ceiling even in aggressive mode; an explicit worker
    // count remains useful for repeatable A/B tests.
    sectorWorkers = clamp<unsigned>(sectorWorkers, 0, 32);
    return WorkerPool("WorldSectorGeneration", sectorWorkers);
  }();
  return pool;
}

JsonObject revisionReasonJson(ServerTileSectorArray::RevisionCounts const& counts, ExplorationProfile* profile, bool stale) {
  static char const* const reasonNames[] = {
    "other", "collisionQuery", "collisionCache", "worldGeneration", "liquid", "entity", "storage"
  };
  static char const* const staleCounterNames[] = {
    "generation.baseTiles.staleRevision.other", "generation.baseTiles.staleRevision.collisionQuery",
    "generation.baseTiles.staleRevision.collisionCache", "generation.baseTiles.staleRevision.worldGeneration",
    "generation.baseTiles.staleRevision.liquid", "generation.baseTiles.staleRevision.entity",
    "generation.baseTiles.staleRevision.storage"
  };
  static char const* const evictedCounterNames[] = {
    "generation.baseTiles.evictedRevision.other", "generation.baseTiles.evictedRevision.collisionQuery",
    "generation.baseTiles.evictedRevision.collisionCache", "generation.baseTiles.evictedRevision.worldGeneration",
    "generation.baseTiles.evictedRevision.liquid", "generation.baseTiles.evictedRevision.entity",
    "generation.baseTiles.evictedRevision.storage"
  };
  static_assert(sizeof(reasonNames) / sizeof(*reasonNames) == static_cast<size_t>(TileRevisionCause::Count),
      "Tile revision reason labels");
  static_assert(sizeof(staleCounterNames) / sizeof(*staleCounterNames) == static_cast<size_t>(TileRevisionCause::Count),
      "Stale revision counter labels");
  static_assert(sizeof(evictedCounterNames) / sizeof(*evictedCounterNames) == static_cast<size_t>(TileRevisionCause::Count),
      "Evicted revision counter labels");
  JsonObject reasons;
  for (size_t i = 0; i < counts.size(); ++i) {
    if (counts[i]) {
      reasons.set(reasonNames[i], counts[i]);
      profile->count(stale ? staleCounterNames[i] : evictedCounterNames[i], counts[i]);
    }
  }
  return reasons;
}

}

WorkerPool& worldSectorWorkerPool() {
  return sectorGenerationWorkerPool();
}

LiquidWorld::LiquidWorld(WorldServer* world) {
  m_worldServer = world;
  auto& root = Root::singleton();
  m_liquidsDatabase = root.liquidsDatabase();
  m_materialDatabase = root.materialDatabase();
}

Vec2I LiquidWorld::uniqueLocation(Vec2I const& location) const {
  return m_worldServer->geometry().xwrap(location);
}

float LiquidWorld::drainLevel(Vec2I const& location) const {
  if (location[1] > m_worldServer->worldTemplate()->undergroundLevel()) {
    auto const& tile = m_worldServer->getServerTile(location);
    if (!m_materialDatabase->blocksLiquidFlow(tile.background)) {
      auto const& belowTile = m_worldServer->getServerTile(location + Vec2I(0, -1));
      if (m_materialDatabase->blocksLiquidFlow(belowTile.background) || m_materialDatabase->blocksLiquidFlow(belowTile.foreground) || belowTile.liquid.source)
        return m_liquidsDatabase->backgroundDrain();
    }
  }
  return 0.0f;
}

CellularLiquidCell<LiquidId> LiquidWorld::cell(Vec2I const& location) const {
  auto const& tile = m_worldServer->getServerTile(location);
  if (m_materialDatabase->blocksLiquidFlow(tile.foreground)) {
    return CellularLiquidCollisionCell();
  } else {
    if (tile.liquid.source)
      return CellularLiquidSourceCell<LiquidId>{tile.liquid.liquid, tile.liquid.pressure};
    else if (tile.liquid.liquid != EmptyLiquidId)
      return CellularLiquidFlowCell<LiquidId>{tile.liquid.liquid, tile.liquid.level, tile.liquid.pressure};
    else
      return CellularLiquidFlowCell<LiquidId>{{}, 0.0f, 0.0f};
  }
}

void LiquidWorld::setFlow(Vec2I const& location, CellularLiquidFlowCell<LiquidId> const& flow) {
  if (flow.liquid) {
    m_worldServer->setLiquid(location, *flow.liquid, flow.level, flow.pressure);

    auto const& tile = m_worldServer->getServerTile(location);
    if (auto materialInteraction = m_materialDatabase->liquidMaterialInteraction(*flow.liquid, tile.background)) {
      if (!materialInteraction->topOnly && tile.liquid.level >= materialInteraction->consumeLiquid) {
        if (auto modifyTile = m_worldServer->modifyServerTile(location)) {
          modifyTile->liquid.take(materialInteraction->consumeLiquid);
          modifyTile->background = materialInteraction->transformTo;
          m_worldServer->activateLiquidLocation(location);
        }
      }
    }
    if (auto modInteraction = m_materialDatabase->liquidModInteraction(*flow.liquid, tile.backgroundMod)) {
      if (!modInteraction->topOnly && tile.liquid.level >= modInteraction->consumeLiquid) {
        if (auto modifyTile = m_worldServer->modifyServerTile(location)) {
          modifyTile->liquid.take(modInteraction->consumeLiquid);
          modifyTile->backgroundMod = modInteraction->transformTo;
          m_worldServer->activateLiquidLocation(location);
        }
      }
    }
  } else {
    m_worldServer->setLiquid(location, EmptyLiquidId, 0.0f, 0.0f);
  }
}

void LiquidWorld::liquidInteraction(Vec2I const& a, LiquidId aLiquid, Vec2I const& b, LiquidId bLiquid) {
  auto profile = m_worldServer->explorationProfile();
  bool crosses = profile && crossesLiquidSector(m_worldServer->geometry(), a, b);
  auto handleInteraction = [this, profile, crosses](Vec2I const& target, Maybe<LiquidInteractionResult> interaction) {
    if (interaction) {
      if (interaction->isLeft()) {
        bool changed = m_worldServer->modifyTile(target, PlaceMaterial{TileLayer::Foreground, interaction->left(), 0}, false);
        if (crosses && changed)
          profile->count("liquid.crossSectorInteractionEffects");
      } else {
        auto liquidLevel = m_worldServer->liquidLevel(target);
        m_worldServer->setLiquid(target, interaction->right(), liquidLevel.level, liquidLevel.level);
        if (crosses)
          profile->count("liquid.crossSectorInteractionEffects");
      }
    }
  };

  handleInteraction(a, m_liquidsDatabase->interact(aLiquid, bLiquid));
  handleInteraction(b, m_liquidsDatabase->interact(bLiquid, aLiquid));
}

void LiquidWorld::liquidCollision(Vec2I const& liquidPos, LiquidId liquidId, Vec2I const& blockPos) {
  auto profile = m_worldServer->explorationProfile();
  bool crosses = profile && crossesLiquidSector(m_worldServer->geometry(), liquidPos, blockPos);
  auto const& blockTile = m_worldServer->getServerTile(blockPos);

  if (auto materialInteraction = m_materialDatabase->liquidMaterialInteraction(liquidId, blockTile.foreground)) {
    if ((!materialInteraction->topOnly || liquidPos[1] > blockPos[1]) && m_worldServer->liquidLevel(liquidPos).level >= materialInteraction->consumeLiquid) {
      auto modifyLiquidTile = m_worldServer->modifyServerTile(liquidPos);
      auto modifyBlockTile = m_worldServer->modifyServerTile(blockPos);
      if (modifyLiquidTile && modifyBlockTile) {
        if (crosses)
          profile->count("liquid.crossSectorCollisionEffects");
        modifyLiquidTile->liquid.take(materialInteraction->consumeLiquid);
        modifyBlockTile->foreground = materialInteraction->transformTo;
        if (!m_materialDatabase->isMultiColor(materialInteraction->transformTo))
          modifyBlockTile->foregroundColorVariant = DefaultMaterialColorVariant;
        m_worldServer->activateLiquidLocation(liquidPos);
      }
    }
  }
  if (auto modInteraction = m_materialDatabase->liquidModInteraction(liquidId, blockTile.foregroundMod)) {
    if ((!modInteraction->topOnly || liquidPos[1] > blockPos[1]) && m_worldServer->liquidLevel(liquidPos).level >= modInteraction->consumeLiquid) {
      auto modifyLiquidTile = m_worldServer->modifyServerTile(liquidPos);
      auto modifyBlockTile = m_worldServer->modifyServerTile(blockPos);
      if (modifyLiquidTile && modifyBlockTile) {
        if (crosses)
          profile->count("liquid.crossSectorCollisionEffects");
        modifyLiquidTile->liquid.take(modInteraction->consumeLiquid);
        modifyBlockTile->foregroundMod = modInteraction->transformTo;
        m_worldServer->activateLiquidLocation(liquidPos);
      }
    }
  }
}

FallingBlocksWorld::FallingBlocksWorld(WorldServer* w)
  : m_worldServer(w), m_materialDatabase(Root::singleton().materialDatabase()) {}

FallingBlockType FallingBlocksWorld::blockType(Vec2I const& pos, bool const& withSignal) {
  auto const& tile =  m_worldServer->getServerTile(pos, withSignal);
  if (tile.rootSource) {
    return FallingBlockType::Immovable;
  } else if (tile.foreground == EmptyMaterialId) {
    if (m_worldServer->tileIsOccupied(pos, TileLayer::Foreground))
      return FallingBlockType::Immovable;
    else
      return FallingBlockType::Open;
  } else if (m_materialDatabase->isCascadingFallingMaterial(tile.foreground)) {
    return FallingBlockType::Cascading;
  } else if (m_materialDatabase->isFallingMaterial(tile.foreground)) {
    return FallingBlockType::Falling;
  } else {
    return FallingBlockType::Immovable;
  }
}

void FallingBlocksWorld::moveBlock(Vec2I const& from, Vec2I const& to) {
  auto fromTile = m_worldServer->modifyServerTile(from, true);
  auto toTile = m_worldServer->modifyServerTile(to, true);
  if (!fromTile || !toTile)
    return;

  if (m_worldServer->isTileProtected(to)) {
    for (auto drop : m_worldServer->destroyBlock(TileLayer::Foreground, from, true, true))
      m_worldServer->addEntity(ItemDrop::createRandomizedDrop(drop, Vec2F(to)));
  } else {
    toTile->foreground = fromTile->foreground;
    toTile->foregroundMod = NoModId;
    toTile->foregroundHueShift = fromTile->foregroundHueShift;
    toTile->foregroundColorVariant = fromTile->foregroundColorVariant;
    toTile->updateCollision(m_materialDatabase->materialCollisionKind(toTile->foreground));

    fromTile->foreground = EmptyMaterialId;
    fromTile->foregroundMod = NoModId;
    fromTile->updateCollision(CollisionKind::None);

    m_worldServer->requestGlobalBreakCheck();
  }
}

DungeonGeneratorWorld::DungeonGeneratorWorld(WorldServer* worldServer, bool markForActivation)
  : m_worldServer(worldServer), m_markForActivation(markForActivation) {}

WorldGeometry DungeonGeneratorWorld::getWorldGeometry() const {
  return m_worldServer->geometry();
}

void DungeonGeneratorWorld::recordMicroDungeonFacade(char const* counter, uint64_t amount) const {
  if (m_markForActivation)
    return;
  if (auto profile = m_worldServer->explorationProfile(); profile && profile->regionalDetailsEnabled())
    profile->count(counter, amount);
}

void DungeonGeneratorWorld::markRegion(RectI const& region) {
  recordMicroDungeonFacade("generation.microDungeonFacade.markRegion");
  if (!m_markForActivation)
    return;

  Logger::debug("Marking {} as dungeon region", region);

  m_worldServer->signalRegion(region);
  m_worldServer->activateLiquidRegion(region);
}

void DungeonGeneratorWorld::markTerrain(PolyF const& region) {
  recordMicroDungeonFacade("generation.microDungeonFacade.markTerrain");
  if (!m_markForActivation)
    return;

  Logger::debug("Marking poly as dungeon terrain region: {}", region);
  m_worldServer->worldTemplate()->addCustomTerrainRegion(region);
}

void DungeonGeneratorWorld::markSpace(PolyF const& region) {
  recordMicroDungeonFacade("generation.microDungeonFacade.markSpace");
  if (!m_markForActivation)
    return;

  Logger::debug("Marking poly as dungeon space region: {}", region);
  m_worldServer->worldTemplate()->addCustomSpaceRegion(region);
}

void DungeonGeneratorWorld::setForegroundMaterial(Vec2I const& position, MaterialId material, MaterialHue hueshift, MaterialColorVariant colorVariant) {
  recordMicroDungeonFacade("generation.microDungeonFacade.foregroundMaterialWrite");
  if (ServerTile* tile = m_worldServer->modifyServerTile(position)) {
    m_worldServer->modifyLiquid(position, EmptyLiquidId, 0);
    tile->foreground = material;
    tile->foregroundHueShift = hueshift;
    tile->foregroundColorVariant = colorVariant;
    tile->foregroundMod = NoModId;
    tile->foregroundModHueShift = MaterialHue();
    tile->collision = Root::singleton().materialDatabase()->materialCollisionKind(tile->foreground);
    tile->collisionCacheDirty = true;
  }
}

void DungeonGeneratorWorld::setBackgroundMaterial(Vec2I const& position, MaterialId material, MaterialHue hueshift, MaterialColorVariant colorVariant) {
  recordMicroDungeonFacade("generation.microDungeonFacade.backgroundMaterialWrite");
  if (ServerTile* tile = m_worldServer->modifyServerTile(position)) {
    m_worldServer->modifyLiquid(position, EmptyLiquidId, 0);
    tile->background = material;
    tile->backgroundHueShift = hueshift;
    tile->backgroundColorVariant = colorVariant;
    tile->backgroundMod = NoModId;
    tile->backgroundModHueShift = MaterialHue();
  }
}

void DungeonGeneratorWorld::placeObject(Vec2I const& pos, String const& objectName, Star::Direction direction, Json const& parameters) {
  recordMicroDungeonFacade("generation.microDungeonFacade.objectPlacement");
  ExplorationProfile::Scope phase(m_worldServer->explorationProfile(), "dungeon.object.signalRegion");
  m_worldServer->signalRegion(RectI::withSize(pos, {1, 1}));

  phase.next("dungeon.object.createForPlacement");
  auto objectDatabase = Root::singleton().objectDatabase();
  if (auto object = objectDatabase->createForPlacement(m_worldServer, objectName, pos, direction, parameters)) {
    phase.next("dungeon.object.addEntity");
    m_worldServer->addEntity(object);
  } else {
    phase.stop();
    Logger::warn("Failed to place dungeon object: {} direction: {} position: {}", objectName, (int)direction, pos);
  }
}

void DungeonGeneratorWorld::placeVehicle(Vec2F const& pos, String const& vehicleName, Json const& parameters) {
  recordMicroDungeonFacade("generation.microDungeonFacade.vehiclePlacement");
  m_worldServer->signalRegion(RectI::withSize(Vec2I(pos), {1, 1}));

  auto vehicleDatabase = Root::singleton().vehicleDatabase();
  auto vehicle = vehicleDatabase->create(vehicleName, parameters.opt().value(JsonObject{}).set("persistent", true));
  vehicle->setPosition(pos);
  m_worldServer->addEntity(vehicle);
}

void DungeonGeneratorWorld::placeSurfaceBiomeItems(Vec2I const& pos) {
  recordMicroDungeonFacade("generation.microDungeonFacade.surfaceBiomeItems");
  List<BiomeItemPlacement> surfaceItems = m_worldServer->worldTemplate()->potentialBiomeItemsAt(pos[0], pos[1]).surfaceBiomeItems;
  placeBiomeItems(pos, surfaceItems);
}

void DungeonGeneratorWorld::placeBiomeTree(Vec2I const& pos) {
  recordMicroDungeonFacade("generation.microDungeonFacade.biomeTree");
  if (auto biome = m_worldServer->worldTemplate()->blockBiome(pos[0], pos[1])) {
    m_worldServer->signalRegion(RectI::withSize(pos, {1, 1}));
    auto seed = m_worldServer->worldTemplate()->seedFor(pos[0], pos[1]);
    if (auto treeVariant = biome->surfacePlaceables.firstTreeType())
      placePlant(Root::singleton().plantDatabase()->createPlant(*treeVariant, seed), pos);
  }
}

// yay, copy paste coding, kyren WILL kill me
void DungeonGeneratorWorld::placePlant(PlantPtr const& plant, Vec2I const& position) {
  recordMicroDungeonFacade("generation.microDungeonFacade.plantPlacement");
  if (!plant)
    return;

  auto spaces = plant->spaces();
  auto roots = plant->roots();
  auto const& primaryRoot = plant->primaryRoot();

  auto background = m_worldServer->getServerTile(position).background;
  bool adjustBackground = background == EmptyMaterialId || background == NullMaterialId;

  auto withinAdjustment = [=](Vec2I const& pos) {
    return PlantAdjustmentLimit - std::abs(pos[0]) > 0 && PlantAdjustmentLimit - std::abs(pos[1]) > 0;
  };

  // Bail out if we don't have at least one free space, and root in the primary
  // root position, or if we're in a dungeon region.
  auto primaryTile = m_worldServer->getServerTile(position);
  auto rootTile = m_worldServer->getServerTile(position + primaryRoot);
  if (isConnectableMaterial(primaryTile.foreground) || !isConnectableMaterial(rootTile.foreground))
    return;

  // First bail out if we can't fit anything we're not adjusting
  for (auto space : spaces) {
    Vec2I pspace = space + position;

    if (withinAdjustment(space) && !m_worldServer->atTile<Plant>(pspace).empty())
      return;

    // Bail out if we hit a different plant's root tile, or if we're not in the
    // adjustment space and we hit a non-empty tile.
    auto tile = m_worldServer->getServerTile(pspace);
    if (tile.rootSource || (!withinAdjustment(space) && !(tile.foreground == EmptyMaterialId || tile.foreground == NullMaterialId)))
      return;
  }

  // Check all the roots outside of the adjustment limit
  for (auto root : roots) {
    root += position;
    if (!withinAdjustment(root) && !isConnectableMaterial(m_worldServer->getServerTile(root).foreground))
      return;
  }

  // Clear all the necessary blocks within the adjustment limit
  for (auto space : spaces) {
    if (!withinAdjustment(space))
      continue;

    space += position;
    if (auto tile = m_worldServer->modifyServerTile(space)) {
      if (isConnectableMaterial(tile->foreground))
        *tile = primaryTile;
      if (adjustBackground)
        tile->background = EmptyMaterialId;
      tile->collision = CollisionKind::None;
      tile->collision = Root::singleton().materialDatabase()->materialCollisionKind(tile->foreground);
      tile->collisionCacheDirty = true;
    } else {
      return;
    }
  }

  // Make all the root blocks a real material based on the primary root.
  for (auto root : roots) {
    root += position;
    if (auto tile = m_worldServer->modifyServerTile(root)) {
      if (!isRealMaterial(tile->foreground)) {
        *tile = rootTile;
        tile->collision = Root::singleton().materialDatabase()->materialCollisionKind(tile->foreground);
        tile->collisionCacheDirty = true;
      }
    } else {
      return;
    }
  }

  plant->setTilePosition(position);
  m_worldServer->addEntity(plant);
  return;
}

void DungeonGeneratorWorld::placeBiomeItems(Vec2I const& pos, List<BiomeItemPlacement>& potentialItems) {
  m_worldServer->signalRegion(RectI::withSize(pos, {1, 1}));
  sort(potentialItems);
  for (auto const& placement : potentialItems) {
    auto seed = m_worldServer->worldTemplate()->seedFor(placement.position[0], placement.position[1]);
    if (placement.item.is<GrassVariant>()) {
      auto& grass = placement.item.get<GrassVariant>();
      placePlant(Root::singleton().plantDatabase()->createPlant(grass, seed), placement.position);
    } else if (placement.item.is<BushVariant>()) {
      auto& bush = placement.item.get<BushVariant>();
      placePlant(Root::singleton().plantDatabase()->createPlant(bush, seed), placement.position);
    } else if (placement.item.is<TreePair>()) {
      auto& treePair = placement.item.get<TreePair>();
      TreeVariant treeVariant;
      if (seed % 2 == 0)
        treeVariant = treePair.first;
      else
        treeVariant = treePair.second;

      placePlant(Root::singleton().plantDatabase()->createPlant(treeVariant, seed), placement.position);
    } else if (placement.item.is<ObjectPool>()) {
      auto& objectPool = placement.item.get<ObjectPool>();
      auto direction = seed % 2 ? Direction::Left : Direction::Right;
      auto objectPair = objectPool.select(seed);
      if (auto object = Root::singleton().objectDatabase()->createForPlacement(
              m_worldServer, objectPair.first, placement.position, direction, objectPair.second))
        m_worldServer->addEntity(object);
    } else if (placement.item.is<TreasureBoxSet>()) {
      auto& treasureBoxSet = placement.item.get<TreasureBoxSet>();
      auto direction = seed % 2 ? Direction::Left : Direction::Right;
      if (auto treasureContainer = Root::singleton().treasureDatabase()->createTreasureChest(m_worldServer, treasureBoxSet, placement.position, direction, seed))
        m_worldServer->addEntity(treasureContainer);
    }
  }
}

void DungeonGeneratorWorld::addDrop(Vec2F const& position, ItemDescriptor const& item) {
  recordMicroDungeonFacade("generation.microDungeonFacade.dropSpawn");
  m_worldServer->addEntity(ItemDrop::createRandomizedDrop(item, position));
}

void DungeonGeneratorWorld::spawnNpc(Vec2F const& position, Json const& parameters) {
  recordMicroDungeonFacade("generation.microDungeonFacade.spawn");
  auto kind = parameters.getString("kind");
  if (kind.equals("npc", String::CaseInsensitive)) {
    recordMicroDungeonFacade("generation.microDungeonFacade.npcSpawn");
    auto npcDatabase = Root::singleton().npcDatabase();
    uint64_t seed = parameters.getUInt("seed", Random::randu64());
    String species = parameters.getString("species");
    String typeName = parameters.getString("typeName", "default");
    JsonObject uniqueParameters = parameters.getObject("parameters", {});
    if (!uniqueParameters.contains("persistent"))
      uniqueParameters["persistent"] = true;
    auto npc = npcDatabase->createNpc(npcDatabase->generateNpcVariant(species, typeName, m_worldServer->threatLevel(), seed, uniqueParameters));
    npc->setPosition(position - npc->feetOffset());
    m_worldServer->addEntity(npc);
  } else if (kind.equals("monster", String::CaseInsensitive)) {
    recordMicroDungeonFacade("generation.microDungeonFacade.monsterSpawn");
    auto monsterDatabase = Root::singleton().monsterDatabase();
    uint64_t seed = parameters.getUInt("seed", Random::randu64());
    String typeName = parameters.getString("typeName");
    JsonObject uniqueParameters = parameters.getObject("parameters", {});
    if (!uniqueParameters.contains("persistent"))
      uniqueParameters["persistent"] = true;
    auto monster = monsterDatabase->createMonster(monsterDatabase->monsterVariant(typeName, seed, uniqueParameters));
    monster->setPosition(position);
    m_worldServer->addEntity(monster);
  } else
    throw StarException(strf("Unknown spawnable kind '{}'", kind));
}

void DungeonGeneratorWorld::spawnStagehand(Vec2F const& position, Json const& definition) {
  recordMicroDungeonFacade("generation.microDungeonFacade.stagehandSpawn");
  auto stagehand = Root::singleton().stagehandDatabase()->createStagehand(definition.getString("type"), definition.get("parameters", Json()));
  stagehand->setPosition(position);
  m_worldServer->addEntity(stagehand);
}

void DungeonGeneratorWorld::setLiquid(Vec2I const& pos, LiquidStore const& liquid) {
  recordMicroDungeonFacade("generation.microDungeonFacade.liquidWrite");
  ServerTile* tile = m_worldServer->modifyServerTile(pos);
  starAssert(tile);
  if (tile)
    tile->liquid = liquid;
}

void DungeonGeneratorWorld::setPlayerStart(Vec2F const& startPosition) {
  recordMicroDungeonFacade("generation.microDungeonFacade.playerStart");
  m_worldServer->setPlayerStart(startPosition);
}

void DungeonGeneratorWorld::connectWireGroup(List<Vec2I> const& wireGroup) {
  recordMicroDungeonFacade("generation.microDungeonFacade.wireGroup");
  recordMicroDungeonFacade("generation.microDungeonFacade.wireEndpoints", wireGroup.size());
  List<WireConnection> outbounds;
  List<WireConnection> inbounds;

  for (auto entry : wireGroup) {
    bool found = false;
    Vec2F posf = centerOfTile(entry);
    RectF bounds = {posf - Vec2F(16, 16), posf + Vec2F(16, 16)};
    for (auto entity : m_worldServer->query<WireEntity>(bounds)) {
      for (size_t i = 0; i < entity->nodeCount(WireDirection::Input); ++i) {
        if (entity->tilePosition() + entity->nodePosition({WireDirection::Input, i}) == entry) {
          inbounds.append(WireConnection{entity->tilePosition(), i});
          found = true;
        }
      }
      for (size_t i = 0; i < entity->nodeCount(WireDirection::Output); ++i) {
        if (entity->tilePosition() + entity->nodePosition({WireDirection::Output, i}) == entry) {
          outbounds.append(WireConnection{entity->tilePosition(), i});
          found = true;
        }
      }
    }
    if (!found)
      Logger::warn("Dungeon wire endpoint not found. {}", entry);
  }

  if (!outbounds.size() || !inbounds.size()) {
    Logger::warn("Dungeon wires did not make a circuit.");
    return;
  }

  for (auto outbound : outbounds) {
    auto out = m_worldServer->atTile<WireEntity>(outbound.entityLocation).first();
    for (auto inbound : inbounds) {
      auto in = m_worldServer->atTile<WireEntity>(inbound.entityLocation).first();
      in->addNodeConnection({WireDirection::Input, inbound.nodeIndex}, outbound);
      out->addNodeConnection({WireDirection::Output, outbound.nodeIndex}, inbound);
    }
  }
}

void DungeonGeneratorWorld::setForegroundMod(Vec2I const& position, ModId mod, MaterialHue hueshift) {
  recordMicroDungeonFacade("generation.microDungeonFacade.foregroundModWrite");
  ServerTile* tile = m_worldServer->modifyServerTile(position);
  if (tile) {
    tile->foregroundMod = mod;
    tile->foregroundModHueShift = hueshift;
  }
}

void DungeonGeneratorWorld::setBackgroundMod(Vec2I const& position, ModId mod, MaterialHue hueshift) {
  recordMicroDungeonFacade("generation.microDungeonFacade.backgroundModWrite");
  ServerTile* tile = m_worldServer->modifyServerTile(position);
  if (tile) {
    tile->backgroundMod = mod;
    tile->foregroundModHueShift = hueshift;
  }
}

void DungeonGeneratorWorld::setTileProtection(DungeonId dungeonId, bool isProtected) {
  recordMicroDungeonFacade("generation.microDungeonFacade.tileProtectionWrite");
  m_worldServer->setTileProtection(dungeonId, isProtected);
}

bool DungeonGeneratorWorld::checkSolid(Vec2I const& position, TileLayer layer) {
  recordMicroDungeonFacade("generation.microDungeonFacade.solidRead");
  auto const& tile = m_worldServer->getServerTile(position);
  return tile.material(layer) != EmptyMaterialId && tile.material(layer) != NullMaterialId;
}

bool DungeonGeneratorWorld::checkOpen(Vec2I const& position, TileLayer layer) {
  recordMicroDungeonFacade("generation.microDungeonFacade.openRead");
  auto const& tile = m_worldServer->getServerTile(position);
  return tile.material(layer) == EmptyMaterialId || tile.material(layer) == NullMaterialId;
}

bool DungeonGeneratorWorld::checkOceanLiquid(Vec2I const& position) {
  recordMicroDungeonFacade("generation.microDungeonFacade.oceanRead");
  auto const& block = m_worldServer->worldTemplate()->blockInfo(position[0], position[1]);
  return block.oceanLiquid != EmptyLiquidId && position[1] < block.oceanLiquidLevel;
}

DungeonId DungeonGeneratorWorld::getDungeonIdAt(Vec2I const& position) {
  recordMicroDungeonFacade("generation.microDungeonFacade.dungeonIdRead");
  return m_worldServer->getServerTile(position).dungeonId;
}

void DungeonGeneratorWorld::setDungeonIdAt(Vec2I const& position, DungeonId dungeonId) {
  recordMicroDungeonFacade("generation.microDungeonFacade.dungeonIdWrite");
  if (auto tile = m_worldServer->modifyServerTile(position))
    tile->dungeonId = dungeonId;
}

void DungeonGeneratorWorld::clearTileEntities(RectI const& bounds, Set<Vec2I> const& positions, bool clearAnchoredObjects) {
  recordMicroDungeonFacade("generation.microDungeonFacade.clearTileEntities");
  recordMicroDungeonFacade("generation.microDungeonFacade.clearTilePositions", positions.size());
  if (clearAnchoredObjects)
    recordMicroDungeonFacade("generation.microDungeonFacade.clearAnchoredObjects");
  auto entities = m_worldServer->entityQuery(RectF(bounds).padded(1), entityTypeFilter<TileEntity>());
  auto geometry = m_worldServer->geometry();
  entities.filter([positions, geometry, clearAnchoredObjects](EntityPtr entity) {
      auto tileEntity = as<TileEntity>(entity);
      for (auto pos : tileEntity->spaces()) {
        if (positions.contains(geometry.xwrap(pos + tileEntity->tilePosition())))
          return true;
      }
      if (clearAnchoredObjects) {
        for (auto pos : tileEntity->roots()) {
          if (positions.contains(geometry.xwrap(pos + tileEntity->tilePosition())))
            return true;
        }
        if (auto object = as<Object>(entity)) {
          for (auto pos : object->anchorPositions()) {
            if (positions.contains(geometry.xwrap(pos)))
              return true;
          }
        }
      }

      return false;
    });

  for (auto entity : entities)
    m_worldServer->removeEntity(entity->entityId(), false);
}

SpawnerWorld::SpawnerWorld(WorldServer* server)
  : m_worldServer(server) {}

WorldGeometry SpawnerWorld::geometry() const {
  return m_worldServer->geometry();
}

List<RectF> SpawnerWorld::clientWindows() const {
  List<RectF> windows;
  for (auto clientId : m_worldServer->clientIds())
    windows.append(m_worldServer->clientWindow(clientId));
  return windows;
}

bool SpawnerWorld::signalRegion(RectF const& region) const {
  return m_worldServer->signalRegion(RectI::integral(region));
}

CollisionKind SpawnerWorld::collision(Vec2I const& position) const {
  return m_worldServer->getServerTile(position + Vec2I(0, 1)).collision;
}

bool SpawnerWorld::isFreeSpace(RectF const& area) const {
  return !m_worldServer->polyCollision(PolyF(area));
}

bool SpawnerWorld::isBackgroundEmpty(Vec2I const& pos) const {
  return m_worldServer->getServerTile(pos).background == EmptyMaterialId;
}

LiquidLevel SpawnerWorld::liquidLevel(Vec2I const& position) const {
  return m_worldServer->liquidLevel(position);
}

bool SpawnerWorld::spawningProhibited(RectF const& area) const {
  RectI region = RectI::integral(area);

  // Don't spawn the entity if its region overlaps with a dungeon
  for (int x = region.xMin(); x < region.xMax(); ++x) {
    for (int y = region.yMin(); y < region.yMax(); ++y) {
      auto const& tile = m_worldServer->getServerTile({x, y});
      if (tile.getCollision() == CollisionKind::Null || tile.dungeonId != NoDungeonId)
        return true;
    }
  }

  return false;
}

uint64_t SpawnerWorld::spawnSeed() const {
  return m_worldServer->worldTemplate()->worldSeed();
}

SpawnProfile SpawnerWorld::spawnProfile(Vec2F const& position) const {
  Vec2I ipos = Vec2I::floor(position);
  // Block biome, *not* environment biome, includes things like detached
  // biomes.
  if (auto biome = m_worldServer->worldTemplate()->blockBiome(ipos[0], ipos[1])) {
    // Dungeons, including ConstructionDungeonId (player constructed areas)
    // should be immune from spawning.
    auto tile = m_worldServer->getServerTile(ipos);
    if (tile.dungeonId == NoDungeonId)
      return biome->spawnProfile;
  }
  return {};
}

float SpawnerWorld::dayLevel() const {
  return m_worldServer->sky()->dayLevel();
}

float SpawnerWorld::threatLevel() const {
  return m_worldServer->threatLevel();
}

EntityId SpawnerWorld::spawnEntity(EntityPtr entity) const {
  m_worldServer->addEntity(entity);
  return entity->entityId();
}

void SpawnerWorld::despawnEntity(EntityId entityId) {
  m_worldServer->removeEntity(entityId, false);
}

EntityPtr SpawnerWorld::getEntity(EntityId entityId) const {
  return m_worldServer->entity(entityId);
}

WorldGenerator::WorldGenerator(WorldServer* server) : m_worldServer(server) {
  m_microDungeonFactory = make_shared<MicroDungeonFactory>();
}

SectorGenerationResult WorldGenerator::generateSectorLevel(WorldStorage* worldStorage, Sector const& sector,
    SectorGenerationLevel generationLevel, bool allowAsync) {
  STAR_TRACE_ZONE("Generate sector level");
  TileRevisionScope revisionScope(TileRevisionCause::WorldGeneration,
      worldStorage->tileArray()->revisionTrackingEnabled());
  if (generationLevel == SectorGenerationLevel::BaseTiles) {
    STAR_TRACE_ZONE("Generate base tiles");
    return prepareTilesAsync(worldStorage, sector, allowAsync);
  } else if (generationLevel == SectorGenerationLevel::MicroDungeons) {
    STAR_TRACE_ZONE("Generate microdungeons");
    if (!worldStorage->floatingDungeonWorld())
      return generateMicroDungeons(worldStorage, sector, allowAsync);
  } else if (generationLevel == SectorGenerationLevel::CaveLiquid) {
    STAR_TRACE_ZONE("Generate cave liquid");
    if (!worldStorage->floatingDungeonWorld())
      generateCaveLiquid(worldStorage, sector);
  } else if (generationLevel == SectorGenerationLevel::Finalize) {
    STAR_TRACE_ZONE("Finalize sector");
    if (!worldStorage->floatingDungeonWorld())
      prepareSector(worldStorage, sector);
    else
      prepareSectorBiomeBlocks(worldStorage, sector);
    m_worldServer->activateLiquidRegion(worldStorage->tileArray()->sectorRegion(sector));
  }
  return {true, {}};
}

void WorldGenerator::sectorLoadLevelChanged(WorldStorage* worldStorage, Sector const& sector, SectorLoadLevel loadLevel) {
  if (loadLevel == SectorLoadLevel::None) {
    if (auto pending = m_pendingBaseTiles.ptr(sector)) {
      // Keep the key until the producer finishes: reload must not start a
      // second job for the same sector. Null also prevents pointer ABA reuse.
      pending->sourceTiles = nullptr;
      pending->task->invalidate();
      if (auto profile = m_worldServer->explorationProfile()) {
        profile->count("generation.baseTiles.pendingInvalidatedOnUnload");
        profile->recordGenerationTask(pending->task->identity, "invalidated", pending->task->reservedBytes);
      }
      worldStorage->tileArray()->takeRevisionTracking(sector);
    }
    if (auto pending = m_pendingMicroDungeonPlans.ptr(sector)) {
      pending->task->invalidate();
      m_readyMicroDungeonPlans.remove(sector);
      if (auto profile = m_worldServer->explorationProfile()) {
        profile->count("generation.microDungeonPlan.pendingInvalidatedOnUnload");
        profile->recordGenerationTask(pending->task->identity, "invalidated", pending->task->reservedBytes);
      }
    }
  }

  if (loadLevel == SectorLoadLevel::Loaded) {
    if (worldStorage->sectorGenerationLevel(sector) == SectorGenerationLevel::Complete)
      m_worldServer->activateLiquidRegion(worldStorage->tileArray()->sectorRegion(sector));
  }
}

List<WorldGenerator::Sector> WorldGenerator::readyBaseTileSectors(WorldStorage* worldStorage) {
  List<Sector> ready;
  double now = Time::monotonicTime();
  profileGenerationMemory(now);
  eraseWhere(m_pendingBaseTiles, [&](auto& entry) {
    if (!entry.second.promise->done())
      return false;
    if (!entry.second.sourceTiles) {
      worldStorage->tileArray()->takeRevisionTracking(entry.first);
      retireBaseTileTask(entry.second.task, entry.second.task->state() == WorldGenerationTaskState::Failed
          ? WorldGenerationTaskOutcome::Failed : WorldGenerationTaskOutcome::Cancelled);
      bool wasReady = m_readyBaseTiles.remove(entry.first);
      if (auto profile = m_worldServer->explorationProfile()) {
        profile->count("generation.baseTiles.unloadedResultsDiscarded");
        if (wasReady)
          profile->count("generation.readyResults.cancelled");
      }
      return true;
    }
    if (!m_readyBaseTiles.contains(entry.first)) {
      m_readyBaseTiles[entry.first] = ReadyBaseTiles{entry.second.task, now};
      if (auto profile = m_worldServer->explorationProfile()) {
        profile->count("generation.readyResults.enqueued");
        profile->count("generation.readyResults.bytes", entry.second.task->reservedBytes);
        profile->recordGenerationTask(entry.second.task->identity, "ready", entry.second.task->reservedBytes);
      }
    }
    return false;
  });
  for (auto const& entry : m_readyBaseTiles)
    ready.append(entry.first);
  if (auto profile = m_worldServer->explorationProfile()) {
    profile->count("generation.readyResults.pendingSamples");
    profile->count("generation.readyResults.pendingTotal", m_readyBaseTiles.size());
  }
  eraseWhere(m_pendingMicroDungeonPlans, [&](auto& entry) {
    if (!entry.second.promise->done() || entry.second.task->valid())
      return false;
    try {
      entry.second.promise->get();
    } catch (...) {}
    retireBaseTileTask(entry.second.task, entry.second.task->state() == WorldGenerationTaskState::Failed
        ? WorldGenerationTaskOutcome::Failed : WorldGenerationTaskOutcome::Cancelled);
    if (auto profile = m_worldServer->explorationProfile())
      profile->count("generation.microDungeonPlan.unloadedResultsDiscarded");
    return true;
  });
  return ready;
}

List<WorldGenerator::Sector> WorldGenerator::readyMicroDungeonPlanSectors(WorldStorage* worldStorage) {
  List<Sector> ready;
  double now = Time::monotonicTime();
  eraseWhere(m_readyMicroDungeonPlans, [&](auto const& entry) {
    auto pending = m_pendingMicroDungeonPlans.ptr(entry.first);
    return !pending || pending->task != entry.second.task || !pending->task->valid();
  });
  for (auto const& entry : m_pendingMicroDungeonPlans) {
    auto const& pending = entry.second;
    if (!pending.promise->done() || !pending.task->valid())
      continue;
    if (!m_readyMicroDungeonPlans.contains(entry.first)) {
      m_readyMicroDungeonPlans[entry.first] = ReadyMicroDungeonPlan{pending.task, now};
      if (auto profile = m_worldServer->explorationProfile()) {
        profile->count("generation.microDungeonPlan.readyResultsEnqueued");
        profile->recordGenerationTask(pending.task->identity, "ready", pending.task->reservedBytes);
      }
    }
    ready.append(entry.first);
  }
  if (auto profile = m_worldServer->explorationProfile()) {
    profile->count("generation.microDungeonPlan.readyResultsPendingSamples");
    profile->count("generation.microDungeonPlan.readyResultsPendingTotal", ready.size());
  }
  return ready;
}

bool WorldGenerator::admitBaseTilesPreparation(WorldStorage* worldStorage, Sector const& sector) {
  // The feeder must only add new private work. Polling an existing promise can
  // apply its result, which remains exclusively the normal owner-thread drain
  // path in WorldStorage::generateQueue.
  auto tileArray = worldStorage->tileArray();
  bool tilesInstalled = worldStorage->sectorLoadLevel(sector) >= SectorLoadLevel::Tiles
      && static_cast<ServerTileSectorArray const&>(*tileArray).sectorArray(sector);
  if (m_pendingBaseTiles.contains(sector) || worldSectorWorkerPool().getWorkerCount() == 0 || !tilesInstalled) {
    if (!tilesInstalled) {
      if (auto profile = m_worldServer->explorationProfile())
        profile->count("generation.baseTiles.tilesUnavailable");
    }
    return false;
  }
  prepareTilesAsync(worldStorage, sector, true);
  return m_pendingBaseTiles.contains(sector);
}

bool WorldGenerator::admitMicroDungeonPlanPreparation(WorldStorage* worldStorage, Sector const& sector) {
  // Never poll an existing promise here: consuming it applies dungeon changes,
  // entities and placement promises, which remain on the normal owner-thread
  // generation path. This feeder is scheduling-only.
  if (m_pendingMicroDungeonPlans.contains(sector) || worldSectorWorkerPool().getWorkerCount() == 0)
    return false;
  auto threading = Root::singleton().configuration()->get("worldThreading", JsonObject());
  if (!threading.getBool("microDungeonPlanPreview", false)
      || !threading.getBool("microDungeonPlanningWorkerPreview", false))
    return false;
  auto tileArray = worldStorage->tileArray();
  if (worldStorage->sectorLoadLevel(sector) < SectorLoadLevel::Tiles
      || !static_cast<ServerTileSectorArray const&>(*tileArray).sectorArray(sector))
    return false;
  // The planning snapshot copies and revalidates the complete padded 3x3
  // halo. The dependency graph's cardinal stage rule alone is insufficient:
  // admitting while a diagonal is still preparing BaseTiles would almost
  // certainly make this private snapshot stale before its normal apply path.
  auto tileHalo = tileArray->sectorRegion(sector).padded(WorldSectorSize - 1);
  for (auto const& haloSector : tileArray->validSectorsFor(tileHalo)) {
    auto level = worldStorage->sectorGenerationLevel(haloSector);
    if (!level || *level < SectorGenerationLevel::BaseTiles) {
      if (auto profile = m_worldServer->explorationProfile())
        profile->count("generation.microDungeonPlan.haloBaseTilesUnavailable");
      return false;
    }
  }
  generateMicroDungeons(worldStorage, sector, true);
  if (auto pending = m_pendingMicroDungeonPlans.ptr(sector)) {
    pending->dependencyPrepared = true;
    return true;
  }
  return false;
}

void WorldGenerator::profileGenerationMemory(double now) {
  auto profile = m_worldServer->explorationProfile();
  if (!profile || now - m_lastGenerationMemoryProfileTime < 1.0)
    return;
  m_lastGenerationMemoryProfileTime = now;
  auto usage = worldGenerationMemoryLedger().snapshot();
  profile->count("generation.memory.process.samples");
  profile->count("generation.memory.process.sampledReservedArrayBytesTotal", usage.reservedArrayBytes);
  profile->count("generation.memory.process.sampledResourcesTotal", usage.classResources[0] + usage.classResources[1]);
  profile->count("generation.memory.process.sampledBaseTilesArrayBytesTotal",
      usage.classBytes[static_cast<size_t>(WorldGenerationMemoryClass::BaseTilesArray)]);
  profile->count("generation.memory.process.sampledMicroDungeonHaloArrayBytesTotal",
      usage.classBytes[static_cast<size_t>(WorldGenerationMemoryClass::MicroDungeonHalo)]);
  profile->maximum("generation.memory.process.sampleMaximumReservedArrayBytes", usage.reservedArrayBytes);
  profile->maximum("generation.memory.process.lifetimePeakReservedArrayBytes", usage.peakReservedArrayBytes);
  profile->count("generation.memory.process.sampledEstimatedBytesTotal", usage.estimatedBytes);
  profile->count("generation.memory.process.sampledSharedJsonNodesTotal", usage.sharedResources);
  profile->count("generation.memory.process.sampledTemplateJsonEstimateTotal", usage.classBytes[2]);
  profile->count("generation.memory.process.sampledMetadataEstimateTotal", usage.classBytes[3]);
  profile->count("generation.memory.process.sampledPlanEstimateTotal", usage.classBytes[4]);
  profile->count("generation.memory.process.sampledWorkerTemplateEstimateTotal", usage.classBytes[5]);
  profile->count("generation.memory.process.sampledWorkerTemplatesTotal", usage.classResources[5]);
  profile->maximum("generation.memory.process.lifetimeIncompleteEstimateUpdates", usage.incompleteEstimateUpdates);
  profile->count("generation.memory.process.sampledReservedBufferBytesTotal", usage.reservedBufferBytes);
  profile->count("generation.memory.process.sampledPlanBufferRequestBytesTotal", usage.classBytes[6]);
  profile->count("generation.memory.process.sampledCandidateBufferRequestBytesTotal", usage.classBytes[7]);
  profile->count("generation.memory.process.sampledBufferResourcesTotal", usage.classResources[6] + usage.classResources[7]);
  profile->maximum("generation.memory.process.lifetimePeakReservedBufferBytes", usage.peakReservedBufferBytes);
  profile->maximum("generation.memory.buffers.modelVersion", 1);
  profile->maximum("generation.memory.process.lifetimePeakEstimatedBytes", usage.peakEstimatedBytes);
  profile->maximum("generation.memory.estimates.modelVersion", WorldGenerationSizeEstimate::ModelVersion);
  profile->maximum("generation.memory.estimates.workerCacheModelVersion", 1);
  if (profile->regionalDetailsEnabled()) profile->count("generation.memory.estimates.enabledSamples");
}

WorldGenerationJsonSnapshotPtr WorldGenerator::templateStoreSnapshot(TemplateStoreCache& cache,
    WorldTemplatePtr const& source, uint64_t revision) {
  auto profile = m_worldServer->explorationProfile();
  bool measure = profile && profile->regionalDetailsEnabled();
  bool changed = cache.source != source || cache.revision != revision || !cache.store;
  if (!changed && (!measure || cache.store->measured)) return cache.store;
  auto value = changed ? source->store() : cache.store->value;
  ExplorationProfile::Scope sizing(measure ? profile : nullptr, "generation.memory.estimates.templateSizing");
  auto store = captureWorldGenerationJson(worldGenerationMemoryLedger(), std::move(value), measure);
  // Publish cache identity only after capture succeeds. A failed allocation
  // must not tag an old/empty store as the new revision.
  cache.source = source;
  cache.revision = revision;
  cache.store = std::move(store);
  if (measure) {
    profile->count("generation.memory.estimates.templateMeasurements");
    if (!cache.store->complete) profile->count("generation.memory.estimates.templateIncomplete");
  }
  return cache.store;
}

shared_ptr<WorldGenerationMemoryReservation> WorldGenerator::trackMemoryEstimate(
    WorldGenerationMemoryClass kind, WorldGenerationSizeEstimate const& estimate) {
  auto profile = m_worldServer->explorationProfile();
  if (!profile || !profile->regionalDetailsEnabled()) return {};
  auto memory = estimate.complete ? worldGenerationMemoryLedger().reserve(kind, estimate.bytes) : nullptr;
  if (!memory) profile->count("generation.memory.estimates.reservationIncomplete");
  return memory;
}

void WorldGenerator::retireBaseTileTask(shared_ptr<WorldGenerationTask> const& task, WorldGenerationTaskOutcome outcome) {
  if (!m_generationTasks || !m_generationTasks->retire(task, outcome))
    throw StarException("Invalid generation task retirement");
  if (auto profile = m_worldServer->explorationProfile()) {
    char const* event = "failed";
    char const* counter = "generation.tasks.failed";
    switch (outcome) {
      case WorldGenerationTaskOutcome::Committed:
        event = "committed";
        counter = "generation.tasks.committed";
        break;
      case WorldGenerationTaskOutcome::Stale:
        event = "stale";
        counter = "generation.tasks.stale";
        break;
      case WorldGenerationTaskOutcome::Cancelled:
        event = "cancelled";
        counter = "generation.tasks.cancelled";
        break;
      case WorldGenerationTaskOutcome::Failed:
        break;
    }
    profile->count(counter);
    profile->recordGenerationTask(task->identity, event, task->reservedBytes);
  }
}

void WorldGenerator::terraformSector(WorldStorage* worldStorage, Sector const& sector) {
  // Logger::info("terraforming sector {}...", sector);
  reapplyBiome(worldStorage, sector);
}

void WorldGenerator::initEntity(WorldStorage*, EntityId entityId, EntityPtr const& entity) {
  entity->init(m_worldServer, entityId, EntityMode::Master);
  if (auto tileEntity = as<TileEntity>(entity))
    m_worldServer->updateTileEntityTiles(tileEntity, false, false);
}

void WorldGenerator::destructEntity(WorldStorage*, EntityPtr const& entity) {
  if (entity->isSlave())
    throw StarException("Cannot destruct slave entity in WorldStorage, something has gone wrong!");
  if (auto tileEntity = as<TileEntity>(entity))
    m_worldServer->updateTileEntityTiles(tileEntity, true, false);
  entity->uninit();
}

bool WorldGenerator::entityKeepAlive(WorldStorage*, EntityPtr const& entity) const {
  return entity->isSlave() || (entity->isMaster() && entity->keepAlive());
}

bool WorldGenerator::entityPersistent(WorldStorage*, EntityPtr const& entity) const {
  return entity->isMaster() && entity->persistent();
}

RpcPromise<Vec2I> WorldGenerator::enqueuePlacement(List<BiomeItemDistribution> distributions, Maybe<DungeonId> id) {
  auto promise = RpcPromise<Vec2I>::createPair();
  m_queuedPlacements.append(QueuedPlacement {
    std::move(distributions),
    id,
    promise.second,
  });
  return promise.first;
}

void WorldGenerator::replaceBiomeBlocks(ServerTile* tile) {
  auto materialDatabase = Root::singleton().materialDatabase();
  MaterialId oldForeground = tile->foreground;

  if (auto blockBiome = m_worldServer->worldTemplate()->biome(tile->blockBiomeIndex)) {
    if (tile->foreground == BiomeMaterialId) {
      tile->foreground = blockBiome->mainBlock;
      tile->foregroundHueShift = m_worldServer->worldTemplate()->biomeMaterialHueShift(tile->blockBiomeIndex, tile->foreground);
    } else if ((tile->foreground >= Biome1MaterialId) && (tile->foreground <= Biome5MaterialId)) {
      auto& subblocks = blockBiome->subBlocks;
      if (subblocks.size())
        tile->foreground = subblocks[(tile->foreground - Biome1MaterialId) % subblocks.size()];
      else
        tile->foreground = blockBiome->mainBlock;
      tile->foregroundHueShift = m_worldServer->worldTemplate()->biomeMaterialHueShift(tile->blockBiomeIndex, tile->foreground);
    }

    if (tile->background == BiomeMaterialId) {
      tile->background = blockBiome->mainBlock;
      tile->backgroundHueShift =
          m_worldServer->worldTemplate()->biomeMaterialHueShift(tile->blockBiomeIndex, tile->background);
    } else if ((tile->background >= Biome1MaterialId) && (tile->background <= Biome5MaterialId)) {
      auto& subblocks = blockBiome->subBlocks;
      if (subblocks.size())
        tile->background = subblocks[(tile->background - Biome1MaterialId) % subblocks.size()];
      else
        tile->background = blockBiome->mainBlock;
      tile->backgroundHueShift = m_worldServer->worldTemplate()->biomeMaterialHueShift(tile->blockBiomeIndex, tile->background);
    }
  } else {
    if (isBiomeMaterial(tile->foreground)) {
      tile->foreground = EmptyMaterialId;
      tile->foregroundHueShift = 0;
    }
    if (isBiomeMod(tile->foregroundMod)) {
      tile->foregroundMod = NoModId;
      tile->foregroundModHueShift = 0;
    }
    if (isBiomeMaterial(tile->background)) {
      tile->background = EmptyMaterialId;
      tile->backgroundHueShift = 0;
    }
    if (isBiomeMod(tile->backgroundMod)) {
      tile->backgroundMod = NoModId;
      tile->backgroundModHueShift = 0;
    }
  }

  tile->updateCollision(biomeForegroundCollision(materialDatabase, oldForeground, tile->foreground, tile->collision));
}

void WorldGenerator::prepareTiles(WorldStorage* worldStorage, ServerTileSectorArray::Sector const& sector) {
  auto materialDatabase = Root::singleton().materialDatabase();
  auto planet = m_worldServer->worldTemplate();
  auto tileArray = worldStorage->tileArray();
  RectI sectorRegion = tileArray->sectorRegion(sector);
  auto tiles = tileArray->sectorArray(sector);
  starAssert(tiles);
  prepareBaseTileArray(planet, materialDatabase, sectorRegion, *tiles);
}

SectorGenerationResult WorldGenerator::prepareTilesAsync(WorldStorage* worldStorage,
    ServerTileSectorArray::Sector const& sector, bool allowAsync) {
  STAR_TRACE_ZONE("Prepare tiles async");
  double now = Time::monotonicTime();
  auto profile = m_worldServer->explorationProfile();
  if (profile && allowAsync) {
    profile->count("generation.baseTiles.pendingDepthSamples");
    profile->count("generation.baseTiles.pendingDepthTotal", m_pendingBaseTiles.size());
    if (now - m_lastPoolProfileTime >= 1.0) {
      auto state = worldSectorWorkerPool().diagnosticState();
      profile->count("generation.baseTiles.poolSamples");
      profile->count("generation.baseTiles.poolWorkersTotal", state.workers);
      profile->count("generation.baseTiles.poolActiveTotal", state.active);
      profile->count("generation.baseTiles.poolQueuedTotal", state.queued);
      m_lastPoolProfileTime = now;
    }
  }
  auto pending = m_pendingBaseTiles.ptr(sector);
  if (pending) {
    Maybe<double> readyAt;
    if (auto ready = m_readyBaseTiles.ptr(sector)) {
      if (ready->task == pending->task)
        readyAt = ready->readyAt;
    }
    if (allowAsync && !pending->promise->done()) {
      if (profile) {
        profile->count("generation.baseTiles.pendingPolls");
        profile->record("generation.baseTiles.pendingAge", now - pending->scheduledAt);
      }
      return {false, {}};
    }

    PreparedBaseTiles result;
    try {
      result = std::move(pending->promise->get());
    } catch (...) {
      retireBaseTileTask(pending->task, WorldGenerationTaskOutcome::Failed);
      m_readyBaseTiles.remove(sector);
      m_pendingBaseTiles.remove(sector);
      throw;
    }
    bool taskCurrent = m_generationTasks->contains(pending->task) && pending->task->valid()
        && pending->task->identity.worldEpoch == m_worldServer->instanceEpoch();
    bool tilesCurrent = taskCurrent && pending->sourceTiles && static_cast<ServerTileSectorArray const&>(*worldStorage->tileArray()).sectorArray(sector) == pending->sourceTiles;
    bool revisionCurrent = worldStorage->tileArray()->sectorRevision(sector) == pending->sourceTileRevision;
    bool templateCurrent = m_worldServer->worldTemplate().get() == pending->sourceTemplate;
    bool templateRevisionCurrent = m_worldServer->worldTemplate()->generationRevision() == pending->sourceTemplateRevision;
    bool snapshotStillCurrent = tilesCurrent && revisionCurrent && templateCurrent && templateRevisionCurrent;
    if (profile) {
      auto revisions = worldStorage->tileArray()->takeRevisionTracking(sector);
      profile->count("generation.baseTiles.resultsReady");
      profile->record("generation.baseTiles.queueDelay", result.queueSeconds);
      profile->record("generation.baseTiles.commitDelay", Time::monotonicTime() - result.completedAt);
      profile->record("generation.baseTiles.resultAge", Time::monotonicTime() - pending->scheduledAt);
      if (!tilesCurrent)
        profile->count("generation.baseTiles.staleTiles");
      if (!revisionCurrent)
        profile->count("generation.baseTiles.staleRevision");
      if (!templateCurrent)
        profile->count("generation.baseTiles.staleTemplate");
      if (!templateRevisionCurrent)
        profile->count("generation.baseTiles.staleTemplateRevision");
      if (!snapshotStillCurrent) {
        auto currentRevision = worldStorage->tileArray()->sectorRevision(sector);
        profile->recordBaseTileOutcome(Vec2I((int)sector[0], (int)sector[1]), true,
            currentRevision - pending->sourceTileRevision,
            revisionReasonJson(revisions, profile, true), now - pending->scheduledAt);
      }
    }
    if (!snapshotStillCurrent) {
      retireBaseTileTask(pending->task, pending->task->valid()
          ? WorldGenerationTaskOutcome::Stale : WorldGenerationTaskOutcome::Cancelled);
      m_readyBaseTiles.remove(sector);
      m_pendingBaseTiles.remove(sector);
      if (profile) {
        profile->count("generation.baseTiles.staleResults");
        if (readyAt)
          profile->count("generation.readyResults.stale");
      }
      if (!allowAsync) {
        prepareTiles(worldStorage, sector);
        return {true, {}};
      }
      return prepareTilesAsync(worldStorage, sector, true);
    }
    auto tileArray = worldStorage->tileArray();
    tileArray->loadSector(sector, std::move(result.tiles->value));
    auto collisionRegion = tileArray->sectorRegion(sector).padded(CollisionGenerator::BlockInfluenceRadius);
    for (int x = collisionRegion.xMin(); x < collisionRegion.xMax(); ++x) {
      for (int y = collisionRegion.yMin(); y < collisionRegion.yMax(); ++y)
        tileArray->modifyCollisionCache({x, y}, [](bool& dirty, auto&) { dirty = true; });
    }
    if (profile) {
      profile->count("generation.baseTiles.resultsApplied");
      if (readyAt) {
        profile->count("generation.readyResults.committed");
        profile->record("generation.readyResults.commitDelay", Time::monotonicTime() - *readyAt);
      }
    }
    retireBaseTileTask(pending->task, WorldGenerationTaskOutcome::Committed);
    m_readyBaseTiles.remove(sector);
    m_pendingBaseTiles.remove(sector);
    return {true, result.elapsedSeconds};
  }

  auto tileArray = worldStorage->tileArray();
  if (worldStorage->sectorLoadLevel(sector) < SectorLoadLevel::Tiles
      || !static_cast<ServerTileSectorArray const&>(*tileArray).sectorArray(sector)) {
    if (profile)
      profile->count("generation.baseTiles.tilesUnavailable");
    return {false, {}};
  }

  auto& workerPool = worldSectorWorkerPool();
  if (!allowAsync || workerPool.getWorkerCount() == 0) {
    prepareTiles(worldStorage, sector);
    return {true, {}};
  }

  auto threading = Root::singleton().configuration()->get("worldThreading", JsonObject());
  bool baseTilesDeliveryCapacityPreview = threading.getBool("generationBaseTilesDeliveryCapacityPreview", false);
  unsigned baseTilesPendingMultiplier = baseTilesDeliveryCapacityPreview
      ? clamp<unsigned>(threading.getUInt("generationBaseTilesPendingMultiplier", 4), 2, 4)
      : 2;
  size_t pendingLimit = workerPool.getWorkerCount() * baseTilesPendingMultiplier;
  if (profile)
    profile->maximum("generation.baseTiles.pendingLimit", pendingLimit);
  if (m_pendingBaseTiles.size() >= pendingLimit) {
    // WorldStorage drains ready BaseTiles before requesting new work. Retain
    // completed results rather than evicting a neighbor needed by the queue.
    // WorldGenerationTasks bounds retained arrays, bytes and task identities.
    if (profile)
      profile->count("generation.baseTiles.admissionDeferred");
    return {false, {}};
  }

  auto maximumBytes = worldGenerationSnapshotBytes(pendingLimit,
      9 * WorldSectorSize * WorldSectorSize * sizeof(ServerTile));
  if (!maximumBytes)
    return {false, {}};
  if (!m_generationTasks) {
    m_generationTasks = make_unique<WorldGenerationTasks>(m_worldServer->instanceEpoch(),
        WorldRegionPartition(WorldGeometry(tileArray->size())), pendingLimit, *maximumBytes);
  }
  WorldGenerationTaskPreparation preparation(*m_generationTasks, Vec2U(sector),
      WorldGenerationTaskStage::BaseTiles, sizeof(ServerTileSectorArray::Array));
  auto task = preparation.task();
  if (!task) {
    if (profile)
      profile->count("generation.tasks.admissionDeferred");
    return {false, {}};
  }
  if (profile) {
    profile->count("generation.tasks.reserved");
    profile->recordGenerationTask(task->identity, "reserved", task->reservedBytes);
  }
  // The reservation outlives every private input declared below, so failed
  // preparation destroys its copies before reclaiming the unsubmitted bytes.
  auto sourceTiles = static_cast<ServerTileSectorArray const&>(*tileArray).sectorArray(sector);
  auto sourceTileRevision = tileArray->sectorRevision(sector);
  auto tilesHolder = makeWorldGenerationTrackedResource<ServerTileSectorArray::ArrayPtr>(
      worldGenerationMemoryLedger(), WorldGenerationMemoryClass::BaseTilesArray,
      sizeof(ServerTileSectorArray::Array));
  if (!tilesHolder) {
    if (profile)
      profile->count("generation.memory.process.admissionDeferred");
    return {false, {}};
  }
  tilesHolder->value = tileArray->copySector(sector);
  starAssert(sourceTiles);
  if (!tilesHolder->value)
    return {false, {}};
  auto sectorRegion = tileArray->sectorRegion(sector);
  auto planet = m_worldServer->worldTemplate();
  auto templateRevision = planet->generationRevision();
  auto templateStore = templateStoreSnapshot(m_baseTileTemplateCache, planet, templateRevision);
  auto materialDatabase = Root::singleton().materialDatabase();
  double scheduledAt = profile ? Time::monotonicTime() : now;
  try {
    // Allocate the owner record before dispatch: a map allocation after the
    // pool accepted the producer must not orphan its promise/reservation.
    auto& pendingRecord = m_pendingBaseTiles[sector];
    WorldGenerationSizeEstimate metadata;
    metadata.add(sizeof(PendingBaseTiles));
    pendingRecord = PendingBaseTiles{trackMemoryEstimate(WorldGenerationMemoryClass::SnapshotMetadataEstimate, metadata),
        task, sourceTiles, sourceTileRevision,
        planet.get(), templateRevision, scheduledAt, {}};
    if (profile)
      tileArray->beginRevisionTracking(sector);
    auto promise = workerPool.addProducer<PreparedBaseTiles>(
      [task, planet, templateRevision, templateStore, materialDatabase, sectorRegion, tilesHolder, scheduledAt,
          memoryLedger = worldGenerationMemoryLedger()]() mutable {
        if (!task->start())
          throw StarException("Generation task started more than once");
        try {
          STAR_TRACE_ZONE("Base tiles worker");
          double start = Time::monotonicTime();
          // Invalidation keeps the reservation until this producer exits. A
          // queued task unloaded in the meantime need not calculate terrain.
          if (task->valid()) {
            auto workerPlanet = workerWorldTemplate(planet, templateRevision, templateStore, memoryLedger);
            prepareBaseTileArray(workerPlanet.value(), materialDatabase, sectorRegion, *tilesHolder->value);
          }
          double completedAt = Time::monotonicTime();
          PreparedBaseTiles result{tilesHolder, completedAt - start, start - scheduledAt, completedAt};
          if (!task->ready())
            throw StarException("Generation task completed in an invalid state");
          return result;
        } catch (...) {
          task->fail();
          throw;
        }
      });
    preparation.handoff();
    // The optional is empty; emplacement only moves a shared promise handle.
    static_assert(std::is_nothrow_move_constructible<WorkerPoolPromise<PreparedBaseTiles>>::value,
        "Generation promise handoff must not allocate or throw");
    pendingRecord.promise.emplace(std::move(promise));
  } catch (...) {
    if (preparation.task()) {
      m_pendingBaseTiles.remove(sector);
      if (profile)
        tileArray->takeRevisionTracking(sector);
    }
    // Unsubmitted inputs unwind before the preparation guard rolls back.
    // Once handed off, only normal promise completion/drain may retire it.
    throw;
  }
  if (profile)
    profile->count("generation.baseTiles.scheduled");
  return {false, {}};
}

SectorGenerationResult WorldGenerator::generateMicroDungeons(WorldStorage* worldStorage,
    ServerTileSectorArray::Sector const& sector, bool allowAsync) {
  STAR_TRACE_ZONE("Microdungeon placements");
  auto profile = m_worldServer->explorationProfile();
  auto threading = Root::singleton().configuration()->get("worldThreading", JsonObject());
  bool planPreview = threading.getBool("microDungeonPlanPreview", false);
  bool workerPreview = threading.getBool("microDungeonPlanningWorkerPreview", false);
  double now = Time::monotonicTime();
  if (profile && workerPreview && allowAsync) {
    profile->count("generation.microDungeonPlan.pendingDepthSamples");
    profile->count("generation.microDungeonPlan.pendingDepthTotal", m_pendingMicroDungeonPlans.size());
  }

  if (auto pending = m_pendingMicroDungeonPlans.ptr(sector)) {
    if (allowAsync && !pending->promise->done()) {
      if (profile) {
        profile->count("generation.microDungeonPlan.pendingPolls");
        profile->record("generation.microDungeonPlan.pendingAge", now - pending->scheduledAt);
      }
      return {false, {}};
    }
    PreparedMicroDungeonPlan const* preparedResult = nullptr;
    try {
      preparedResult = &pending->promise->get();
    } catch (...) {
      retireBaseTileTask(pending->task, WorldGenerationTaskOutcome::Failed);
      m_pendingMicroDungeonPlans.remove(sector);
      m_readyMicroDungeonPlans.remove(sector);
      throw;
    }
    bool current = m_generationTasks->contains(pending->task) && pending->task->valid()
        && pending->task->identity.worldEpoch == m_worldServer->instanceEpoch()
        && m_worldServer->worldTemplate().get() == pending->sourceTemplate
        && m_worldServer->worldTemplate()->generationRevision() == pending->sourceTemplateRevision;
    {
      ExplorationProfile::Scope timing(profile, "generation.microDungeonPlan.apply.revalidate");
      for (auto const& revision : pending->sourceSectorRevisions)
        current = current && worldStorage->tileArray()->sectorRevision(revision.first) == revision.second;
    }
    if (!current) {
      bool dependencyPrepared = pending->dependencyPrepared;
      retireBaseTileTask(pending->task, pending->task->valid()
          ? WorldGenerationTaskOutcome::Stale : WorldGenerationTaskOutcome::Cancelled);
      m_pendingMicroDungeonPlans.remove(sector);
      m_readyMicroDungeonPlans.remove(sector);
      if (profile) {
        profile->count("generation.microDungeonPlan.staleResults");
        if (dependencyPrepared)
          profile->count("generation.preparationFeeder.microDungeonPlanStale");
        profile->count("generation.microDungeonPlan.retry");
      }
      return allowAsync ? generateMicroDungeons(worldStorage, sector, true) : SectorGenerationResult{false, {}};
    }
    // A definition can take a long first load but requires no world mutation.
    // Keep this owner-thread-only preview separate from applying the plan so a
    // selected visible plan can warm its bounded list before the commit pass.
    // The time limit controls admission between definitions; an individual
    // asset read remains non-preemptible and is recorded as an overrun.
    auto threading = Root::singleton().configuration()->get("worldThreading", JsonObject());
    bool definitionPrewarmPreview = allowAsync && planPreview && workerPreview
        && threading.getBool("generationMicroDungeonDefinitionPrewarmPreview", false);
    if (definitionPrewarmPreview && !pending->definitionsPrewarmed) {
      // Empty plans have no definition to materialize. Applying them now
      // avoids adding an otherwise useless owner-thread round trip.
      if (preparedResult->plans.empty()) {
        pending->definitionsPrewarmed = true;
        if (profile)
          profile->count("generation.microDungeonDefinitionPrewarm.skippedEmptyPlans");
      } else {
        unsigned prewarmMilliseconds = clamp<unsigned>(
            threading.getUInt("generationMicroDungeonDefinitionPrewarmMilliseconds", 1), 1, 8);
        unsigned prewarmCandidateLimit = clamp<unsigned>(
            threading.getUInt("generationMicroDungeonDefinitionPrewarmCandidateLimit", 1), 1, 8);
        double deadline = Time::monotonicTime() + prewarmMilliseconds / 1000.0;
        unsigned warmed = 0;
        if (profile)
          profile->count("generation.microDungeonDefinitionPrewarm.passes");
        while (pending->definitionPrewarmNextPlan < preparedResult->plans.size()
            && warmed < prewarmCandidateLimit && Time::monotonicTime() < deadline) {
          auto const& plan = preparedResult->plans[pending->definitionPrewarmNextPlan++];
          DungeonDefinitionLoadTiming definitionTiming;
          double started = Time::monotonicTime();
          Root::singleton().dungeonDefinitions()->get(plan.dungeonName, &definitionTiming);
          double elapsed = Time::monotonicTime() - started;
          if (profile) {
            profile->count("generation.microDungeonDefinitionPrewarm.attempted");
            profile->count(definitionTiming.cacheHit
                ? "generation.microDungeonDefinitionPrewarm.cacheHits"
                : "generation.microDungeonDefinitionPrewarm.cacheMisses");
            if (definitionTiming.cacheEvicted)
              profile->count("generation.microDungeonDefinitionPrewarm.evictions");
            profile->record("generation.microDungeonDefinitionPrewarm.definition", elapsed);
            if (Time::monotonicTime() >= deadline)
              profile->count("generation.microDungeonDefinitionPrewarm.deadlineReached");
          }
          ++warmed;
        }
        pending->definitionsPrewarmed = pending->definitionPrewarmNextPlan >= preparedResult->plans.size();
        if (profile && !pending->definitionsPrewarmed)
          profile->count("generation.microDungeonDefinitionPrewarm.deferred");
        return {false, {}};
      }
    }
    // Keep the ready payload alive through owner callbacks without copying
    // its vectors/strings or destructively consuming it before apply succeeds.
    auto resultOwner = *pending->promise;
    auto const& result = resultOwner.get();
    auto task = pending->task;
    bool dependencyPrepared = pending->dependencyPrepared;
    if (profile && !result.memoryEstimateComplete)
      profile->count("generation.memory.estimates.planIncomplete");
    if (profile && result.memoryEstimateSeconds > 0)
      profile->record("generation.memory.estimates.planSizing", result.memoryEstimateSeconds);
    ExplorationProfile::Scope facadeTiming(profile, "generation.microDungeonPlan.apply.facade");
    auto facade = make_shared<DungeonGeneratorWorld>(m_worldServer, false);
    facadeTiming.stop();
    ExplorationProfile::Scope plansTiming(profile, "generation.microDungeonPlan.apply.plans");
    for (auto const& plan : result.plans)
      applyMicroDungeonPlan(worldStorage, plan, facade, profile, planPreview);
    plansTiming.stop();
    retireBaseTileTask(task, WorldGenerationTaskOutcome::Committed);
    m_pendingMicroDungeonPlans.remove(sector);
    Maybe<double> readyAt;
    if (auto ready = m_readyMicroDungeonPlans.ptr(sector))
      readyAt = ready->readyAt;
    m_readyMicroDungeonPlans.remove(sector);
    if (profile) {
      profile->count("generation.microDungeonPlan.resultsApplied");
      if (readyAt)
        profile->record("generation.microDungeonPlan.readyResultDelay", now - *readyAt);
      if (dependencyPrepared)
        profile->count("generation.preparationFeeder.microDungeonPlanCommitted");
      profile->record("generation.microDungeonPlan.queueDelay", result.queueSeconds);
    }
    return {true, result.elapsedSeconds};
  }

  auto& workerPool = worldSectorWorkerPool();
  if (workerPreview && allowAsync && workerPool.getWorkerCount() > 0) {
    size_t pendingLimit = workerPool.getWorkerCount() * 2;
    if (m_pendingMicroDungeonPlans.size() >= pendingLimit)
      return {false, {}};
    auto tileArray = worldStorage->tileArray();
    RectI sectorTiles = tileArray->sectorRegion(sector);
    RectI tileHalo = sectorTiles.padded(WorldSectorSize - 1);
    auto haloSectors = tileArray->validSectorsFor(tileHalo);
    // Enumerate/check metadata without allocating the private tile buffers.
    for (auto const& haloSector : haloSectors) {
      if (!static_cast<ServerTileSectorArray const&>(*tileArray).sectorArray(haloSector))
        return {false, {}};
    }
    auto bytes = worldGenerationSnapshotBytes(haloSectors.size(), sizeof(ServerTileSectorArray::Array));
    bool baseTilesDeliveryCapacityPreview = threading.getBool("generationBaseTilesDeliveryCapacityPreview", false);
    unsigned baseTilesPendingMultiplier = baseTilesDeliveryCapacityPreview
        ? clamp<unsigned>(threading.getUInt("generationBaseTilesPendingMultiplier", 4), 2, 4)
        : 2;
    size_t taskPendingLimit = max(pendingLimit, workerPool.getWorkerCount() * (size_t)baseTilesPendingMultiplier);
    auto maximumBytes = worldGenerationSnapshotBytes(taskPendingLimit,
        9 * WorldSectorSize * WorldSectorSize * sizeof(ServerTile));
    if (!bytes || !maximumBytes)
      return {false, {}};
    if (!m_generationTasks)
      m_generationTasks = make_unique<WorldGenerationTasks>(m_worldServer->instanceEpoch(),
          WorldRegionPartition(WorldGeometry(tileArray->size())), taskPendingLimit, *maximumBytes);
    WorldGenerationTaskPreparation preparation(*m_generationTasks, Vec2U(sector),
        WorldGenerationTaskStage::MicroDungeonPlan, *bytes);
    auto task = preparation.task();
    if (!task) {
      if (profile)
        profile->count("generation.tasks.admissionDeferred");
      return {false, {}};
    }
    auto snapshotHolder = makeWorldGenerationTrackedResource<MicroDungeonPlanSnapshot>(
        worldGenerationMemoryLedger(), WorldGenerationMemoryClass::MicroDungeonHalo, *bytes);
    if (!snapshotHolder) {
      if (profile)
        profile->count("generation.memory.process.admissionDeferred");
      return {false, {}};
    }
    auto templateSource = m_worldServer->worldTemplate();
    auto templateRevision = templateSource->generationRevision();
    auto templateStore = templateStoreSnapshot(m_microDungeonTemplateCache, templateSource, templateRevision);
    auto& snapshot = snapshotHolder->value;
    snapshot = MicroDungeonPlanSnapshot{{}, templateStore, templateRevision, sector,
        sectorTiles, tileHalo, {}, {}, {}};
    for (auto const& haloSector : haloSectors) {
      snapshot.sectorRevisions[haloSector] = tileArray->sectorRevision(haloSector);
      auto tiles = tileArray->copySector(haloSector);
      if (!tiles)
        return {false, {}};
      snapshot.haloTiles.append({haloSector, std::move(tiles)});
    }
    for (auto const& entry : m_queuedPlacements.entries())
      snapshot.queuedPlacements.append({entry.second.distributions, entry.first});
    if (profile && profile->regionalDetailsEnabled()) {
      ExplorationProfile::Scope sizing(profile, "generation.memory.estimates.snapshotSizing");
      WorldGenerationSizeEstimate metadata;
      metadata.add(sizeof(MicroDungeonPlanSnapshot));
      metadata.add(snapshot.sectorRevisions.size(), sizeof(decltype(snapshot.sectorRevisions)::value_type));
      metadata.add(snapshot.haloTiles.capacity(), sizeof(decltype(snapshot.haloTiles)::value_type));
      metadata.add(snapshot.queuedPlacements.capacity(), sizeof(QueuedMicroDungeonPlacementSnapshot));
      for (auto const& placement : snapshot.queuedPlacements)
        metadata.add(placement.distributions.capacity(), sizeof(BiomeItemDistribution));
      snapshot.metadataMemory = trackMemoryEstimate(WorldGenerationMemoryClass::SnapshotMetadataEstimate, metadata);
    }
    auto revisions = snapshot.sectorRevisions;
    WorldGenerationSizeEstimate pendingMetadata;
    pendingMetadata.add(sizeof(PendingMicroDungeonPlan));
    pendingMetadata.add(revisions.size(), sizeof(decltype(revisions)::value_type));
    auto pendingMemory = trackMemoryEstimate(WorldGenerationMemoryClass::SnapshotMetadataEstimate, pendingMetadata);
    bool measurePlan = profile && profile->regionalDetailsEnabled();
    auto planMemory = measurePlan
        ? trackMemoryEstimate(WorldGenerationMemoryClass::MicroDungeonPlanEstimate, {}) : nullptr;
    MicroDungeonPlanAllocator planAllocator(worldGenerationMemoryLedger(), WorldGenerationMemoryClass::MicroDungeonPlanBuffer);
    try {
      auto& pendingRecord = m_pendingMicroDungeonPlans[sector];
      pendingRecord = PendingMicroDungeonPlan{pendingMemory, task, std::move(revisions), templateSource.get(), templateRevision, now, {}};
      auto promise = workerPool.addProducer<PreparedMicroDungeonPlan>([task, templateSource, templateRevision, templateStore, snapshotHolder, planMemory, planAllocator, measurePlan, now,
          memoryLedger = worldGenerationMemoryLedger()]() mutable {
        if (!task->start())
          throw StarException("Generation task started more than once");
        try {
          double started = Time::monotonicTime();
          MicroDungeonPlans plans(planAllocator);
          if (task->valid()) {
            auto workerPlanet = workerWorldTemplate(templateSource, templateRevision, templateStore, memoryLedger);
            plans = planMicroDungeons(workerPlanet.value(), snapshotHolder->value, planAllocator);
          }
          bool estimateComplete = true;
          double estimateSeconds = 0;
          if (measurePlan) {
            double sizingStarted = Time::monotonicTime();
            WorldGenerationSizeEstimate estimate;
            estimate.add(sizeof(PreparedMicroDungeonPlan));
            // Descriptor capacity is already charged by the buffer allocator.
            for (auto const& plan : plans) estimate.stringCapacity(plan.dungeonName);
            estimateComplete = estimate.complete && planMemory && planMemory->resizeEstimate(estimate.bytes);
            estimateSeconds = Time::monotonicTime() - sizingStarted;
          }
          double completed = Time::monotonicTime();
          if (!task->ready())
            throw StarException("Generation task completed in an invalid state");
          return PreparedMicroDungeonPlan{planMemory, std::move(plans), completed - started,
              started - now, completed, estimateComplete, estimateSeconds};
        } catch (...) { task->fail(); throw; }
      });
      preparation.handoff();
      static_assert(std::is_nothrow_move_constructible<WorkerPoolPromise<PreparedMicroDungeonPlan>>::value,
          "Generation promise handoff must not allocate or throw");
      pendingRecord.promise.emplace(std::move(promise));
    } catch (...) {
      if (preparation.task())
        m_pendingMicroDungeonPlans.remove(sector);
      throw;
    }
    if (profile) {
      profile->count("generation.microDungeonPlan.scheduled");
      profile->count("generation.microDungeonPlan.snapshotBytes", *bytes);
      profile->recordGenerationTask(task->identity, "reserved", *bytes);
    }
    return {false, {}};
  }

  auto facade = make_shared<DungeonGeneratorWorld>(m_worldServer, false);
  ExplorationProfile::Scope phase(profile, "generation.microDungeonPlan.collect");

  auto templateSource = m_worldServer->worldTemplate();
  auto templateRevision = templateSource->generationRevision();
  auto templateStore = templateStoreSnapshot(m_microDungeonTemplateCache, templateSource, templateRevision);
  auto tileArray = worldStorage->tileArray();
  RectI sectorTiles = tileArray->sectorRegion(sector);
  MicroDungeonPlanSnapshot snapshot{{}, templateStore, templateRevision, sector,
      sectorTiles, sectorTiles.padded(WorldSectorSize - 1), {}, {}, {}};
  // Planning does not inspect tiles yet, but the captured halo and revisions
  // establish the revalidation footprint required before moving it to a
  // worker. The worker will receive copied tiles only when a private dungeon
  // writer can consume them without touching the live world.
  for (auto const& haloSector : tileArray->validSectorsFor(snapshot.tileHalo))
    snapshot.sectorRevisions[haloSector] = tileArray->sectorRevision(haloSector);
  for (auto const& entry : m_queuedPlacements.entries())
    snapshot.queuedPlacements.append({entry.second.distributions, entry.first});

  phase.next("generation.microDungeonPlan.describe");
  auto plans = planMicroDungeons(templateSource, snapshot,
      MicroDungeonPlanAllocator(worldGenerationMemoryLedger(), WorldGenerationMemoryClass::MicroDungeonPlanBuffer));

  if (profile && planPreview) {
    profile->count("generation.microDungeonPlan.candidates", plans.size());
    for (auto const& plan : plans) {
      if (plan.queuedPlacementId)
        profile->count("generation.microDungeonPlan.queuedCandidates");
    }
  }

  // The factory below is the live-world boundary. Keep it explicit in the
  // profile: selection above is value-only, while this phase can write tiles,
  // create entities and fulfil placement promises.
  phase.next("generation.microDungeonPlan.apply");
  for (auto const& plan : plans) {
    applyMicroDungeonPlan(worldStorage, plan, facade, profile, planPreview);
  }

  return {true, {}};
}

WorldGenerator::MicroDungeonPlans WorldGenerator::planMicroDungeons(WorldTemplatePtr const& templateSource,
    MicroDungeonPlanSnapshot const& snapshot, MicroDungeonPlanAllocator const& allocator) {
  STAR_TRACE_ZONE("Microdungeon plan candidates");
  starAssert(templateSource);
  RectI bounds = snapshot.sectorTiles.padded(WorldSectorSize - 1);
  using Candidate = pair<BiomeItemPlacement, Maybe<WorldGenerationPlacementId>>;
  std::vector<Candidate, WorldGenerationBufferAllocator<Candidate>> candidates(
      WorldGenerationBufferAllocator<Candidate>(allocator, WorldGenerationMemoryClass::MicroDungeonCandidateBuffer));
  for (int x = snapshot.sectorTiles.xMin(); x < snapshot.sectorTiles.xMax(); ++x) {
    for (int y = snapshot.sectorTiles.yMin(); y < snapshot.sectorTiles.yMax(); ++y) {
      auto potential = templateSource->potentialBiomeItemsAt(x, y);
      for (auto const& placement : templateSource->validBiomeItems(x, y, potential))
        candidates.push_back({placement, {}});
      for (auto const& queued : snapshot.queuedPlacements) {
        WorldTemplate::PotentialBiomeItems queuedItems;
        templateSource->addPotentialBiomeItems(x, y, queuedItems, queued.distributions, BiomePlacementArea::Surface);
        templateSource->addPotentialBiomeItems(x, y, queuedItems, queued.distributions, BiomePlacementArea::Underground);
        for (auto placement : templateSource->validBiomeItems(x, y, queuedItems))
          candidates.push_back({std::move(placement), queued.queuedPlacementId});
      }
    }
  }
  sort(candidates);
  MicroDungeonPlans plans(allocator);
  auto planCount = std::count_if(candidates.begin(), candidates.end(), [](Candidate const& candidate) {
    return candidate.first.item.is<MicroDungeonNames>();
  });
  plans.reserve(planCount);
  for (auto const& candidate : candidates) {
    auto const& placement = candidate.first;
    if (!placement.item.is<MicroDungeonNames>())
      continue;
    auto seed = templateSource->seedFor(placement.position[0], placement.position[1]);
    auto const& dungeonName = staticRandomFrom(placement.item.get<MicroDungeonNames>(), seed);
    starAssert(!dungeonName.empty());
    plans.push_back(MicroDungeonPlan{bounds, placement.position, seed, dungeonName, candidate.second});
  }
  return plans;
}

void WorldGenerator::applyMicroDungeonPlan(WorldStorage* worldStorage, MicroDungeonPlan const& plan,
    DungeonGeneratorWorldPtr const& facade, ExplorationProfile* profile, bool planPreview) {
  // A plan may outlive other plans' successful placements. Never resolve an
  // asynchronous descriptor through an index into a compacted container.
  if (plan.queuedPlacementId && !m_queuedPlacements.contains(*plan.queuedPlacementId))
    return;

  // This is the only live-world generation call in the microdungeon pipeline.
  // Planning above is value-only; all tiles, entities and promises remain in
  // the legacy owner-thread path below.
  ExplorationProfile::Scope factoryTiming(profile, "generation.microDungeonPlan.apply.factory");
  double factoryStarted = profile ? Time::monotonicTime() : 0.0;
  MicroDungeonFactory::GenerationTiming factoryDetails;
  auto generateResult = m_microDungeonFactory->generate(plan.bounds, plan.dungeonName, plan.position,
      plan.seed, m_worldServer->threatLevel(), facade, false,
      profile && profile->regionalDetailsEnabled() ? &factoryDetails : nullptr);
  factoryTiming.stop();
  double factorySeconds = profile ? Time::monotonicTime() - factoryStarted : 0.0;
  if (profile) {
    if (profile->regionalDetailsEnabled()) {
      profile->count(factoryDetails.definitionCacheHit
          ? "generation.microDungeonDefinitionCache.hits"
          : "generation.microDungeonDefinitionCache.misses");
      if (factoryDetails.definitionCacheEvicted)
        profile->count("generation.microDungeonDefinitionCache.evictions");
      profile->maximum("generation.microDungeonDefinitionCache.entries", factoryDetails.definitionCacheEntries);
      profile->maximum("generation.microDungeonDefinitionCache.capacity", factoryDetails.definitionCacheCapacity);
    }
    Vec2I sector(-1, -1);
    if (auto planSector = worldStorage->sectorForPosition(plan.position))
      sector = Vec2I((int)(*planSector)[0], (int)(*planSector)[1]);
    profile->recordSlowMicroDungeonFactory(xxHash3(plan.dungeonName), sector,
        generateResult ? generateResult->second.size() : 0, bool(generateResult), factorySeconds,
        factoryDetails.definitionSeconds, factoryDetails.definitionCacheLookupSeconds,
        factoryDetails.definitionPathResolveSeconds, factoryDetails.definitionReadAndBuildSeconds,
        factoryDetails.definitionAssetJsonSeconds, factoryDetails.definitionMetadataSeconds,
        factoryDetails.definitionTilesetSeconds, factoryDetails.definitionPartsSeconds,
        factoryDetails.definitionImagePartsSeconds, factoryDetails.definitionTmxPartsSeconds,
        factoryDetails.definitionImagePartCount, factoryDetails.definitionTmxPartCount,
        factoryDetails.definitionTmxAssetJsonSeconds, factoryDetails.definitionTmxMapSeconds,
        factoryDetails.definitionTmxTilesetsSeconds, factoryDetails.definitionTmxTileLayersSeconds,
        factoryDetails.definitionTmxObjectGroupsSeconds, factoryDetails.definitionTmxMapCount,
        factoryDetails.definitionTmxTileLayerCount, factoryDetails.definitionTmxObjectGroupCount,
        factoryDetails.definitionCacheMiss, factoryDetails.anchorSeconds, factoryDetails.collisionSeconds,
        factoryDetails.canPlaceSeconds, factoryDetails.buildSeconds, factoryDetails.placementAttempts,
        factoryDetails.boundsRejected, factoryDetails.collisionRejected, factoryDetails.placementRejected,
        factoryDetails.anchorFound);
  }
  if (!generateResult)
    return;

  if (profile && planPreview) {
    profile->count("generation.microDungeonPlan.accepted");
    profile->count("generation.microDungeonPlan.footprintTiles", generateResult->second.size());
    HashSet<WorldStorage::Sector> footprintSectors;
    for (auto const& position : generateResult->second) {
      if (auto affectedSector = worldStorage->sectorForPosition(position))
        footprintSectors.add(*affectedSector);
    }
    profile->record("generation.microDungeonPlan.footprintSectors", footprintSectors.size());
    if (footprintSectors.size() > 1)
      profile->count("generation.microDungeonPlan.crossSectorAccepted");
  }

  Maybe<DungeonId> dungeonId;
  ExplorationProfile::Scope publishTiming(profile, "generation.microDungeonPlan.apply.publish");
  if (plan.queuedPlacementId) {
    // Reacquire only after the factory returns: its facade can enqueue more
    // placements. No pointer/reference into the registry crosses that call.
    auto queued = m_queuedPlacements.take(*plan.queuedPlacementId);
    starAssert(queued);
    dungeonId = queued->dungeonId;
    queued->promise.fulfill(plan.position);
  }
  for (auto position : generateResult->second) {
    if (ServerTile* tile = m_worldServer->modifyServerTile(position)) {
      replaceBiomeBlocks(tile);
      tile->dungeonId = dungeonId.value(tile->dungeonId);
    }
  }
  publishTiming.stop();
}

void WorldGenerator::generateCaveLiquid(WorldStorage* worldStorage, ServerTileSectorArray::Sector const& sector) {
  Set<Vec2I> openNodes = caveLiquidSeeds(worldStorage, sector);

  if (!openNodes.size())
    return;

  auto tileArray = worldStorage->tileArray();

  Vec2I dimensions(tileArray->size());

  auto wrapCoords = [=](Vec2I const& coord) -> Vec2I { return Vec2I{pmod<int>(coord[0], dimensions[0]), coord[1]}; };

  RectI sectorTiles = tileArray->sectorRegion(sector);
  RectI bounds = sectorTiles.padded(Vec2I(WorldSectorSize - 1, WorldSectorSize - 1));

  bounds.min()[1] = clamp<int>(bounds.min()[1], 0, dimensions[1] - 1);
  bounds.max()[1] = clamp<int>(bounds.max()[1], 0, dimensions[1] - 1);

  auto materialDatabase = Root::singleton().materialDatabase();

  auto samplePoint = sectorTiles.center();
  auto blockInfo = m_worldServer->worldTemplate()->blockInfo(samplePoint[0], samplePoint[1]);
  LiquidId fillLiquid = blockInfo.caveLiquid;
  bool fillMicrodungeons = blockInfo.fillMicrodungeons;
  bool encloseLiquids = blockInfo.encloseLiquids;

  Set<Vec2I> badNodes;

  for (int i = bounds.xMin(); i <= bounds.xMax(); i++) {
    badNodes.add({i, bounds.yMin()});
    badNodes.add({i, bounds.yMax()});
  }
  for (int i = bounds.yMin(); i <= bounds.yMax(); i++) {
    badNodes.add({bounds.xMin(), i});
    badNodes.add({bounds.xMax(), i});
  }

  Set<Vec2I> candidateNodes;

  auto propose = [&](Vec2I const& position) {
    if (!bounds.contains(position))
      return;
    if (candidateNodes.contains(position))
      return;
    if (badNodes.contains(position))
      return;
    auto tile = tileArray->tile(wrapCoords(position));
    starAssert(tile.foreground != NullMaterialId);
    if (tile.foreground != EmptyMaterialId) {
      // Not sure why this doesn't poison solid materials, but it does (occasionally) encounter that case
      if (!BlockCollisionSet.contains(materialDatabase->materialCollisionKind(tile.foreground)))
        badNodes.add(position);
      return;
    }
    if ((tile.dungeonId != NoDungeonId && (!fillMicrodungeons || tile.dungeonId != BiomeMicroDungeonId))
        || (!encloseLiquids && tile.background == EmptyMaterialId)
        || (tile.liquid.liquid != fillLiquid && tile.liquid.liquid != EmptyLiquidId)) {
      badNodes.add(position);
      return;
    }
    candidateNodes.add(position);
    openNodes.add(position);
  };

  while (openNodes.size()) {
    auto node = *openNodes.begin();
    openNodes.remove(node);
    propose(node + Vec2I(-1, 0));
    propose(node + Vec2I(1, 0));
    propose(node + Vec2I(0, -1));
  }

  Set<Vec2I> visitedNodes;

  auto poison = [&](Vec2I position) {
    if (!bounds.contains(position))
      return;
    if (visitedNodes.contains(position))
      return;
    visitedNodes.add(position);
    auto tile = tileArray->tile(wrapCoords(position));
    starAssert(tile.foreground != NullMaterialId);
    if (tile.foreground != EmptyMaterialId)
      return;
    badNodes.add(position);
  };

  while (badNodes.size()) {
    auto node = *badNodes.begin();
    badNodes.remove(node);
    candidateNodes.remove(node);
    poison(node + Vec2I(-1, 0));
    poison(node + Vec2I(1, 0));
    poison(node + Vec2I(0, 1)); // upwards, not downwards
  }

  Set<Vec2I> solidSurroundings(candidateNodes);

  auto solids = [&](Vec2I position) {
    auto tile = tileArray->tile(wrapCoords(position));
    starAssert(tile.foreground != NullMaterialId);
    if (tile.foreground != EmptyMaterialId)
      solidSurroundings.add(position);
  };

  for (auto position : candidateNodes) {
    solids(position + Vec2I(1, 0));
    solids(position + Vec2I(-1, 0));
    solids(position + Vec2I(0, 1));
    solids(position + Vec2I(0, -1));
  }

  MaterialId biomeBlock = m_worldServer->worldTemplate()->biome(tileArray->tile(samplePoint).blockBiomeIndex)->mainBlock;
  Map<Vec2I, float> drops = determineLiquidLevel(candidateNodes, solidSurroundings);
  for (auto iter = drops.begin(); iter != drops.end(); ++iter) {
    auto tile = tileArray->modifyTile(wrapCoords(iter->first));
    starAssert(tile);
    if (!tile)
      continue;
    if (iter->second)
      tile->liquid = LiquidStore::filled(fillLiquid, 1.0f, iter->second);
    if (encloseLiquids && tile->background == EmptyMaterialId)
      tile->background = biomeBlock;
  }
}

void WorldGenerator::prepareSector(WorldStorage* worldStorage, ServerTileSectorArray::Sector const& sector) {
  auto materialDatabase = Root::singleton().materialDatabase();
  auto planet = m_worldServer->worldTemplate();
  auto tileArray = worldStorage->tileArray();
  RectI sectorTiles = tileArray->sectorRegion(sector);

  for (int x = sectorTiles.xMin(); x < sectorTiles.xMax(); ++x) {
    for (int y = sectorTiles.yMin(); y < sectorTiles.yMax(); ++y) {
      Vec2I position(x, y);
      ServerTile* tile = tileArray->modifyTile(position);
      starAssert(tile);
      if (!tile)
        continue;
      starAssert(tile->foreground != NullMaterialId);

      if (tile->liquid.source) {
        auto blockInfo = planet->blockInfo(position[0], position[1]);
        // make sure that ocean liquid never exists on tiles without empty
        // background (except in real dungeons)
        if (!isRealDungeon(tile->dungeonId) && tile->background != EmptyMaterialId)
          tile->liquid.source = false;
        // pressurize liquid under the ocean
        if (blockInfo.oceanLiquid != EmptyLiquidId && position[1] < blockInfo.oceanLiquidLevel) {
          float pressure = blockInfo.oceanLiquidLevel - position[1];
          tile->liquid.pressure = pressure;
        }
      }

      if (!isRealMaterial(tile->foreground))
        tile->foregroundColorVariant = DefaultMaterialColorVariant;
      if (!isRealMaterial(tile->background))
        tile->backgroundColorVariant = DefaultMaterialColorVariant;

      replaceBiomeBlocks(tile);
      placeBiomeGrass(worldStorage, tile, position);

      tile->updateCollision(biomeForegroundCollision(materialDatabase, tile->foreground, tile->foreground, tile->collision));
    }
  }

  List<BiomeItemPlacement> placementQueue;
  for (int x = sectorTiles.xMin(); x < sectorTiles.xMax(); ++x) {
    for (int y = sectorTiles.yMin(); y < sectorTiles.yMax(); ++y) {
      auto tile = tileArray->tile(Vec2I(x, y));
      if (tile.dungeonId == NoDungeonId) {
        auto potential = m_worldServer->worldTemplate()->potentialBiomeItemsAt(x, y);
        for (auto const& placement : m_worldServer->worldTemplate()->validBiomeItems(x, y, potential))
          placementQueue.append(placement);
      }
    }
  }

  sort(placementQueue);
  for (auto const& placement : placementQueue) {
    auto seed = m_worldServer->worldTemplate()->seedFor(placement.position[0], placement.position[1]);
    if (placement.item.is<GrassVariant>()) {
      auto& grass = placement.item.get<GrassVariant>();
      placePlant(worldStorage, Root::singleton().plantDatabase()->createPlant(grass, seed), placement.position);
    } else if (placement.item.is<BushVariant>()) {
      auto& bush = placement.item.get<BushVariant>();
      placePlant(worldStorage, Root::singleton().plantDatabase()->createPlant(bush, seed), placement.position);
    } else if (placement.item.is<TreePair>()) {
      auto& treePair = placement.item.get<TreePair>();
      TreeVariant treeVariant;
      if (seed % 2 == 0)
        treeVariant = treePair.first;
      else
        treeVariant = treePair.second;

      placePlant(worldStorage, Root::singleton().plantDatabase()->createPlant(treeVariant, seed), placement.position);
    } else if (placement.item.is<ObjectPool>()) {
      auto& objectPool = placement.item.get<ObjectPool>();
      auto direction = seed % 2 ? Direction::Left : Direction::Right;
      auto objectPair = objectPool.select(seed);
      if (auto object = Root::singleton().objectDatabase()->createForPlacement(
              m_worldServer, objectPair.first, placement.position, direction, objectPair.second))
        m_worldServer->addEntity(object);
    } else if (placement.item.is<TreasureBoxSet>()) {
      auto& treasureBoxSet = placement.item.get<TreasureBoxSet>();
      auto direction = seed % 2 ? Direction::Left : Direction::Right;
      if (auto treasureContainer = Root::singleton().treasureDatabase()->createTreasureChest(
              m_worldServer, treasureBoxSet, placement.position, direction, seed))
        m_worldServer->addEntity(treasureContainer);
    }
  }

  for (int x = sectorTiles.xMin(); x < sectorTiles.xMax(); ++x) {
    for (int y = sectorTiles.yMin(); y < sectorTiles.yMax(); ++y) {
      if (auto* tile = worldStorage->tileArray()->modifyTile({x, y}))
        tile->collisionCacheDirty = true;
    }
  }
}

void WorldGenerator::prepareSectorBiomeBlocks(WorldStorage* worldStorage, ServerTileSectorArray::Sector const& sector) {
  auto tileArray = worldStorage->tileArray();
  auto materialDatabase = Root::singleton().materialDatabase();
  RectI sectorTiles = tileArray->sectorRegion(sector);

  for (int x = sectorTiles.xMin(); x < sectorTiles.xMax(); ++x) {
    for (int y = sectorTiles.yMin(); y < sectorTiles.yMax(); ++y) {
      Vec2I position(x, y);
      ServerTile* tile = tileArray->modifyTile(position);

      replaceBiomeBlocks(tile);
      placeBiomeGrass(worldStorage, tile, position);

      tile->updateCollision(biomeForegroundCollision(materialDatabase, tile->foreground, tile->foreground, tile->collision));
    }
  }
}

void WorldGenerator::placeBiomeGrass(WorldStorage* worldStorage, ServerTile* tile, Vec2I const& position) {
  if (auto blockBiome = m_worldServer->worldTemplate()->biome(tile->blockBiomeIndex)) {
    // determine layer for grass mod calculation
    TileLayer modLayer = tile->foreground != EmptyMaterialId ? TileLayer::Foreground : TileLayer::Background;

    // don't place mods in dungeons unless explicitly specified, also don't
    // touch non-grass mods
    if (tile->mod(modLayer) == BiomeModId || tile->mod(modLayer) == UndergroundBiomeModId
        || (tile->dungeonId == NoDungeonId && tile->mod(modLayer) == NoModId)) {
      // check whether we're floor or ceiling
      auto tileAbove = worldStorage->tileArray()->tile(position + Vec2I(0, 1));
      auto tileBelow = worldStorage->tileArray()->tile(position + Vec2I(0, -1));
      bool isFloor = (tile->foreground != EmptyMaterialId && tileAbove.foreground == EmptyMaterialId) || (tile->background != EmptyMaterialId && tileAbove.background == EmptyMaterialId);
      bool isCeiling = !isFloor && ((tile->foreground != EmptyMaterialId && tileBelow.foreground == EmptyMaterialId) || (tile->background != EmptyMaterialId && tileBelow.background == EmptyMaterialId));

      // get the appropriate placeables for above/below ground
      BiomePlaceables const* placeables;
      if ((isFloor && tileAbove.background != EmptyMaterialId) || (isCeiling && tileBelow.background != EmptyMaterialId))
        placeables = &blockBiome->undergroundPlaceables;
      else
        placeables = &blockBiome->surfacePlaceables;

      // determine the proper grass mod or lack thereof
      ModId grassModId = NoModId;
      if (isFloor) {
        auto grassChance = staticRandomFloat(m_worldServer->worldTemplate()->worldSeed(), position[0], position[1]);
        if (isRealMod(placeables->grassMod) && grassChance <= placeables->grassModDensity)
          grassModId = placeables->grassMod;
      } else if (isCeiling) {
        auto grassChance = staticRandomFloat(m_worldServer->worldTemplate()->worldSeed(), position[0], position[1]);
        if (isRealMod(placeables->ceilingGrassMod) && grassChance <= placeables->ceilingGrassModDensity)
          grassModId = placeables->ceilingGrassMod;
      }

      // set the selected grass mod
      if (modLayer == TileLayer::Foreground) {
        tile->foregroundMod = grassModId;
        tile->backgroundMod = NoModId;
      } else {
        tile->foregroundMod = NoModId;
        tile->backgroundMod = grassModId;
      }
    }

    // update hue shifts appropriately
    tile->foregroundModHueShift = m_worldServer->worldTemplate()->biomeModHueShift(tile->blockBiomeIndex, tile->foregroundMod);
    tile->backgroundModHueShift = m_worldServer->worldTemplate()->biomeModHueShift(tile->blockBiomeIndex, tile->backgroundMod);
  }
}

void WorldGenerator::reapplyBiome(WorldStorage* worldStorage, ServerTileSectorArray::Sector const& sector) {
  auto materialDatabase = Root::singleton().materialDatabase();
  auto planet = m_worldServer->worldTemplate();
  auto tileArray = worldStorage->tileArray();
  RectI sectorTiles = tileArray->sectorRegion(sector);

  // Logger::info("Reapplying biome in sector {}...", sectorTiles);

  auto entities = m_worldServer->entityQuery(RectF(sectorTiles.padded(1)));
  List<TileEntityPtr> biomeTileEntities;
  for (auto entity : entities) {
    if (auto plant = as<Plant>(entity)) {
      biomeTileEntities.append(as<TileEntity>(entity));
    } else if (auto object = as<Object>(entity)) {
      if (object->biomePlaced())
        biomeTileEntities.append(as<TileEntity>(entity));
    }
  }

  List<Vec2I> biomeItemTiles;

  for (int x = sectorTiles.xMin(); x < sectorTiles.xMax(); ++x) {
    for (int y = sectorTiles.yMin(); y < sectorTiles.yMax(); ++y) {
      Vec2I position(x, y);
      ServerTile* tile = m_worldServer->modifyServerTile(position);
      starAssert(tile);
      if (!tile)
        continue;

      auto blockInfo = planet->blockBiomeInfo(position[0], position[1]);
      if (blockInfo.blockBiomeIndex != tile->blockBiomeIndex) {
        auto newBiome = planet->biome(blockInfo.blockBiomeIndex);
        auto oldBiome = planet->biome(tile->blockBiomeIndex);
        MaterialId oldForeground = tile->foreground;

        biomeTileEntities.filter([&, position](TileEntityPtr tileEntity) {
            if (tileEntity->tilePosition() == position) {
              m_worldServer->removeEntity(tileEntity->entityId(), false);
              return false;
            }
            return true;
          });

        // update biome index
        tile->blockBiomeIndex = blockInfo.blockBiomeIndex;
        tile->environmentBiomeIndex = blockInfo.environmentBiomeIndex;
        tile->biomeTransition = true;

        // replace biome blocks
        if (tile->foreground == oldBiome->mainBlock || oldBiome->subBlocks.contains(tile->foreground)) {
          tile->foreground = blockInfo.foreground;
          tile->foregroundColorVariant = DefaultMaterialColorVariant;
          if (tile->foreground == newBiome->mainBlock)
            tile->foregroundHueShift = newBiome->materialHueShift;
        }

        if (tile->background == oldBiome->mainBlock || oldBiome->subBlocks.contains(tile->background)) {
          tile->background = blockInfo.background;
          tile->backgroundColorVariant = DefaultMaterialColorVariant;
          if (tile->background == newBiome->mainBlock)
            tile->backgroundHueShift = newBiome->materialHueShift;
        }

        if (tile->foreground != EmptyMaterialId || tile->background != EmptyMaterialId) {
          // remove old biome mods
          if (tile->foregroundMod == oldBiome->surfacePlaceables.grassMod || tile->foregroundMod == oldBiome->surfacePlaceables.ceilingGrassMod ||
              tile->foregroundMod == oldBiome->undergroundPlaceables.grassMod || tile->foregroundMod == oldBiome->undergroundPlaceables.ceilingGrassMod) {

            tile->foregroundMod = NoModId;
            tile->foregroundModHueShift = 0;
          }

          if (tile->backgroundMod == oldBiome->surfacePlaceables.grassMod || tile->backgroundMod == oldBiome->surfacePlaceables.ceilingGrassMod ||
              tile->backgroundMod == oldBiome->undergroundPlaceables.grassMod || tile->backgroundMod == oldBiome->undergroundPlaceables.ceilingGrassMod) {

            tile->backgroundMod = NoModId;
            tile->backgroundModHueShift = 0;
          }

          // apply new biome mods
          TileLayer modLayer = tile->foreground != EmptyMaterialId ? TileLayer::Foreground : TileLayer::Background;

          if (tile->mod(modLayer) == NoModId) {
            // check whether we're floor or ceiling
            auto tileAbove = worldStorage->tileArray()->tile(position + Vec2I(0, 1));
            auto tileBelow = worldStorage->tileArray()->tile(position + Vec2I(0, -1));
            bool isFloor = tile->foreground != EmptyMaterialId && tileAbove.foreground == EmptyMaterialId;
            bool isCeiling = !isFloor && tile->foreground != EmptyMaterialId && tileBelow.foreground == EmptyMaterialId;
            bool isModFloor, isModCeiling;
            if (modLayer == TileLayer::Foreground) {
              isModFloor = isFloor;
              isModCeiling = isCeiling;
            } else {
              isModFloor = tile->background != EmptyMaterialId && tileAbove.background == EmptyMaterialId;
              isModCeiling = !isModFloor && tile->background != EmptyMaterialId && tileBelow.background == EmptyMaterialId;
            }

            // get the appropriate placeables for above/below ground
            BiomePlaceables const* placeables;
            if ((isFloor && tileAbove.background != EmptyMaterialId) || (isCeiling && tileBelow.background != EmptyMaterialId))
              placeables = &newBiome->undergroundPlaceables;
            else
              placeables = &newBiome->surfacePlaceables;

            // determine the proper grass mod or lack thereof
            ModId grassModId = NoModId;
            if (isModFloor) {
              auto grassChance = staticRandomFloat(m_worldServer->worldTemplate()->worldSeed(), position[0], position[1]);
              if (isRealMod(placeables->grassMod) && grassChance <= placeables->grassModDensity)
                grassModId = placeables->grassMod;
            } else if (isModCeiling) {
              auto grassChance = staticRandomFloat(m_worldServer->worldTemplate()->worldSeed(), position[0], position[1]);
              if (isRealMod(placeables->ceilingGrassMod) && grassChance <= placeables->ceilingGrassModDensity)
                grassModId = placeables->ceilingGrassMod;
            }

            // set the selected grass mod
            if (modLayer == TileLayer::Foreground && materialDatabase->supportsMod(tile->foreground, grassModId)) {
              tile->foregroundMod = grassModId;
              tile->backgroundMod = NoModId;
            } else if (modLayer == TileLayer::Background && materialDatabase->supportsMod(tile->background, grassModId)) {
              tile->foregroundMod = NoModId;
              tile->backgroundMod = grassModId;
            }
          }
        } else {
          tile->foregroundMod = NoModId;
          tile->backgroundMod = NoModId;
        }

        tile->updateCollision(biomeForegroundCollision(materialDatabase, oldForeground, tile->foreground, tile->collision));
      }

      if (tile->biomeTransition && !blockInfo.biomeTransition) {
        tile->biomeTransition = false;
        if (!isSolidColliding(tile->collision))
          biomeItemTiles.append(position);
      }
    }
  }

  auto simplePlacePlant = [&](PlantPtr const& plant, Vec2I const& position) {
      if (!plant)
        return false;

      auto spaces = plant->spaces();
      auto roots = plant->roots();
      auto const& primaryRoot = plant->primaryRoot();

      auto blockBiome = planet->worldLayout()->getBiome(worldStorage->tileArray()->tile(position).blockBiomeIndex);

      auto positionValid = [&](Vec2I const& pos) {
          auto primaryTile = worldStorage->tileArray()->tile(pos);
          auto primaryRootTile = worldStorage->tileArray()->tile(pos + primaryRoot);
          if (isConnectableMaterial(primaryTile.foreground) || !isConnectableMaterial(primaryRootTile.foreground))
            return false;

          for (auto root : roots) {
            auto rootTile = worldStorage->tileArray()->tile(root + pos);
            if (!isConnectableMaterial(rootTile.foreground) || rootTile.blockBiomeIndex != primaryTile.blockBiomeIndex ||
                (rootTile.foreground != blockBiome->mainBlock && !blockBiome->subBlocks.contains(rootTile.foreground)))
              return false;
          }

          for (auto space : spaces) {
            Vec2I pspace = space + pos;

            if (!m_worldServer->atTile<TileEntity>(pspace).empty())
              return false;

            auto tile = worldStorage->tileArray()->tile(pspace);
            if (tile.foreground != EmptyMaterialId)
              return false;
          }

          return true;
        };

      List<Vec2I> tryPositions = {
          position,
          position + Vec2I{-1, 0},
          position + Vec2I{1, 0},
          position + Vec2I{-2, 0},
          position + Vec2I{2, 0},
          position + Vec2I{-1, 1},
          position + Vec2I{-1, -1},
          position + Vec2I{1, 1},
          position + Vec2I{1, -1}
        };

      for (auto pos : tryPositions) {
        if (positionValid(pos)) {
          plant->setTilePosition(pos);
          m_worldServer->addEntity(plant);
          return true;
        }
      }

      return false;
    };

  auto placeBiomeItem = [&](BiomeItemPlacement biomeItemPlacement, Vec2I position) {
      auto seed = m_worldServer->worldTemplate()->seedFor(position[0], position[1]);
      if (biomeItemPlacement.item.is<GrassVariant>()) {
        auto& grass = biomeItemPlacement.item.get<GrassVariant>();
        simplePlacePlant(Root::singleton().plantDatabase()->createPlant(grass, seed), position);
      } else if (biomeItemPlacement.item.is<BushVariant>()) {
        auto& bush = biomeItemPlacement.item.get<BushVariant>();
        simplePlacePlant(Root::singleton().plantDatabase()->createPlant(bush, seed), position);
      } else if (biomeItemPlacement.item.is<TreePair>()) {
        auto& treePair = biomeItemPlacement.item.get<TreePair>();
        TreeVariant treeVariant;
        if (seed % 2 == 0)
          treeVariant = treePair.first;
        else
          treeVariant = treePair.second;

        simplePlacePlant(Root::singleton().plantDatabase()->createPlant(treeVariant, seed), position);
      } else if (biomeItemPlacement.item.is<ObjectPool>()) {
        auto& objectPool = biomeItemPlacement.item.get<ObjectPool>();
        auto direction = seed % 2 ? Direction::Left : Direction::Right;
        auto objectPair = objectPool.select(seed);
        if (auto object = Root::singleton().objectDatabase()->createForPlacement(m_worldServer, objectPair.first, position, direction, objectPair.second)) {
          if (object->biomePlaced())
            m_worldServer->addEntity(object);
        }
      }
    };

  for (auto position : biomeItemTiles) {
    ServerTile* tile = m_worldServer->modifyServerTile(position);

    auto blockBiome = planet->worldLayout()->getBiome(tile->blockBiomeIndex);
    auto tileAbove = m_worldServer->getServerTile(position + Vec2I{0, 1});
    auto tileBelow = m_worldServer->getServerTile(position + Vec2I{0, -1});

    if (tile->background != EmptyMaterialId) {
      for (auto const& itemDistribution : blockBiome->undergroundPlaceables.itemDistributions) {
        if (itemDistribution.mode() == BiomePlacementMode::Background) {
          if (auto itemToPlace = itemDistribution.itemToPlace(position[0], position[1]))
            placeBiomeItem(*itemToPlace, position);
        }
      }

      if (isSolidColliding(tileAbove.collision)) {
        for (auto const& itemDistribution : blockBiome->undergroundPlaceables.itemDistributions) {
          if (itemDistribution.mode() == BiomePlacementMode::Ceiling) {
            if (auto itemToPlace = itemDistribution.itemToPlace(position[0], position[1]))
              placeBiomeItem(*itemToPlace, position);
          }
        }
      }

      if (isSolidColliding(tileBelow.collision)) {
        for (auto const& itemDistribution : blockBiome->undergroundPlaceables.itemDistributions) {
          if (itemDistribution.mode() == BiomePlacementMode::Floor) {
            if (auto itemToPlace = itemDistribution.itemToPlace(position[0], position[1]))
              placeBiomeItem(*itemToPlace, position);
          }
        }
      }
    } else {
      if (isSolidColliding(tileBelow.collision)) {
        for (auto const& itemDistribution : blockBiome->surfacePlaceables.itemDistributions) {
          if (itemDistribution.mode() == BiomePlacementMode::Floor) {
            if (auto itemToPlace = itemDistribution.itemToPlace(position[0], position[1]))
              placeBiomeItem(*itemToPlace, position);
          }
        }
      }
    }
  }
}

Set<Vec2I> WorldGenerator::caveLiquidSeeds(WorldStorage* worldStorage, ServerTileSectorArray::Sector const& sector) {
  RectI sectorTiles = worldStorage->tileArray()->sectorRegion(sector);
  auto samplePoint = sectorTiles.center();
  auto blockInfo = m_worldServer->worldTemplate()->blockInfo(samplePoint[0], samplePoint[1]);
  float seedDensity = blockInfo.caveLiquidSeedDensity;
  Set<Vec2I> nodes;
  if (seedDensity > 0) {
    int frequency = int(100 / seedDensity);
    for (int y = frequency * floor(sectorTiles.min()[1] / frequency); y < sectorTiles.max()[1]; y += frequency) {
      for (int x = frequency * floor(sectorTiles.min()[0] / frequency); x < sectorTiles.max()[0]; x += frequency) {
        if (sectorTiles.contains(Vec2I(x, y)))
          nodes.add(Vec2I(x, y));
      }
    }
  }
  return nodes;
}

Map<Vec2I, float> WorldGenerator::determineLiquidLevel(Set<Vec2I> const& spots, Set<Vec2I> const& filled) {
  Set<Vec2I> openSet(spots);
  Map<Vec2I, float> results;

  auto geometry = m_worldServer->geometry();

  while (openSet.size() > 0) {
    Set<Vec2I> cluster;
    Set<Vec2I> openCluster;
    openCluster.add(*(openSet.begin()));
    while (openCluster.size() > 0) {
      Vec2I node = *(openCluster.begin());
      openCluster.remove(node);
      if (openSet.contains(node)) {
        openSet.remove(node);
        cluster.add(node);
        openCluster.add(geometry.xwrap(Vec2I(node.x(), node.y() + 1)));
        openCluster.add(geometry.xwrap(Vec2I(node.x(), node.y() - 1)));
        openCluster.add(geometry.xwrap(Vec2I(node.x() + 1, node.y())));
        openCluster.add(geometry.xwrap(Vec2I(node.x() - 1, node.y())));
      }
    }
    levelCluster(cluster, filled, results);
  }
  return results;
}

void WorldGenerator::levelCluster(Set<Vec2I>& cluster, Set<Vec2I> const& filled, Map<Vec2I, float>& results) {
  int maxY = std::numeric_limits<int>::min();
  int minY = std::numeric_limits<int>::max();
  for (auto iter = cluster.begin(); iter != cluster.end(); iter++) {
    auto droplet = (*iter);
    if (filled.contains(droplet + Vec2I(1, 0)) && filled.contains(droplet + Vec2I(-1, 0))
        && filled.contains(droplet + Vec2I(0, -1))) {
      if (droplet.y() > maxY)
        maxY = droplet.y();
      if (!filled.contains(droplet + Vec2I(0, 1))) {
        if (droplet.y() <= minY)
          minY = droplet.y();
      }
    } else {
      if (droplet.y() <= minY)
        minY = droplet.y() - 1;
    }
  }
  int liquidLevel = std::min(maxY, minY);
  for (auto iter = cluster.begin(); iter != cluster.end(); iter++) {
    int pressure = (liquidLevel - (*iter).y());
    if (pressure >= 0)
      results[*iter] = 1.0f + pressure;
  }
}

bool WorldGenerator::placePlant(WorldStorage* worldStorage, PlantPtr const& plant, Vec2I const& position) {
  if (!plant)
    return false;

  auto spaces = plant->spaces();
  auto roots = plant->roots();
  auto const& primaryRoot = plant->primaryRoot();

  auto background = m_worldServer->getServerTile(position).background;
  bool adjustBackground = background == EmptyMaterialId || background == NullMaterialId;

  auto withinAdjustment = [=](Vec2I const& pos) {
    return PlantAdjustmentLimit - std::abs(pos[0]) > 0 && PlantAdjustmentLimit - std::abs(pos[1]) > 0;
  };

  // Bail out if we don't have at least one free space, and root in the primary
  // root position, or if we're in a dungeon region.
  auto primaryTile = worldStorage->tileArray()->tile(position);
  auto rootTile = worldStorage->tileArray()->tile(position + primaryRoot);
  if (primaryTile.dungeonId != NoDungeonId || rootTile.dungeonId != NoDungeonId)
    return false;
  if (isConnectableMaterial(primaryTile.foreground) || !isConnectableMaterial(rootTile.foreground))
    return false;

  // First bail out if we can't fit anything we're not adjusting
  for (auto space : spaces) {
    Vec2I pspace = space + position;

    if (withinAdjustment(space) && !m_worldServer->atTile<Plant>(pspace).empty())
      return false;

    // Bail out if we hit a different plant's root tile, or if we're not in the
    // adjustment space and we hit a non-empty tile.
    auto tile = worldStorage->tileArray()->tile(pspace);
    if (tile.rootSource || (!withinAdjustment(space) && tile.foreground != EmptyMaterialId))
      return false;
  }

  // Check all the roots outside of the adjustment limit
  for (auto root : roots) {
    root += position;
    if (!withinAdjustment(root) && !isConnectableMaterial(worldStorage->tileArray()->tile(root).foreground))
      return false;
  }

  // Clear all the necessary blocks within the adjustment limit
  for (auto space : spaces) {
    if (!withinAdjustment(space))
      continue;

    space += position;
    if (auto tile = worldStorage->tileArray()->modifyTile(space)) {
      if (isConnectableMaterial(tile->foreground))
        *tile = primaryTile;
      if (adjustBackground)
        tile->background = EmptyMaterialId;
      tile->collision = CollisionKind::None;
      tile->collisionCacheDirty = true;
    } else {
      return false;
    }
  }

  // Make all the root blocks a real material based on the primary root.
  for (auto root : roots) {
    root += position;
    if (auto tile = worldStorage->tileArray()->modifyTile(root)) {
      if (!isRealMaterial(tile->foreground)) {
        *tile = rootTile;
        tile->collision = Root::singleton().materialDatabase()->materialCollisionKind(tile->foreground);
        tile->collisionCacheDirty = true;
      }
    } else {
      return false;
    }
  }

  plant->setTilePosition(position);
  m_worldServer->addEntity(plant);
  return true;
}

}
