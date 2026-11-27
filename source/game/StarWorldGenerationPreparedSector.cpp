#include "StarWorldGenerationPreparedSector.hpp"

namespace Star {

bool WorldGenerationPreparedProductIdentity::operator==(
    WorldGenerationPreparedProductIdentity const& other) const {
  return sector == other.sector && templateRevision == other.templateRevision
      && distributionRevision == other.distributionRevision
      && placementEpoch == other.placementEpoch;
}

bool WorldGenerationPreparedWorldStoragePath::operator==(
    WorldGenerationPreparedWorldStoragePath const& other) const {
  return worldEpoch == other.worldEpoch && sector == other.sector && productId == other.productId
      && kind == other.kind;
}

bool WorldGenerationPreparedSectorProduct::matches(
    WorldGenerationPreparedProductIdentity const& expected) const {
  return identity == expected;
}

bool WorldGenerationPreparedSectorProduct::mutatesPersistedLevel() const {
  return false;
}

bool WorldGenerationPreparedSectorProduct::canInstall() const {
  if (kind != WorldGenerationPreparedProductKind::BaseTilesPrivate)
    return false;
  return baseTiles && baseTiles->installAuthorized && baseTiles->ticket.valid();
}

uint64_t WorldGenerationPreparedSectorStore::hashIdentity(
    WorldGenerationPreparedProductIdentity const& identity) {
  uint64_t h = 14695981039346656037ull;
  auto mix = [&](uint64_t v) {
    h ^= v;
    h *= 1099511628211ull;
  };
  mix((uint64_t)(uint32_t)identity.sector[0]);
  mix((uint64_t)(uint32_t)identity.sector[1]);
  mix(identity.templateRevision);
  mix(identity.distributionRevision);
  mix(identity.placementEpoch);
  return h;
}

WorldGenerationPreparedPayloadView WorldGenerationPreparedSectorStore::buildPayloadView(
    WorldGenerationPreparedProductId id, WorldGenerationPreparedSectorProduct const& product) {
  WorldGenerationPreparedPayloadView view;
  view.productId = id;
  view.kind = product.kind;
  view.identity = product.identity;
  view.installAuthorized = product.canInstall();
  switch (product.kind) {
    case WorldGenerationPreparedProductKind::Empty:
      view.payloadBytes = 0;
      view.contentToken = hashIdentity(product.identity);
      break;
    case WorldGenerationPreparedProductKind::OrderedCandidates:
      view.payloadBytes = product.candidates.size() * sizeof(WorldGenerationPlacementId);
      view.contentToken = hashIdentity(product.identity) ^ (uint64_t)product.candidates.size();
      break;
    case WorldGenerationPreparedProductKind::BaseTilesPrivate:
      if (product.baseTiles) {
        view.payloadBytes = product.baseTiles->retainedBytes;
        view.contentToken = hashIdentity(product.identity) ^ product.baseTiles->ticket.tileRevision
            ^ (product.baseTiles->ticket.templateRevision << 1);
        view.installAuthorized = product.baseTiles->installAuthorized;
      }
      break;
    case WorldGenerationPreparedProductKind::TerrainFields:
    case WorldGenerationPreparedProductKind::StepDelta:
      view.payloadBytes = product.opaquePayloadBytes;
      view.contentToken = product.opaqueContentToken ? product.opaqueContentToken
                                                     : hashIdentity(product.identity);
      break;
  }
  return view;
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

WorldGenerationPreparedSectorProduct WorldGenerationPreparedSectorStore::makeBaseTilesPrivate(
    WorldGenerationPreparedProductIdentity identity,
    WorldGenerationBaseTilesIdentity ticket,
    size_t retainedBytes) {
  WorldGenerationPreparedSectorProduct product;
  product.identity = std::move(identity);
  product.kind = WorldGenerationPreparedProductKind::BaseTilesPrivate;
  WorldGenerationPreparedBaseTilesRef ref;
  ref.ticket = std::move(ticket);
  ref.retainedBytes = retainedBytes;
  ref.installAuthorized = false;
  product.baseTiles = std::move(ref);
  return product;
}

WorldGenerationPreparedSectorProduct WorldGenerationPreparedSectorStore::makeTerrainFields(
    WorldGenerationPreparedProductIdentity identity, uint64_t contentToken, size_t payloadBytes) {
  WorldGenerationPreparedSectorProduct product;
  product.identity = std::move(identity);
  product.kind = WorldGenerationPreparedProductKind::TerrainFields;
  product.opaqueContentToken = contentToken;
  product.opaquePayloadBytes = payloadBytes;
  return product;
}

WorldGenerationPreparedSectorProduct WorldGenerationPreparedSectorStore::makeStepDelta(
    WorldGenerationPreparedProductIdentity identity, uint64_t contentToken, size_t payloadBytes) {
  WorldGenerationPreparedSectorProduct product;
  product.identity = std::move(identity);
  product.kind = WorldGenerationPreparedProductKind::StepDelta;
  product.opaqueContentToken = contentToken;
  product.opaquePayloadBytes = payloadBytes;
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

Maybe<WorldGenerationPreparedPayloadView> WorldGenerationPreparedSectorStore::payloadView(
    WorldGenerationPreparedProductId id) const {
  auto it = m_products.find(id);
  if (it == m_products.end())
    return {};
  return buildPayloadView(id, it->second);
}

WorldGenerationPreparedWorldStoragePath WorldGenerationPreparedSectorStore::worldStoragePath(
    uint64_t worldEpoch, WorldGenerationPreparedProductId id) const {
  WorldGenerationPreparedWorldStoragePath path;
  path.worldEpoch = worldEpoch;
  path.productId = id;
  auto it = m_products.find(id);
  if (it != m_products.end()) {
    path.sector = it->second.identity.sector;
    path.kind = it->second.kind;
  }
  return path;
}

WorldGenerationPreparedHandoff WorldGenerationPreparedSectorStore::probeHandoff(uint64_t worldEpoch,
    WorldGenerationPreparedProductId id, WorldGenerationPreparedProductIdentity const& liveIdentity) const {
  WorldGenerationPreparedHandoff handoff;
  handoff.path = worldStoragePath(worldEpoch, id);
  auto it = m_products.find(id);
  if (it == m_products.end()) {
    handoff.outcome = WorldGenerationPreparedHandoffOutcome::MissingProduct;
    return handoff;
  }
  handoff.payload = buildPayloadView(id, it->second);
  if (!it->second.matches(liveIdentity)) {
    handoff.outcome = WorldGenerationPreparedHandoffOutcome::StaleIdentity;
    return handoff;
  }
  if (it->second.kind == WorldGenerationPreparedProductKind::BaseTilesPrivate
      && !it->second.canInstall()) {
    handoff.outcome = WorldGenerationPreparedHandoffOutcome::NotAuthorized;
    return handoff;
  }
  if (it->second.kind != WorldGenerationPreparedProductKind::Empty
      && handoff.payload.payloadBytes == 0
      && it->second.kind != WorldGenerationPreparedProductKind::OrderedCandidates) {
    handoff.outcome = WorldGenerationPreparedHandoffOutcome::EmptyPayload;
    return handoff;
  }
  // Empty proof and ordered candidates are Ready without WorldStorage writes.
  // BaseTilesPrivate Ready only when installAuthorized (not in default shadow).
  if (it->second.kind == WorldGenerationPreparedProductKind::BaseTilesPrivate) {
    handoff.outcome = WorldGenerationPreparedHandoffOutcome::Ready;
    handoff.writesWorldStorage = false; // probe never writes
    return handoff;
  }
  handoff.outcome = WorldGenerationPreparedHandoffOutcome::Ready;
  handoff.writesWorldStorage = false;
  return handoff;
}

}
