#include "StarWorldGenerationTemporaryMemory.hpp"
#include "StarWorldGenerationPayloadBuffer.hpp"
#include "StarWorldGenerationPlacementQueue.hpp"
#include "StarBiomeItemSelection.hpp"
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
using Slot = WorldGenerationTemporarySlot;
constexpr auto Kind = WorldGenerationMemoryClass::MicroDungeonTemporaryEstimate;
size_t bytes(WorldGenerationMemoryLedger const& ledger) {
  return ledger.snapshot().classBytes[static_cast<size_t>(Kind)];
}
struct Counters {
  unsigned copies = 0;
  bool failCopy = false, badSize = false;
  WorldGenerationMemoryLedger* observer = nullptr;
  size_t copyCharge = 0, destructionCharge = 0;
};
struct Data {
  Data(std::shared_ptr<Counters> counters, size_t count = 32) : counters(std::move(counters)), heap(count, 7) {}
  Data(Data const& other) : counters(other.counters), heap(other.heap) {
    ++counters->copies;
    if (counters->observer) counters->copyCharge = bytes(*counters->observer);
    if (counters->failCopy) throw std::runtime_error("controlled temporary copy failure");
  }
  Data(Data&&) noexcept = default;
  ~Data() {
    if (counters && counters->observer && !heap.empty()) counters->destructionCharge = bytes(*counters->observer);
  }
  std::shared_ptr<Counters> counters;
  std::vector<unsigned> heap;
};
struct DataSizer {
  WorldGenerationBackingEstimate operator()(Data const& value) const noexcept {
    WorldGenerationBackingEstimate result;
    result.storage.add(value.heap.capacity(), sizeof(unsigned));
    result.storage.complete = !value.counters->badSize;
    return result;
  }
};
using DataScope = WorldGenerationScopedTemporary<Data, DataSizer>;
WorldGenerationBackingEstimate forecast(size_t size) {
  WorldGenerationBackingEstimate result; result.storage.add(size); return result;
}
void moveValidate(Data input, Data& output) { output.heap = std::move(input.heap); }
auto validateData = [](Data&& input, Data& output) { moveValidate(std::move(input), output); };

struct Potential {
  List<BiomeItemPlacement> surfaceBiomeItems, caveSurfaceBiomeItems, caveCeilingBiomeItems,
      caveBackgroundBiomeItems, oceanItems;
};
using PotentialScope = WorldGenerationScopedTemporary<Potential, PotentialBiomeItemBackingSizer>;
using ItemScope = WorldGenerationScopedTemporary<List<BiomeItemPlacement>, BiomeItemListBackingSizer>;
struct Block {
  bool biomeTransition = false, terrain = false, foregroundCave = false, backgroundCave = false;
  LiquidId oceanLiquid = EmptyLiquidId;
  int oceanLiquidLevel = 7;
};
Potential fixture(unsigned offset = 0, bool micro = true) {
  StringSet colliding;
  for (unsigned i = 0; colliding.size() < 3; ++i) {
    auto text = toString(i); if ((hash<String>{}(text) & 7) == 7) colliding.add(text);
  }
  MicroDungeonNames names(std::move(colliding));
  Potential result;
  unsigned category = 0;
  for (auto list : {&result.surfaceBiomeItems, &result.caveSurfaceBiomeItems,
      &result.caveCeilingBiomeItems, &result.caveBackgroundBiomeItems, &result.oceanItems}) {
    for (unsigned i = 0; i < 3; ++i) {
      BiomeItem item = micro && i < 2 ? BiomeItem{names} : BiomeItem{TreasureBoxSet(String("not micro"))};
      list->append({std::move(item), Vec2I(offset * 32 + category * 3 + i, 7), 0.5f});
    }
    ++category;
  }
  return result;
}
void selectInto(Potential input, List<BiomeItemPlacement>& output, unsigned mode = 0) {
  Block block, above, below;
  below.terrain = true;
  block.oceanLiquid = 1;
  if (mode) { block.terrain = true; block.foregroundCave = true; above.terrain = true; }
  resetBiomeItemSelectionOutput(input, output);
  appendSelectedBiomeItems(7, block, above, below, input, output);
}
}

