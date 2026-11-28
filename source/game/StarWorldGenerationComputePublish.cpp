#include "StarWorldGenerationComputePublish.hpp"

namespace Star {

WorldGenerationComputePublishRecord WorldGenerationComputePublishGate::stageReady(
    WorldGenerationComputeService& service, WorldGenerationComputeTicket ticket,
    WorldGenerationComputeInputIdentity const& liveIdentity, uint64_t liveInputFingerprint) {
  WorldGenerationComputePublishRecord record;
  record.ticket = ticket;
  record.identity = liveIdentity;
  record.inputFingerprint = liveInputFingerprint;

  WorldGenerationComputeResult result;
  auto report = service.commit(ticket, liveIdentity, liveInputFingerprint, &result);
  record.backend = report.backend;
  record.inputFingerprint = report.submittedInputFingerprint;

  if (report.verdict == WorldGenerationComputeCommitVerdict::UnknownTicket) {
    record.verdict = WorldGenerationComputePublishVerdict::UnknownTicket;
    return record;
  }
  if (report.verdict == WorldGenerationComputeCommitVerdict::NotReady) {
    record.verdict = WorldGenerationComputePublishVerdict::NotReady;
    return record;
  }
  if (report.verdict == WorldGenerationComputeCommitVerdict::StaleIdentity
      || report.verdict == WorldGenerationComputeCommitVerdict::StaleInput) {
    record.verdict = WorldGenerationComputePublishVerdict::StaleIdentity;
    return record;
  }
  if (report.verdict == WorldGenerationComputeCommitVerdict::Cancelled
      || report.verdict == WorldGenerationComputeCommitVerdict::Failed) {
    record.verdict = WorldGenerationComputePublishVerdict::GateDisabled;
    return record;
  }
  if (report.verdict != WorldGenerationComputeCommitVerdict::Accepted) {
    record.verdict = WorldGenerationComputePublishVerdict::NotReady;
    return record;
  }

  record.identity = result.identity;
  record.backend = result.backend;
  record.outputFingerprint = WorldGenerationComputeService::fingerprintInput(result.values);
  record.valueCount = result.values.size();
  record.verdict = WorldGenerationComputePublishVerdict::Staged;
  m_staged.append(record);
  return record;
}

WorldGenerationComputePublishVerdict WorldGenerationComputePublishGate::applyToLiveWorld(
    WorldGenerationComputePublishRecord const& /*record*/) noexcept {
  // Cloud-safe stub: refuse every live apply. Sector/tile mutation remains blocked.
  return WorldGenerationComputePublishVerdict::RefusedLiveWorld;
}

List<WorldGenerationComputePublishRecord> WorldGenerationComputePublishGate::listStaged() const {
  return m_staged;
}

size_t WorldGenerationComputePublishGate::clearStaged() noexcept {
  size_t n = m_staged.size();
  m_staged.clear();
  return n;
}

char const* WorldGenerationComputePublishGate::publishVerdictName(
    WorldGenerationComputePublishVerdict verdict) noexcept {
  switch (verdict) {
    case WorldGenerationComputePublishVerdict::Staged: return "Staged";
    case WorldGenerationComputePublishVerdict::RefusedLiveWorld: return "RefusedLiveWorld";
    case WorldGenerationComputePublishVerdict::NotReady: return "NotReady";
    case WorldGenerationComputePublishVerdict::StaleIdentity: return "StaleIdentity";
    case WorldGenerationComputePublishVerdict::UnknownTicket: return "UnknownTicket";
    case WorldGenerationComputePublishVerdict::GateDisabled: return "GateDisabled";
  }
  return nullptr;
}

uint64_t WorldGenerationComputePublishGate::fingerprintPublishRecord(
    WorldGenerationComputePublishRecord const& record) noexcept {
  uint64_t hash = 14695981039346656037ull;
  auto mix = [&](uint64_t v) {
    hash ^= v;
    hash *= 1099511628211ull;
  };
  mix(record.ticket.id);
  mix(WorldGenerationComputeService::fingerprintIdentity(record.identity));
  mix((uint64_t)record.backend);
  mix(record.inputFingerprint);
  mix(record.outputFingerprint);
  mix(record.valueCount);
  mix((uint64_t)record.verdict);
  return hash;
}

}
