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
    JsonObjectConstPtr object;
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
        object = node.objectPtr();
        owner = object;
        estimate.add(sizeof(JsonObject));
        // Includes inline keys/values, empty/sentinel slots and padding.
        estimate.add(object->bucketStorageBytes());
        break;
      default:
        continue;
    }
    if (!visited.insert(owner).second) continue;
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
    } else if (object && !object->empty()) {
      // Each key accompanies one child already subject to the traversal bound.
      // Never register a limit-dependent partial byte count for a shared owner:
      // reserveSharedJson requires one canonical model across all captures.
      bool keysFit = object->size() <= maximumValues - snapshot->visitedValues - pending.size();
      if (keysFit) {
        for (auto const& entry : *object) {
          estimate.stringCapacity(entry.first);
          enqueue(entry.second);
        }
        snapshot->sizedObjectKeys += object->size();
      } else {
        ++snapshot->omittedObjects;
        snapshot->complete = false;
        for (auto const& entry : *object) if (!enqueue(entry.second)) break;
        continue;
      }
    }
    auto memory = estimate.complete ? ledger.reserveSharedJson(owner, estimate.bytes) : nullptr;
    if (memory) snapshot->accounting.push_back(std::move(memory));
    else snapshot->complete = false;
  }
  if (!pending.empty()) snapshot->complete = false;
  return snapshot;
}

}
