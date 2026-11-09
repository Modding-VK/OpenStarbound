#include "StarWorldGenerationNamePool.hpp"
#include "StarBiomeItemSelection.hpp"
#include "StarBiomePlacementTypes.hpp"
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
constexpr auto NameBuffer = WorldGenerationMemoryClass::MicroDungeonNameBuffer;
constexpr auto NameEstimate = WorldGenerationMemoryClass::MicroDungeonNameEstimate;
size_t bytes(WorldGenerationMemoryLedger const& ledger, WorldGenerationMemoryClass kind) {
  return ledger.snapshot().classBytes[static_cast<size_t>(kind)];
}
String longName(char c = 'a') { return String(std::string(1024, c)); }

struct Block {
  bool biomeTransition = false, terrain = false, foregroundCave = false, backgroundCave = false;
  LiquidId oceanLiquid = EmptyLiquidId;
  int oceanLiquidLevel = 0;
};
struct Potential {
  List<BiomeItemPlacement> surfaceBiomeItems, caveSurfaceBiomeItems, caveCeilingBiomeItems,
      caveBackgroundBiomeItems, oceanItems;
};
Potential potential() {
  Potential result;
  unsigned id = 0;
  for (auto list : {&result.surfaceBiomeItems, &result.caveSurfaceBiomeItems, &result.caveCeilingBiomeItems,
      &result.caveBackgroundBiomeItems, &result.oceanItems}) {
    for (unsigned i = 0; i < 2; ++i) {
      ++id;
      list->append({BiomeItem{TreasureBoxSet(toString(id))}, Vec2I(id, 7), float(id)});
    }
  }
  return result;
}

// Independent reference retaining the inherited conditions, order, pass-by-value
// copy and list moves. This does not fake Root or the terrain/cache queries.
List<BiomeItemPlacement> legacy(int y, Block const& block, Block const& above, Block const& below, Potential values) {
  if (block.biomeTransition) return {};
  List<BiomeItemPlacement> result;
  if (!below.biomeTransition && below.terrain && !block.terrain && !below.foregroundCave)
    result.appendAll(std::move(values.surfaceBiomeItems));
  if (!below.biomeTransition && below.terrain && block.terrain && !below.foregroundCave && block.foregroundCave)
    result.appendAll(std::move(values.caveSurfaceBiomeItems));
  if (!above.biomeTransition && above.terrain && block.terrain && !above.foregroundCave && block.foregroundCave)
    result.appendAll(std::move(values.caveCeilingBiomeItems));
  if (block.terrain && block.foregroundCave && !block.backgroundCave)
    result.appendAll(std::move(values.caveBackgroundBiomeItems));
  if (block.oceanLiquid != EmptyLiquidId && y == block.oceanLiquidLevel)
    result.appendAll(std::move(values.oceanItems));
  return result;
}
void selectInto(int y, Block const& block, Block const& above, Block const& below,
    Potential values, List<BiomeItemPlacement>& result) {
  resetBiomeItemSelectionOutput(values, result);
  appendSelectedBiomeItems(y, block, above, below, values, result);
}
}

TEST(WorldGenerationPlanningStorageTest, RepeatedNamesShareOneImmutableStringWithoutGrowingRequests) {
  WorldGenerationMemoryLedger ledger;
  {
    WorldGenerationNamePool pool(ledger);
    auto first = pool.intern(longName());
    auto before = ledger.snapshot();
    for (unsigned i = 0; i < 1000; ++i) {
      auto same = pool.intern(longName());
      EXPECT_EQ(same, first);
      EXPECT_EQ(same->value().utf8Ptr(), first->value().utf8Ptr());
    }
    EXPECT_EQ(pool.size(), 1u);
    EXPECT_EQ(ledger.snapshot().reservedBytes, before.reservedBytes);
    EXPECT_EQ(ledger.snapshot().peakReservedBytes, before.peakReservedBytes);
    EXPECT_TRUE(first->estimateComplete());
    static_assert(std::is_const<WorldGenerationName::element_type>::value, "Output names are immutable");
  }
  EXPECT_EQ(ledger.snapshot().reservedBytes, 0u);
}

