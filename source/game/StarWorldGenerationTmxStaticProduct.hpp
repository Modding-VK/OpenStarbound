#pragma once

#include "StarGameTypes.hpp"
#include "StarHash.hpp"
#include "StarList.hpp"
#include "StarString.hpp"
#include "StarVector.hpp"

#include <cstdint>
#include <optional>
#include <utility>

namespace Star {

// MD-02 first slice: private static TMX intermediate product. Holds decoded
// layer grids, object footprints and connectors without RandomBrush/NpcBrush
// binding or facade writes. Owner supplies already-resolved map bytes/fields.
struct WorldGenerationTmxStaticIdentity {
  static constexpr unsigned ModelVersion = 1;
  static constexpr unsigned FormatVersion = 1;
  String assetKey;
  uint64_t assetGeneration = 0;
  uint64_t contentHash = 0;
  unsigned formatVersion = FormatVersion;
  unsigned productVersion = ModelVersion;

  bool valid() const {
    return productVersion == ModelVersion
        && formatVersion == FormatVersion
        && !assetKey.empty()
        && assetGeneration != 0
        && contentHash != 0;
  }

  bool operator==(WorldGenerationTmxStaticIdentity const& other) const {
    return assetKey == other.assetKey
        && assetGeneration == other.assetGeneration
        && contentHash == other.contentHash
        && formatVersion == other.formatVersion
        && productVersion == other.productVersion;
  }

  bool matches(WorldGenerationTmxStaticIdentity const& expectation) const {
    return valid() && expectation.valid() && *this == expectation;
  }
};

enum class WorldGenerationTmxStaticLayerKind : uint8_t {
  Foreground = 0,
  Background = 1
};

struct WorldGenerationTmxStaticLayer {
  String name;
  WorldGenerationTmxStaticLayerKind kind = WorldGenerationTmxStaticLayerKind::Foreground;
  unsigned width = 0;
  unsigned height = 0;
  // Row-major raw GIDs as stored in the map (flip bits preserved).
  List<uint32_t> gids;

  bool valid() const {
    return width != 0 && height != 0 && gids.size() == (size_t)width * (size_t)height;
  }
};

struct WorldGenerationTmxStaticObject {
  String name;
  Vec2I position;
  Vec2I size{1, 1};
  bool connector = false;
  String connectorDirection;
};

struct WorldGenerationTmxStaticConnector {
  String name;
  Vec2I position;
  String direction;
};

struct WorldGenerationTmxStaticTilesetRef {
  uint32_t firstGid = 0;
  String assetPath;
  uint32_t tileCount = 0;
  uint64_t tilesetContentHash = 0;

  bool valid() const {
    return firstGid != 0 && !assetPath.empty() && tileCount != 0 && tilesetContentHash != 0;
  }

