#pragma once

#include <array>
#include <vector>
#include <unordered_map>
#include <limits>
#include <typeindex>

#include "StarException.hpp"

namespace Star {

// Constant size only allocator using fixed size blocks of memory.  much faster
// than general purpose allocators, but not thread safe.  Useful as the
// allocator for containers that mostly allocate one element at a time, such as
// std::list, std::map, std::set etc.
template <typename T, size_t BlockSize>
class BlockAllocator {
public:
  typedef T value_type;

  typedef T* pointer;
  typedef T const* const_pointer;

  typedef T& reference;
  typedef T const& const_reference;

  // Allocator can be shared, but since it is NOT thread safe this should not
  // be done by default.
  typedef std::false_type propagate_on_container_copy_assignment;
  typedef std::true_type propagate_on_container_move_assignment;
  typedef std::true_type propagate_on_container_swap;

  template <class U>
  struct rebind {
    typedef BlockAllocator<U, BlockSize> other;
  };

  BlockAllocator();
  // Copy constructed BlockAllocators of the same type share underlying
  // resources.
  BlockAllocator(BlockAllocator const& other) = default;
  BlockAllocator(BlockAllocator&& other) = default;
  // Rebound types share the family, with a separate pool per element type.
  template <class U>
  BlockAllocator(BlockAllocator<U, BlockSize> const& other);

  BlockAllocator& operator=(BlockAllocator const& rhs) = default;
  BlockAllocator& operator=(BlockAllocator&& rhs) = default;

  // If n is != 1, will fall back on std::allocator<T>
  T* allocate(size_t n);
  void deallocate(T* p, size_t n);

  // Owner-thread diagnostic across this allocator's rebound family. Includes
  // pooled blocks, pool bookkeeping and live multi-element requests, not
  // malloc overhead, family hash buckets or payload-owned allocations.
  size_t requestedStorageBytes() const;

  template <typename... Args>
  void construct(pointer p, Args&&... args) const;
  void destroy(pointer p) const;

  // Allocators compare equal when they share the same family.
  template <class U>
  bool operator==(BlockAllocator<U, BlockSize> const& rhs) const;
  template <class U>
  bool operator!=(BlockAllocator<U, BlockSize> const& rhs) const;

private:
  template <typename OtherT, size_t OtherBlockSize>
  friend class BlockAllocator;
  friend struct BlockAllocatorFailureTestAccess;

  using ChunkIndex =
    std::conditional_t<BlockSize <= std::numeric_limits<uint8_t>::max(), uint8_t,
      std::conditional_t<BlockSize <= std::numeric_limits<uint16_t>::max(), uint16_t,
        std::conditional_t<BlockSize <= std::numeric_limits<uint32_t>::max(), uint32_t,
          std::conditional_t<BlockSize <= std::numeric_limits<uint64_t>::max(), uint64_t, uintmax_t>>>>;

  static ChunkIndex const NullChunkIndex = std::numeric_limits<ChunkIndex>::max();

  struct Unallocated {
    ChunkIndex prev;
    ChunkIndex next;
  };

  typedef std::aligned_union_t<0, T, Unallocated> Chunk;

  struct Block {
    T* allocate();
    void deallocate(T* ptr);

    bool full() const;
    bool empty() const;

    Chunk* chunkPointer(ChunkIndex chunkIndex);

    std::array<Chunk, BlockSize> chunks;
    ChunkIndex firstUnallocated = NullChunkIndex;
    ChunkIndex allocationCount = 0;
  };

  struct Data {
    std::vector<unique_ptr<Block>> blocks;
    Block* unfilledBlock;
    std::allocator<T> multiAllocator;
    size_t multiBytes = 0;
  };

  typedef std::unordered_map<std::type_index,
      pair<shared_ptr<void>, size_t (*)(void const*)>> BlockAllocatorFamily;

  static Data* getAllocatorData(BlockAllocatorFamily& family);

  // Compile-time seam for the two throwing steps. Production supplies the
  // normal make_unique/vector insert; fault injectors exist only in tests.
  template <typename Producer, typename Inserter>
  void addBlock(Producer produce, Inserter insert);

