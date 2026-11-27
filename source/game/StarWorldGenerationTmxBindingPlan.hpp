#pragma once

#include "StarList.hpp"
#include "StarWorldGenerationTmxStaticProduct.hpp"

#include <cstdint>
#include <optional>
#include <utility>

namespace Star {

// MD-03 first slice: authoritative-order binding plan over a static TMX
// product. Records RandomBrush/NpcBrush choice identities without consuming
// live RNG streams or writing through a dungeon facade.
enum class WorldGenerationTmxBindingKind : uint8_t {
  RandomBrush = 0,
  NpcBrush = 1,
  ConnectorPick = 2
};

struct WorldGenerationTmxBindingChoice {
  WorldGenerationTmxBindingKind kind = WorldGenerationTmxBindingKind::RandomBrush;
  Vec2I position;
  uint64_t sequence = 0;
  uint64_t choiceId = 0;
  String label;

  bool operator==(WorldGenerationTmxBindingChoice const& other) const {
    return kind == other.kind
        && position == other.position
        && sequence == other.sequence
        && choiceId == other.choiceId
        && label == other.label;
  }
};

struct WorldGenerationTmxBindingPlanIdentity {
  static constexpr unsigned ModelVersion = 1;
  static constexpr unsigned ProductVersion = 1;
  WorldGenerationTmxStaticIdentity staticIdentity;
  uint64_t bindingSeed = 0;
  unsigned productVersion = ProductVersion;

  bool valid() const {
    return productVersion == ProductVersion
        && staticIdentity.valid()
        && bindingSeed != 0;
  }

  bool operator==(WorldGenerationTmxBindingPlanIdentity const& other) const {
    return staticIdentity == other.staticIdentity
        && bindingSeed == other.bindingSeed
        && productVersion == other.productVersion;
  }

  bool matches(WorldGenerationTmxBindingPlanIdentity const& expectation) const {
    return valid() && expectation.valid() && *this == expectation;
  }
};

struct WorldGenerationTmxBindingRequest {
  WorldGenerationTmxBindingKind kind = WorldGenerationTmxBindingKind::RandomBrush;
  Vec2I position;
  String label;
  // Owner-precomputed option count and selected index for this request. The
  // plan records the choice; it does not draw from Random.
  uint32_t optionCount = 0;
  uint32_t selectedIndex = 0;
};

inline List<WorldGenerationTmxBindingChoice> planWorldGenerationTmxBindingChoices(
    List<WorldGenerationTmxBindingRequest> const& requests) {
  List<WorldGenerationTmxBindingChoice> choices;
  uint64_t sequence = 0;
  for (auto const& request : requests) {
    if (request.optionCount == 0 || request.selectedIndex >= request.optionCount)
      continue;
    ++sequence;
    WorldGenerationTmxBindingChoice choice;
    choice.kind = request.kind;
    choice.position = request.position;
    choice.sequence = sequence;
    choice.choiceId = ((uint64_t)request.kind << 32) | request.selectedIndex;
    choice.label = request.label;
    choices.append(std::move(choice));
  }
  return choices;
}

// Private binding plan. tryConsume revalidates only; never spawns NPCs/objects.
class WorldGenerationTmxBindingPlan {
public:
  static std::optional<WorldGenerationTmxBindingPlan> tryPlan(
      WorldGenerationTmxBindingPlanIdentity identity,
      WorldGenerationTmxStaticProduct const& staticProduct,
      List<WorldGenerationTmxBindingRequest> const& requests) {
    if (!identity.valid() || !staticProduct.active())
      return std::nullopt;
    if (!staticProduct.matches(identity.staticIdentity))
      return std::nullopt;
    auto choices = planWorldGenerationTmxBindingChoices(requests);
    return WorldGenerationTmxBindingPlan(std::move(identity), std::move(choices));
  }

  WorldGenerationTmxBindingPlanIdentity const& identity() const { return m_identity; }
  List<WorldGenerationTmxBindingChoice> const& choices() const { return m_choices; }
  bool active() const { return m_active; }

  bool matches(WorldGenerationTmxBindingPlanIdentity const& expectation) const {
    return active() && m_identity.matches(expectation);
  }

  bool tryConsume(WorldGenerationTmxBindingPlanIdentity const& liveExpectation) {
    if (!active() || !m_identity.matches(liveExpectation))
      return false;
    return true;
  }

  void discard() {
    m_active = false;
    m_choices.clear();
    m_identity = {};
  }

private:
  WorldGenerationTmxBindingPlan(WorldGenerationTmxBindingPlanIdentity identity,
      List<WorldGenerationTmxBindingChoice> choices)
    : m_identity(std::move(identity)), m_choices(std::move(choices)), m_active(true) {}

  WorldGenerationTmxBindingPlanIdentity m_identity;
  List<WorldGenerationTmxBindingChoice> m_choices;
  bool m_active = false;
};

}