  bool operator==(WorldGenerationTmxStaticTilesetRef const& other) const {
    return firstGid == other.firstGid && assetPath == other.assetPath
        && tileCount == other.tileCount && tilesetContentHash == other.tilesetContentHash;
  }
};

struct WorldGenerationTmxStaticBlueprint {
  WorldGenerationTmxStaticIdentity identity;
  List<WorldGenerationTmxStaticLayer> layers;
  List<WorldGenerationTmxStaticObject> objects;
  List<WorldGenerationTmxStaticTilesetRef> tilesetRefs;
};

inline uint64_t worldGenerationTmxStaticContentHash(WorldGenerationTmxStaticBlueprint const& blueprint) {
  uint64_t acc = 1469598103934665603ull;
  auto mix = [&](uint64_t value) {
    acc ^= value;
    acc *= 1099511628211ull;
  };
  Star::hash<String> stringHash;
  for (auto const& layer : blueprint.layers) {
    mix((uint64_t)layer.kind);
    mix(layer.width);
    mix(layer.height);
    mix((uint64_t)stringHash(layer.name));
    for (auto gid : layer.gids)
      mix(gid);
  }
  for (auto const& object : blueprint.objects) {
    mix((uint64_t)stringHash(object.name));
    mix((uint64_t)(uint32_t)object.position[0]);
    mix((uint64_t)(uint32_t)object.position[1]);
    mix((uint64_t)(uint32_t)object.size[0]);
    mix((uint64_t)(uint32_t)object.size[1]);
    mix(object.connector ? 1ull : 0ull);
    mix((uint64_t)stringHash(object.connectorDirection));
  }
  for (auto const& ref : blueprint.tilesetRefs) {
    mix(ref.firstGid);
    mix((uint64_t)stringHash(ref.assetPath));
    mix(ref.tileCount);
    mix(ref.tilesetContentHash);
  }
  return acc ? acc : 1ull;
}

// Private static product. tryConsume never materializes brushes or writes tiles.
class WorldGenerationTmxStaticProduct {
public:
  static std::optional<WorldGenerationTmxStaticProduct> tryBuild(WorldGenerationTmxStaticBlueprint blueprint) {
    if (blueprint.layers.empty())
      return std::nullopt;
    for (auto const& layer : blueprint.layers) {
      if (!layer.valid())
        return std::nullopt;
    }
    if (blueprint.identity.contentHash == 0)
      blueprint.identity.contentHash = worldGenerationTmxStaticContentHash(blueprint);
    if (!blueprint.identity.valid())
      return std::nullopt;

    for (auto const& ref : blueprint.tilesetRefs) {
      if (!ref.valid())
        return std::nullopt;
    }
    List<WorldGenerationTmxStaticConnector> connectors;
    for (auto const& object : blueprint.objects) {
      if (object.connector)
        connectors.append({object.name, object.position, object.connectorDirection});
    }
    return WorldGenerationTmxStaticProduct(std::move(blueprint.identity), std::move(blueprint.layers),
        std::move(blueprint.objects), std::move(connectors), std::move(blueprint.tilesetRefs));
  }

  WorldGenerationTmxStaticIdentity const& identity() const { return m_identity; }
  List<WorldGenerationTmxStaticLayer> const& layers() const { return m_layers; }
  List<WorldGenerationTmxStaticObject> const& objects() const { return m_objects; }
  List<WorldGenerationTmxStaticConnector> const& connectors() const { return m_connectors; }
  List<WorldGenerationTmxStaticTilesetRef> const& tilesetRefs() const { return m_tilesetRefs; }
  bool active() const { return m_active; }

  size_t retainedBytesEstimate() const {
    size_t bytes = sizeof(*this);
    for (auto const& layer : m_layers)
      bytes += layer.gids.size() * sizeof(uint32_t);
    bytes += m_objects.size() * sizeof(WorldGenerationTmxStaticObject);
    bytes += m_connectors.size() * sizeof(WorldGenerationTmxStaticConnector);
    bytes += m_tilesetRefs.size() * sizeof(WorldGenerationTmxStaticTilesetRef);
    return bytes;
  }

  bool matches(WorldGenerationTmxStaticIdentity const& expectation) const {
    return active() && m_identity.matches(expectation);
  }

  // Owner revalidation only. Does not bind RNG or publish through a facade.
  bool tryConsume(WorldGenerationTmxStaticIdentity const& liveExpectation) {
    if (!active() || !m_identity.matches(liveExpectation))
      return false;
    return true;
  }

  void discard() {
    m_active = false;
    m_layers.clear();
    m_objects.clear();
    m_connectors.clear();
    m_tilesetRefs.clear();
    m_identity = {};
  }

private:
  WorldGenerationTmxStaticProduct(WorldGenerationTmxStaticIdentity identity,
      List<WorldGenerationTmxStaticLayer> layers,
      List<WorldGenerationTmxStaticObject> objects,
      List<WorldGenerationTmxStaticConnector> connectors,
      List<WorldGenerationTmxStaticTilesetRef> tilesetRefs)
    : m_identity(std::move(identity))
    , m_layers(std::move(layers))
    , m_objects(std::move(objects))
    , m_connectors(std::move(connectors))
    , m_tilesetRefs(std::move(tilesetRefs))
    , m_active(true) {}

  WorldGenerationTmxStaticIdentity m_identity;
  List<WorldGenerationTmxStaticLayer> m_layers;
  List<WorldGenerationTmxStaticObject> m_objects;
  List<WorldGenerationTmxStaticConnector> m_connectors;
  List<WorldGenerationTmxStaticTilesetRef> m_tilesetRefs;
  bool m_active = false;
};

}
