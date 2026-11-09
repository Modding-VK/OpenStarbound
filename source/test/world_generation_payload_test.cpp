#include "StarWorldGenerationPayloadBuffer.hpp"
#include "StarBiomePlacementTypes.hpp"
#include "StarWorldGenerationPlacementQueue.hpp"
#include "StarStaticRandom.hpp"
#include "StarWorkerPool.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

#include "gtest/gtest.h"

using namespace Star;

namespace {
constexpr auto Payload = WorldGenerationMemoryClass::MicroDungeonCandidateEstimate;
constexpr auto Raw = WorldGenerationMemoryClass::MicroDungeonCandidateBuffer;
struct Counts {
  unsigned copies = 0, moves = 0, sizes = 0;
  bool failCopy = false, badSize = false;
  WorldGenerationMemoryLedger* observer = nullptr;
  size_t copyObserved = 0, destructionObserved = 0;
};
struct Probe {
  Probe(unsigned value, std::shared_ptr<Counts> counts)
    : value(value), counts(std::move(counts)), heap(64, value) {}
  Probe(Probe const& other) : value(other.value), counts(other.counts), heap(other.heap) {
    ++counts->copies;
    if (counts->observer) counts->copyObserved = counts->observer->snapshot().estimatedBytes;
    if (counts->failCopy) throw std::runtime_error("controlled payload copy failure");
  }
  Probe(Probe&& other) noexcept(false)
    : value(other.value), counts(std::move(other.counts)), heap(std::move(other.heap)) { ++counts->moves; }
  Probe& operator=(Probe&&) = default;
  ~Probe() {
    if (counts && counts->observer && !heap.empty())
      counts->destructionObserved = counts->observer->snapshot().estimatedBytes;
  }
  bool operator<(Probe const& other) const { return value < other.value; }
  unsigned value;
  std::shared_ptr<Counts> counts;
  std::vector<unsigned> heap;
};
struct Sizer {
  WorldGenerationBackingEstimate operator()(Probe const& value) const noexcept {
    ++value.counts->sizes;
    WorldGenerationBackingEstimate result;
    result.storage.add(value.heap.capacity(), sizeof(unsigned));
    result.storage.complete = !value.counts->badSize;
    return result;
  }
};
using ProbeBuffer = WorldGenerationPayloadBuffer<Probe, Sizer>;
WorldGenerationBackingEstimate prediction(size_t bytes = 64 * sizeof(unsigned)) {
  WorldGenerationBackingEstimate result; result.storage.add(bytes); return result;
}
size_t payloadBytes(WorldGenerationMemoryLedger const& ledger) {
  return ledger.snapshot().classBytes[static_cast<size_t>(Payload)];
}
using Candidate = pair<BiomeItemPlacement, Maybe<WorldGenerationPlacementId>>;
struct CandidateSizer {
  WorldGenerationBackingEstimate operator()(Candidate const& value) const noexcept {
    return estimateBiomeItemGenerationBacking(value.first.item);
  }
};
using CandidateBuffer = WorldGenerationPayloadBuffer<Candidate, CandidateSizer>;
}

TEST(WorldGenerationPayloadTest, UnusedBufferNeverReservesPayloadOrRawStorage) {
  WorldGenerationMemoryLedger ledger;
  { ProbeBuffer buffer(ledger, Sizer{}); buffer.sort(); EXPECT_TRUE(buffer.empty()); }
  EXPECT_EQ(ledger.snapshot().resources, 0u);
  EXPECT_EQ(ledger.snapshot().peakReservedBytes, 0u);
}

TEST(WorldGenerationPayloadTest, ReservesKnownOverlapBeforeFactoryAndReconcilesAfterwards) {
  WorldGenerationMemoryLedger ledger;
  auto counts = std::make_shared<Counts>();
  {
    ProbeBuffer buffer(ledger, Sizer{});
    buffer.append(prediction(), [&] {
      EXPECT_EQ(payloadBytes(ledger), 2 * prediction().storage.bytes);
      return Probe{3, counts};
    });
    EXPECT_EQ(payloadBytes(ledger), prediction().storage.bytes);
    EXPECT_EQ(buffer.knownBacking().storage.bytes, payloadBytes(ledger));
    EXPECT_EQ(ledger.snapshot().reservedBytes, ledger.snapshot().reservedBufferBytes + ledger.snapshot().estimatedBytes);
    EXPECT_TRUE(buffer.estimateComplete());
    EXPECT_EQ(buffer.begin()->value, 3u);
  }
  EXPECT_EQ(ledger.snapshot().reservedBytes, 0u);
}

