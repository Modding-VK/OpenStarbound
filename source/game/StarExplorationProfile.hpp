#pragma once

#include <array>
#include <map>
#include <tuple>

#include "StarEntity.hpp"
#include "StarJson.hpp"

namespace Star {

STAR_CLASS(ExplorationProfile);
STAR_CLASS(World);
struct WorldGenerationTaskIdentity;

// Optional wall-clock diagnostics. Callers must serialize access; the world
// server and its storage share a collector under the existing world lock.
class ExplorationProfile {
public:
  // Returns null when disabled. Configuration is read once, at creation.
  static ExplorationProfilePtr create(String component, String context = {});

  class Scope {
  public:
    Scope(ExplorationProfile* profile, char const* stage);
    ~Scope();
    Scope(Scope const&) = delete;
    Scope& operator=(Scope const&) = delete;
    void stop();
    void next(char const* stage);

  private:
    ExplorationProfile* m_profile;
    char const* m_stage;
    double m_start;
  };

  class ObjectWorldCallScope;

  // Times one entity update and attributes Lua update callbacks and synchronous
  // logging performed on the same thread to it.
  class EntityScope {
  public:
    EntityScope(ExplorationProfile* profile, Entity const& entity);
    ~EntityScope();
    EntityScope(EntityScope const&) = delete;
    EntityScope& operator=(EntityScope const&) = delete;
    void recordLua(double seconds);

  private:
    friend class ObjectWorldCallScope;
    ExplorationProfile* m_profile;
    Entity const* m_entity;
    EntityScope* m_previousScope;
    double* m_previousLogAccumulator;
    double m_start;
    double m_luaSeconds = 0;
    double m_loggingSeconds = 0;

  };

  // Attributes synchronous world API calls only when a profiled object is
  // currently updating on this thread. Fixed labels avoid mod-name cardinality.
  class ObjectWorldCallScope {
  public:
    explicit ObjectWorldCallScope(char const* stage, Maybe<EntityId> target = {}, char const* operation = nullptr);
    void setTarget(EntityId target) { m_target = target; }
    // Optional fixed labels, enabled only by regionalDetails. Phases partition
    // this call; nested calls remain inclusive, like the parent API timing.
    void nextPhase(char const* phase);
    ~ObjectWorldCallScope();
    ObjectWorldCallScope(ObjectWorldCallScope const&) = delete;
    ObjectWorldCallScope& operator=(ObjectWorldCallScope const&) = delete;
  private:
    ExplorationProfile* m_profile = nullptr;
    char const* m_stage;
    char const* m_operation;
    char const* m_phase = nullptr;
    double m_phaseStart = 0;
    Maybe<EntityId> m_target;
    EntityId m_rootId = 0;
    Vec2I m_rootRegion;
    double m_start = 0;
  };

  // Records LuaUpdatableComponent::update only while an EntityScope is active
  // on the same thread.
  class EntityLuaScope {
  public:
    EntityLuaScope();
    ~EntityLuaScope();
    EntityLuaScope(EntityLuaScope const&) = delete;
    EntityLuaScope& operator=(EntityLuaScope const&) = delete;

  private:
    EntityScope* m_entityScope;
    double m_start;
  };

  // Fixed labels keep diagnostic cardinality independent of mod function names.
  enum class NpcStage : size_t {
    Update, ScriptDispatch, Movement, Status, Shared, Init, Uninit, Damage,
    DamagedOther, ShouldDestroy, Destroy, Interact, ReceiveMessage, CallScript,
    LuaUpdate, LuaInvoke, LuaInit, LuaUninit, LuaMessage, LuaThreadMessage, LuaEval,
    PathInit, PathExplore, ScriptedEntityCall, SendEntityMessage, WorldInteract,
    Count
  };

  // Entry scopes select the server collector; nested scopes inherit it on this
  // thread only. No world/entity reference survives the synchronous call.
  class NpcScope {
  public:
    NpcScope(World* world, NpcStage stage);
    explicit NpcScope(NpcStage stage);
    ~NpcScope();
    NpcScope(NpcScope const&) = delete;
    NpcScope& operator=(NpcScope const&) = delete;

  private:
    friend class ExplorationProfile;
    NpcScope(ExplorationProfile* profile, NpcStage stage);
    ExplorationProfile* m_profile;
    NpcScope* m_previous;
    NpcStage m_stage;
    double m_start = 0;
    double m_childSeconds = 0;
    double m_loggingSeconds = 0;
    double* m_previousLogAccumulator = nullptr;
  };