  shared_ptr<BlockAllocatorFamily> m_family;
  Data* m_data;
};

template <typename T, size_t BlockSize>
BlockAllocator<T, BlockSize>::BlockAllocator() {
  m_family = make_shared<BlockAllocatorFamily>();
  m_data = getAllocatorData(*m_family);
  m_data->blocks.reserve(32);
  m_data->unfilledBlock = nullptr;
}

template <typename T, size_t BlockSize>
template <class U>
BlockAllocator<T, BlockSize>::BlockAllocator(BlockAllocator<U, BlockSize> const& other)
  : m_family(other.m_family) {
  m_data = getAllocatorData(*m_family);
}

template <typename T, size_t BlockSize>
T* BlockAllocator<T, BlockSize>::allocate(size_t n) {
  if (n == 1) {
    if (m_data->unfilledBlock == nullptr) {
      for (auto const& p : m_data->blocks) {
        if (!p->full()) {
          m_data->unfilledBlock = p.get();
          break;
        }
      }

      if (!m_data->unfilledBlock) {
        addBlock([] { return make_unique<Block>(); },
            [](auto& blocks, auto position, auto&& block) {
              blocks.insert(position, std::move(block));
            });
      }
    }

    auto allocated = m_data->unfilledBlock->allocate();
    if (m_data->unfilledBlock->full())
      m_data->unfilledBlock = nullptr;
    return allocated;
  } else {
    auto allocated = m_data->multiAllocator.allocate(n);
    m_data->multiBytes += n * sizeof(T);
    return allocated;
  }
}

template <typename T, size_t BlockSize>
template <typename Producer, typename Inserter>
void BlockAllocator<T, BlockSize>::addBlock(Producer produce, Inserter insert) {
  auto block = produce();
  auto newBlock = block.get();
  auto sortedPosition = std::lower_bound(m_data->blocks.begin(), m_data->blocks.end(), block.get(), [](std::unique_ptr<Block> const& a, Block* b) {
      return a.get() < b;
    });
  insert(m_data->blocks, sortedPosition, std::move(block));
  // Publish only after the vector owns the block. If either throwing step
  // fails, the local owner frees it without leaving a dangling selection.
  m_data->unfilledBlock = newBlock;
}

template <typename T, size_t BlockSize>
void BlockAllocator<T, BlockSize>::deallocate(T* p, size_t n) {
  if (n == 1) {
    starAssert(p);

    auto i = std::upper_bound(m_data->blocks.begin(), m_data->blocks.end(), p, [](T* a, std::unique_ptr<Block> const& b) {
        return a < (T*)b->chunkPointer(0);
      });

    starAssert(i != m_data->blocks.begin());
    --i;

    (*i)->deallocate(p);

    if (!m_data->unfilledBlock) {
      m_data->unfilledBlock = i->get();
    } else if ((*i)->empty()) {
      if (m_data->unfilledBlock != i->get())
        m_data->blocks.erase(i);
    }
  } else {
    m_data->multiAllocator.deallocate(p, n);
    m_data->multiBytes -= n * sizeof(T);
  }
}

template <typename T, size_t BlockSize>
size_t BlockAllocator<T, BlockSize>::requestedStorageBytes() const {
  size_t bytes = 0;
  for (auto const& entry : *m_family)
    if (entry.second.first)
      bytes += entry.second.second(entry.second.first.get());
  return bytes;
}

template <typename T, size_t BlockSize>
template <typename... Args>
void BlockAllocator<T, BlockSize>::construct(pointer p, Args&&... args) const {
  new (p) T(std::forward<Args>(args)...);
}

template <typename T, size_t BlockSize>
void BlockAllocator<T, BlockSize>::destroy(pointer p) const {
  p->~T();
}

template <typename T, size_t BlockSize>
template <class U>
bool BlockAllocator<T, BlockSize>::operator==(BlockAllocator<U, BlockSize> const& rhs) const {
  return m_family == rhs.m_family;
}

template <typename T, size_t BlockSize>
template <class U>
bool BlockAllocator<T, BlockSize>::operator!=(BlockAllocator<U, BlockSize> const& rhs) const {
  return m_family != rhs.m_family;
}

template <typename T, size_t BlockSize>
T* BlockAllocator<T, BlockSize>::Block::allocate() {
  starAssert(allocationCount < BlockSize);

  T* allocated;
  if (firstUnallocated == NullChunkIndex) {
    allocated = (T*)chunkPointer(allocationCount);
  } else {
    void* chunk = chunkPointer(firstUnallocated);
    starAssert(((Unallocated*)chunk)->prev == NullChunkIndex);
    firstUnallocated = ((Unallocated*)chunk)->next;
    if (firstUnallocated != NullChunkIndex)
      ((Unallocated*)chunkPointer(firstUnallocated))->prev = NullChunkIndex;
    allocated = (T*)chunk;
  }

  ++allocationCount;
  return allocated;
}

template <typename T, size_t BlockSize>
void BlockAllocator<T, BlockSize>::Block::deallocate(T* ptr) {
  starAssert(allocationCount > 0);

  ChunkIndex chunkIndex = ptr - (T*)chunkPointer(0);
  starAssert((T*)chunkPointer(chunkIndex) == ptr);

  auto c = (Unallocated*)chunkPointer(chunkIndex);
  c->prev = NullChunkIndex;
  c->next = firstUnallocated;
  if (firstUnallocated != NullChunkIndex)
    ((Unallocated*)chunkPointer(firstUnallocated))->prev = chunkIndex;
  firstUnallocated = chunkIndex;
  --allocationCount;
}

template <typename T, size_t BlockSize>
bool BlockAllocator<T, BlockSize>::Block::full() const {
  return allocationCount == BlockSize;
}

template <typename T, size_t BlockSize>
bool BlockAllocator<T, BlockSize>::Block::empty() const {
  return allocationCount == 0;
}

template <typename T, size_t BlockSize>
auto BlockAllocator<T, BlockSize>::Block::chunkPointer(ChunkIndex chunkIndex) -> Chunk* {
  starAssert(chunkIndex < BlockSize);
  return &chunks[chunkIndex];
}

template <typename T, size_t BlockSize>
typename BlockAllocator<T, BlockSize>::Data* BlockAllocator<T, BlockSize>::getAllocatorData(BlockAllocatorFamily& family) {
  auto& entry = family[typeid(Data)];
  if (!entry.first) {
    auto data = make_shared<Data>();
    entry.second = [](void const* pointer) {
      auto const& state = *static_cast<Data const*>(pointer);
      return sizeof(Data) + state.blocks.capacity() * sizeof(unique_ptr<Block>)
          + state.blocks.size() * sizeof(Block) + state.multiBytes;
    };
    entry.first = std::move(data);
  }
  return (Data*)entry.first.get();
}

}
