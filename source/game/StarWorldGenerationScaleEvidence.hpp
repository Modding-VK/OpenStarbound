#pragma once

#include "StarWorldGenerationAdoptionGates.hpp"
#include "StarWorldGenerationEquivalenceVersion.hpp"
#include "StarWorldGenerationExecutionProfile.hpp"
#include "StarWorldGenerationPlatformMatrix.hpp"
#include "StarWorldGenerationScenarioCampaign.hpp"
#include "StarWorldGenerationScaleMetrics.hpp"

#include <cstdint>

namespace Star {

// Third H5 / MOD-02+MOD-04+VAL cut: pack scale evidence for Integrator review.
// readyForIntegratorReview means contracts + campaign/gates are consistent for
// pull consideration — never an authorization to merge INT or publish GPU.
// Fourth cut (MOD-03): recommend a conceptual profile from the pack without
// silently renaming existing performanceMode strings.
// Fifth cut (VAL): optional Desktop probe attestation (tip + contract counts).

struct WorldGenerationDesktopProbeAttestation {
  static constexpr unsigned ModelVersion = 1;
  // Lower 32 bits of the tip short SHA (first 8 hex digits). 0 means unset.
  uint32_t tipPrefix = 0;
  uint32_t testsPassed = 0;
  uint32_t testsExpected = 0;
  bool whatIfBootstrapOk = false;
  bool recorded = false;

