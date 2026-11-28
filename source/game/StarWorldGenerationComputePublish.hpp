#pragma once

#include "StarWorldGenerationCompute.hpp"

namespace Star {

// GPU-02 / m50 publish-live vertical (cloud-safe stub):
// Stage Ready compute results for review. Never writes tiles/entities/generateQueue.
// applyToLiveWorld always refuses in this tip — live sector adoption is a real blocker.

enum class WorldGenerationComputePublishVerdict : uint8_t {
  Staged = 0,
  RefusedLiveWorld = 1,
  NotReady = 2,
  StaleIdentity = 3,
  UnknownTicket = 4,
  GateDisabled = 5
};

struct WorldGenerationComputePublishRecord {
  WorldGenerationComputeTicket ticket{};
  WorldGenerationComputeInputIdentity identity{};
  WorldGenerationComputeBackend backend = WorldGenerationComputeBackend::CpuReference;
  uint64_t inputFingerprint = 0;
  uint64_t outputFingerprint = 0;
  size_t valueCount = 0;
  WorldGenerationComputePublishVerdict verdict = WorldGenerationComputePublishVerdict::GateDisabled;
};

class WorldGenerationComputePublishGate {
public:
  WorldGenerationComputePublishGate() = default;

  // Commits a Ready ticket (consumes it) into a staged review record when live
  // identity/input fingerprints match. Never writes tiles/entities/generateQueue.
  WorldGenerationComputePublishRecord stageReady(WorldGenerationComputeService& service,
      WorldGenerationComputeTicket ticket, WorldGenerationComputeInputIdentity const& liveIdentity,
      uint64_t liveInputFingerprint);

  // Explicit live-world apply — always RefusedLiveWorld until a future tip that
  // implements audited sector commit (not this vertical).
  WorldGenerationComputePublishVerdict applyToLiveWorld(
      WorldGenerationComputePublishRecord const& record) noexcept;

  size_t stagedCount() const noexcept { return m_staged.size(); }
  List<WorldGenerationComputePublishRecord> listStaged() const;
  size_t clearStaged() noexcept;

  static char const* publishVerdictName(WorldGenerationComputePublishVerdict verdict) noexcept;
  static uint64_t fingerprintPublishRecord(WorldGenerationComputePublishRecord const& record) noexcept;

private:
  List<WorldGenerationComputePublishRecord> m_staged;
};

}