TEST(WorldGenerationPayloadTest, GrowingVectorKeepsOldAndNewKnownHeapsReservedDuringCopy) {
  WorldGenerationMemoryLedger ledger;
  auto counts = std::make_shared<Counts>(); counts->observer = &ledger;
  ProbeBuffer buffer(ledger, Sizer{});
  buffer.append(prediction(), [&] { return Probe{1, counts}; });
  while (buffer.size() < buffer.capacity()) buffer.append(prediction(), [&] { return Probe{1, counts}; });
  auto old = buffer.knownBacking().storage.bytes;
  buffer.append(prediction(), [&] { return Probe{2, counts}; });
  EXPECT_GE(counts->copyObserved, 2 * old + 2 * prediction().storage.bytes);
  EXPECT_EQ(payloadBytes(ledger), buffer.size() * prediction().storage.bytes);
}

TEST(WorldGenerationPayloadTest, FactoryAddsNoElementCopiesMovesOrDifferentCapacityHistory) {
  WorldGenerationMemoryLedger ledger, referenceLedger;
  auto actual = std::make_shared<Counts>(), expected = std::make_shared<Counts>();
  ProbeBuffer buffer(ledger, Sizer{});
  std::vector<Probe, WorldGenerationBufferAllocator<Probe>> reference(
      WorldGenerationBufferAllocator<Probe>(referenceLedger, Raw));
  for (unsigned i = 0; i < 64; ++i) {
    reference.push_back(Probe{i, expected});
    buffer.append(prediction(), [&] { return Probe{i, actual}; });
    EXPECT_EQ(buffer.capacity(), reference.capacity());
    EXPECT_EQ(actual->copies, expected->copies);
    EXPECT_EQ(actual->moves, expected->moves);
  }
}

TEST(WorldGenerationPayloadTest, RefusalBeforeFactoryPreservesExistingPayloadAndCanRetry) {
  constexpr size_t limit = 1024 * 1024;
  WorldGenerationMemoryLedger ledger(limit);
  auto counts = std::make_shared<Counts>();
  ProbeBuffer buffer(ledger, Sizer{});
  buffer.append(prediction(), [&] { return Probe{1, counts}; });
  auto before = ledger.snapshot().reservedBytes;
  auto block = ledger.reserve(WorldGenerationMemoryClass::BaseTilesArray, limit - before);
  bool called = false;
  EXPECT_THROW(buffer.append(prediction(), [&] { called = true; return Probe{2, counts}; }), std::bad_alloc);
  EXPECT_FALSE(called);
  EXPECT_EQ(buffer.size(), 1u);
  EXPECT_EQ(buffer.begin()->value, 1u);
  block.reset();
  EXPECT_EQ(ledger.snapshot().reservedBytes, before);
  buffer.append(prediction(), [&] { return Probe{2, counts}; });
  EXPECT_EQ(buffer.size(), 2u);
}

TEST(WorldGenerationPayloadTest, ThrowingFactoryRestoresBothEmptyAndNonemptyCharges) {
  WorldGenerationMemoryLedger ledger;
  auto counts = std::make_shared<Counts>();
  ProbeBuffer buffer(ledger, Sizer{});
  auto fail = []() -> Probe { throw std::runtime_error("controlled factory failure"); };
  EXPECT_THROW(buffer.append(prediction(), fail), std::runtime_error);
  EXPECT_TRUE(buffer.empty());
  EXPECT_EQ(ledger.snapshot().resources, 0u);
  buffer.append(prediction(), [&] { return Probe{1, counts}; });
  auto before = ledger.snapshot().reservedBytes;
  EXPECT_THROW(buffer.append(prediction(), fail), std::runtime_error);
  EXPECT_EQ(ledger.snapshot().reservedBytes, before);
  EXPECT_EQ(buffer.size(), 1u);
}

TEST(WorldGenerationPayloadTest, FailedElementCopyDuringGrowthRollsBackWithoutChangingOriginal) {
  WorldGenerationMemoryLedger ledger;
  auto counts = std::make_shared<Counts>();
  ProbeBuffer buffer(ledger, Sizer{});
  buffer.append(prediction(), [&] { return Probe{1, counts}; });
  while (buffer.size() < buffer.capacity()) buffer.append(prediction(), [&] { return Probe{1, counts}; });
  auto size = buffer.size(), capacity = buffer.capacity(), before = ledger.snapshot().reservedBytes;
  counts->failCopy = true;
  EXPECT_THROW(buffer.append(prediction(), [&] { return Probe{2, counts}; }), std::runtime_error);
  EXPECT_EQ(buffer.size(), size);
  EXPECT_EQ(buffer.capacity(), capacity);
  EXPECT_EQ(ledger.snapshot().reservedBytes, before);
  for (auto const& value : buffer) EXPECT_EQ(value.value, 1u);
  counts->failCopy = false;
  buffer.append(prediction(), [&] { return Probe{2, counts}; });
  EXPECT_EQ(buffer.size(), size + 1);
}