TEST(WorldGenerationPlanningStorageTest, DistinctNamesAndCallerLifetimeDoNotAlias) {
  WorldGenerationMemoryLedger ledger;
  WorldGenerationNamePool pool(ledger);
  String source = longName();
  auto a = pool.intern(source);
  source = longName('b');
  auto b = pool.intern(source);
  source.clear();
  EXPECT_NE(a, b);
  EXPECT_EQ(a->value(), longName());
  EXPECT_EQ(b->value(), longName('b'));
  EXPECT_EQ(pool.intern(longName()), a);
  EXPECT_EQ(pool.size(), 2u);
}

TEST(WorldGenerationPlanningStorageTest, MatchingIsExactForCaseUtf8EmbeddedNullAndEmptyNames) {
  WorldGenerationMemoryLedger ledger;
  WorldGenerationNamePool pool(ledger);
  std::vector<String> values{String(""), String("Alpha"), String("alpha"), String("á🌙"), String("a\0b", 3), String("a")};
  std::vector<WorldGenerationName> names;
  for (auto const& value : values) names.push_back(pool.intern(value));
  EXPECT_EQ(pool.size(), values.size());
  for (size_t i = 0; i < values.size(); ++i) {
    EXPECT_EQ(names[i]->value(), values[i]);
    EXPECT_EQ(pool.intern(values[i]), names[i]);
  }
}

TEST(WorldGenerationPlanningStorageTest, NoNamesDoNotAllocateIndexStorage) {
  WorldGenerationMemoryLedger ledger;
  { WorldGenerationNamePool pool(ledger); EXPECT_EQ(pool.size(), 0u); }
  EXPECT_EQ(ledger.snapshot().resources, 0u);
  EXPECT_EQ(ledger.snapshot().peakReservedBytes, 0u);
}

TEST(WorldGenerationPlanningStorageTest, RetiredIndexLeavesOnlyNamesHeldByOutputs) {
  WorldGenerationMemoryLedger ledger;
  WorldGenerationName held;
  size_t during = 0;
  {
    WorldGenerationNamePool pool(ledger);
    held = pool.intern(longName());
    pool.intern(longName('b'));
    during = bytes(ledger, NameBuffer);
  }
  EXPECT_GT(bytes(ledger, NameBuffer), 0u);
  EXPECT_LT(bytes(ledger, NameBuffer), during);
  EXPECT_EQ(bytes(ledger, NameEstimate), held->value().utf8().capacity() + 1);
  EXPECT_EQ(held->value(), longName());
  held.reset();
  EXPECT_EQ(ledger.snapshot().reservedBytes, 0u);
}

TEST(WorldGenerationPlanningStorageTest, WeakOwnerRetainsControlBlockButNotStringEstimate) {
  WorldGenerationMemoryLedger ledger;
  std::weak_ptr<WorldGenerationRetainedName const> weak;
  {
    WorldGenerationNamePool pool(ledger);
    weak = pool.intern(longName());
  }
  EXPECT_TRUE(weak.expired());
  EXPECT_EQ(bytes(ledger, NameEstimate), 0u);
  EXPECT_GT(bytes(ledger, NameBuffer), 0u);
  weak.reset();
  EXPECT_EQ(ledger.snapshot().reservedBytes, 0u);
}

TEST(WorldGenerationPlanningStorageTest, PoolAndOutputsSurviveOriginalFacade) {
  WorldGenerationMemoryLedger observer;
  std::unique_ptr<WorldGenerationNamePool> pool;
  {
    WorldGenerationMemoryLedger facade(observer);
    pool = std::make_unique<WorldGenerationNamePool>(facade);
  }
  auto name = pool->intern(longName());
  pool.reset();
  EXPECT_EQ(name->value(), longName());
  name.reset();
  EXPECT_EQ(observer.snapshot().reservedBytes, 0u);
}