TEST(WorldGenerationTemporaryTest, EmptySkyScopesDoNotCreateLedgerResources) {
  WorldGenerationMemoryLedger ledger;
  WorldGenerationTemporaryMemory memory(ledger);
  ItemScope output(memory, Slot::Output, {}, [] { return List<BiomeItemPlacement>{}; }, BiomeItemListBackingSizer{});
  for (unsigned i = 0; i < 64; ++i) {
    PotentialScope input(memory, Slot::Ambient, {}, [] { return Potential{}; }, PotentialBiomeItemBackingSizer{});
    validateWorldGenerationTemporaryCopy(memory, input, output,
        [](Potential&& value, auto& result) { selectInto(std::move(value), result); });
  }
  EXPECT_EQ(ledger.snapshot().resources, 0u);
  EXPECT_EQ(ledger.snapshot().peakReservedBytes, 0u);
}

TEST(WorldGenerationTemporaryTest, PredictionIsChargedBeforeFactoryAndReleasedAfterPhysicalValue) {
  WorldGenerationMemoryLedger ledger;
  auto counters = std::make_shared<Counters>(); counters->observer = &ledger;
  WorldGenerationTemporaryMemory memory(ledger);
  {
    DataScope value(memory, Slot::Ambient, forecast(128), [&] {
      EXPECT_EQ(bytes(ledger), 128u); return Data{counters};
    }, DataSizer{});
    EXPECT_EQ(bytes(ledger), value.knownBacking().storage.bytes);
  }
  EXPECT_GE(counters->destructionCharge, 128u);
  EXPECT_EQ(ledger.snapshot().resources, 0u);
}

TEST(WorldGenerationTemporaryTest, FourSlotsCoexistWithoutForgettingAnotherOwner) {
  WorldGenerationMemoryLedger ledger;
  WorldGenerationTemporaryMemory memory(ledger);
  auto a = memory.acquire(Slot::Ambient, forecast(11));
  auto b = memory.acquire(Slot::Queued, forecast(13));
  auto c = memory.acquire(Slot::Validation, forecast(17));
  { auto d = memory.acquire(Slot::Output, forecast(19)); EXPECT_EQ(bytes(ledger), 60u); }
  EXPECT_EQ(bytes(ledger), 41u);
  EXPECT_EQ(ledger.snapshot().resources, 1u);
}

TEST(WorldGenerationTemporaryTest, OccupiedAndInvalidSlotsRefuseBeforeFactory) {
  WorldGenerationMemoryLedger ledger;
  WorldGenerationTemporaryMemory memory(ledger);
  auto active = memory.acquire(Slot::Ambient, forecast(11));
  EXPECT_THROW(memory.acquire(Slot::Ambient, forecast(5)), std::logic_error);
  EXPECT_THROW(memory.acquire(Slot::Count, {}), std::out_of_range);
  EXPECT_EQ(bytes(ledger), 11u);
}

TEST(WorldGenerationTemporaryTest, AJobReusesOneZeroByteTokenBetweenSourceScopes) {
  WorldGenerationMemoryLedger ledger;
  auto counters = std::make_shared<Counters>();
  WorldGenerationTemporaryMemory memory(ledger);
  DataScope output(memory, Slot::Output, {}, [&] { return Data{counters, 0}; }, DataSizer{});
  for (unsigned i = 0; i < 64; ++i) {
    { DataScope source(memory, Slot::Ambient, {}, [&] { return Data{counters}; }, DataSizer{}); }
    EXPECT_EQ(bytes(ledger), 0u);
    EXPECT_EQ(ledger.snapshot().classResources[static_cast<size_t>(Kind)], 1u);
  }
}

