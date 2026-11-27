#pragma once

#include "StarWorldGenerationAdoptionGates.hpp"
#include "StarWorldGenerationEquivalenceVersion.hpp"
#include "StarWorldGenerationPlatformMatrix.hpp"
#include "StarWorldGenerationScenarioCampaign.hpp"
#include "StarWorldGenerationScaleMetrics.hpp"

#include <cstdint>

namespace Star {

// Third H5 / MOD-02+MOD-04+VAL cut: pack scale evidence for Integrator review.
// readyForIntegratorReview means contracts + campaign/gates are consistent for
// pull consideration — never an authorization to merge INT or publish GPU.

struct WorldGenerationScaleEvidenceBundle {
  static constexpr unsigned ModelVersion = 1;
  WorldGenerationEquivalenceVersion equivalence;
  WorldGenerationPlatform probePlatform = WorldGenerationPlatform::Windows;
  WorldGenerationCompatStatus platformStatus = WorldGenerationCompatStatus::Unsupported;
  WorldGenerationCompatStatus hardwareComputeStatus = WorldGenerationCompatStatus::Unsupported;
  bool cpuFallbackRequired = true;
  bool scaleCapabilitiesClaimed = false;
  bool scaleMeasuredGain = false;
  bool campaignCpuOnlyCovered = false;
  bool campaignHasFailures = false;
  bool gatesAdopt = false;
  bool gatesAuthoritativeGpuBlocked = true;
  bool shadowOutputComparison = false;
  bool readyForIntegratorReview = false;
};

inline WorldGenerationEquivalenceVersion worldGenerationScaleEvidenceEquivalence(
    uint32_t generationEpoch,
    uint32_t profileEpoch,
    uint32_t comparatorEpoch) noexcept {
  WorldGenerationEquivalenceVersion version;
  version.generationEpoch = generationEpoch;
  version.profileEpoch = profileEpoch;
  version.comparatorEpoch = comparatorEpoch;
  return version;
}

inline WorldGenerationScaleEvidenceBundle buildWorldGenerationScaleEvidence(
    WorldGenerationScaleMetricsSnapshot const& scale,
    WorldGenerationScenarioCampaignSnapshot const& campaign,
    WorldGenerationAdoptionEvidence const& adoptionEvidence,
    WorldGenerationEquivalenceVersion const& equivalence,
    WorldGenerationPlatform probePlatform = WorldGenerationPlatform::Windows) noexcept {
  WorldGenerationScaleEvidenceBundle bundle;
  bundle.equivalence = equivalence;
  bundle.probePlatform = probePlatform;

  auto platform = worldGenerationPlatformSupport(probePlatform);
  bundle.platformStatus = platform.status;
  auto compute = worldGenerationSubsystemSupport(WorldGenerationSubsystem::HardwareCompute);
  bundle.hardwareComputeStatus = compute.status;
  bundle.cpuFallbackRequired = compute.cpuFallbackRequired;

  bundle.scaleCapabilitiesClaimed = scale.capabilitiesClaimed;
  bundle.scaleMeasuredGain = scale.measuredDeliveryGain();
  bundle.campaignCpuOnlyCovered = campaign.cpuOnlyMatrixCovered();
  bundle.campaignHasFailures = campaign.failedCount() != 0;

  auto gates = summarizeWorldGenerationScaleGates(adoptionEvidence);
  bundle.gatesAdopt = gates.adopt;
  bundle.gatesAuthoritativeGpuBlocked = gates.authoritativeGpuBlocked;
  bundle.shadowOutputComparison = gates.shadowOutputComparison;

  bool platformOk = worldGenerationPlatformInScope(probePlatform);
  bool computeOptionalOk = compute.status == WorldGenerationCompatStatus::Optional
      && compute.cpuFallbackRequired;
  // Integrator may review when: supported probe platform, optional GPU with CPU
  // fallback, no campaign failures, CPU-only matrix covered or GPU shadow
  // comparison present, and adoption still refuses authoritative GPU alone.
  bundle.readyForIntegratorReview = platformOk
      && computeOptionalOk
      && !bundle.campaignHasFailures
      && (bundle.campaignCpuOnlyCovered || bundle.shadowOutputComparison)
      && bundle.gatesAuthoritativeGpuBlocked
      && !bundle.gatesAdopt
      && equivalence.generationEpoch != 0;
  return bundle;
}

inline WorldGenerationScaleEvidenceBundle buildWorldGenerationScaleEvidenceFromRecorders(
    WorldGenerationScaleMetricsRecorder const& scaleRecorder,
    WorldGenerationScenarioCampaignRecorder const& campaignRecorder,
    WorldGenerationDeliveryMetricsSnapshot const& delivery,
    bool reportDocumented,
    WorldGenerationEquivalenceVersion const& equivalence,
    WorldGenerationPlatform probePlatform = WorldGenerationPlatform::Windows) noexcept {
  auto adoption = worldGenerationAdoptionFromScaleMetrics(
      scaleRecorder.snapshot(), delivery, reportDocumented);
  return buildWorldGenerationScaleEvidence(
      scaleRecorder.snapshot(), campaignRecorder.snapshot(), adoption, equivalence, probePlatform);
}

}
