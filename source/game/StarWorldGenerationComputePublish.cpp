#include "StarWorldGenerationComputePublish.hpp"

namespace Star {

bool WorldGenerationComputePublishGate::isLivePublishKernel(
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
  record.values = std::move(result.values);
  record.valueCount = record.values.size();
  record.outputFingerprint = WorldGenerationComputeService::fingerprintInput(record.values);
  record.verdict = WorldGenerationComputePublishVerdict::Staged;
  m_staged.append(record);
  return record;
}

WorldGenerationComputePublishVerdict WorldGenerationComputePublishGate::applyToLiveWorld(
    WorldGenerationComputePublishRecord const& record) {
  if (!m_livePublishLimited)
    return WorldGenerationComputePublishVerdict::RefusedLiveWorld;
  if (record.verdict != WorldGenerationComputePublishVerdict::Staged)
    return WorldGenerationComputePublishVerdict::RefusedLiveWorld;
  if (!isLivePublishKernel(record.identity.kernel))
    return WorldGenerationComputePublishVerdict::RefusedLiveWorld;
  if (record.values.empty() || record.valueCount != record.values.size())
    return WorldGenerationComputePublishVerdict::RefusedLiveWorld;
  if (record.valueCount > MaxLivePublishValues)
    return WorldGenerationComputePublishVerdict::RefusedLiveWorld;
  if (record.outputFingerprint
      != WorldGenerationComputeService::fingerprintInput(record.values))
    return WorldGenerationComputePublishVerdict::RefusedLiveWorld;

  // Limited live sink only — no WorldStorage / entity / generateQueue mutation.
  m_live = {};
  m_live.ticket = record.ticket;
  m_live.identity = record.identity;
  m_live.backend = record.backend;
  m_live.inputFingerprint = record.inputFingerprint;
  m_live.outputFingerprint = record.outputFingerprint;
  m_live.values = record.values;
  m_hasLive = true;

  // Drop matching staged entry if present (best-effort by ticket id).
  for (size_t i = 0; i < m_staged.size(); ++i) {
    if (m_staged[i].ticket.id == record.ticket.id) {
      m_staged.eraseAt(i);
      break;
    }
  }

  return WorldGenerationComputePublishVerdict::PublishedLiveLimited;
}

size_t WorldGenerationComputePublishGate::clearLivePublication() noexcept {
  if (!m_hasLive)
    return 0;
  m_live = {};
  m_hasLive = false;
  return 1;
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
    case WorldGenerationComputePublishVerdict::PublishedLiveLimited: return "PublishedLiveLimited";
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