  bool contractsPass() const noexcept {
    return recorded && testsExpected != 0 && testsPassed == testsExpected;
  }
};

inline uint32_t worldGenerationTipPrefixFromHex8(char const* hex8) noexcept {
  if (!hex8)
    return 0;
  uint32_t value = 0;
  for (int i = 0; i < 8; ++i) {
    char c = hex8[i];
    if (c == '\0')
      return 0;
    uint32_t nibble = 0;
    if (c >= '0' && c <= '9')
      nibble = static_cast<uint32_t>(c - '0');
    else if (c >= 'a' && c <= 'f')
      nibble = static_cast<uint32_t>(10 + c - 'a');
    else if (c >= 'A' && c <= 'F')
      nibble = static_cast<uint32_t>(10 + c - 'A');
    else
      return 0;
    value = (value << 4) | nibble;
  }
  return value;
}

struct WorldGenerationScaleEvidenceBundle {
  static constexpr unsigned ModelVersion = 2;
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
  bool desktopAttested = false;
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
  bundle.desktopAttested = false;
  bundle.readyForIntegratorReview = platformOk
      && computeOptionalOk
      && !bundle.campaignHasFailures
      && (bundle.campaignCpuOnlyCovered || bundle.shadowOutputComparison)
      && bundle.gatesAuthoritativeGpuBlocked
      && !bundle.gatesAdopt
      && equivalence.generationEpoch != 0;
  return bundle;
}

// When Desktop records an attestation, contracts must pass and tipPrefix must
// match the tip under review. Missing attestation leaves readiness unchanged.
inline bool applyWorldGenerationDesktopAttestation(
    WorldGenerationScaleEvidenceBundle& bundle,
    WorldGenerationDesktopProbeAttestation const& attestation,
    uint32_t expectedTipPrefix) noexcept {
  if (!attestation.recorded)
    return false;
  bool tipOk = attestation.tipPrefix != 0 && attestation.tipPrefix == expectedTipPrefix;
  bundle.desktopAttested = tipOk && attestation.contractsPass();
  if (attestation.recorded)
    bundle.readyForIntegratorReview = bundle.readyForIntegratorReview && bundle.desktopAttested;
  return bundle.desktopAttested;
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

enum class WorldGenerationScaleProfileAdvice : uint8_t {
  KeepExisting = 0,
  PreferDynamicBias,
  PreferHighThroughput,
  ForceCpuCompleteFallback,
  RejectUnknown,
  Count
};

struct WorldGenerationScaleProfileRecommendation {
  static constexpr unsigned ModelVersion = 1;
  WorldGenerationExecutionProfile suggested = WorldGenerationExecutionProfile::Balanced;
  WorldGenerationScaleProfileAdvice advice = WorldGenerationScaleProfileAdvice::KeepExisting;
  WorldGenerationFallbackKind fallback = WorldGenerationFallbackKind::CpuComplete;
  bool allowsGpuSelection = false;
  // Always true: callers must opt in; never rewrite performanceMode in place.
  bool requiresExplicitUserOptIn = true;
};

inline WorldGenerationScaleProfileRecommendation recommendWorldGenerationProfileFromScaleEvidence(
    WorldGenerationScaleEvidenceBundle const& bundle,
    WorldGenerationPerformanceMode currentMode) noexcept {
  auto binding = bindWorldGenerationExecutionProfile(currentMode);
  WorldGenerationScaleProfileRecommendation rec;
  rec.suggested = binding.conceptual;
  rec.fallback = binding.fallback;
  rec.allowsGpuSelection = binding.allowsGpuSelection;
  rec.requiresExplicitUserOptIn = true;

  if (binding.migration == WorldGenerationProfileMigration::RejectedUnknown) {
    rec.advice = WorldGenerationScaleProfileAdvice::RejectUnknown;
    rec.suggested = WorldGenerationExecutionProfile::Manual;
    rec.allowsGpuSelection = false;
    rec.fallback = WorldGenerationFallbackKind::CpuComplete;
    return rec;
  }

  if (currentMode == WorldGenerationPerformanceMode::Legacy) {
    rec.advice = WorldGenerationScaleProfileAdvice::KeepExisting;
    rec.allowsGpuSelection = false;
    return rec;
  }

  if (bundle.campaignHasFailures || !bundle.gatesAuthoritativeGpuBlocked || bundle.gatesAdopt) {
    rec.advice = WorldGenerationScaleProfileAdvice::ForceCpuCompleteFallback;
    rec.suggested = WorldGenerationExecutionProfile::Balanced;
    rec.fallback = WorldGenerationFallbackKind::CpuComplete;
    rec.allowsGpuSelection = false;
    return rec;
  }

  if (bundle.readyForIntegratorReview && bundle.scaleMeasuredGain
      && bundle.shadowOutputComparison) {
    rec.advice = WorldGenerationScaleProfileAdvice::PreferDynamicBias;
    rec.suggested = WorldGenerationExecutionProfile::DynamicBias;
    rec.fallback = WorldGenerationFallbackKind::CompatiblePath;
    rec.allowsGpuSelection = true;
    return rec;
  }

  if (bundle.readyForIntegratorReview && bundle.campaignCpuOnlyCovered
      && currentMode == WorldGenerationPerformanceMode::Aggressive) {
    rec.advice = WorldGenerationScaleProfileAdvice::PreferHighThroughput;
    rec.suggested = WorldGenerationExecutionProfile::HighThroughput;
    rec.fallback = WorldGenerationFallbackKind::CompatiblePath;
    rec.allowsGpuSelection = binding.allowsGpuSelection;
    return rec;
  }

  rec.advice = WorldGenerationScaleProfileAdvice::KeepExisting;
  return rec;
}

// Deterministic integrity stamp for Integrator continuity. Not a cryptographic
// hash and not a merge/publish authorization.
inline uint64_t fingerprintWorldGenerationScaleEvidence(
    WorldGenerationScaleEvidenceBundle const& bundle) noexcept {
  auto mix = [](uint64_t h, uint64_t v) noexcept {
    h ^= v + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
    return h;
  };
  uint64_t h = 0xcbf29ce484222325ull;
  h = mix(h, WorldGenerationScaleEvidenceBundle::ModelVersion);
  h = mix(h, bundle.equivalence.generationEpoch);
  h = mix(h, bundle.equivalence.profileEpoch);
  h = mix(h, bundle.equivalence.comparatorEpoch);
  h = mix(h, static_cast<uint64_t>(bundle.probePlatform));
  h = mix(h, static_cast<uint64_t>(bundle.platformStatus));
  h = mix(h, static_cast<uint64_t>(bundle.hardwareComputeStatus));
  h = mix(h, bundle.cpuFallbackRequired ? 1u : 0u);
  h = mix(h, bundle.scaleCapabilitiesClaimed ? 1u : 0u);
  h = mix(h, bundle.scaleMeasuredGain ? 1u : 0u);
  h = mix(h, bundle.campaignCpuOnlyCovered ? 1u : 0u);
  h = mix(h, bundle.campaignHasFailures ? 1u : 0u);
  h = mix(h, bundle.gatesAdopt ? 1u : 0u);
  h = mix(h, bundle.gatesAuthoritativeGpuBlocked ? 1u : 0u);
  h = mix(h, bundle.shadowOutputComparison ? 1u : 0u);
  h = mix(h, bundle.desktopAttested ? 1u : 0u);
  h = mix(h, bundle.readyForIntegratorReview ? 1u : 0u);
  return h;
}

struct WorldGenerationScaleEvidenceManifest {
  static constexpr unsigned ModelVersion = 1;
  uint64_t fingerprint = 0;
  bool readyForIntegratorReview = false;
  bool desktopAttested = false;
  uint32_t generationEpoch = 0;
};

inline WorldGenerationScaleEvidenceManifest manifestWorldGenerationScaleEvidence(
    WorldGenerationScaleEvidenceBundle const& bundle) noexcept {
  WorldGenerationScaleEvidenceManifest manifest;
  manifest.fingerprint = fingerprintWorldGenerationScaleEvidence(bundle);
  manifest.readyForIntegratorReview = bundle.readyForIntegratorReview;
  manifest.desktopAttested = bundle.desktopAttested;
  manifest.generationEpoch = bundle.equivalence.generationEpoch;
  return manifest;
}

}
