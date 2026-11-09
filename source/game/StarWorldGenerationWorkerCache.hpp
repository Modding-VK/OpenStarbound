#pragma once

#include "StarWorldGenerationMemory.hpp"

#include <cstdint>

namespace Star {

// One entry on its owning worker, with weak source identity. No world/ledger
// facade is retained, and a failed replacement leaves the old entry intact.
template <typename Source, typename Payload>
class WorldGenerationWorkerCache {
  struct Entry {
    std::shared_ptr<WorldGenerationMemoryReservation> memory;
    std::shared_ptr<Payload> value;
    size_t (*measure)(Payload const&);
    bool estimateComplete = true;

    void refresh() noexcept {
      try {
        estimateComplete = memory->resizeEstimate(measure(*value));
      } catch (...) {
        estimateComplete = false;
      }
      if (!estimateComplete) {
        // Diagnostics must not replace the generation exception or fail an
        // otherwise valid result. The previous charge remains in place.
        try { memory->noteIncompleteEstimate(); } catch (...) {}
      }
    }
  };

public:
  WorldGenerationWorkerCache() = default;
  WorldGenerationWorkerCache(WorldGenerationWorkerCache const&) = delete;
  WorldGenerationWorkerCache& operator=(WorldGenerationWorkerCache const&) = delete;
  WorldGenerationWorkerCache(WorldGenerationWorkerCache&&) = default;
  WorldGenerationWorkerCache& operator=(WorldGenerationWorkerCache&&) = default;

  // Borrow the payload through this scope, not an independently retained copy.
  // Update even if generation throws; keep the entry alive across replacement.
  class Use {
  public:
    explicit Use(std::shared_ptr<Entry> entry) : m_entry(std::move(entry)) {}
    Use(Use const&) = delete;
    Use& operator=(Use const&) = delete;
    Use(Use&&) = default;
    Use& operator=(Use&&) = delete;
    ~Use() { if (m_entry) m_entry->refresh(); }
    explicit operator bool() const { return bool(m_entry); }
    std::shared_ptr<Payload> const& value() const { return m_entry->value; }
    bool estimateComplete() const { return m_entry->estimateComplete; }

  private:
    std::shared_ptr<Entry> m_entry;
  };

  template <typename Producer>
  Use get(std::shared_ptr<Source> const& source, uint64_t revision,
      WorldGenerationMemoryLedger& ledger, size_t initialBytes,
      size_t (*measure)(Payload const&), Producer producer) {
    if (!m_entry || m_source.lock() != source || m_revision != revision) {
      auto memory = ledger.reserve(WorldGenerationMemoryClass::WorkerTemplateEstimate, initialBytes);
      if (!memory) return Use({});
      auto next = std::make_shared<Entry>();
      next->memory = std::move(memory);
      next->measure = measure;
      next->value = producer();
      if (!next->value) return Use({});
      next->refresh();
      m_source = source;
      m_revision = revision;
      m_entry = std::move(next);
    }
    return Use(m_entry);
  }

private:
  std::weak_ptr<Source> m_source;
  uint64_t m_revision = 0;
  std::shared_ptr<Entry> m_entry;
};

}