  static void recordNpcPathResult(Maybe<bool> result);
  static ExplorationProfile* activeNpcProfile();
  bool npcDetailsEnabled() const { return m_npcDetails; }
  void recordNpcPopulation(uint64_t updatedEntities);

  void setContext(String context);
  void record(char const* stage, double seconds);
  void count(char const* counter, uint64_t amount = 1);
  // Keep bounded sector coordinates for correlating queue stalls with the route.
  void recordQueuedSector(Vec2I sector, double seconds);
  void recordCompletedSector(Vec2I sector, double seconds);
  void recordTilePacket(Vec2I tileOrigin);
  void recordBaseTileOutcome(Vec2I sector, bool stale, uint64_t revisionDelta,
      JsonObject reasons, double ageSeconds);
  // Bounded lifecycle trace; only regionalDetails enables per-task records.
  void recordGenerationTask(WorldGenerationTaskIdentity const& identity, char const* event, size_t reservedBytes);
  // Call at an outer operation boundary, never inside a timed scope. Final
  // reports preserve short windows when a world closes before the interval.
  void report(bool final = false);

private:
  struct Timing {
    // Fixed histogram: <1 us, then eight linear buckets per power of two.
    static constexpr size_t HistogramSize = 256;
    std::array<uint64_t, HistogramSize> histogram{};
    uint64_t samples = 0;
    uint64_t slowSamples = 0;
    double totalSeconds = 0;
    double maximumSeconds = 0;
    void add(double seconds, double slowSeconds);
    double percentileUpperMilliseconds(unsigned percentile) const;
    JsonObject toJson() const;
  };

  struct NpcTiming : Timing {
    double exclusiveSeconds = 0;
    double loggingSeconds = 0;
  };

  struct EntityTiming : Timing {
    double luaSeconds = 0;
    double loggingSeconds = 0;
  };

  struct SlowEntity {
    String type;
    EntityId id;
    String name;
    String kind;
    Vec2F position;
    uint64_t threadId;
    double totalSeconds;
    double luaSeconds;
    double loggingSeconds;
  };

  struct SectorWait {
    Vec2I sector;
    double seconds;
  };

  struct TilePacketSample {
    Vec2I tileOrigin;
    double monotonicSeconds;
  };

  struct BaseTileOutcome {
    Vec2I sector;
    uint64_t revisionDelta;
    JsonObject reasons;
    double ageSeconds;
  };

  void recordEntity(Entity const& entity, double totalSeconds, double luaSeconds, double loggingSeconds);
  static thread_local EntityScope* s_currentEntityScope;
  static thread_local NpcScope* s_currentNpcScope;
  static ExplorationProfile* npcProfile(World* world);

  String m_component;
  String m_context;
  uint64_t m_instance = 0;
  double m_interval = 5;
  double m_slowSeconds = 0.05;
  double m_windowStart = 0;
  unsigned m_entityDetailLimit = 12;
  bool m_npcDetails = false;
  bool m_regionalDetails = false;
  // Fixed caps per reporting window; never retain entity pointers or payloads.
  struct Aggregate {
    uint64_t calls = 0;
    double seconds = 0;
    double luaSeconds = 0;
    Vec2I rootRegion;
  };
  std::map<std::tuple<EntityId, EntityId, String>, Aggregate> m_objectDependencies;
  std::map<std::tuple<int, int, String>, Aggregate> m_regionalEntities;
  uint64_t m_dependencyOverflow = 0;
  uint64_t m_regionOverflow = 0;
  std::array<NpcTiming, static_cast<size_t>(NpcStage::Count)> m_npcTimings{};
  std::array<uint64_t, 3> m_npcPathResults{};
  uint64_t m_npcPopulationSamples = 0;
  uint64_t m_npcUpdatedEntities = 0;
  uint64_t m_npcMaximumUpdatedEntities = 0;
  StringMap<Timing> m_timings;
  StringMap<uint64_t> m_counts;
  StringMap<EntityTiming> m_entityTimings;
  List<SlowEntity> m_slowEntities;
  Maybe<SectorWait> m_oldestQueuedSector;
  List<SectorWait> m_slowSectors;
  List<TilePacketSample> m_tilePackets;
  List<BaseTileOutcome> m_staleBaseTiles;
  List<BaseTileOutcome> m_evictedBaseTiles;
  JsonArray m_generationTaskEvents;
};

}
