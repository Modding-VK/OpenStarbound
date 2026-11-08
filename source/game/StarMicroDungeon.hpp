#pragma once

#include "StarRect.hpp"
#include "StarString.hpp"

namespace Star {

STAR_CLASS(DungeonGeneratorWorldFacade);
STAR_CLASS(MicroDungeonFactory);

class MicroDungeonFactory {
public:
  struct GenerationTiming {
    double definitionSeconds = 0;
    double anchorSeconds = 0;
    double collisionSeconds = 0;
    double canPlaceSeconds = 0;
    double buildSeconds = 0;
    uint32_t placementAttempts = 0;
    uint32_t boundsRejected = 0;
    uint32_t collisionRejected = 0;
    uint32_t placementRejected = 0;
    bool anchorFound = false;
  };

  MicroDungeonFactory();

  Maybe<pair<List<RectI>, Set<Vec2I>>> generate(RectI const& bounds,
      String const& dungeonName,
      Vec2I const& position,
      uint64_t seed,
      float threatLevel,
      DungeonGeneratorWorldFacadePtr facade,
      bool forcePlacement = false,
      GenerationTiming* timing = nullptr);

private:
  List<int> m_placementshifts;
  bool m_generating;
};

}
