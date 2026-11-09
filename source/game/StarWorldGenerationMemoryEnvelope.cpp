#include "StarWorldGenerationMemoryEnvelope.hpp"

namespace Star {
namespace {
using Class = WorldGenerationMemoryClass;
using Quantity = WorldGenerationMemoryQuantity;
using Collection = WorldGenerationMemoryCollection;
using Frontier = WorldGenerationMemoryFrontier;
constexpr WorldGenerationMemoryClassInfo Classes[] = {
  {Class::BaseTilesArray, Quantity::ArrayPayload, Collection::Always, Frontier::BeforeAllocation},
  {Class::MicroDungeonHalo, Quantity::ArrayPayload, Collection::Always, Frontier::BeforeAllocation},
  {Class::TemplateJsonEstimate, Quantity::Estimate, Collection::RegionalProfiling, Frontier::ObservationOrPrediction},
  {Class::SnapshotMetadataEstimate, Quantity::Estimate, Collection::RegionalProfiling, Frontier::ObservationOrPrediction},
  {Class::MicroDungeonPlanEstimate, Quantity::Estimate, Collection::RegionalProfiling, Frontier::ObservationOrPrediction},
  {Class::WorkerTemplateEstimate, Quantity::Estimate, Collection::Always, Frontier::JobBoundary},
  {Class::MicroDungeonPlanBuffer, Quantity::AllocatorRequest, Collection::Always, Frontier::BeforeAllocation},
  {Class::MicroDungeonCandidateBuffer, Quantity::AllocatorRequest, Collection::Always, Frontier::BeforeAllocation},
  {Class::MicroDungeonDistributionBuffer, Quantity::AllocatorRequest, Collection::Always, Frontier::BeforeAllocation},
  {Class::MicroDungeonDistributionEstimate, Quantity::Estimate, Collection::Always, Frontier::ObservationOrPrediction},
  {Class::MicroDungeonNameBuffer, Quantity::AllocatorRequest, Collection::Always, Frontier::BeforeAllocation},
  {Class::MicroDungeonNameEstimate, Quantity::Estimate, Collection::Always, Frontier::ObservationOrPrediction},
  {Class::MicroDungeonCandidateEstimate, Quantity::Estimate, Collection::Always, Frontier::ObservationOrPrediction},
  {Class::MicroDungeonTemporaryEstimate, Quantity::Estimate, Collection::Always, Frontier::ObservationOrPrediction}
};
constexpr bool classTableComplete() {
  if (sizeof(Classes) / sizeof(Classes[0]) != static_cast<size_t>(Class::Count)) return false;
  for (size_t i = 0; i < static_cast<size_t>(Class::Count); ++i)
    if (static_cast<size_t>(Classes[i].kind) != i) return false;
  return true;
}
static_assert(classTableComplete(), "Classify every generation memory class exactly once and in enum order");

bool add(size_t& total, size_t value) noexcept {
  if (value > std::numeric_limits<size_t>::max() - total) return false;
  total += value;
  return true;
}
}

WorldGenerationMemoryClassInfo const* worldGenerationMemoryClassInfo(Class kind) noexcept {
  auto index = static_cast<size_t>(kind);
  return index < static_cast<size_t>(Class::Count) ? &Classes[index] : nullptr;
}

WorldGenerationMemoryEnvelope worldGenerationMemoryEnvelope(WorldGenerationMemorySnapshot const& usage) noexcept {
  WorldGenerationMemoryEnvelope result;
  result.historicalIncompleteUpdates = usage.incompleteEstimateUpdates;
  for (auto const& info : Classes) {
    auto index = static_cast<size_t>(info.kind);
    auto bytes = usage.classBytes[index];
    result.coherent &= add(result.trackedBytes, bytes);
    result.coherent &= add(result.resources, usage.classResources[index]);
    if (bytes && !usage.classResources[index]) result.coherent = false;
    if (info.quantity == Quantity::ArrayPayload) result.coherent &= add(result.arrayPayloadBytes, bytes);
    else if (info.quantity == Quantity::AllocatorRequest) result.coherent &= add(result.allocatorRequestBytes, bytes);
    else {
      result.coherent &= add(result.estimatedBytes, bytes);
      if (info.collection == Collection::Always) result.coherent &= add(result.alwaysEstimatedBytes, bytes);
      else result.coherent &= add(result.profilingEstimatedBytes, bytes);
    }
  }
  result.coherent = result.coherent && result.trackedBytes == usage.reservedBytes
      && result.arrayPayloadBytes == usage.reservedArrayBytes
      && result.allocatorRequestBytes == usage.reservedBufferBytes
      && result.estimatedBytes == usage.estimatedBytes && result.resources == usage.resources
      && usage.sharedResources == usage.classResources[static_cast<size_t>(Class::TemplateJsonEstimate)]
      && usage.sharedRegistryEntries == usage.sharedResources;
  return result;
}

WorldGenerationMemoryAdvice WorldGenerationMemoryPressurePolicy::evaluate(WorldGenerationMemoryEnvelope const& usage,
    WorldGenerationMemoryDemand demand, size_t additionalBytes) noexcept {
  using Pressure = WorldGenerationMemoryPressure;
  using Reason = WorldGenerationMemoryAdviceReason;
  using Demand = WorldGenerationMemoryDemand;
  if (!m_limits.enabled) {
    m_pressure = Pressure::Unrestricted;
    return {m_pressure, Reason::Disabled, true};
  }
  if (!(m_limits.resumeBytes < m_limits.softBytes && m_limits.softBytes < m_limits.hardBytes)) {
    return {Pressure::Invalid, Reason::InvalidLimits, false};
  }
  if (!usage.coherent) {
    return {Pressure::Invalid, Reason::InvalidEnvelope, false};
  }
  if (demand != Demand::Visible && demand != Demand::Dependency && demand != Demand::Speculative) {
    return {Pressure::Invalid, Reason::InvalidDemand, false};
  }
  auto bytes = usage.trackedBytes;
  bool recovering = m_pressure == Pressure::Soft || m_pressure == Pressure::Hard;
  if (bytes >= m_limits.hardBytes) m_pressure = Pressure::Hard;
  else if (bytes >= m_limits.softBytes || (recovering && bytes > m_limits.resumeBytes)) m_pressure = Pressure::Soft;
  else m_pressure = Pressure::Normal;
  if (!additionalBytes) return {m_pressure, Reason::Reuse, true};
  if (m_pressure == Pressure::Hard) return {m_pressure, Reason::AtHardLimit, false};
  if (additionalBytes > m_limits.hardBytes - bytes) return {m_pressure, Reason::ExceedsHardLimit, false};
  if (demand == Demand::Speculative) {
    if (m_pressure == Pressure::Soft) return {m_pressure, Reason::SpeculationPaused, false};
    if (additionalBytes > m_limits.softBytes - bytes) return {m_pressure, Reason::ExceedsSoftLimit, false};
  }
  return {m_pressure, Reason::Fits, true};
}

}