TEST(WorldGenerationPlanningStorageTest, FullyReservedLedgerRefusesNewNameAndAllowsExistingLookup) {
  constexpr size_t limit = 1024 * 1024;
  WorldGenerationMemoryLedger ledger(limit);
  WorldGenerationNamePool pool(ledger);
  auto original = pool.intern(longName());
  auto before = ledger.snapshot().reservedBytes;
  auto blocker = ledger.reserve(WorldGenerationMemoryClass::BaseTilesArray, limit - before);
  EXPECT_THROW(pool.intern(longName('b')), std::bad_alloc);
  EXPECT_EQ(pool.intern(longName()), original);
  EXPECT_EQ(pool.size(), 1u);
  EXPECT_EQ(ledger.snapshot().reservedBytes, limit);
  blocker.reset();
  EXPECT_EQ(ledger.snapshot().reservedBytes, before);
  EXPECT_TRUE(pool.intern(longName('b')));
}

TEST(WorldGenerationPlanningStorageTest, PartialCapacityFailuresRollBackAndPermitRetry) {
  // Observe this implementation's shared-object request, excluding the index.
  // Do not assume libstdc++/libc++ control-block or unordered node sizes.
  WorldGenerationMemoryLedger probe;
  WorldGenerationName held;
  { WorldGenerationNamePool pool(probe); held = pool.intern(longName()); }
  auto backing = bytes(probe, NameEstimate), metadata = bytes(probe, NameBuffer);
  ASSERT_GT(metadata, 0u);
  for (size_t available : {size_t(0), backing, backing + metadata}) {
    constexpr size_t limit = 1024 * 1024;
    WorldGenerationMemoryLedger ledger(limit);
    WorldGenerationNamePool pool(ledger);
    auto original = pool.intern(longName());
    auto before = ledger.snapshot().reservedBytes;
    auto blocker = ledger.reserve(WorldGenerationMemoryClass::BaseTilesArray, limit - before - available);
    EXPECT_THROW(pool.intern(longName('b')), std::bad_alloc);
    EXPECT_EQ(pool.size(), 1u);
    EXPECT_EQ(pool.intern(longName()), original);
    EXPECT_EQ(ledger.snapshot().reservedBytes, limit - available);
    blocker.reset();
    EXPECT_EQ(ledger.snapshot().reservedBytes, before);
    EXPECT_TRUE(pool.intern(longName('b')));
  }
}

TEST(WorldGenerationPlanningStorageTest, RehashKeepsOwnedKeyPointersAndOutputHandlesValid) {
  WorldGenerationMemoryLedger ledger;
  WorldGenerationNamePool pool(ledger);
  std::vector<WorldGenerationName> values;
  for (unsigned i = 0; i < 256; ++i) values.push_back(pool.intern(toString(i)));
  EXPECT_EQ(pool.size(), 256u);
  for (unsigned i = 0; i < 256; ++i) {
    EXPECT_EQ(pool.intern(toString(i)), values[i]);
    EXPECT_EQ(values[i]->value(), toString(i));
  }
}

TEST(WorldGenerationPlanningStorageTest, WorkerRetainsNameAfterPoolAndFacadeRetire) {
  WorldGenerationMemoryLedger observer;
  WorkerPool workers("Retained output name contract", 1);
  std::mutex mutex;
  std::condition_variable condition;
  bool started = false, released = false;
  Maybe<WorkerPoolPromise<bool>> result;
  {
    WorldGenerationMemoryLedger facade(observer);
    WorldGenerationNamePool pool(facade);
    auto name = pool.intern(longName());
    result = workers.addProducer<bool>([&, name] {
      std::unique_lock<std::mutex> lock(mutex);
      started = true; condition.notify_all();
      condition.wait(lock, [&] { return released; });
      return name->value() == longName();
    });
  }
  {
    std::unique_lock<std::mutex> lock(mutex);
    EXPECT_TRUE(condition.wait_for(lock, std::chrono::seconds(5), [&] { return started; }));
    EXPECT_GT(observer.snapshot().reservedBytes, 0u);
    released = true;
  }
  condition.notify_all();
  EXPECT_TRUE(result->get());
  workers.finish();
  result.reset();
  EXPECT_EQ(observer.snapshot().reservedBytes, 0u);
}

