#include "StarMicroDungeon.hpp"
#include "StarTrace.hpp"
#include "StarRoot.hpp"
#include "StarInterpolation.hpp"
#include "StarLogging.hpp"
#include "StarDungeonGenerator.hpp"
#include "StarTime.hpp"

namespace Star {

// Placed on server so it can keep a cacheing system which allows for a quick
// scan if a piece fits.

MicroDungeonFactory::MicroDungeonFactory() {
  m_generating = false;

  m_placementshifts.push_back(0);
  for (int i = 1; i < 4; ++i)
    m_placementshifts.push_back(i);
  for (int i = 1; i < 4; ++i)
    m_placementshifts.push_back(-i);
}

Maybe<pair<List<RectI>, Set<Vec2I>>> MicroDungeonFactory::generate(RectI const& bounds,
    String const& dungeonName,
    Vec2I const& position,
    uint64_t seed,
    float threatLevel,
    DungeonGeneratorWorldFacadePtr facade,
    bool forcePlacement,
    GenerationTiming* timing) {
  STAR_TRACE_ZONE("MicroDungeonFactory generate");
  Dungeon::DungeonGeneratorWriter writer(facade, {}, {});

  if (m_generating)
    throw DungeonException("Not reentrant.");

  m_generating = true;
  auto generatingGuard = finally([this]() { m_generating = false; });

  double phaseStarted = timing ? Time::monotonicTime() : 0.0;
  DungeonGenerator dungeonGenerator(dungeonName, seed, threatLevel, BiomeMicroDungeonId);
  if (timing)
    timing->definitionSeconds = Time::monotonicTime() - phaseStarted;

  try {
    // don't bother scanning around because its used in a bruteforce manner for now.
    // try to stay a bit stable generation wise, maybe trash the cache after a sector is done ?

    phaseStarted = timing ? Time::monotonicTime() : 0.0;
    auto anchorPart = dungeonGenerator.pickAnchor();
    if (timing) {
      timing->anchorSeconds = Time::monotonicTime() - phaseStarted;
      timing->anchorFound = bool(anchorPart);
    }
    if (!anchorPart) {
      Logger::debug("No valid anchor piece found for microdungeon at {}, skipping", position);
      return {};
    }

    if (forcePlacement) {
      phaseStarted = timing ? Time::monotonicTime() : 0.0;
      auto result = dungeonGenerator.buildDungeon(anchorPart, position - anchorPart->anchorPoint(), &writer, forcePlacement);
      if (timing)
        timing->buildSeconds += Time::monotonicTime() - phaseStarted;
      return result;
    } else {
      for (int dy : m_placementshifts) {
        if (timing)
          ++timing->placementAttempts;
        auto pos = position - anchorPart->anchorPoint() + Vec2I{0, dy};
        if (!bounds.contains(pos) || !bounds.contains(pos + Vec2I{anchorPart->size()} - Vec2I{1, 1})) {
          if (timing)
            ++timing->boundsRejected;
          continue;
        }

        bool collision = false;
        phaseStarted = timing ? Time::monotonicTime() : 0.0;
        anchorPart->forEachTile([&](Vec2I tilePos, Dungeon::Tile const& tile) -> bool {
            if (tile.usesPlaces()) {
              if (facade->getDungeonIdAt(pos + tilePos) != NoDungeonId) {
                collision = true;
                return true;
              }
            }
            return false;
          });

        if (timing)
          timing->collisionSeconds += Time::monotonicTime() - phaseStarted;

        if (collision) {
          if (timing)
            ++timing->collisionRejected;
          continue;
        }
        phaseStarted = timing ? Time::monotonicTime() : 0.0;
        bool canPlace = anchorPart->canPlace(pos, &writer);
        if (timing)
          timing->canPlaceSeconds += Time::monotonicTime() - phaseStarted;
        if (canPlace) {
          phaseStarted = timing ? Time::monotonicTime() : 0.0;
          auto result = dungeonGenerator.buildDungeon(anchorPart, pos, &writer, forcePlacement);
          if (timing)
            timing->buildSeconds += Time::monotonicTime() - phaseStarted;
          return result;
        }
        if (timing)
          ++timing->placementRejected;
      }
    }
  } catch (std::exception const& e) {
    throw DungeonException(strf("Error generating microdungeon named '{}'", dungeonGenerator.definition()->name()), e);
  }

  return {};
}

}
