#pragma once

#include "StarWorldGenerationHardwareSelection.hpp"

#include <cstdint>
#include <limits>

namespace Star {

// First H5 / VAL-03 cut: versioned delivery milestones and resource counters.
// Names match the instrumentation requirements; values are caller-supplied
// observations, not live WorldThread/GPU drivers. Incomplete coverage must be
// declared explicitly — never invent a complete pipeline from this recorder.

enum class WorldGenerationDeliveryMilestone : uint8_t {
  Interest = 0,
  Preparation,
  Eligibility,
  Publication,
  Delivery,
  PassExit,
  Resources,
  Count
};

enum class WorldGenerationPassExitReason : uint8_t {
  Empty = 0,
  Worker,
  Dependency,
  Policy,
  Stages,
  Deadline,
  Error,
  Unknown
};

struct WorldGenerationInterestMetric {
  uint64_t origin = 0;
  uint64_t sequence = 0;
  uint32_t priority = 0;
  double timeMs = 0;
  uint32_t initialLevel = 0;
  bool recorded = false;
};

struct WorldGenerationPreparationMetric {
  uint64_t reservationId = 0;
  double startMs = 0;
  double endMs = 0;
  WorldGenerationHardwareRoutingBackend backend = WorldGenerationHardwareRoutingBackend::Cpu;
  size_t bytes = 0;
  uint64_t inputRevision = 0;
  bool recorded = false;
};

struct WorldGenerationEligibilityMetric {
  uint64_t missingPrerequisite = 0;
  double releasedMs = 0;
  bool recorded = false;
};

struct WorldGenerationPublicationMetric {
  bool validated = false;
  double exclusiveCostMs = 0;
  uint32_t effectCount = 0;
  uint32_t confirmedLevel = 0;
  bool recorded = false;
};

struct WorldGenerationDeliveryMetric {
  double enqueueMs = 0;
  double sendMs = 0;
  double receiveMs = 0;
  double applyMs = 0;
  bool recorded = false;
};

struct WorldGenerationPassExitMetric {
  WorldGenerationPassExitReason reason = WorldGenerationPassExitReason::Unknown;
  double timeMs = 0;
  bool recorded = false;
};

struct WorldGenerationResourceMetric {
  double usefulCpuMs = 0;
  size_t uniqueRamBytes = 0;
  size_t inFlightRamBytes = 0;
  size_t vramBytes = 0;
  size_t transferBytes = 0;
  double gpuWaitMs = 0;
  // Calibrated wall-clock transfer cost. transferBytes alone never imply ms.
  double transferMs = 0;
  bool transferTimingCalibrated = false;
  double materializeMs = 0;
  bool materializeTimingCalibrated = false;
  bool recorded = false;
  bool coverageComplete = false;
};

struct WorldGenerationDeliveryMetricsSnapshot {
  static constexpr unsigned ModelVersion = 2;
  WorldGenerationInterestMetric interest;
  WorldGenerationPreparationMetric preparation;
  WorldGenerationEligibilityMetric eligibility;
  WorldGenerationPublicationMetric publication;
  WorldGenerationDeliveryMetric delivery;
  WorldGenerationPassExitMetric passExit;
  WorldGenerationResourceMetric resources;
  uint32_t recordedMask = 0;

  bool has(WorldGenerationDeliveryMilestone milestone) const noexcept {
    auto bit = static_cast<uint32_t>(milestone);
    return bit < static_cast<uint32_t>(WorldGenerationDeliveryMilestone::Count)
        && (recordedMask & (1u << bit)) != 0;
  }

  uint32_t recordedCount() const noexcept {
    uint32_t count = 0;
    for (uint32_t bit = 0; bit < static_cast<uint32_t>(WorldGenerationDeliveryMilestone::Count); ++bit)
      count += (recordedMask & (1u << bit)) ? 1u : 0u;
    return count;
  }

  bool allMilestonesRecorded() const noexcept {
    return recordedCount() == static_cast<uint32_t>(WorldGenerationDeliveryMilestone::Count);
  }
};

// Caller-local recorder. Not a telemetry bus, Tracy integration or publish path.
class WorldGenerationDeliveryMetricsRecorder {
public:
  WorldGenerationDeliveryMetricsSnapshot const& snapshot() const noexcept { return m_snapshot; }

  void recordInterest(WorldGenerationInterestMetric metric) noexcept {
    metric.recorded = true;
    m_snapshot.interest = metric;
    mark(WorldGenerationDeliveryMilestone::Interest);
  }

  void recordPreparation(WorldGenerationPreparationMetric metric) noexcept {
    metric.recorded = true;
    m_snapshot.preparation = metric;
    mark(WorldGenerationDeliveryMilestone::Preparation);
  }

  void recordEligibility(WorldGenerationEligibilityMetric metric) noexcept {
    metric.recorded = true;
    m_snapshot.eligibility = metric;
    mark(WorldGenerationDeliveryMilestone::Eligibility);
  }

  void recordPublication(WorldGenerationPublicationMetric metric) noexcept {
    metric.recorded = true;
    m_snapshot.publication = metric;
    mark(WorldGenerationDeliveryMilestone::Publication);
  }

  void recordDelivery(WorldGenerationDeliveryMetric metric) noexcept {
    metric.recorded = true;
    m_snapshot.delivery = metric;
    mark(WorldGenerationDeliveryMilestone::Delivery);
  }

  void recordPassExit(WorldGenerationPassExitMetric metric) noexcept {
    metric.recorded = true;
    m_snapshot.passExit = metric;
    mark(WorldGenerationDeliveryMilestone::PassExit);
  }

  void recordResources(WorldGenerationResourceMetric metric) noexcept {
    metric.recorded = true;
    m_snapshot.resources = metric;
    mark(WorldGenerationDeliveryMilestone::Resources);
  }

  // Build a delivery-cost sample from preparation + resource observations when
  // both exist. Returns false if coverage is incomplete — never fabricates GPU
  // wait or transfer from a partial record.
  bool tryObservedDeliveryCost(WorldGenerationDeliveryCost& out) const noexcept {
    if (!m_snapshot.has(WorldGenerationDeliveryMilestone::Preparation)
        || !m_snapshot.has(WorldGenerationDeliveryMilestone::Resources))
      return false;
    if (!m_snapshot.resources.coverageComplete)
      return false;
    auto const& prep = m_snapshot.preparation;
    if (!(prep.endMs >= prep.startMs))
      return false;
    out = {};
    out.executionMs = prep.endMs - prep.startMs;
    out.fenceMs = m_snapshot.resources.gpuWaitMs;
    out.materializeMs = 0;
    // Bytes alone never become milliseconds. Only an explicit calibration
    // (Desktop/H4 probe timings) may populate transfer/materialize.
    if (m_snapshot.resources.transferTimingCalibrated)
      out.transferMs = m_snapshot.resources.transferMs;
    else
      out.transferMs = 0;
    if (m_snapshot.resources.materializeTimingCalibrated)
      out.materializeMs = m_snapshot.resources.materializeMs;
    else
      out.materializeMs = 0;
    return out.valid();
  }

private:
  void mark(WorldGenerationDeliveryMilestone milestone) noexcept {
    m_snapshot.recordedMask |= (1u << static_cast<uint32_t>(milestone));
  }

  WorldGenerationDeliveryMetricsSnapshot m_snapshot;
};

}
