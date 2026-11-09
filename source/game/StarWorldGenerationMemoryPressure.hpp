#pragma once

#include <cstddef>

namespace Star {

struct WorldGenerationMemoryEnvelope;

enum class WorldGenerationMemoryPressure { Unrestricted, Normal, Soft, Hard, Invalid };
enum class WorldGenerationMemoryDemand { Visible, Dependency, Speculative };
enum class WorldGenerationMemoryAdviceReason {
  Disabled, InvalidLimits, InvalidEnvelope, InvalidDemand, Reuse,
  AtHardLimit, ExceedsHardLimit, SpeculationPaused, ExceedsSoftLimit, Fits
};

struct WorldGenerationMemoryPressureLimits {
  bool enabled = false;
  size_t resumeBytes = 0;
  size_t softBytes = 0;
  size_t hardBytes = 0;
};

struct WorldGenerationMemoryAdvice {
  WorldGenerationMemoryPressure pressure;
  WorldGenerationMemoryAdviceReason reason;
  bool allowRequest;
  bool drainExisting = true;
};

// Caller-local model, or shared only under the owning ledger mutex. Advice
// alone is not a reservation, eviction, hardware query or world authority.
class WorldGenerationMemoryPressurePolicy {
public:
  explicit WorldGenerationMemoryPressurePolicy(WorldGenerationMemoryPressureLimits limits = {}) noexcept
    : m_limits(limits) {}
  WorldGenerationMemoryAdvice evaluate(WorldGenerationMemoryEnvelope const& usage,
      WorldGenerationMemoryDemand demand, size_t additionalBytes) noexcept;
private:
  WorldGenerationMemoryPressureLimits m_limits;
  // Invalid input cannot erase the last valid recovery state.
  WorldGenerationMemoryPressure m_pressure = WorldGenerationMemoryPressure::Unrestricted;
};

struct WorldGenerationMemoryAdmissionSnapshot {
  static constexpr unsigned ModelVersion = 1;
  bool enabled = false;
  size_t pressureChecks = 0;
  size_t pressureRefusals = 0;
  // Last checked observation, not pressure recomputed after every release.
  WorldGenerationMemoryAdvice lastAdvice{WorldGenerationMemoryPressure::Unrestricted,
      WorldGenerationMemoryAdviceReason::Disabled, true};
};

}
