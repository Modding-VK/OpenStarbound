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
    double definitionCacheLookupSeconds = 0;
    double definitionPathResolveSeconds = 0;
    double definitionReadAndBuildSeconds = 0;
    double definitionAssetJsonSeconds = 0;
    double definitionMetadataSeconds = 0;
    double definitionTilesetSeconds = 0;
    double definitionPartsSeconds = 0;
    double definitionImagePartsSeconds = 0;
    double definitionTmxPartsSeconds = 0;
    double definitionTmxAssetJsonSeconds = 0;
    double definitionTmxMapSeconds = 0;
    double definitionTmxTilesetsSeconds = 0;
    double definitionTmxTileLayersSeconds = 0;
    double definitionTmxObjectGroupsSeconds = 0;
    size_t definitionCacheEntries = 0;
    size_t definitionCacheCapacity = 0;
    uint32_t definitionImagePartCount = 0;
    uint32_t definitionTmxPartCount = 0;
    uint32_t definitionTmxMapCount = 0;
    uint32_t definitionTmxTileLayerCount = 0;
    uint32_t definitionTmxObjectGroupCount = 0;
    double anchorSeconds = 0;
    double collisionSeconds = 0;
    double canPlaceSeconds = 0;
    double buildSeconds = 0;
    uint32_t placementAttempts = 0;
    uint32_t boundsRejected = 0;
    uint32_t collisionRejected = 0;
    uint32_t placementRejected = 0;
    bool definitionCacheHit = false;
    bool definitionCacheMiss = false;
    bool definitionCacheEvicted = false;
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
