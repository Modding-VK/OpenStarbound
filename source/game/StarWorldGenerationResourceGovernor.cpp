#include "StarMathCommon.hpp"
#include "StarWorldGenerationResourceGovernor.hpp"

namespace Star {

WorldGenerationResourceGovernor::WorldGenerationResourceGovernor(WorldGenerationResourceProfile profile)
  : m_profile(profile) {}

WorldGenerationResourceProfile WorldGenerationResourceGovernor::profile() const {
  return m_profile;
}

void WorldGenerationResourceGovernor::setProfile(WorldGenerationResourceProfile profile) {
  m_profile = profile;
}

WorldGenerationExecutivePassBudget WorldGenerationResourceGovernor::suggest(
    WorldGenerationResourceObservation const& observation) const {
  WorldGenerationExecutivePassBudget budget;
  size_t baseStarts = m_profile == WorldGenerationResourceProfile::HighThroughput ? 4 : 1;
  size_t drainBase = m_profile == WorldGenerationResourceProfile::HighThroughput ? 64 : 16;

  // Prefer draining ready completions before starting more work.
  budget.maxDrainMessages = observation.mailboxDepth == 0
      ? drainBase
      : min(drainBase, max<size_t>(observation.mailboxDepth, 1));

  size_t starts = min(baseStarts, observation.executableFrontier);
  if (observation.targetPassTicks != 0 && observation.lastPassTicksSpent > observation.targetPassTicks)
    starts = min<size_t>(starts, 1);
  if (observation.retainedProducts > 32)
    starts = min<size_t>(starts, 1);
  budget.maxStarts = starts;
  return budget;
}

}
