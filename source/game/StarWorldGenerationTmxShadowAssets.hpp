#pragma once

#include "StarJson.hpp"
#include "StarMap.hpp"
#include "StarString.hpp"
#include "StarWorldGenerationTmxStaticDecode.hpp"

#include <optional>
#include <utility>

namespace Star {

// MD-02 Assets-backed shadow slice: in-memory fixture store for TMX tileset
// Json. Resolves Tiled relative sources to /tilesets/... without Root/Assets.
struct WorldGenerationTmxShadowAssetStore {
  HashMap<String, Json> entries;

  bool contains(String const& path) const { return entries.contains(path); }

  Maybe<Json> getJson(String const& path) const {
    if (auto const* entry = entries.ptr(path))
      return *entry;
    return {};
  }

  void put(String path, Json json) { entries[std::move(path)] = std::move(json); }
};

inline String worldGenerationTmxTilesetAssetPath(String const& relativePath) {
  // Keep from the last /tilesets/ segment (same rule as live TMX loading).
  size_t i = relativePath.findLast("/tilesets/", String::CaseInsensitive);
  if (i == NPos)
    return relativePath;
  return relativePath.slice(i);
}

inline uint64_t worldGenerationTmxTilesetJsonHash(Json const& tilesetJson) {
  String repr = tilesetJson.repr();
  uint64_t acc = 1469598103934665603ull;
  Star::hash<String> stringHash;
  acc ^= (uint64_t)stringHash(repr);
  acc *= 1099511628211ull;
  return acc ? acc : 1ull;
}

inline Maybe<WorldGenerationTmxStaticTilesetRef> worldGenerationTmxResolveTilesetRef(
    Json const& tilesetEntry, WorldGenerationTmxShadowAssetStore const& assets) {
  if (!tilesetEntry.contains("source") || !tilesetEntry.contains("firstgid"))
    return {}; // embedded tilesets unsupported in this shadow slice
  uint32_t firstGid = (uint32_t)tilesetEntry.getUInt("firstgid");
  if (firstGid == 0)
    return {};
  String assetPath = worldGenerationTmxTilesetAssetPath(tilesetEntry.getString("source"));
  auto json = assets.getJson(assetPath);
  if (!json)
    return {};
  if (!json->contains("tilecount") || json->getUInt("tilecount") == 0)
    return {};
  WorldGenerationTmxStaticTilesetRef ref;
  ref.firstGid = firstGid;
  ref.assetPath = std::move(assetPath);
  ref.tileCount = (uint32_t)json->getUInt("tilecount");
  ref.tilesetContentHash = worldGenerationTmxTilesetJsonHash(*json);
  return ref;
}

inline std::optional<WorldGenerationTmxStaticBlueprint> tryDecodeWorldGenerationTmxStaticBlueprintWithAssets(
    WorldGenerationTmxStaticDecodeRequest request, WorldGenerationTmxShadowAssetStore const& assets) {
  Json mapJson = request.mapJson;
  auto blueprint = tryDecodeWorldGenerationTmxStaticBlueprint(std::move(request));
  if (!blueprint)
    return std::nullopt;

  if (!mapJson.contains("tilesets"))
    return blueprint;
  if (mapJson.get("tilesets").type() != Json::Type::Array)
    return std::nullopt;

  List<WorldGenerationTmxStaticTilesetRef> refs;
  for (Json const& entry : mapJson.getArray("tilesets")) {
    if (entry.type() != Json::Type::Object)
      return std::nullopt;
    auto ref = worldGenerationTmxResolveTilesetRef(entry, assets);
    if (!ref)
      return std::nullopt;
    refs.append(std::move(*ref));
  }

  blueprint->tilesetRefs = std::move(refs);
  blueprint->identity.contentHash = worldGenerationTmxStaticContentHash(*blueprint);
  if (!blueprint->identity.valid())
    return std::nullopt;
  return blueprint;
}

inline std::optional<WorldGenerationTmxStaticProduct> tryDecodeWorldGenerationTmxStaticProductWithAssets(
    WorldGenerationTmxStaticDecodeRequest request, WorldGenerationTmxShadowAssetStore const& assets) {
  auto blueprint = tryDecodeWorldGenerationTmxStaticBlueprintWithAssets(std::move(request), assets);
  if (!blueprint)
    return std::nullopt;
  return WorldGenerationTmxStaticProduct::tryBuild(std::move(*blueprint));
}

}
