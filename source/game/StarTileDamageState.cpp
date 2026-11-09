#include "StarTileDamage.hpp"

namespace Star {

// Pure lifetime routines moved unchanged from StarTileDamage.cpp so value-only
// generation contracts can construct real tiles without linking Root/assets.
TileDamageStatus::TileDamageStatus() {
  reset();
}

void TileDamageStatus::reset() {
  m_damagePercentage = 0.0f;
  m_damageEffectTimeFactor = 0.0f;
  m_harvested = false;
  m_damageSourcePosition = Vec2F();
  m_damageType = TileDamageType::Protected;
  m_damageEffectPercentage = 0.0f;
}

}
