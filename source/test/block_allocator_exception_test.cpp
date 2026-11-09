#include "StarBlockAllocator.hpp"
#include "StarWorldGenerationWorkerCache.hpp"

#include <new>

#include "gtest/gtest.h"

namespace Star {

// No global new override, runtime hook, allocator policy or object layout
// change. Exercise the same addBlock path used by allocate(1).
struct BlockAllocatorFailureTestAccess {
  template <typename Exception = std::bad_alloc, typename T, size_t N>
  static void failInsertion(BlockAllocator<T, N>& allocator) {
    using Block = typename BlockAllocator<T, N>::Block;
    allocator.addBlock([] { return make_unique<Block>(); },
        [](auto&, auto, auto&&) { throw Exception(); });
  }

  template <typename T, size_t N>
  static void failCreation(BlockAllocator<T, N>& allocator) {
    using Block = typename BlockAllocator<T, N>::Block;
    allocator.addBlock([]() -> unique_ptr<Block> { throw std::bad_alloc(); },
        [](auto& blocks, auto position, auto&& block) {
          blocks.insert(position, std::move(block));
        });
  }

  template <typename T, size_t N>
  static bool selectionIsOwned(BlockAllocator<T, N> const& allocator) {
    auto selected = allocator.m_data->unfilledBlock;
    if (!selected) return true;
    for (auto const& block : allocator.m_data->blocks)
      if (selected == block.get()) return true;
    return false;
  }

  template <typename T, size_t N>
  static size_t capacity(BlockAllocator<T, N> const& allocator) {
    return allocator.m_data->blocks.capacity();
  }

  template <typename T, size_t N>
  static size_t blocks(BlockAllocator<T, N> const& allocator) {
    return allocator.m_data->blocks.size();
  }
};

}

using namespace Star;

namespace {
struct Cell { unsigned value; };
struct OtherCell { unsigned value; };
using Pool = BlockAllocator<Cell, 2>;
using Access = BlockAllocatorFailureTestAccess;

Cell* cell(Pool& pool, unsigned value) {
  auto pointer = pool.allocate(1);
  pool.construct(pointer, Cell{value});
  return pointer;
}

void release(Pool& pool, std::vector<Cell*>& pointers) {
  for (auto pointer : pointers) { pool.destroy(pointer); pool.deallocate(pointer, 1); }
  pointers.clear();
}

std::vector<Cell*> fillCapacity(Pool& pool) {
  std::vector<Cell*> pointers;
  auto count = Access::capacity(pool) * 2;
  for (size_t i = 0; i < count; ++i) pointers.push_back(cell(pool, i));
  return pointers;
}
}

TEST(BlockAllocatorExceptionTest, CreationFailureLeavesAnEmptyPoolRecoverable) {
  Pool pool;
  auto before = pool.requestedStorageBytes();
  EXPECT_THROW(Access::failCreation(pool), std::bad_alloc);
  EXPECT_EQ(pool.requestedStorageBytes(), before);
  ASSERT_TRUE(Access::selectionIsOwned(pool));
  auto pointer = cell(pool, 42);
  EXPECT_EQ(pointer->value, 42u);
  pool.destroy(pointer); pool.deallocate(pointer, 1);
}

TEST(BlockAllocatorExceptionTest, InsertionFailureInEmptyReboundPoolDoesNotPublishFreedBlock) {
  BlockAllocator<OtherCell, 2> source;
  Pool pool(source);
  ASSERT_EQ(Access::capacity(pool), 0u);
  auto before = pool.requestedStorageBytes();
  EXPECT_THROW(Access::failInsertion(pool), std::bad_alloc);
  EXPECT_EQ(Access::blocks(pool), 0u);
  EXPECT_EQ(pool.requestedStorageBytes(), before);
  // Stop here on regression; never dereference the dangling pointer.
  ASSERT_TRUE(Access::selectionIsOwned(pool));
  auto pointer = cell(pool, 7);
  EXPECT_EQ(pointer->value, 7u);
  pool.destroy(pointer); pool.deallocate(pointer, 1);
}

TEST(BlockAllocatorExceptionTest, FullVectorInsertionFailurePreservesLiveValuesAndRetry) {
  Pool pool;
  auto pointers = fillCapacity(pool);
  ASSERT_EQ(Access::blocks(pool), Access::capacity(pool));
  auto before = pool.requestedStorageBytes();
  EXPECT_THROW(Access::failInsertion(pool), std::bad_alloc);
  EXPECT_EQ(pool.requestedStorageBytes(), before);
  for (size_t i = 0; i < pointers.size(); ++i) EXPECT_EQ(pointers[i]->value, i);
  ASSERT_TRUE(Access::selectionIsOwned(pool));
  auto pointer = cell(pool, 123);
  EXPECT_EQ(pointer->value, 123u);
  pointers.push_back(pointer);
  release(pool, pointers);
}

