#pragma once

#include "StarList.hpp"
#include "StarVector.hpp"

#include <cstdint>
#include <optional>
#include <utility>

namespace Star {

// IO-01: one decode ticket can upgrade Prefetch→Visible without a second
// producer for the same bytes. Promotion is owner-side validation only; this
// type never touches WorldStorage or installs tiles.
enum class WorldGenerationDecodeTargetLoad : uint8_t { Prefetch = 0, Visible = 1 };

enum class WorldGenerationDecodePromotionState : uint8_t {
  PendingDecode = 0,
  Decoded = 1,
  Promoted = 2,
  Cancelled = 3,
  Stale = 4
};

struct WorldGenerationDecodePromotionIdentity {
  uint64_t worldEpoch = 0;
  uint64_t storageEpoch = 0;
  Vec2U sector{};
  uint64_t contentVersion = 0;
  uint64_t requestTicket = 0;

  bool operator==(WorldGenerationDecodePromotionIdentity const& other) const {
    return worldEpoch == other.worldEpoch
        && storageEpoch == other.storageEpoch
        && sector == other.sector
        && contentVersion == other.contentVersion
        && requestTicket == other.requestTicket;
  }
};

class WorldGenerationDecodePromotionTicket {
public:
  WorldGenerationDecodePromotionTicket(WorldGenerationDecodePromotionIdentity identity,
      WorldGenerationDecodeTargetLoad target)
    : m_identity(std::move(identity)), m_target(target) {}

  WorldGenerationDecodePromotionIdentity const& identity() const { return m_identity; }
  WorldGenerationDecodeTargetLoad target() const { return m_target; }
  WorldGenerationDecodePromotionState state() const { return m_state; }
  bool hasDecodedBytes() const { return m_decodedBytes != 0; }
  size_t decodedBytes() const { return m_decodedBytes; }

  // Same in-flight identity upgrades Prefetch→Visible without a new decode.
  bool upgradeToVisible() {
    if (m_state == WorldGenerationDecodePromotionState::Cancelled
        || m_state == WorldGenerationDecodePromotionState::Stale
        || m_state == WorldGenerationDecodePromotionState::Promoted)
      return false;
    m_target = WorldGenerationDecodeTargetLoad::Visible;
    return true;
  }

  bool completeDecode(size_t bytes) {
    if (bytes == 0 || m_state != WorldGenerationDecodePromotionState::PendingDecode)
      return false;
    m_decodedBytes = bytes;
    m_state = WorldGenerationDecodePromotionState::Decoded;
    return true;
  }

  // Owner validates live epochs/content before install. Mismatch → Stale.
  bool tryPromote(WorldGenerationDecodePromotionIdentity const& live) {
    if (m_state != WorldGenerationDecodePromotionState::Decoded)
      return false;
    if (!(live == m_identity) || m_target != WorldGenerationDecodeTargetLoad::Visible) {
      m_state = WorldGenerationDecodePromotionState::Stale;
      return false;
    }
    m_state = WorldGenerationDecodePromotionState::Promoted;
    return true;
  }

  bool cancel() {
    if (m_state == WorldGenerationDecodePromotionState::Promoted)
      return false;
    m_state = WorldGenerationDecodePromotionState::Cancelled;
    return true;
  }

  // A newer request for the same sector/content must reuse decoded bytes when
  // still Pending/Decoded instead of spawning another producer.
  static bool sameDecodeWork(WorldGenerationDecodePromotionIdentity const& a,
      WorldGenerationDecodePromotionIdentity const& b) {
    return a.worldEpoch == b.worldEpoch
        && a.storageEpoch == b.storageEpoch
        && a.sector == b.sector
        && a.contentVersion == b.contentVersion;
  }

private:
  WorldGenerationDecodePromotionIdentity m_identity;
  WorldGenerationDecodeTargetLoad m_target = WorldGenerationDecodeTargetLoad::Prefetch;
  WorldGenerationDecodePromotionState m_state = WorldGenerationDecodePromotionState::PendingDecode;
  size_t m_decodedBytes = 0;
};

// Registry of in-flight decode tickets keyed by sector/content, not by the
// transient requestTicket, so Visible can attach to an existing Prefetch.
class WorldGenerationDecodePromotionRegistry {
public:
  std::optional<WorldGenerationDecodePromotionTicket*> findReusable(
      WorldGenerationDecodePromotionIdentity const& identity) {
    for (auto& ticket : m_tickets) {
      if (ticket.state() == WorldGenerationDecodePromotionState::Cancelled
          || ticket.state() == WorldGenerationDecodePromotionState::Stale
          || ticket.state() == WorldGenerationDecodePromotionState::Promoted)
        continue;
      if (WorldGenerationDecodePromotionTicket::sameDecodeWork(ticket.identity(), identity))
        return &ticket;
    }
    return std::nullopt;
  }

  WorldGenerationDecodePromotionTicket& request(WorldGenerationDecodePromotionIdentity identity,
      WorldGenerationDecodeTargetLoad target) {
    if (auto existing = findReusable(identity)) {
      if (target == WorldGenerationDecodeTargetLoad::Visible)
        (*existing)->upgradeToVisible();
      return **existing;
    }
    m_tickets.append(WorldGenerationDecodePromotionTicket(std::move(identity), target));
    return m_tickets[m_tickets.size() - 1];
  }

  size_t size() const { return m_tickets.size(); }

private:
  List<WorldGenerationDecodePromotionTicket> m_tickets;
};

}