TEST(WorldGenerationTemporaryTest, CopyAndOutputForecastsExistBeforeTheSingleCopy) {
  WorldGenerationMemoryLedger ledger;
  auto counters = std::make_shared<Counters>(); counters->observer = &ledger;
  WorldGenerationTemporaryMemory memory(ledger);
  DataScope source(memory, Slot::Ambient, {}, [&] { return Data{counters}; }, DataSizer{});
  DataScope output(memory, Slot::Output, {}, [&] { return Data{counters, 0}; }, DataSizer{});
  validateWorldGenerationTemporaryCopy(memory, source, output, validateData);
  EXPECT_EQ(counters->copies, 1u);
  EXPECT_GE(counters->copyCharge, 2 * source.knownBacking().storage.bytes);
  EXPECT_EQ(output.value().heap, source.value().heap);
  EXPECT_EQ(bytes(ledger), source.knownBacking().storage.bytes + output.knownBacking().storage.bytes);
}

TEST(WorldGenerationTemporaryTest, FullRefusalDoesNotCopyOrMutateSourceAndCanRetry) {
  constexpr size_t limit = 1024 * 1024;
  WorldGenerationMemoryLedger ledger(limit);
  auto counters = std::make_shared<Counters>();
  WorldGenerationTemporaryMemory memory(ledger);
  DataScope source(memory, Slot::Ambient, {}, [&] { return Data{counters}; }, DataSizer{});
  DataScope output(memory, Slot::Output, {}, [&] { return Data{counters, 0}; }, DataSizer{});
  auto before = bytes(ledger);
  auto blocker = ledger.reserve(WorldGenerationMemoryClass::BaseTilesArray, limit - before);
  EXPECT_THROW(validateWorldGenerationTemporaryCopy(memory, source, output, validateData), std::bad_alloc);
  EXPECT_EQ(counters->copies, 0u);
  EXPECT_TRUE(output.value().heap.empty());
  EXPECT_EQ(bytes(ledger), before);
  blocker.reset();
  validateWorldGenerationTemporaryCopy(memory, source, output, validateData);
  EXPECT_EQ(counters->copies, 1u);
}

TEST(WorldGenerationTemporaryTest, CopyRefusalAfterOutputPredictionRestoresOriginalCharge) {
  WorldGenerationMemoryLedger ledger(3 * 128);
  auto counters = std::make_shared<Counters>();
  WorldGenerationTemporaryMemory memory(ledger);
  DataScope source(memory, Slot::Ambient, {}, [&] { return Data{counters}; }, DataSizer{});
  DataScope output(memory, Slot::Output, {}, [&] { return Data{counters, 0}; }, DataSizer{});
  auto before = bytes(ledger);
  EXPECT_THROW(validateWorldGenerationTemporaryCopy(memory, source, output, validateData), std::bad_alloc);
  EXPECT_EQ(counters->copies, 0u);
  EXPECT_EQ(bytes(ledger), before);
  EXPECT_TRUE(output.value().heap.empty());
  EXPECT_TRUE(memory.estimateComplete());
}

TEST(WorldGenerationTemporaryTest, CopyConstructionFailureRollsBackAndAllowsRetry) {
  WorldGenerationMemoryLedger ledger;
  auto counters = std::make_shared<Counters>();
  WorldGenerationTemporaryMemory memory(ledger);
  DataScope source(memory, Slot::Ambient, {}, [&] { return Data{counters}; }, DataSizer{});
  DataScope output(memory, Slot::Output, {}, [&] { return Data{counters, 0}; }, DataSizer{});
  auto before = bytes(ledger);
  counters->failCopy = true;
  EXPECT_THROW(validateWorldGenerationTemporaryCopy(memory, source, output, validateData), std::runtime_error);
  EXPECT_EQ(bytes(ledger), before);
  EXPECT_EQ(source.value().heap.size(), 32u);
  counters->failCopy = false;
  validateWorldGenerationTemporaryCopy(memory, source, output, validateData);
  EXPECT_EQ(output.value().heap.size(), 32u);
}

