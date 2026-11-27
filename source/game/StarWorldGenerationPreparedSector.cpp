#include "StarWorldGenerationPreparedSector.hpp"

namespace Star {

bool WorldGenerationPreparedProductIdentity::operator==(
    WorldGenerationPreparedProductIdentity const& other) const {
  return sector == other.sector && templateRevision == other.templateRevision
      && distributionRevision == other.distributionRevision
      && placementEpoch == other.placementEpoch;
}

bool WorldGenerationPreparedSectorProduct::matches(
    WorldGenerationPreparedProductIdentity const& expected) const {
  return identity == expected;
}

bool WorldGenerationPreparedSectorProduct::mutatesPersistedLevel() const {
  return false;
}

WorldGenerationPreparedSectorProduct WorldGenerationPreparedSectorStore::makeEmpty(
    WorldGenerationPreparedProductIdentity identity) {
  WorldGenerationPreparedSectorProduct product;
  product.identity = std::move(identity);
  product.kind = WorldGenerationPreparedProductKind::Empty;
  return product;
}

WorldGenerationPreparedSectorProduct WorldGenerationPreparedSectorStore::makeOrderedCandidates(
    WorldGenerationPreparedProductIdentity identity,
    List<WorldGenerationMicroDungeonCandidate> candidates) {
  WorldGenerationPreparedSectorProduct product;
  product.identity = std::move(identity);
  product.kind = WorldGenerationPreparedProductKind::OrderedCandidates;
  product.candidates = std::move(candidates);
  return product;
}

WorldGenerationPreparedProductId WorldGenerationPreparedSectorStore::put(
    WorldGenerationPreparedSectorProduct product) {
  auto id = m_nextId++;
  m_products[id] = std::move(product);
  return id;
}

Maybe<WorldGenerationPreparedSectorProduct> WorldGenerationPreparedSectorStore::get(
    WorldGenerationPreparedProductId id) const {
  auto it = m_products.find(id);
  if (it == m_products.end())
    return {};
  return it->second;
}

bool WorldGenerationPreparedSectorStore::contains(WorldGenerationPreparedProductId id) const {
  return m_products.contains(id);
}

bool WorldGenerationPreparedSectorStore::erase(WorldGenerationPreparedProductId id) {
  return m_products.remove(id);
}

size_t WorldGenerationPreparedSectorStore::size() const {
  return m_products.size();
}

WorldGenerationPreparedProductId WorldGenerationPreparedSectorStore::shareOrPut(
    WorldGenerationPreparedSectorProduct product) {
  for (auto const& entry : m_products) {
    if (entry.second.kind == product.kind && entry.second.identity == product.identity)
      return entry.first;
  }
  return put(std::move(product));
}

size_t WorldGenerationPreparedSectorStore::invalidateStale(
    WorldGenerationPreparedProductIdentity const& live) {
  List<WorldGenerationPreparedProductId> stale;
  for (auto const& entry : m_products) {
    if (entry.second.identity.sector != live.sector)
      continue;
    if (!entry.second.matches(live))
      stale.append(entry.first);
  }
  for (auto id : stale)
    m_products.remove(id);
  return stale.size();
}

}
