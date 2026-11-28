#pragma once

#include "StarWorldGenerationCompute.hpp"
#include "StarJson.hpp"

#include <memory>

namespace Star {

STAR_CLASS(WorldGenerationComputeRuntime);

// GPU-02 WorldThread vertical (cloud-safe):
// Adopt compute + GlDevice owners on the world thread and pump fences each update
// without blocking. Results never write the live world. Default inactive.
//
// Does not claim H4 50%. Real GL on this thread still needs Desktop validation
// (hidden SDL context is separate from the renderer; dedicated servers stay on
// rehearsal). Multi-driver evidence remains a separate gate.
class WorldGenerationComputeRuntime {
public:
  struct Options {
    bool enabled = false;
    // When true (default), admit deferred GpuOpenGlCompute tickets without a
    // live gpuOpenGlCompute claim and materialize on CPU after pumpFences.
    bool gpuFenceRehearsal = true;
    bool deferredCompletion = true;
  };

  WorldGenerationComputeRuntime() = default;
  ~WorldGenerationComputeRuntime();

  WorldGenerationComputeRuntime(WorldGenerationComputeRuntime const&) = delete;
  WorldGenerationComputeRuntime& operator=(WorldGenerationComputeRuntime const&) = delete;

  // Reads optional object "worldGenerationCompute":
  //   worldThreadPump (bool, default false)
  //   gpuFenceRehearsal (bool, default true)
  //   deferredCompletion (bool, default true)
  static Options optionsFromConfig(Json const& config) noexcept;

  // Must run on the WorldServerThread that will own compute/GL for this world.
  void startOnWorldThread(Options options);
  // Non-blocking fence + deferred completion advance. No-op when inactive.
  size_t pumpUpdate(size_t maxCompletions = 0);
  void stopOnWorldThread();

  bool active() const noexcept { return m_active; }
  bool rehearsal() const noexcept { return m_rehearsal; }
  size_t lastPumpCompletions() const noexcept { return m_lastPumpCompletions; }
  WorldGenerationComputeService* service() noexcept { return m_service.get(); }
  WorldGenerationComputeService const* service() const noexcept { return m_service.get(); }

private:
  bool m_active = false;
  bool m_rehearsal = false;
  size_t m_lastPumpCompletions = 0;
  std::unique_ptr<WorldGenerationComputeService> m_service;
};

}
