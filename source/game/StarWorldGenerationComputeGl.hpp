#pragma once

#include "StarWorldGenerationCompute.hpp"

#include <cstdint>

namespace Star {

// Opt-in OpenGL compute device for worldgen (GPU-01 probe + dispatch).
// Normal builds leave STAR_ENABLE_WORLDGEN_GL_COMPUTE undefined: probe never claims GPU.
// Diagnostic builds (-DSTAR_ENABLE_WORLDGEN_GL_COMPUTE=ON) perform a real 4.3+/ARB_compute
// probe on a hidden SDL context owned by this device (same thread as dispatch).
//
// Milestone-50 vertical step: GL owner-thread affinity + non-blocking fence harness.
// WorldThread must adopt the owner before GL calls; pumpFences never blocks the update.

struct WorldGenerationComputeGlProbeInfo {
  bool buildEnabled = false;
  bool contextCreated = false;
  bool computeAvailable = false;
  int contextMajor = 0;
  int contextMinor = 0;
  char const* glVersion = "";
  char const* glVendor = "";
  char const* glRenderer = "";
  char const* glslVersion = "";
  char const* failureReason = "";
};

enum class WorldGenerationComputeGlFenceStatus : uint8_t {
  Invalid = 0,
  Pending = 1,
  Signaled = 2,
  Failed = 3
};

struct WorldGenerationComputeGlFence {
  uint64_t id = 0;

  bool operator==(WorldGenerationComputeGlFence const& rhs) const { return id == rhs.id; }
  bool operator!=(WorldGenerationComputeGlFence const& rhs) const { return id != rhs.id; }
  explicit operator bool() const { return id != 0; }
};

class WorldGenerationComputeGlDevice {
public:
  static bool isBuildEnabled() noexcept;

  // Creates (or reuses) the hidden context and reports capabilities. Never claims
  // gpuOpenGlCompute unless compute shaders are actually detected.
  static WorldGenerationComputeCapabilities probeCapabilities() noexcept;

  static WorldGenerationComputeGlProbeInfo const& lastProbeInfo() noexcept;

  // Exact-integer DenseUint32* dispatch. Returns false on any GL error
  // or if compute is unavailable. Uses SSBO + fence; does not publish world data.
  // Requires owner thread when one has been adopted.
  static bool dispatch(WorldGenerationComputeKernel kernel, uint32_t kernelVersion,
      List<uint32_t> const& input, uint32_t parameter, List<uint32_t>& output);

  // GPU-02 / milestone-50: GL context owner affinity (separate from service owner).
  // When adopted, probe/dispatch/fence/shutdown refuse foreign threads.
  static void adoptOwnerThread() noexcept;
  static void clearOwnerThread() noexcept;
  static bool hasOwnerThread() noexcept;
  static bool isOwnerThread() noexcept;

  // Fence harness (milestone-50): non-blocking completion tracking.
  // - Stub build (no STAR_ENABLE_WORLDGEN_GL_COMPUTE): creates a Pending fence that
  //   becomes Signaled on pumpFences (unit-testable without a GPU).
  // - GL build: inserts glFenceSync on the current context; poll uses ClientWaitSync
  //   with timeout 0 (never blocks the WorldThread update).
  static Maybe<WorldGenerationComputeGlFence> createFence() noexcept;
  static WorldGenerationComputeGlFenceStatus pollFence(WorldGenerationComputeGlFence fence) noexcept;
  // Advances Pending fences without blocking; returns how many became Signaled/Failed.
  static size_t pumpFences(size_t maxCompletions = 0) noexcept;
  static bool retireFence(WorldGenerationComputeGlFence fence) noexcept;
  static size_t pendingFenceCount() noexcept;

  static void shutdown() noexcept;
};

}
