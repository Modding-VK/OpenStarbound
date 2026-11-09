#include "StarWorldGenerationMemoryEstimate.hpp"

#include <set>

namespace Star {

void WorldGenerationSizeEstimate::add(size_t count, size_t elementBytes) {
  auto maximum = std::numeric_limits<size_t>::max();
  if ((elementBytes && count > maximum / elementBytes) || count * elementBytes > maximum - bytes) {
    complete = false;
    return;
  }
  bytes += count * elementBytes;
}

void WorldGenerationSizeEstimate::stringCapacity(String const& string) {
  add(string.utf8().capacity());
  add(1);
}

WorldGenerationJsonSnapshotPtr captureWorldGenerationJson(WorldGenerationMemoryLedger& ledger,
    Json value, bool measure, size_t maximumValues) {
  auto snapshot = std::make_shared<WorldGenerationJsonSnapshot>();
  snapshot->value = std::move(value);
  snapshot->measured = measure;
  if (!measure) return snapshot;
  snapshot->complete = true;
  std::vector<Json> pending{snapshot->value};
  std::set<std::shared_ptr<void const>, std::owner_less<std::shared_ptr<void const>>> visited;
  while (!pending.empty() && snapshot->visitedValues < maximumValues) {
    auto node = std::move(pending.back());
    pending.pop_back();
    ++snapshot->visitedValues;
    std::shared_ptr<void const> owner;
    WorldGenerationSizeEstimate estimate;
    switch (node.type()) {
      case Json::Type::String:
        owner = node.stringPtr();
        estimate.add(sizeof(String));
        estimate.stringCapacity(*node.stringPtr());
        break;
      case Json::Type::Array:
        owner = node.arrayPtr();
        estimate.add(sizeof(JsonArray));
        estimate.add(node.arrayPtr()->capacity(), sizeof(Json));
        break;
      case Json::Type::Object:
        owner = node.objectPtr();
        estimate.add(sizeof(JsonObject));
        // FlatHashMap does not expose bucket capacity. Count occupied
        // representations only; key heap storage/unused buckets are excluded.
        estimate.add(node.objectPtr()->size(), sizeof(JsonObject::value_type));
        break;
      default:
        continue;
    }
    if (!visited.insert(owner).second) continue;
    auto memory = estimate.complete ? ledger.reserveSharedJson(owner, estimate.bytes) : nullptr;
    if (memory) snapshot->accounting.push_back(std::move(memory));
    else snapshot->complete = false;
    auto enqueue = [&](Json const& child) {
      if (pending.size() >= maximumValues - snapshot->visitedValues) {
        snapshot->complete = false;
        return false;
      }
      pending.push_back(child);
      return true;
    };
    if (node.isType(Json::Type::Array)) {
      for (auto const& child : *node.arrayPtr()) if (!enqueue(child)) break;
    } else if (node.isType(Json::Type::Object)) {
      for (auto const& entry : *node.objectPtr()) if (!enqueue(entry.second)) break;
    }
  }
  if (!pending.empty()) snapshot->complete = false;
  return snapshot;
}

}
