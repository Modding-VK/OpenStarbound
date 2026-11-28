#include "StarWorldGenerationDecodePromotion.hpp"

#include "gtest/gtest.h"

using namespace Star;

namespace {
WorldGenerationDecodePromotionIdentity id(uint64_t ticket, Vec2U sector = {1, 2},
    uint64_t content = 5) {
  return {1, 2, sector, content, ticket};
}
}

TEST(WorldGenerationDecodePromotionTest, PrefetchUpgradesToVisibleWithoutSecondProducer) {
  WorldGenerationDecodePromotionRegistry registry;
  auto& first = registry.request(id(10), WorldGenerationDecodeTargetLoad::Prefetch);
  ASSERT_TRUE(first.completeDecode(64));
  auto& second = registry.request(id(11), WorldGenerationDecodeTargetLoad::Visible);
  EXPECT_EQ(1u, registry.size());
  EXPECT_EQ(&first, &second);
  EXPECT_EQ(WorldGenerationDecodeTargetLoad::Visible, second.target());
  EXPECT_TRUE(second.tryPromote(id(10)));
  EXPECT_EQ(WorldGenerationDecodePromotionState::Promoted, second.state());
}

TEST(WorldGenerationDecodePromotionTest, StaleLiveIdentityRefusesPromotion) {
  WorldGenerationDecodePromotionTicket ticket(id(1), WorldGenerationDecodeTargetLoad::Visible);
  ASSERT_TRUE(ticket.completeDecode(32));
  auto live = id(1);
  live.contentVersion = 99;
  EXPECT_FALSE(ticket.tryPromote(live));
  EXPECT_EQ(WorldGenerationDecodePromotionState::Stale, ticket.state());
}

TEST(WorldGenerationDecodePromotionTest, CancelBlocksPromotionAndUpgradeAfterPromoteFails) {
  WorldGenerationDecodePromotionTicket ticket(id(3), WorldGenerationDecodeTargetLoad::Prefetch);
  ASSERT_TRUE(ticket.completeDecode(8));
  EXPECT_TRUE(ticket.cancel());
  EXPECT_FALSE(ticket.upgradeToVisible());
  EXPECT_FALSE(ticket.tryPromote(id(3)));
}

TEST(WorldGenerationDecodePromotionTest, DifferentContentCreatesSeparateDecodeWork) {
  WorldGenerationDecodePromotionRegistry registry;
  auto& a = registry.request(id(1, {0, 0}, 1), WorldGenerationDecodeTargetLoad::Prefetch);
  auto& b = registry.request(id(2, {0, 0}, 2), WorldGenerationDecodeTargetLoad::Prefetch);
  EXPECT_EQ(2u, registry.size());
  EXPECT_NE(&a, &b);
}