TEST(WorldGenerationPlanningStorageTest, SharedAndEstimatedRequestsStayDisjoint) {
  WorldGenerationMemoryLedger ledger;
  WorldGenerationNamePool pool(ledger);
  auto name = pool.intern(longName());
  EXPECT_EQ(ledger.snapshot().reservedBytes, bytes(ledger, NameBuffer) + bytes(ledger, NameEstimate));
  EXPECT_EQ(ledger.snapshot().reservedBufferBytes, bytes(ledger, NameBuffer));
  EXPECT_EQ(ledger.snapshot().estimatedBytes, bytes(ledger, NameEstimate));
  auto fixed = ledger.reserve(NameBuffer, 4);
  EXPECT_FALSE(fixed->resizeEstimate(5));
}

TEST(WorldGenerationPlanningStorageTest, IndependentJobsDoNotCreateAProcessGlobalNameCache) {
  WorldGenerationMemoryLedger ledger;
  WorldGenerationNamePool first(ledger), second(ledger);
  auto a = first.intern(longName()), b = second.intern(longName());
  EXPECT_NE(a, b);
  EXPECT_EQ(a->value(), b->value());
}

TEST(WorldGenerationPlanningStorageTest, EightConcurrentReadersUseRetainedValuesAfterIndexRetires) {
  WorldGenerationMemoryLedger ledger;
  WorldGenerationName value;
  { WorldGenerationNamePool pool(ledger); value = pool.intern(longName()); }
  std::atomic<unsigned> failures{0};
  std::vector<std::thread> readers;
  for (unsigned i = 0; i < 8; ++i) readers.emplace_back([value, &failures] {
    for (unsigned n = 0; n < 128; ++n) if (value->value() != longName()) ++failures;
  });
  for (auto& reader : readers) reader.join();
  EXPECT_EQ(failures.load(), 0u);
  value.reset();
  EXPECT_EQ(ledger.snapshot().reservedBytes, 0u);
}

TEST(WorldGenerationPlanningStorageTest, All4096GeometryCombinationsMatchInheritedSelectionAndOrder) {
  auto source = potential();
  List<BiomeItemPlacement> scratch;
  for (unsigned flags = 0; flags < 4096; ++flags) {
    Block block, above, below;
    block.biomeTransition = flags & 1; block.terrain = flags & 2;
    block.foregroundCave = flags & 4; block.backgroundCave = flags & 8;
    below.biomeTransition = flags & 16; below.terrain = flags & 32; below.foregroundCave = flags & 64;
    above.biomeTransition = flags & 128; above.terrain = flags & 256; above.foregroundCave = flags & 512;
    block.oceanLiquid = (flags & 1024) ? 1 : 0; block.oceanLiquidLevel = (flags & 2048) ? 7 : 8;
    auto expected = legacy(7, block, above, below, source);
    selectInto(7, block, above, below, source, scratch);
    ASSERT_EQ(scratch.size(), expected.size()) << flags;
    for (size_t i = 0; i < scratch.size(); ++i) {
      EXPECT_EQ(scratch[i].item.get<TreasureBoxSet>(), expected[i].item.get<TreasureBoxSet>()) << flags;
      EXPECT_EQ(scratch[i].position, expected[i].position);
      EXPECT_EQ(scratch[i].priority, expected[i].priority);
    }
  }
}

TEST(WorldGenerationPlanningStorageTest, ScratchRetainsCapacityButClearsStaleResults) {
  Block block, below, above;
  below.terrain = true;
  auto source = potential();
  List<BiomeItemPlacement> scratch;
  selectInto(7, block, above, below, source, scratch);
  ASSERT_EQ(scratch.size(), 2u);
  auto storage = scratch.ptr(); auto capacity = scratch.capacity();
  for (unsigned i = 0; i < 256; ++i) {
    selectInto(7, block, above, below, source, scratch);
    EXPECT_EQ(scratch.ptr(), storage);
    EXPECT_EQ(scratch.capacity(), capacity);
    EXPECT_EQ(scratch.size(), 2u);
    scratch.clear();
  }
  block.biomeTransition = true;
  selectInto(7, block, above, below, source, scratch);
  EXPECT_TRUE(scratch.empty());
  EXPECT_EQ(scratch.capacity(), capacity);
}

