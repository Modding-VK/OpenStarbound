#pragma once

#include "StarEntity.hpp"
#include "StarJson.hpp"

namespace Star {

STAR_CLASS(ExplorationProfile);

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
    ExplorationProfile* m_profile;
    Entity const* m_entity;
    EntityScope* m_previousScope;
    double* m_previousLogAccumulator;
    double m_start;
    double m_luaSeconds = 0;
    double m_loggingSeconds = 0;

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

  void setContext(String context);
  void record(char const* stage, double seconds);
  void count(char const* counter, uint64_t amount = 1);
  // Call at an outer operation boundary, never inside a timed scope. Final
  // reports preserve short windows when a world closes before the interval.
  void report(bool final = false);

private:
  struct Timing {
    uint64_t samples = 0;
    uint64_t slowSamples = 0;
    double totalSeconds = 0;
    double maximumSeconds = 0;
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

  void recordEntity(Entity const& entity, double totalSeconds, double luaSeconds, double loggingSeconds);
  static thread_local EntityScope* s_currentEntityScope;

  String m_component;
  String m_context;
  uint64_t m_instance = 0;
  double m_interval = 5;
  double m_slowSeconds = 0.05;
  double m_windowStart = 0;
  unsigned m_entityDetailLimit = 12;
  StringMap<Timing> m_timings;
  StringMap<uint64_t> m_counts;
  StringMap<EntityTiming> m_entityTimings;
  List<SlowEntity> m_slowEntities;
};

}
