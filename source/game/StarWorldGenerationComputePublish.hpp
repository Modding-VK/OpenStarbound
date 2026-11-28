#pragma once

#include "StarWorldGenerationCompute.hpp"

namespace Star {

// GPU-02 / m50 publish gate:
// Stage Ready compute results for review. Optional limited live publication installs
// Terrain/Biome values into an in-memory sink only — never WorldStorage tiles,
// entities, or generateQueue. Default: live apply refused.

enum class WorldGenerationComputePublishVerdict : uint8_t {
  Staged = 0,
  RefusedLiveWorld = 1,
  NotReady = 2,
  StaleIdentity = 3,
  UnknownTicket = 4,
  GateDisabled = 5,
  PublishedLiveLimited = 6
};

struct WorldGenerationComputePublishRecord {
  WorldGenerationComputeTicket ticket{};
  WorldGenerationComputeInputIdentity identity{};
  WorldGenerationComputeBackend backend = WorldGenerationComputeBackend::CpuReference;
  uint64_t inputFingerprint = 0;
  uint64_t outputFingerprint = 0;
  size_t valueCount = 0;
  List<uint32_t> values;
  WorldGenerationComputePublishVerdict verdict = WorldGenerationComputePublishVerdict::GateDisabled;
};

// In-memory live publication sink (preparation-visible). Not a WorldServer write.
struct WorldGenerationComputeLivePublication {
  WorldGenerationComputeTicket ticket{};
  WorldGenerationComputeInputIdentity identity{};
  WorldGenerationComputeBackend backend = WorldGenerationComputeBackend::CpuReference;
  uint64_t inputFingerprint = 0;
  uint64_t outputFingerprint = 0;
  List<uint32_t> values;
};

class WorldGenerationComputePublishGate {
public:
  // Hard cap for limited live publish (words). Above this → RefusedLiveWorld.
  static constexpr size_t MaxLivePublishValues = 1048576u;

  WorldGenerationComputePublishGate() = default;

  // Commits a Ready ticket (consumes it) into a staged review record when live
  // identity/input fingerprints match. Keeps values for optional limited apply.
  WorldGenerationComputePublishRecord stageReady(WorldGenerationComputeService& service,
      WorldGenerationComputeTicket ticket, WorldGenerationComputeInputIdentity const& liveIdentity,
      uint64_t liveInputFingerprint);

  // Default refuses. When livePublishLimited is enabled: Terrain/Biome staged records
  // only, size-capped, installed into the in-memory sink (not WorldStorage).
  WorldGenerationComputePublishVerdict applyToLiveWorld(
      WorldGenerationComputePublishRecord const& record);

  void setLivePublishLimited(bool enabled) noexcept { m_livePublishLimited = enabled; }
  bool livePublishLimited() const noexcept { return m_livePublishLimited; }

  bool hasLivePublication() const noexcept { return m_hasLive; }
  WorldGenerationComputeLivePublication const* livePublication() const noexcept {
    return m_hasLive ? &m_live : nullptr;
  }
  size_t clearLivePublication() noexcept;

  size_t stagedCount() const noexcept { return m_staged.size(); }
  List<WorldGenerationComputePublishRecord> listStaged() const;
  size_t clearStaged() noexcept;

  static bool isLivePublishKernel(WorldGenerationComputeKernel kernel) noexcept;
  static char const* publishVerdictName(WorldGenerationComputePublishVerdict verdict) noexcept;
  static uint64_t fingerprintPublishRecord(WorldGenerationComputePublishRecord const& record) noexcept;

private:
  bool m_livePublishLimited = false;
  bool m_hasLive = false;
  WorldGenerationComputeLivePublication m_live{};
  List<WorldGenerationComputePublishRecord> m_staged;
};

}