TEST(WorldGenerationTemporaryTest, ThrowingValidationKeepsMovedOutputCoveredAndUsable) {
  WorldGenerationMemoryLedger ledger;
  auto counters = std::make_shared<Counters>();
  WorldGenerationTemporaryMemory memory(ledger);
  DataScope source(memory, Slot::Ambient, {}, [&] { return Data{counters}; }, DataSizer{});
  DataScope output(memory, Slot::Output, {}, [&] { return Data{counters, 0}; }, DataSizer{});
  EXPECT_THROW(validateWorldGenerationTemporaryCopy(memory, source, output, [](Data&& input, Data& result) {
    result.heap = std::move(input.heap); throw std::runtime_error("controlled validation failure");
  }), std::runtime_error);
  EXPECT_EQ(output.value().heap.size(), 32u);
  EXPECT_EQ(bytes(ledger), source.knownBacking().storage.bytes + output.knownBacking().storage.bytes);
}

TEST(WorldGenerationTemporaryTest, SourceFactoryFailureReleasesItsSlotForReuse) {
  WorldGenerationMemoryLedger ledger;
  WorldGenerationTemporaryMemory memory(ledger);
  auto counters = std::make_shared<Counters>();
  EXPECT_THROW((DataScope(memory, Slot::Ambient, forecast(128), []() -> Data {
    throw std::runtime_error("controlled source failure");
  }, DataSizer{})), std::runtime_error);
  EXPECT_EQ(ledger.snapshot().resources, 0u);
  DataScope source(memory, Slot::Ambient, {}, [&] { return Data{counters}; }, DataSizer{});
  EXPECT_GT(bytes(ledger), 0u);
}

TEST(WorldGenerationTemporaryTest, FailedPopulationReconcilesThePartialPrivateInput) {
  WorldGenerationMemoryLedger ledger;
  auto counters = std::make_shared<Counters>();
  WorldGenerationTemporaryMemory memory(ledger);
  DataScope source(memory, Slot::Queued, {}, [&] { return Data{counters, 0}; }, DataSizer{});
  EXPECT_THROW(source.mutate([](Data& value) {
    value.heap.resize(32); throw std::runtime_error("controlled population failure");
  }), std::runtime_error);
  EXPECT_EQ(bytes(ledger), source.knownBacking().storage.bytes);
  EXPECT_GT(bytes(ledger), 0u);
}

TEST(WorldGenerationTemporaryTest, OverflowPredictionDoesNotAcquireSlotOrRunFactory) {
  WorldGenerationMemoryLedger ledger;
  WorldGenerationTemporaryMemory memory(ledger);
  auto active = memory.acquire(Slot::Ambient, forecast(1));
  auto counters = std::make_shared<Counters>(); bool ran = false;
  EXPECT_THROW((DataScope(memory, Slot::Queued, forecast(std::numeric_limits<size_t>::max()), [&] {
    ran = true; return Data{counters};
  }, DataSizer{})), std::bad_alloc);
  EXPECT_FALSE(ran);
  EXPECT_EQ(bytes(ledger), 1u);
}

TEST(WorldGenerationTemporaryTest, AdoptionRefusalIsDiagnosticNotADeepHardCapOrValueFilter) {
  WorldGenerationMemoryLedger ledger(0);
  WorldGenerationTemporaryMemory memory(ledger);
  auto counters = std::make_shared<Counters>();
  {
    DataScope source(memory, Slot::Ambient, {}, [&] { return Data{counters}; }, DataSizer{});
    EXPECT_EQ(source.value().heap.size(), 32u);
    EXPECT_EQ(bytes(ledger), 0u);
    EXPECT_FALSE(memory.estimateComplete());
    EXPECT_GT(ledger.snapshot().incompleteEstimateUpdates, 0u);
  }
  EXPECT_EQ(ledger.snapshot().resources, 0u);
}

