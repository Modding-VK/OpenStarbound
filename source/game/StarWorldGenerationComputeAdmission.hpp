#pragma once

#include "StarWorldGenerationCompute.hpp"
#include "StarWorldGenerationComputePublish.hpp"

namespace Star {

// GPU-02 / m50 preparation-admission vertical (cloud-safe):
// Admit Terrain/Biome preparation work into the compute service and track it
// for later staging via PublishGate. Never writes tiles/entities/generateQueue.
// DenseUint32 kernels are refused here (horizontal STOP).
//
// Backend selection is a heuristic stub for GPU-05 prep — not measured transport
// cost, not a 50% claim, and not GPU-05 acceptance.

enum class WorldGenerationComputeAdmissionVerdict : uint8_t {
  Admitted = 0,
  RefusedKernel = 1,
  RefusedInactive = 2,
  RefusedCapacity = 3,
  RefusedEmpty = 4
};

struct WorldGenerationComputeAdmissionRequest {
  WorldGenerationComputeInputIdentity identity{};
  List<uint32_t> input;
  uint32_t parameter = 0;
  // When true, prefer CPU even if GPU appears available (urgent / low-latency).
  bool preferLowLatency = false;
};

struct WorldGenerationComputeAdmissionDecision {
  WorldGenerationComputeBackend backend = WorldGenerationComputeBackend::CpuReference;
  char const* reason = "uninitialized";
  // Opaque heuristic score (bytes-weighted); for tests/observation only.
  uint64_t costScore = 0;
};

struct WorldGenerationComputeAdmissionRecord {
  WorldGenerationComputeTicket ticket{};
  WorldGenerationComputeInputIdentity identity{};
  WorldGenerationComputeBackend backend = WorldGenerationComputeBackend::CpuReference;
  uint64_t inputFingerprint = 0;
  char const* decisionReason = "";
  WorldGenerationComputeAdmissionVerdict verdict =
      WorldGenerationComputeAdmissionVerdict::RefusedInactive;
};

class WorldGenerationComputeAdmission {
public:
  // Preparation kernels only (TerrainField* / BiomeIdBucket). DenseUint32* → false.
  static bool isPreparationKernel(WorldGenerationComputeKernel kernel) noexcept;

  // Heuristic backend pick. Does not measure real upload/dispatch/readback cost.
  static WorldGenerationComputeAdmissionDecision selectBackend(
      WorldGenerationComputeCapabilities const& capabilities,
      WorldGenerationComputeAdmissionRequest const& request, bool gpuFenceRehearsal) noexcept;

  static char const* admissionVerdictName(WorldGenerationComputeAdmissionVerdict verdict) noexcept;
  static char const* fingerprintReason(WorldGenerationComputeAdmissionDecision const& decision) noexcept;

  // Submit preparation work; track fingerprints for later stageReadyOpen.
  // Does not stage and does not apply to the live world.
  WorldGenerationComputeAdmissionRecord admit(WorldGenerationComputeService& service,
      WorldGenerationComputeAdmissionRequest request, bool gpuFenceRehearsal);

  size_t pendingCount() const noexcept { return m_pending.size(); }
  List<WorldGenerationComputeAdmissionRecord> listPending() const;

  // Stage Ready pending tickets through the publish gate (consumes them).
  // Returns how many records reached Staged. Live apply remains refused by the gate.
  size_t stageReadyOpen(WorldGenerationComputeService& service,
      WorldGenerationComputePublishGate& gate, size_t maxRecords = 0);

  size_t clearPending() noexcept;

private:
  List<WorldGenerationComputeAdmissionRecord> m_pending;
};

}
