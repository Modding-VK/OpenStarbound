#pragma once

#include "StarList.hpp"
#include "StarMaybe.hpp"

#include <cstdint>
#include <mutex>
#include <unordered_map>

namespace Star {

// Optional worldgen compute path. Independent of WorldServer, Root, Lua and the
// renderer. GPU-01 defines the shared CPU/GPU surface; this build always offers
// the CPU reference backend and never claims OpenGL compute without a real probe.

enum class WorldGenerationComputeBackend : uint8_t {
  CpuReference = 0,
  GpuOpenGlCompute = 1
};

enum class WorldGenerationComputeKernel : uint8_t {
  // First exact-integer candidate from GPU-03. CPU reference executes it now;
  // a future GPU path must match bit-for-bit before any publication.
  DenseUint32Xor = 1
};

enum class WorldGenerationComputePollStatus : uint8_t {
  Pending = 0,
  Ready = 1,
  Failed = 2,
  Cancelled = 3,
  UnknownTicket = 4
};

struct WorldGenerationComputeCapabilities {
  bool cpuReference = true;
  bool gpuOpenGlCompute = false;
  uint32_t maxWorkGroupInvocations = 1;
  uint64_t maxImmutableBufferBytes = 16ull * 1024ull * 1024ull;
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

// Owner-thread service: submit copies immutable inputs, poll observes completion,
// cancel revokes publication. Results never write the live world.
class WorldGenerationComputeService {
public:
  static WorldGenerationComputeCapabilities probeCapabilities() noexcept;

  explicit WorldGenerationComputeService(WorldGenerationComputeCapabilities capabilities =
      probeCapabilities());

  WorldGenerationComputeService(WorldGenerationComputeService const&) = delete;
  WorldGenerationComputeService& operator=(WorldGenerationComputeService const&) = delete;

  WorldGenerationComputeCapabilities const& capabilities() const noexcept { return m_capabilities; }

  // Returns none when the requested backend/kernel is unavailable, the buffer
  // exceeds the probed limit, or the identity/kernel is unsupported.
  Maybe<WorldGenerationComputeTicket> submit(WorldGenerationComputeInputIdentity identity,
      List<uint32_t> input, uint32_t parameter,
      WorldGenerationComputeBackend backend = WorldGenerationComputeBackend::CpuReference);

  WorldGenerationComputePollStatus poll(WorldGenerationComputeTicket ticket,
      WorldGenerationComputeResult* out = nullptr);

  // Revokes publication. Memory for an in-flight job is retained until poll
  // observes Cancelled/Failed/Ready and the ticket is retired.
  bool cancel(WorldGenerationComputeTicket ticket);

  size_t openTickets() const;

private:
  enum class EntryState : uint8_t { Pending, Ready, Failed, Cancelled };

  struct Entry {
    WorldGenerationComputeInputIdentity identity;
    WorldGenerationComputeBackend backend = WorldGenerationComputeBackend::CpuReference;
    EntryState state = EntryState::Pending;
    uint32_t parameter = 0;
    List<uint32_t> input;
    List<uint32_t> output;
    bool publicationRevoked = false;
  };

  static bool supported(WorldGenerationComputeKernel kernel) noexcept;
  static List<uint32_t> runCpuDenseUint32Xor(List<uint32_t> const& input, uint32_t parameter);
  void completeCpu(Entry& entry);

  WorldGenerationComputeCapabilities m_capabilities;
  mutable std::mutex m_mutex;
  uint64_t m_nextTicket = 1;
  std::unordered_map<uint64_t, Entry> m_entries;
};

}
