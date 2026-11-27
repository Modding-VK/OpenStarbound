#pragma once

#include "StarList.hpp"
#include "StarMaybe.hpp"

#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

namespace Star {

// Optional worldgen compute path. Independent of WorldServer, Root, Lua and the
// renderer. CPU reference is always available. OpenGL compute is never claimed
// without a real 4.3+/extension probe. The probe/dispatch live behind
// STAR_ENABLE_WORLDGEN_GL_COMPUTE (diagnostic / Windows harness); normal builds
// keep gpuOpenGlCompute false.

enum class WorldGenerationComputeBackend : uint8_t {
  CpuReference = 0,
  GpuOpenGlCompute = 1
};

enum class WorldGenerationComputeKernel : uint8_t {
  // Exact-integer dense transforms (GPU-03 first candidate).
  DenseUint32Xor = 1,
  DenseUint32Add = 2
};

enum class WorldGenerationComputePollStatus : uint8_t {
  Pending = 0,
  Ready = 1,
  Failed = 2,
  Cancelled = 3,
  UnknownTicket = 4
};

enum class WorldGenerationComputeShadowVerdict : uint8_t {
  Equal = 0,
  Diverged = 1,
  PrimaryFailed = 2,
  ShadowUnavailable = 3,
  Cancelled = 4
};

struct WorldGenerationComputeCapabilities {
  bool cpuReference = true;
  bool gpuOpenGlCompute = false;
  uint32_t maxWorkGroupInvocations = 64;
  uint32_t maxInFlight = 8;
  uint64_t maxImmutableBufferBytes = 16ull * 1024ull * 1024ull;
  uint64_t maxStagingBytes = 32ull * 1024ull * 1024ull;
};

struct WorldGenerationComputeInputIdentity {
  uint64_t worldEpoch = 0;
  uint64_t sequence = 0;
  uint32_t kernelVersion = 1;
  WorldGenerationComputeKernel kernel = WorldGenerationComputeKernel::DenseUint32Xor;

  bool operator==(WorldGenerationComputeInputIdentity const& rhs) const;
  bool operator!=(WorldGenerationComputeInputIdentity const& rhs) const;
};

struct WorldGenerationComputeTicket {
  uint64_t id = 0;

  bool operator==(WorldGenerationComputeTicket const& rhs) const { return id == rhs.id; }
  bool operator!=(WorldGenerationComputeTicket const& rhs) const { return id != rhs.id; }
  explicit operator bool() const { return id != 0; }
};

struct WorldGenerationComputeResult {
  WorldGenerationComputeInputIdentity identity;
  WorldGenerationComputeBackend backend = WorldGenerationComputeBackend::CpuReference;
  List<uint32_t> values;
};

struct WorldGenerationComputeBatchItem {
  WorldGenerationComputeInputIdentity identity;
  List<uint32_t> input;
  uint32_t parameter = 0;
};

struct WorldGenerationComputeShadowReport {
  WorldGenerationComputeShadowVerdict verdict = WorldGenerationComputeShadowVerdict::Equal;
  WorldGenerationComputeInputIdentity identity;
  size_t comparedValues = 0;
  size_t divergenceCount = 0;
  size_t firstDivergenceIndex = 0;
  uint32_t primarySample = 0;
  uint32_t shadowSample = 0;
  WorldGenerationComputeBackend primaryBackend = WorldGenerationComputeBackend::CpuReference;
  WorldGenerationComputeBackend shadowBackend = WorldGenerationComputeBackend::CpuReference;
};

// Owner-thread service: explicit queue, reusable staging pool, in-flight limit,
// fence pump, cancel/fail recovery. Results never write the live world.
class WorldGenerationComputeService {
public:
  static WorldGenerationComputeCapabilities probeCapabilities() noexcept;

  explicit WorldGenerationComputeService(WorldGenerationComputeCapabilities capabilities =
      probeCapabilities());
  ~WorldGenerationComputeService();

  WorldGenerationComputeService(WorldGenerationComputeService const&) = delete;
  WorldGenerationComputeService& operator=(WorldGenerationComputeService const&) = delete;

