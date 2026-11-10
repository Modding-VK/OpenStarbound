// Existing maintenance bodies relocated unchanged so Root lifecycle tests can
// link real cleanup without pulling every item/entity factory implementation.
#include "StarObjectDatabase.hpp"
#include "StarItemDatabase.hpp"
#include "StarMonsterDatabase.hpp"
#include "StarTenantDatabase.hpp"
#include "StarImageMetadataDatabase.hpp"

namespace Star {

void ObjectDatabase::cleanup() {
  MutexLocker locker(m_cacheMutex);
  m_configCache.cleanup([](String const&, ObjectConfigPtr const& config) {
      return !config.unique();
    });
}

void ItemDatabase::cleanup() {
  {
    MutexLocker locker(m_cacheMutex);
    m_itemCache.cleanup([](ItemCacheEntry const&, ItemPtr const& item) {
      return !item.unique();
    });
  }
}

void MonsterDatabase::cleanup() {
  MutexLocker locker(m_cacheMutex);
  m_monsterCache.cleanup();
}

void TenantDatabase::cleanup() {
  MutexLocker locker(m_cacheMutex);
  m_tenantCache.cleanup();
}

void ImageMetadataDatabase::cleanup() const {
  MutexLocker locker(m_mutex);

  m_sizeCache.cleanup();
  m_spacesCache.cleanup();
  m_regionCache.cleanup();
}

}
