#include "StarWorldGenerationComputeRuntime.hpp"
#include "StarWorldGenerationComputeGl.hpp"

namespace Star {

WorldGenerationComputeRuntime::~WorldGenerationComputeRuntime() {
  // Destructor may run off the world thread after join; only tear down if still active.
  if (m_active)
    stopOnWorldThread();
}

WorldGenerationComputeRuntime::Options WorldGenerationComputeRuntime::optionsFromConfig(
    Json const& config) noexcept {
  Options options;
  try {
    Json block = config.get("worldGenerationCompute", JsonObject());
    if (!block.isType(Json::Type::Object))
      return options;
    options.enabled = block.getBool("worldThreadPump", false);
    options.gpuFenceRehearsal = block.getBool("gpuFenceRehearsal", true);
    options.deferredCompletion = block.getBool("deferredCompletion", true);
  } catch (...) {
    // Config parse failures leave the runtime inactive (safe default).
    options = {};
  }
  return options;
}

void WorldGenerationComputeRuntime::startOnWorldThread(Options options) {
  stopOnWorldThread();
  if (!options.enabled)
    return;

  auto caps = WorldGenerationComputeService::probeCapabilities();
  m_service = std::make_unique<WorldGenerationComputeService>(caps);
  m_service->setDeferredCpuCompletion(options.deferredCompletion);
  m_service->setGpuFenceRehearsal(options.gpuFenceRehearsal);
  WorldGenerationComputeGlDevice::adoptOwnerThread();
  m_service->adoptOwnerThread();

  m_rehearsal = options.gpuFenceRehearsal;
  m_lastPumpCompletions = 0;
  m_active = true;
}

size_t WorldGenerationComputeRuntime::pumpUpdate(size_t maxCompletions) {
  if (!m_active || !m_service)
    return 0;
  m_lastPumpCompletions = m_service->pump(maxCompletions);
  return m_lastPumpCompletions;
}

void WorldGenerationComputeRuntime::stopOnWorldThread() {
  if (!m_active && !m_service)
    return;
  if (m_service) {
    m_service->cancelAll();
    m_service->clearOwnerThread();
  }
  WorldGenerationComputeGlDevice::clearOwnerThread();
  WorldGenerationComputeGlDevice::shutdown();
  m_service.reset();
  m_active = false;
  m_rehearsal = false;
  m_lastPumpCompletions = 0;
}

}
