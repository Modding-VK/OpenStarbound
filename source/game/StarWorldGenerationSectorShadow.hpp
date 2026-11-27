#pragma once

#include "StarWorldGenerationBaseTiles.hpp"
#include "StarWorldGenerationCompositeContext.hpp"
#include "StarWorldGenerationMemory.hpp"

#include <optional>
#include <utility>

namespace Star {

// Owner-thread private BaseTiles sector shadow. Admits array bytes against a
// validated product identity taken from a composite capture (or an equivalent
// ticket), never installs into the live world, and releases on discard or
// destruction. Not a job, worker handoff, mailbox entry, or commit authority.
class WorldGenerationSectorShadow {
public:
  WorldGenerationSectorShadow(WorldGenerationSectorShadow const&) = delete;
  WorldGenerationSectorShadow& operator=(WorldGenerationSectorShadow const&) = delete;
  WorldGenerationSectorShadow(WorldGenerationSectorShadow&&) = default;
  WorldGenerationSectorShadow& operator=(WorldGenerationSectorShadow&&) = default;

  static constexpr size_t retainedBytes() { return sizeof(ServerTileSectorArray::Array); }

  // Validate identity first. Refused admission allocates neither the tracked
  // reservation nor the private array. The composite overload reuses the
  // BaseTiles identity already composed with selectors/static inputs.
  static std::optional<WorldGenerationSectorShadow> tryAdmit(
      WorldGenerationMemoryLedger& ledger, WorldGenerationBaseTilesIdentity identity) {
    if (!identity.valid())
      return std::nullopt;
    auto tiles = makeWorldGenerationTrackedResource<ServerTileSectorArray::ArrayPtr>(
        ledger, WorldGenerationMemoryClass::BaseTilesArray, retainedBytes());
    if (!tiles)
      return std::nullopt;
    tiles->value = std::make_unique<ServerTileSectorArray::Array>();
    if (!tiles->value)
      return std::nullopt;
    return WorldGenerationSectorShadow(std::move(identity), std::move(tiles));
  }

  static std::optional<WorldGenerationSectorShadow> tryAdmit(
      WorldGenerationMemoryLedger& ledger, WorldGenerationCompositeContext const& composite) {
    return tryAdmit(ledger, composite.baseTilesIdentity());
  }

  WorldGenerationBaseTilesIdentity const& identity() const { return m_identity; }

  ServerTileSectorArray::Array& tiles() { return *m_tiles->value; }
  ServerTileSectorArray::Array const& tiles() const { return *m_tiles->value; }

  bool active() const { return m_tiles && m_tiles->value; }

  // Owner revalidation only: equal identity while the private array is held.
  // Not permission to publish or a read lease on the live sector.
  bool matches(WorldGenerationBaseTilesIdentity const& expectation) const {
    return active() && expectation.matches(m_identity);
  }

  // Explicit discard without publication. Destruction does the same.
  void discard() {
    m_tiles.reset();
    m_identity = {};
  }

private:
  WorldGenerationSectorShadow(WorldGenerationBaseTilesIdentity identity,
      std::shared_ptr<WorldGenerationTrackedResource<ServerTileSectorArray::ArrayPtr>> tiles)
    : m_identity(std::move(identity)), m_tiles(std::move(tiles)) {}

  WorldGenerationBaseTilesIdentity m_identity;
  std::shared_ptr<WorldGenerationTrackedResource<ServerTileSectorArray::ArrayPtr>> m_tiles;
};

}