TEST(BlockAllocatorExceptionTest, CreationFailureInFullPoolPreservesLiveValuesAndRetry) {
  Pool pool;
  auto pointers = fillCapacity(pool);
  auto before = pool.requestedStorageBytes();
  EXPECT_THROW(Access::failCreation(pool), std::bad_alloc);
  EXPECT_EQ(pool.requestedStorageBytes(), before);
  ASSERT_TRUE(Access::selectionIsOwned(pool));
  for (size_t i = 0; i < pointers.size(); ++i) EXPECT_EQ(pointers[i]->value, i);
  pointers.push_back(cell(pool, 1000));
  release(pool, pointers);
}

TEST(BlockAllocatorExceptionTest, RepeatedInsertionFailuresDoNotAccumulateBlocksOrBytes) {
  BlockAllocator<OtherCell, 2> source;
  Pool pool(source);
  auto before = pool.requestedStorageBytes();
  for (unsigned i = 0; i < 50; ++i) {
    EXPECT_THROW(Access::failInsertion(pool), std::bad_alloc);
    EXPECT_EQ(Access::blocks(pool), 0u);
    EXPECT_EQ(pool.requestedStorageBytes(), before);
    ASSERT_TRUE(Access::selectionIsOwned(pool));
  }
  auto pointer = cell(pool, 99);
  pool.destroy(pointer); pool.deallocate(pointer, 1);
}

TEST(BlockAllocatorExceptionTest, SharedAliasRecoversWithoutInvalidatingAnotherReboundType) {
  BlockAllocator<OtherCell, 2> source;
  auto other = source.allocate(1);
  source.construct(other, OtherCell{77});
  Pool pool(source), alias(pool);
  auto before = pool.requestedStorageBytes();
  EXPECT_THROW(Access::failInsertion(alias), std::bad_alloc);
  EXPECT_EQ(pool.requestedStorageBytes(), before);
  EXPECT_EQ(source.requestedStorageBytes(), before);
  EXPECT_EQ(other->value, 77u);
  ASSERT_TRUE(Access::selectionIsOwned(pool));
  auto pointer = cell(pool, 88);
  EXPECT_EQ(pointer->value, 88u);
  alias.destroy(pointer); alias.deallocate(pointer, 1);
  source.destroy(other); source.deallocate(other, 1);
}

TEST(BlockAllocatorExceptionTest, NonAllocationInsertionExceptionAlsoLeavesPoolRecoverable) {
  struct InjectedException {};
  BlockAllocator<OtherCell, 2> source;
  Pool pool(source);
  auto before = pool.requestedStorageBytes();
  EXPECT_THROW(Access::failInsertion<InjectedException>(pool), InjectedException);
  EXPECT_EQ(pool.requestedStorageBytes(), before);
  ASSERT_TRUE(Access::selectionIsOwned(pool));
  auto pointer = cell(pool, 11);
  pool.destroy(pointer); pool.deallocate(pointer, 1);
}

TEST(BlockAllocatorExceptionTest, CacheScopeUnwindsAndRetainedPoolCanRetryNextJob) {
  struct Payload {
    Pool pool;
    std::vector<Cell*> pointers;
    ~Payload() { release(pool, pointers); }
  };
  WorldGenerationMemoryLedger ledger;
  auto source = std::make_shared<unsigned>(0);
  auto measure = +[](Payload const& p) {
    return sizeof(Payload) + p.pool.requestedStorageBytes() + p.pointers.capacity() * sizeof(Cell*);
  };
  auto producer = [] { return std::make_shared<Payload>(); };
  {
    WorldGenerationWorkerCache<unsigned, Payload> cache;
    size_t before = 0;
    {
      auto use = cache.get(source, 1, ledger, sizeof(Payload), measure, producer);
      use.value()->pointers = fillCapacity(use.value()->pool);
      before = measure(*use.value());
    }
    EXPECT_EQ(ledger.snapshot().estimatedBytes, before);
    try {
      auto use = cache.get(source, 1, ledger, sizeof(Payload), measure, producer);
      Access::failInsertion(use.value()->pool);
      FAIL() << "controlled insertion did not throw";
    } catch (std::bad_alloc const&) {}
    EXPECT_EQ(ledger.snapshot().estimatedBytes, before);
    {
      auto use = cache.get(source, 1, ledger, sizeof(Payload), measure, producer);
      ASSERT_TRUE(Access::selectionIsOwned(use.value()->pool));
      use.value()->pointers.push_back(cell(use.value()->pool, 55));
      EXPECT_EQ(use.value()->pointers.back()->value, 55u);
    }
    EXPECT_GT(ledger.snapshot().estimatedBytes, before);
  }
  EXPECT_EQ(ledger.snapshot().estimatedBytes, 0u);
}
