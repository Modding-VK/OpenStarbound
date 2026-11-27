#pragma once

#include "StarGameTypes.hpp"
#include "StarJson.hpp"
#include "StarWorldGenerationTmxStaticProduct.hpp"

#include <algorithm>
#include <optional>
#include <utility>

namespace Star {

// MD-02 decode slice: CPU-only Json TMX → static blueprint/product. Accepts
// controlled in-memory Json (tilelayer array data + objectgroups). Does not
// load tilesets via Root/Assets and does not materialize brushes.
struct WorldGenerationTmxStaticDecodeRequest {
  String assetKey;
  uint64_t assetGeneration = 0;
  Json mapJson;
};

inline JsonObject worldGenerationTmxNormalizeProperties(Json const& properties) {
  JsonObject out;
  if (properties.type() == Json::Type::Array) {
    for (auto const& entry : properties.toArray()) {
      if (entry.contains("name"))
        out.set(entry.getString("name"), entry.get("value"));
    }
  } else if (properties.type() == Json::Type::Object) {
    out = properties.toObject();
  }
  return out;
}

inline Maybe<WorldGenerationTmxStaticLayer> worldGenerationTmxDecodeTileLayer(Json const& layer) {
  if (layer.getString("type", "") != "tilelayer")
    return {};
  if (layer.contains("compression"))
    return {}; // compressed payloads need Assets/codec path — out of this slice
  unsigned width = (unsigned)layer.getUInt("width");
  unsigned height = (unsigned)layer.getUInt("height");
  if (width == 0 || height == 0)
    return {};
  if (!layer.contains("data") || layer.get("data").type() != Json::Type::Array)
    return {};

  WorldGenerationTmxStaticLayer out;
  out.name = layer.getString("name", "layer");
  String lowerName = out.name.toLower();
  out.kind = (lowerName == "back" || lowerName == "background")
      ? WorldGenerationTmxStaticLayerKind::Background
      : WorldGenerationTmxStaticLayerKind::Foreground;
  out.width = width;
  out.height = height;
  out.gids.reserve(width * height);
  for (Json const& cell : layer.getArray("data"))
    out.gids.append((uint32_t)cell.toUInt());
  if (!out.valid())
    return {};
  return out;
}

inline WorldGenerationTmxStaticObject worldGenerationTmxDecodeObject(Json const& objectJson) {
  WorldGenerationTmxStaticObject out;
  out.name = objectJson.getString("name", objectJson.getString("type", "object"));
  // Tiled object x/y/width/height are in pixels; Starbound tiles use TilePixels.
  int x = (int)objectJson.getInt("x", 0);
  int y = (int)objectJson.getInt("y", 0);
  int w = (int)objectJson.getInt("width", (int)TilePixels);
  int h = (int)objectJson.getInt("height", (int)TilePixels);
  out.position = Vec2I(x / (int)TilePixels, y / (int)TilePixels);
  out.size = Vec2I(std::max(1, w / (int)TilePixels), std::max(1, h / (int)TilePixels));

  JsonObject props;
  if (objectJson.contains("properties"))
    props = worldGenerationTmxNormalizeProperties(objectJson.get("properties"));
  if (props.contains("connector") && props.get("connector").toBool()) {
    out.connector = true;
    out.connectorDirection = props.contains("direction") ? props.get("direction").toString() : String();
  } else if (props.contains("direction")) {
    // Direction-only markers still count as connectors in the static product.
    out.connector = true;
    out.connectorDirection = props.get("direction").toString();
  }
  return out;
}

inline std::optional<WorldGenerationTmxStaticBlueprint> tryDecodeWorldGenerationTmxStaticBlueprint(
    WorldGenerationTmxStaticDecodeRequest request) {
  if (request.assetKey.empty() || request.assetGeneration == 0)
    return std::nullopt;
  if (!request.mapJson.isType(Json::Type::Object))
    return std::nullopt;

  Json const& tmx = request.mapJson;
  if (tmx.getUInt("tilewidth", 0) != TilePixels || tmx.getUInt("tileheight", 0) != TilePixels)
    return std::nullopt;
  if (!tmx.contains("layers") || tmx.get("layers").type() != Json::Type::Array)
    return std::nullopt;

  WorldGenerationTmxStaticBlueprint blueprint;
  blueprint.identity.assetKey = std::move(request.assetKey);
  blueprint.identity.assetGeneration = request.assetGeneration;

  for (Json const& layerJson : tmx.getArray("layers")) {
    String layerType = layerJson.getString("type", "");
    if (layerType == "tilelayer") {
      auto layer = worldGenerationTmxDecodeTileLayer(layerJson);
      if (!layer)
        return std::nullopt;
      blueprint.layers.append(std::move(*layer));
    } else if (layerType == "objectgroup") {
      if (!layerJson.contains("objects") || layerJson.get("objects").type() != Json::Type::Array)
        return std::nullopt;
      for (Json const& objectJson : layerJson.getArray("objects"))
        blueprint.objects.append(worldGenerationTmxDecodeObject(objectJson));
    } else {
      return std::nullopt;
    }
  }

  if (blueprint.layers.empty())
    return std::nullopt;
  blueprint.identity.contentHash = worldGenerationTmxStaticContentHash(blueprint);
  if (!blueprint.identity.valid())
    return std::nullopt;
  return blueprint;
}

inline std::optional<WorldGenerationTmxStaticProduct> tryDecodeWorldGenerationTmxStaticProduct(
    WorldGenerationTmxStaticDecodeRequest request) {
  auto blueprint = tryDecodeWorldGenerationTmxStaticBlueprint(std::move(request));
  if (!blueprint)
    return std::nullopt;
  return WorldGenerationTmxStaticProduct::tryBuild(std::move(*blueprint));
}

}