  WorldGenerationComputeCapabilities const& capabilities() const noexcept { return m_capabilities; }
  size_t openTickets() const;
  size_t inFlight() const;
  size_t stagingBytes() const;
  size_t pooledBuffers() const;

  // Deferred CPU completion leaves Pending until pump()/complete(). Synchronous
  // mode (default) finishes CPU work before submit returns.
  void setDeferredCpuCompletion(bool deferred) noexcept;

  Maybe<WorldGenerationComputeTicket> submit(WorldGenerationComputeInputIdentity identity,
      List<uint32_t> input, uint32_t parameter,
      WorldGenerationComputeBackend backend = WorldGenerationComputeBackend::CpuReference);

  // One ticket per item; fails entirely (no partial admit) if capacity is short.
  Maybe<List<WorldGenerationComputeTicket>> submitBatch(List<WorldGenerationComputeBatchItem> items,
      WorldGenerationComputeBackend backend = WorldGenerationComputeBackend::CpuReference);

  // Advances deferred fences without blocking the caller on GPU/GL.
  size_t pump(size_t maxCompletions = 0);

  bool complete(WorldGenerationComputeTicket ticket);

  WorldGenerationComputePollStatus poll(WorldGenerationComputeTicket ticket,
      WorldGenerationComputeResult* out = nullptr);

  bool cancel(WorldGenerationComputeTicket ticket);

  // Context/driver failure: revoke open tickets, drop pooled staging owned by
  // failed work, and reject later duplicate polls for those ids.
  size_t failContext();

  // GPU-04: run primary and shadow on identical inputs. Without GPU capability
  // or failed GL dispatch, shadowBackend GpuOpenGlCompute yields
  // ShadowUnavailable. With a successful probe, GPU path uses compute shaders
  // (DenseUint32*) rather than the CPU helper.
  WorldGenerationComputeShadowReport compareShadow(WorldGenerationComputeInputIdentity identity,
      List<uint32_t> const& input, uint32_t parameter,
      WorldGenerationComputeBackend primary = WorldGenerationComputeBackend::CpuReference,
      WorldGenerationComputeBackend shadow = WorldGenerationComputeBackend::GpuOpenGlCompute);

  static List<uint32_t> runKernel(WorldGenerationComputeKernel kernel, uint32_t kernelVersion,
      List<uint32_t> const& input, uint32_t parameter);

private:
  enum class EntryState : uint8_t { Pending, Ready, Failed, Cancelled };

  struct Entry {
    WorldGenerationComputeInputIdentity identity;
    WorldGenerationComputeBackend backend = WorldGenerationComputeBackend::CpuReference;
    EntryState state = EntryState::Pending;
    uint32_t parameter = 0;
    List<uint32_t> input;
    List<uint32_t> output;
    uint64_t stagingBytes = 0;
    bool publicationRevoked = false;
    bool contextFailed = false;
  };

  static bool supported(WorldGenerationComputeKernel kernel, uint32_t version) noexcept;
  static uint64_t bufferBytes(List<uint32_t> const& values) noexcept;
  List<uint32_t> acquireBuffer(size_t size);
  void releaseBuffer(List<uint32_t>&& buffer);
  void completeEntry(Entry& entry);
  bool runBackendKernel(WorldGenerationComputeBackend backend,
      WorldGenerationComputeKernel kernel, uint32_t kernelVersion, List<uint32_t> const& input,
      uint32_t parameter, List<uint32_t>& out) const;
  Maybe<WorldGenerationComputeTicket> submitLocked(WorldGenerationComputeInputIdentity identity,
      List<uint32_t> input, uint32_t parameter, WorldGenerationComputeBackend backend);
  WorldGenerationComputePollStatus pollLocked(WorldGenerationComputeTicket ticket,
      WorldGenerationComputeResult* out);

  WorldGenerationComputeCapabilities m_capabilities;
  bool m_deferredCpu = false;
  mutable std::mutex m_mutex;
  uint64_t m_nextTicket = 1;
  uint64_t m_stagingBytes = 0;
  std::unordered_map<uint64_t, Entry> m_entries;
  std::unordered_set<uint64_t> m_rejectedDuplicates;
  List<List<uint32_t>> m_bufferPool;
};

}