TEST(WorldGenerationTemporaryTest, IncompleteSizingPreservesDataButRefusesNewCopyPredictions) {
  WorldGenerationMemoryLedger ledger;
  WorldGenerationTemporaryMemory memory(ledger);
  auto outputCounters = std::make_shared<Counters>();
  DataScope output(memory, Slot::Output, {}, [&] { return Data{outputCounters, 0}; }, DataSizer{});
  auto counters = std::make_shared<Counters>(); counters->badSize = true;
  DataScope source(memory, Slot::Ambient, {}, [&] { return Data{counters}; }, DataSizer{});
  EXPECT_FALSE(memory.estimateComplete());
  EXPECT_EQ(source.value().heap.size(), 32u);
  EXPECT_THROW(validateWorldGenerationTemporaryCopy(memory, source, output, validateData), std::bad_alloc);
  EXPECT_EQ(counters->copies, 0u);
}

TEST(WorldGenerationTemporaryTest, TrackerAndScopesRemainValidAfterLedgerFacadeCloses) {
  WorldGenerationMemoryLedger observer;
  std::unique_ptr<WorldGenerationTemporaryMemory> memory;
  { WorldGenerationMemoryLedger facade(observer); memory = std::make_unique<WorldGenerationTemporaryMemory>(facade); }
  auto counters = std::make_shared<Counters>();
  { DataScope value(*memory, Slot::Ambient, {}, [&] { return Data{counters}; }, DataSizer{}); }
  memory.reset();
  EXPECT_EQ(observer.snapshot().reservedBytes, 0u);
}

TEST(WorldGenerationTemporaryTest, RetainedScratchCapacityRemainsChargedAfterClearingElements) {
  WorldGenerationMemoryLedger ledger;
  WorldGenerationTemporaryMemory memory(ledger);
  ItemScope output(memory, Slot::Output, {}, [] { return List<BiomeItemPlacement>{}; }, BiomeItemListBackingSizer{});
  output.mutate([](auto& list) {
    list.reserve(16);
    list.append({BiomeItem{TreasureBoxSet(String(std::string(256, 'x')))}, Vec2I(1, 7), 0.5f});
  });
  auto before = bytes(ledger), raw = output.value().capacity() * sizeof(BiomeItemPlacement);
  output.mutate([](auto& list) { list.clear(); });
  EXPECT_TRUE(output.value().empty());
  EXPECT_EQ(bytes(ledger), raw);
  EXPECT_LT(bytes(ledger), before);
}

TEST(WorldGenerationTemporaryTest, SizerIncludesAllFiveListCapacitiesAndKnownPayloads) {
  auto input = fixture();
  WorldGenerationBackingEstimate expected;
  for (auto list : {&input.surfaceBiomeItems, &input.caveSurfaceBiomeItems,
      &input.caveCeilingBiomeItems, &input.caveBackgroundBiomeItems, &input.oceanItems}) {
    expected.storage.add(list->capacity(), sizeof(BiomeItemPlacement));
    for (auto const& item : *list) expected.add(estimateBiomeItemGenerationBacking(item.item));
  }
  auto actual = PotentialBiomeItemBackingSizer{}(input);
  EXPECT_EQ(actual.storage.bytes, expected.storage.bytes);
  EXPECT_EQ(actual.opaqueItems, expected.opaqueItems);
}

TEST(WorldGenerationTemporaryTest, NativeAmbientAndQueuedScopesBothStayCountedAcrossValidation) {
  WorldGenerationMemoryLedger ledger;
  WorldGenerationTemporaryMemory memory(ledger);
  ItemScope output(memory, Slot::Output, {}, [] { return List<BiomeItemPlacement>{}; }, BiomeItemListBackingSizer{});
  PotentialScope ambient(memory, Slot::Ambient, {}, [] { return fixture(0); }, PotentialBiomeItemBackingSizer{});
  PotentialScope queued(memory, Slot::Queued, {}, [] { return fixture(1); }, PotentialBiomeItemBackingSizer{});
  validateWorldGenerationTemporaryCopy(memory, queued, output,
      [](Potential&& value, auto& result) { selectInto(std::move(value), result); });
  EXPECT_EQ(bytes(ledger), ambient.knownBacking().storage.bytes + queued.knownBacking().storage.bytes
      + output.knownBacking().storage.bytes);
  EXPECT_EQ(ledger.snapshot().classResources[static_cast<size_t>(Kind)], 1u);
}

