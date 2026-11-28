#include "StarWorldGenerationComputeAdmission.hpp"

namespace Star {

bool WorldGenerationComputeAdmission::isPreparationKernel(
    WorldGenerationComputeKernel kernel) noexcept {
  switch (kernel) {
    case WorldGenerationComputeKernel::TerrainFieldMax2:
    case WorldGenerationComputeKernel::TerrainFieldMin2:
    case WorldGenerationComputeKernel::TerrainFieldClampU16:
    case WorldGenerationComputeKernel::BiomeIdBucket:
      return true;
    default:
      return false;
  }
}

WorldGenerationComputeAdmissionDecision WorldGenerationComputeAdmission::selectBackend(
    WorldGenerationComputeCapabilities const& capabilities,
    WorldGenerationComputeAdmissionRequest const& request, bool gpuFenceRehearsal) noexcept {
  WorldGenerationComputeAdmissionDecision decision;
  size_t n = request.input.size();
  decision.costScore = (uint64_t)n * 4ull;

  if (!capabilities.cpuReference && !capabilities.gpuOpenGlCompute) {
    decision.backend = WorldGenerationComputeBackend::CpuReference;
    decision.reason = "no_backend";
    return decision;
  }

  // Urgent / low-latency preparation stays on CPU when available.
  if (request.preferLowLatency && capabilities.cpuReference) {
    decision.backend = WorldGenerationComputeBackend::CpuReference;
    decision.reason = "cpu_low_latency";
    return decision;
  }

  // Small jobs: CPU avoids transfer overhead (heuristic threshold, not measured).
  constexpr size_t kSmallJobWords = 64;
  if (n <= kSmallJobWords && capabilities.cpuReference) {
    decision.backend = WorldGenerationComputeBackend::CpuReference;
    decision.reason = "cpu_small_job";
    return decision;
  }

  if (capabilities.gpuOpenGlCompute) {
    decision.backend = WorldGenerationComputeBackend::GpuOpenGlCompute;
    decision.reason = "gpu_amortized_heuristic";
    return decision;
  }

  // Cloud/CI: rehearsal may admit GPU tickets that materialize on CPU after pump.
  if (gpuFenceRehearsal && capabilities.cpuReference && n > kSmallJobWords) {
    decision.backend = WorldGenerationComputeBackend::GpuOpenGlCompute;
    decision.reason = "gpu_rehearsal_amortized";
    return decision;
  }

  decision.backend = WorldGenerationComputeBackend::CpuReference;
  decision.reason = capabilities.cpuReference ? "cpu_fallback" : "no_backend";
  return decision;
}

char const* WorldGenerationComputeAdmission::admissionVerdictName(
    WorldGenerationComputeAdmissionVerdict verdict) noexcept {
  switch (verdict) {
    case WorldGenerationComputeAdmissionVerdict::Admitted: return "Admitted";
    case WorldGenerationComputeAdmissionVerdict::RefusedKernel: return "RefusedKernel";
    case WorldGenerationComputeAdmissionVerdict::RefusedInactive: return "RefusedInactive";
    case WorldGenerationComputeAdmissionVerdict::RefusedCapacity: return "RefusedCapacity";
    case WorldGenerationComputeAdmissionVerdict::RefusedEmpty: return "RefusedEmpty";
  }
  return nullptr;
}

char const* WorldGenerationComputeAdmission::fingerprintReason(
    WorldGenerationComputeAdmissionDecision const& decision) noexcept {
  return decision.reason ? decision.reason : "";
}

WorldGenerationComputeAdmissionRecord WorldGenerationComputeAdmission::admit(
    WorldGenerationComputeService& service, WorldGenerationComputeAdmissionRequest request,
    bool gpuFenceRehearsal) {
  WorldGenerationComputeAdmissionRecord record;
  record.identity = request.identity;
  record.inputFingerprint = WorldGenerationComputeService::fingerprintInput(request.input);

  if (request.input.empty()) {
    record.verdict = WorldGenerationComputeAdmissionVerdict::RefusedEmpty;
    return record;
  }
  if (!isPreparationKernel(request.identity.kernel)) {
    record.verdict = WorldGenerationComputeAdmissionVerdict::RefusedKernel;
    return record;
  }

  auto decision = selectBackend(service.capabilities(), request, gpuFenceRehearsal);
  record.backend = decision.backend;
  record.decisionReason = decision.reason;

  auto ticket = service.submit(std::move(request.identity), std::move(request.input),
      request.parameter, decision.backend);
  if (!ticket) {
    record.verdict = WorldGenerationComputeAdmissionVerdict::RefusedCapacity;
    return record;
  }

  record.ticket = *ticket;
  record.verdict = WorldGenerationComputeAdmissionVerdict::Admitted;
  m_pending.append(record);
  return record;
}

List<WorldGenerationComputeAdmissionRecord> WorldGenerationComputeAdmission::listPending() const {
  return m_pending;
}

size_t WorldGenerationComputeAdmission::stageReadyOpen(WorldGenerationComputeService& service,
    WorldGenerationComputePublishGate& gate, size_t maxRecords) {
  size_t staged = 0;
  size_t i = 0;
  while (i < m_pending.size()) {
    if (maxRecords != 0 && staged >= maxRecords)
      break;
    auto& pending = m_pending[i];
    auto report = gate.stageReady(
        service, pending.ticket, pending.identity, pending.inputFingerprint);
    if (report.verdict == WorldGenerationComputePublishVerdict::NotReady) {
      ++i;
      continue;
    }
    // Staged, stale, unknown, etc. — drop from pending (ticket consumed or gone).
    if (report.verdict == WorldGenerationComputePublishVerdict::Staged)
      ++staged;
    m_pending.eraseAt(i);
  }
  return staged;
}

size_t WorldGenerationComputeAdmission::clearPending() noexcept {
  size_t n = m_pending.size();
  m_pending.clear();
  return n;
}

}
