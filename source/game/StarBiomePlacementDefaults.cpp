// Asset-free value defaults extracted unchanged from PlantDatabase/TileDamage.
// Config/database constructors remain in their original translation units.
#include "StarPlantDatabase.hpp"

namespace Star {

TreeVariant::TreeVariant()
  : stemHueShift(), foliageHueShift(), ceiling(), ephemeral() {}

GrassVariant::GrassVariant() : hueShift(), ceiling(), ephemeral() {}

BushVariant::BushVariant() : baseHueShift(), modHueShift(), ceiling(), ephemeral() {}

TileDamageParameters::TileDamageParameters()
  : m_damageRecoveryPerSecond(0.0f), m_maximumEffectTime(0.0f), m_totalHealth(0), m_requiredHarvestLevel(0) {}

}