TEST(WorldGenerationTemporaryTest, OpaqueJsonPayloadIsPreservedThroughTrackedCopy) {
  WorldGenerationMemoryLedger ledger;
  WorldGenerationTemporaryMemory memory(ledger);
  ItemScope output(memory, Slot::Output, {}, [] { return List<BiomeItemPlacement>{}; }, BiomeItemListBackingSizer{});
  PotentialScope source(memory, Slot::Ambient, {}, [] {
    Potential value; ObjectPool objects; objects.add(1, {String("object"), JsonObject{{"kept", true}}});
    value.surfaceBiomeItems.append({BiomeItem{std::move(objects)}, Vec2I(1, 7), 0.5f}); return value;
  }, PotentialBiomeItemBackingSizer{});
  validateWorldGenerationTemporaryCopy(memory, source, output,
      [](Potential&& value, auto& result) { selectInto(std::move(value), result); });
  ASSERT_EQ(output.value().size(), 1u);
  EXPECT_TRUE(output.value()[0].item.get<ObjectPool>().items()[0].second.second.getBool("kept"));
  EXPECT_EQ(output.knownBacking().opaqueItems, 1u);
}

TEST(WorldGenerationTemporaryTest, WorkerCompletesLocalScopesAfterCallerFacadeRetires) {
  WorldGenerationMemoryLedger observer;
  WorkerPool pool("Temporary storage contract", 1);
  std::mutex mutex; std::condition_variable condition;
  bool started = false, released = false;
  Maybe<WorkerPoolPromise<bool>> result;
  {
    WorldGenerationMemoryLedger facade(observer);
    result = pool.addProducer<bool>([&, facade]() mutable {
      WorldGenerationTemporaryMemory memory(facade);
      auto counters = std::make_shared<Counters>();
      DataScope source(memory, Slot::Ambient, {}, [&] { return Data{counters}; }, DataSizer{});
      { std::unique_lock<std::mutex> lock(mutex); started = true; condition.notify_all();
        condition.wait(lock, [&] { return released; }); }
      return source.value().heap.size() == 32;
    });
  }
  { std::unique_lock<std::mutex> lock(mutex);
    EXPECT_TRUE(condition.wait_for(lock, std::chrono::seconds(5), [&] { return started; }));
    EXPECT_GT(bytes(observer), 0u); released = true; }
  condition.notify_all(); EXPECT_TRUE(result->get()); pool.finish(); result.reset();
  EXPECT_EQ(observer.snapshot().reservedBytes, 0u);
}

TEST(WorldGenerationTemporaryTest, EightConcurrentJobsReleaseTheirAggregateReservations) {
  WorldGenerationMemoryLedger ledger;
  std::atomic<unsigned> complete{0}; std::vector<std::thread> threads;
  for (unsigned i = 0; i < 8; ++i) threads.emplace_back([&] {
    auto counters = std::make_shared<Counters>();
    WorldGenerationTemporaryMemory memory(ledger);
    DataScope output(memory, Slot::Output, {}, [&] { return Data{counters, 0}; }, DataSizer{});
    for (unsigned n = 0; n < 16; ++n) {
      DataScope source(memory, Slot::Ambient, {}, [&] { return Data{counters}; }, DataSizer{});
      validateWorldGenerationTemporaryCopy(memory, source, output, validateData);
      output.mutate([](auto& value) { value.heap.clear(); });
    }
    ++complete;
  });
  for (auto& thread : threads) thread.join();
  EXPECT_EQ(complete.load(), 8u);
  EXPECT_EQ(ledger.snapshot().reservedBytes, 0u);
}

