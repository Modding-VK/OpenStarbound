#pragma once

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

  String m_component;
  String m_context;
  uint64_t m_instance = 0;
  double m_interval = 5;
  double m_slowSeconds = 0.05;
  double m_windowStart = 0;
  StringMap<Timing> m_timings;
  StringMap<uint64_t> m_counts;
};

}