TEST(WorldGenerationPayloadTest, RawBufferRefusalAfterFactoryStillReleasesAllCandidateReservations) {
  WorldGenerationMemoryLedger ledger(2 * prediction().storage.bytes);
  auto counts = std::make_shared<Counts>();
  ProbeBuffer buffer(ledger, Sizer{});
  bool called = false;
  EXPECT_THROW(buffer.append(prediction(), [&] { called = true; return Probe{1, counts}; }), std::bad_alloc);
  EXPECT_TRUE(called);
  EXPECT_TRUE(buffer.empty());
  EXPECT_EQ(ledger.snapshot().resources, 0u);
}

TEST(WorldGenerationPayloadTest, OverflowOrIncompletePredictionRefusesBeforeConstruction) {
  WorldGenerationMemoryLedger ledger;
  auto counts = std::make_shared<Counts>();
  ProbeBuffer buffer(ledger, Sizer{});
  bool called = false;
  auto factory = [&] { called = true; return Probe{1, counts}; };
  EXPECT_THROW(buffer.append(prediction(std::numeric_limits<size_t>::max()), factory), std::bad_alloc);
  auto incomplete = prediction(); incomplete.storage.complete = false;
  EXPECT_THROW(buffer.append(incomplete, factory), std::bad_alloc);
  EXPECT_FALSE(called);
  EXPECT_EQ(ledger.snapshot().resources, 0u);
}

TEST(WorldGenerationPayloadTest, LateReconciliationFailureIsDiagnosticNotPayloadRejection) {
  auto raw = WorldGenerationBufferAllocator<Probe>::requestBytes(1);
  WorldGenerationMemoryLedger ledger(raw);
  auto counts = std::make_shared<Counts>();
  ProbeBuffer buffer(ledger, Sizer{});
  buffer.append(prediction(0), [&] { return Probe{7, counts}; });
  EXPECT_EQ(buffer.size(), 1u);
  EXPECT_EQ(buffer.begin()->value, 7u);
  EXPECT_FALSE(buffer.estimateComplete());
  EXPECT_EQ(payloadBytes(ledger), 0u);
  EXPECT_GT(buffer.knownBacking().storage.bytes, 0u);
  EXPECT_EQ(ledger.snapshot().incompleteEstimateUpdates, 1u);
}

TEST(WorldGenerationPayloadTest, IncompleteActualSizingKeepsChargeAndStopsFurtherUnboundedPredictions) {
  WorldGenerationMemoryLedger ledger;
  auto counts = std::make_shared<Counts>(); counts->badSize = true;
  ProbeBuffer buffer(ledger, Sizer{});
  buffer.append(prediction(), [&] { return Probe{7, counts}; });
  EXPECT_FALSE(buffer.estimateComplete());
  EXPECT_EQ(payloadBytes(ledger), 2 * prediction().storage.bytes);
  EXPECT_EQ(buffer.begin()->value, 7u);
  bool called = false;
  EXPECT_THROW(buffer.append(prediction(), [&] { called = true; return Probe{8, counts}; }), std::bad_alloc);
  EXPECT_FALSE(called);
}

TEST(WorldGenerationPayloadTest, PayloadDestructorStillObservesItsReservation) {
  WorldGenerationMemoryLedger ledger;
  auto counts = std::make_shared<Counts>(); counts->observer = &ledger;
  { ProbeBuffer buffer(ledger, Sizer{}); buffer.append(prediction(), [&] { return Probe{1, counts}; }); }
  EXPECT_GE(counts->destructionObserved, prediction().storage.bytes);
  EXPECT_EQ(ledger.snapshot().reservedBytes, 0u);
}

TEST(WorldGenerationPayloadTest, BufferCanAppendAfterOriginalFacadeCloses) {
  WorldGenerationMemoryLedger observer;
  auto counts = std::make_shared<Counts>();
  std::unique_ptr<ProbeBuffer> buffer;
  { WorldGenerationMemoryLedger facade(observer); buffer = std::make_unique<ProbeBuffer>(facade, Sizer{}); }
  buffer->append(prediction(), [&] { return Probe{1, counts}; });
  EXPECT_GT(observer.snapshot().reservedBytes, 0u);
  buffer.reset();
  EXPECT_EQ(observer.snapshot().reservedBytes, 0u);
}