TEST(WorldGenerationPlanningStorageTest, ReusedScratchPreservesMicroDungeonCopyOrderAndStaticChoice) {
  Potential source;
  MicroDungeonNames names(StringSet{"alpha", "beta", "gamma", "delta"});
  names.reserve(257);
  source.surfaceBiomeItems.append({BiomeItem{names}, Vec2I(1, 7), 0.5f});
  Block block, above, below; below.terrain = true;
  List<BiomeItemPlacement> scratch;
  for (unsigned iteration = 0; iteration < 64; ++iteration) {
    auto expected = legacy(7, block, above, below, source);
    selectInto(7, block, above, below, source, scratch);
    ASSERT_EQ(scratch.size(), 1u);
    auto const& a = expected[0].item.get<MicroDungeonNames>();
    auto const& b = scratch[0].item.get<MicroDungeonNames>();
    EXPECT_EQ(a.values(), b.values());
    EXPECT_EQ(staticRandomFrom(a, uint64_t(iteration)), staticRandomFrom(b, uint64_t(iteration)));
  }
}

TEST(WorldGenerationPlanningStorageTest, InterningAfterSelectionPreservesEachPlanNameAndSequence) {
  WorldGenerationMemoryLedger ledger;
  WorldGenerationNamePool pool(ledger);
  StringSet candidates{"alpha", "beta", "gamma", "delta"};
  std::vector<String> expected;
  std::vector<WorldGenerationName> current;
  for (uint64_t seed = 0; seed < 256; ++seed) {
    auto const& chosen = staticRandomFrom(candidates, seed);
    expected.push_back(chosen);
    current.push_back(pool.intern(chosen));
  }
  for (size_t i = 0; i < current.size(); ++i) EXPECT_EQ(current[i]->value(), expected[i]);
  EXPECT_LE(pool.size(), candidates.size());
}

TEST(WorldGenerationPlanningStorageTest, WraparoundHashCollisionsRequireFreshMicroDungeonOutputStorage) {
  StringSet colliding;
  for (unsigned i = 0; colliding.size() < 3; ++i) {
    auto name = toString(i);
    if ((hash<String>{}(name) & 7) == 7) colliding.add(name);
  }
  MicroDungeonNames names(std::move(colliding));
  // Normalization is not necessarily idempotent for a wrapped cluster.
  auto copy = names;
  ASSERT_NE(names.values(), copy.values());
  Potential source;
  for (unsigned i = 0; i < 5; ++i)
    source.surfaceBiomeItems.append({BiomeItem{names}, Vec2I(i, 7), 0.5f});
  Block block, above, below; below.terrain = true;
  List<BiomeItemPlacement> guarded, unsafe;
  bool reproduced = false, choiceChanged = false;
  for (unsigned iteration = 0; iteration < 64; ++iteration) {
    auto expected = legacy(7, block, above, below, source);
    auto values = source;
    unsafe.clear();
    appendSelectedBiomeItems(7, block, above, below, values, unsafe);
    selectInto(7, block, above, below, source, guarded);
    ASSERT_EQ(guarded.size(), expected.size());
    for (size_t i = 0; i < expected.size(); ++i) {
      auto const& reference = expected[i].item.get<MicroDungeonNames>();
      auto const& actual = guarded[i].item.get<MicroDungeonNames>();
      EXPECT_EQ(reference.values(), actual.values());
      EXPECT_EQ(staticRandomFrom(reference, uint64_t(iteration)), staticRandomFrom(actual, uint64_t(iteration)));
      if (reference.values() != unsafe[i].item.get<MicroDungeonNames>().values()) reproduced = true;
      if (staticRandomFrom(reference, uint64_t(iteration))
          != staticRandomFrom(unsafe[i].item.get<MicroDungeonNames>(), uint64_t(iteration))) choiceChanged = true;
    }
  }
  EXPECT_TRUE(reproduced); // Control without the guard really changes selection order.
  EXPECT_TRUE(choiceChanged);
}
