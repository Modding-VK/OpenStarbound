#pragma once

#include "StarWorldGenerationCompute.hpp"
#include "StarWorldGenerationComputeAdmission.hpp"
#include "StarWorldGenerationComputePublish.hpp"
#include "StarJson.hpp"

#include <memory>

namespace Star {

STAR_CLASS(WorldGenerationComputeRuntime);

// GPU-02 WorldThread + publish-gate + preparation-admission (cloud-safe):
// Adopt compute + GlDevice owners on the world thread and pump fences each update
// without blocking. Preparation Terrain/Biome work may be admitted and staged for
// review; live world apply always refuses. Default inactive.
//
// Does not claim H4 50%. Real GL on this thread still needs Desktop validation
// (hidden SDL context is separate from the renderer; dual-context prep is stubbed).
// Multi-driver evidence remains a separate gate. Backend selection is heuristic.
class WorldGenerationComputeRuntime {
public:
  struct Options {
    bool enabled = false;
    // When true (default), admit deferred GpuOpenGlCompute tickets without a
    // live gpuOpenGlCompute claim and materialize on CPU after pumpFences.
    bool gpuFenceRehearsal = true;
    bool deferredCompletion = true;
    // When true, pumpUpdate also stages Ready admitted preparation tickets.
    bool autoStageReady = false;
  };

  WorldGenerationComputeRuntime() = default;
  ~WorldGenerationComputeRuntime();

  WorldGenerationComputeRuntime(WorldGenerationComputeRuntime const&) = delete;
  WorldGenerationComputeRuntime& operator=(WorldGenerationComputeRuntime const&) = delete;

  // Reads optional object "worldGenerationCompute":
  //   worldThreadPump (bool, default false)
  //   gpuFenceRehearsal (bool, default true)
  //   deferredCompletion (bool, default true)
  //   autoStageReady (bool, default false)
  static Options optionsFromConfig(Json const& config) noexcept;

  // Must run on the WorldServerThread that will own compute/GL for this world.
  void startOnWorldThread(Options options);
  // Non-blocking fence + deferred completion advance. Optionally auto-stages.
  size_t pumpUpdate(size_t maxCompletions = 0);
  void stopOnWorldThread();

  bool active() const noexcept { return m_active; }
  bool rehearsal() const noexcept { return m_rehearsal; }
  bool autoStageReady() const noexcept { return m_autoStageReady; }
  size_t lastPumpCompletions() const noexcept { return m_lastPumpCompletions; }
  size_t lastAutoStaged() const noexcept { return m_lastAutoStaged; }
  WorldGenerationComputeService* service() noexcept { return m_service.get(); }
  WorldGenerationComputeService const* service() const noexcept { return m_service.get(); }
  WorldGenerationComputePublishGate* publishGate() noexcept {
    return m_active ? &m_publishGate : nullptr;
  }
  WorldGenerationComputePublishGate const* publishGate() const noexcept {
    return m_active ? &m_publishGate : nullptr;
  }
  WorldGenerationComputeAdmission* admission() noexcept {
    return m_active ? &m_admission : nullptr;
  }
  WorldGenerationComputeAdmission const* admission() const noexcept {
    return m_active ? &m_admission : nullptr;
  }

  // Admit Terrain/Biome preparation work (DenseUint32 refused). Empty when inactive.
  WorldGenerationComputeAdmissionRecord admitPreparation(
      WorldGenerationComputeAdmissionRequest request);
  // Stage Ready admitted tickets into the publish gate (no live apply).
  size_t stageReadyOpen(size_t maxRecords = 0);

  // Observation helpers for dual-context prep (no live sharing claim).
  static char const* dualContextPrepStatus() noexcept;
  static bool rendererShareSupported() noexcept;

private:
  bool m_active = false;
  bool m_rehearsal = false;
  bool m_autoStageReady = false;
  size_t m_lastPumpCompletions = 0;
  size_t m_lastAutoStaged = 0;
  std::unique_ptr<WorldGenerationComputeService> m_service;
  WorldGenerationComputePublishGate m_publishGate;
  WorldGenerationComputeAdmission m_admission;
};

}