TEST(WorldGenerationPayloadTest, WorkerRetainsBufferAfterCallerAndFacadeRetire) {
  WorldGenerationMemoryLedger observer;
  auto counts = std::make_shared<Counts>();
  WorkerPool workers("Candidate payload contract", 1);
  std::mutex mutex; std::condition_variable condition;
  bool started = false, released = false;
  Maybe<WorkerPoolPromise<bool>> result;
  {
    WorldGenerationMemoryLedger facade(observer);
    auto buffer = std::make_shared<ProbeBuffer>(facade, Sizer{});
    buffer->append(prediction(), [&] { return Probe{7, counts}; });
    result = workers.addProducer<bool>([&, buffer] {
      std::unique_lock<std::mutex> lock(mutex);
      started = true; condition.notify_all();
      condition.wait(lock, [&] { return released; });
      return buffer->begin()->value == 7;
    });
  }
  {
    std::unique_lock<std::mutex> lock(mutex);
    EXPECT_TRUE(condition.wait_for(lock, std::chrono::seconds(5), [&] { return started; }));
    EXPECT_GT(payloadBytes(observer), 0u);
    released = true;
  }
  condition.notify_all();
  EXPECT_TRUE(result->get());
  workers.finish(); result.reset();
  EXPECT_EQ(observer.snapshot().reservedBytes, 0u);
}

TEST(WorldGenerationPayloadTest, EightConcurrentJobsShareLedgerButReleaseEveryPayload) {
  WorldGenerationMemoryLedger ledger;
  std::atomic<unsigned> completed{0};
  std::vector<std::thread> threads;
  for (unsigned i = 0; i < 8; ++i) threads.emplace_back([&] {
    auto counts = std::make_shared<Counts>();
    ProbeBuffer buffer(ledger, Sizer{});
    for (unsigned j = 0; j < 32; ++j) buffer.append(prediction(), [&] { return Probe{31 - j, counts}; });
    buffer.sort();
    if (buffer.begin()->value == 0 && buffer.size() == 32) ++completed;
  });
  for (auto& thread : threads) thread.join();
  EXPECT_EQ(completed.load(), 8u);
  EXPECT_EQ(ledger.snapshot().reservedBytes, 0u);
}

TEST(WorldGenerationPayloadTest, FullSizingOccursOnlyOnGrowthAndSortOtherwiseNewElementOnly) {
  WorldGenerationMemoryLedger ledger;
  auto counts = std::make_shared<Counts>();
  ProbeBuffer buffer(ledger, Sizer{});
  unsigned expected = 0;
  for (unsigned i = 0; i < 128; ++i) {
    bool grows = buffer.size() == buffer.capacity();
    buffer.append(prediction(), [&] { return Probe{127 - i, counts}; });
    expected += grows ? buffer.size() : 1;
    EXPECT_EQ(counts->sizes, expected);
  }
  buffer.sort();
  EXPECT_EQ(counts->sizes, expected + 128);
}

TEST(WorldGenerationPayloadTest, AllSixValueVariantsReportKnownStorageAndExplicitOpaqueInternals) {
  auto text = String(std::string(256, 'x'));
  BiomeItem names{MicroDungeonNames(StringSet{text, "short"})};
  auto before = names.get<MicroDungeonNames>().values();
  auto n = estimateBiomeItemGenerationBacking(names);
  EXPECT_EQ(n.opaqueItems, 0u);
  EXPECT_GE(n.storage.bytes, 2 * sizeof(String) + text.utf8Size());
  EXPECT_EQ(names.get<MicroDungeonNames>().values(), before);
  BiomeItem treasure{TreasureBoxSet(text)};
  auto t = estimateBiomeItemGenerationBacking(treasure);
  EXPECT_EQ(t.opaqueItems, 0u);
  EXPECT_EQ(t.storage.bytes, treasure.get<TreasureBoxSet>().utf8().capacity() + 1);
  GrassVariant grass; grass.name = text; grass.images.reserve(8); grass.images.append(text);
  BiomeItem g{std::move(grass)}; auto ge = estimateBiomeItemGenerationBacking(g);
  EXPECT_EQ(ge.opaqueItems, 1u);
  EXPECT_GE(ge.storage.bytes, g.get<GrassVariant>().images.capacity() * sizeof(String) + text.utf8Size());
  BushVariant bush; bush.bushName = text; bush.shapes.reserve(4);
  bush.shapes.append({text, StringList{text, "mod"}});
  BiomeItem b{std::move(bush)}; auto be = estimateBiomeItemGenerationBacking(b);
  EXPECT_EQ(be.opaqueItems, 1u);
  EXPECT_GE(be.storage.bytes, b.get<BushVariant>().shapes.capacity() * sizeof(BushVariant::BushShape));
  TreePair trees; trees.first.stemName = text; trees.second.foliageDirectory = text;
  BiomeItem tr{std::move(trees)}; auto te = estimateBiomeItemGenerationBacking(tr);
  EXPECT_EQ(te.opaqueItems, 1u); EXPECT_GE(te.storage.bytes, 2 * text.utf8Size());
  ObjectPool objects; objects.add(1, {text, JsonObject{{"unmeasured", true}}});
  BiomeItem o{std::move(objects)}; auto oe = estimateBiomeItemGenerationBacking(o);
  EXPECT_EQ(oe.opaqueItems, 1u);
  EXPECT_GE(oe.storage.bytes, o.get<ObjectPool>().items().capacity() * sizeof(ObjectPool::ItemsType) + text.utf8Size());
}