TEST(WorldGenerationTemporaryTest, NativeCandidateReplayPreservesGrowthOrderIdsAndChoices) {
  using Candidate = pair<BiomeItemPlacement, Maybe<WorldGenerationPlacementId>>;
  struct Sizer { WorldGenerationBackingEstimate operator()(Candidate const& c) const noexcept {
    return estimateBiomeItemGenerationBacking(c.first.item);
  }};
  WorldGenerationMemoryLedger actualLedger, referenceLedger;
  WorldGenerationPayloadBuffer<Candidate, Sizer> actual(actualLedger, Sizer{});
  std::vector<Candidate, WorldGenerationBufferAllocator<Candidate>> reference(
      WorldGenerationBufferAllocator<Candidate>(referenceLedger, WorldGenerationMemoryClass::MicroDungeonCandidateBuffer));
  WorldGenerationTemporaryMemory memory(actualLedger);
  ItemScope output(memory, Slot::Output, {}, [] { return List<BiomeItemPlacement>{}; }, BiomeItemListBackingSizer{});
  List<BiomeItemPlacement> oldOutput;
  for (unsigned point = 0; point < 24; ++point) {
    unsigned mode = point % 2; auto id = WorldGenerationPlacementId(7 + mode);
    auto oldAmbient = fixture(point), oldQueued = fixture(point);
    selectInto(oldAmbient, oldOutput, mode);
    for (auto const& item : oldOutput) reference.push_back({item, {}});
    oldOutput.clear(); selectInto(oldQueued, oldOutput, mode);
    for (auto item : oldOutput) reference.push_back({std::move(item), id});
    oldOutput.clear();
    PotentialScope ambient(memory, Slot::Ambient, {}, [&] { return fixture(point); }, PotentialBiomeItemBackingSizer{});
    auto validate = [&](Potential&& input, auto& result) { selectInto(std::move(input), result, mode); };
    validateWorldGenerationTemporaryCopy(memory, ambient, output, validate);
    for (auto const& item : output.value())
      actual.append(estimateBiomeItemGenerationBacking(item.item), [&] { return Candidate{item, {}}; });
    output.mutate([](auto& items) { items.clear(); });
    PotentialScope queued(memory, Slot::Queued, {}, [&] { return fixture(point); }, PotentialBiomeItemBackingSizer{});
    validateWorldGenerationTemporaryCopy(memory, queued, output, validate);
    for (auto item : output.value())
      actual.append(estimateBiomeItemGenerationBacking(item.item), [&] { return Candidate{std::move(item), id}; });
    output.mutate([](auto& items) { items.clear(); });
    EXPECT_EQ(actual.capacity(), reference.capacity());
  }
  actual.sort(); std::sort(reference.begin(), reference.end(), std::less<Candidate>{});
  ASSERT_EQ(actual.size(), 432u); ASSERT_EQ(actual.size(), reference.size());
  size_t index = 0;
  for (auto const& current : actual) {
    auto const& previous = reference[index++];
    EXPECT_EQ(current.first.position, previous.first.position); EXPECT_EQ(current.second, previous.second);
    EXPECT_EQ(current.first.priority, previous.first.priority);
    ASSERT_EQ(current.first.item.is<MicroDungeonNames>(), previous.first.item.is<MicroDungeonNames>());
    if (current.first.item.is<MicroDungeonNames>()) {
      auto const& a = current.first.item.get<MicroDungeonNames>(); auto const& b = previous.first.item.get<MicroDungeonNames>();
      EXPECT_EQ(a.values(), b.values());
      for (uint64_t seed = 0; seed < 64; ++seed) EXPECT_EQ(staticRandomFrom(a, seed), staticRandomFrom(b, seed));
    }
  }
}
