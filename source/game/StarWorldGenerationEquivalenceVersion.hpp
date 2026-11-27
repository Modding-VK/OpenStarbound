#pragma once

#include <cstdint>

namespace Star {

// First H5 / MOD-02 cut: versioned equivalence identity for comparing state
// without silently normalizing content differences. A version bump is required
// when generation math/seed semantics change; profiles alone do not bump it.

enum class WorldGenerationEquivalenceDomain : uint8_t {
  Tiles = 0,
  Liquids,
  Collision,
  Entities,
  Identities,
  Inventories,
  Promises,
  Metadata,
  Count
};

struct WorldGenerationEquivalenceVersion {
  static constexpr unsigned ModelVersion = 1;
  uint32_t generationEpoch = 0;
  uint32_t profileEpoch = 0;
  uint32_t comparatorEpoch = 0;

  bool compatibleWith(WorldGenerationEquivalenceVersion const& other) const noexcept {
    return generationEpoch == other.generationEpoch
        && comparatorEpoch == other.comparatorEpoch;
  }
};

struct WorldGenerationEquivalenceFieldMask {
  uint32_t domains = 0;

  void include(WorldGenerationEquivalenceDomain domain) noexcept {
    domains |= (1u << static_cast<uint32_t>(domain));
  }

  bool contains(WorldGenerationEquivalenceDomain domain) const noexcept {
    return (domains & (1u << static_cast<uint32_t>(domain))) != 0;
  }

  uint32_t count() const noexcept {
    uint32_t n = 0;
    for (uint32_t i = 0; i < static_cast<uint32_t>(WorldGenerationEquivalenceDomain::Count); ++i)
      n += (domains & (1u << i)) ? 1u : 0u;
    return n;
  }
};

struct WorldGenerationEquivalenceDecision {
  bool equivalent = false;
  bool versionCompatible = false;
  bool normalizationDeclared = false;
  WorldGenerationEquivalenceFieldMask compared;
  WorldGenerationEquivalenceFieldMask diverged;
};

inline WorldGenerationEquivalenceDecision compareWorldGenerationEquivalence(
    WorldGenerationEquivalenceVersion const& left,
    WorldGenerationEquivalenceVersion const& right,
    WorldGenerationEquivalenceFieldMask compared,
    WorldGenerationEquivalenceFieldMask diverged,
    bool normalizationDeclared) noexcept {
  WorldGenerationEquivalenceDecision decision;
  decision.compared = compared;
  decision.diverged = diverged;
  decision.normalizationDeclared = normalizationDeclared;
  decision.versionCompatible = left.compatibleWith(right);
  // Content divergence under an undeclared normalization is never equivalence.
  bool silentNormalize = diverged.domains != 0 && !normalizationDeclared;
  decision.equivalent = decision.versionCompatible
      && compared.domains != 0
      && diverged.domains == 0
      && !silentNormalize;
  if (silentNormalize)
    decision.equivalent = false;
  return decision;
}

inline bool worldGenerationRequiresGenerationEpochBump(
    bool mathChanged, bool seedSemanticsChanged) noexcept {
  return mathChanged || seedSemanticsChanged;
}

}