TEST(WorldGenerationPayloadTest, NativeOpaqueCandidateIsNotFilteredOrRejectedByCoverage) {
  WorldGenerationMemoryLedger ledger;
  CandidateBuffer values(ledger, CandidateSizer{});
  ObjectPool objects; objects.add(1, {String("object name"), JsonObject{{"kept", true}}});
  BiomeItemPlacement item(BiomeItem{std::move(objects)}, Vec2I(3, 7), 0.5f);
  values.append(estimateBiomeItemGenerationBacking(item.item), [&] { return Candidate{item, {}}; });
  values.sort();
  EXPECT_EQ(values.size(), 1u);
  EXPECT_TRUE(values.begin()->first.item.is<ObjectPool>());
  EXPECT_TRUE(values.begin()->first.item.get<ObjectPool>().items()[0].second.second.getBool("kept"));
  EXPECT_EQ(values.knownBacking().opaqueItems, 1u);
  EXPECT_TRUE(values.estimateComplete());
}

TEST(WorldGenerationPayloadTest, NativeCandidateGrowthSortAndChoiceMatchLegacyWithWrappedCollisions) {
  StringSet colliding;
  for (unsigned i = 0; colliding.size() < 3; ++i) {
    auto text = toString(i); if ((hash<String>{}(text) & 7) == 7) colliding.add(text);
  }
  MicroDungeonNames names(std::move(colliding));
  auto copy = names; ASSERT_NE(names.values(), copy.values());
  List<BiomeItemPlacement> source;
  for (unsigned i = 0; i < 5; ++i) source.append({BiomeItem{names}, Vec2I(i, 7), 0.5f});
  source.append({BiomeItem{TreasureBoxSet(String("not micro"))}, Vec2I(9, 7), 0.5f});
  WorldGenerationMemoryLedger actualLedger, referenceLedger;
  CandidateBuffer actual(actualLedger, CandidateSizer{});
  std::vector<Candidate, WorldGenerationBufferAllocator<Candidate>> reference(
      WorldGenerationBufferAllocator<Candidate>(referenceLedger, Raw));
  for (unsigned pass = 0; pass < 8; ++pass) {
    for (auto const& placement : source) {
      reference.push_back({placement, {}});
      actual.append(estimateBiomeItemGenerationBacking(placement.item), [&] { return Candidate{placement, {}}; });
    }
    for (auto placement : source) reference.push_back({std::move(placement), WorldGenerationPlacementId(7)});
    for (auto placement : source)
      actual.append(estimateBiomeItemGenerationBacking(placement.item), [&] {
        return Candidate{std::move(placement), WorldGenerationPlacementId(7)};
      });
    EXPECT_EQ(actual.capacity(), reference.capacity());
  }
  std::sort(reference.begin(), reference.end(), std::less<Candidate>{}); actual.sort();
  ASSERT_EQ(actual.size(), reference.size());
  size_t i = 0;
  for (auto const& current : actual) {
    auto const& previous = reference[i];
    EXPECT_EQ(current.first.position, previous.first.position);
    EXPECT_EQ(current.first.priority, previous.first.priority);
    EXPECT_EQ(current.second, previous.second);
    ASSERT_EQ(current.first.item.is<MicroDungeonNames>(), previous.first.item.is<MicroDungeonNames>());
    if (current.first.item.is<MicroDungeonNames>()) {
      auto const& a = current.first.item.get<MicroDungeonNames>();
      auto const& b = previous.first.item.get<MicroDungeonNames>();
      EXPECT_EQ(a.values(), b.values());
      for (uint64_t seed = 0; seed < 64; ++seed) EXPECT_EQ(staticRandomFrom(a, seed), staticRandomFrom(b, seed));
    }
    ++i;
  }
}
