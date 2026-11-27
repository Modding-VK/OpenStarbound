#pragma once

#include "StarWorldGenerationCompute.hpp"

#include <cstdint>

namespace Star {

// Opt-in OpenGL compute device for worldgen (GPU-01 probe + DenseUint32* dispatch).
// Normal builds leave STAR_ENABLE_WORLDGEN_GL_COMPUTE undefined: probe never claims GPU.
// Diagnostic builds (-DSTAR_ENABLE_WORLDGEN_GL_COMPUTE=ON) perform a real 4.3+/ARB_compute
// probe on a hidden SDL context owned by this device (same thread as dispatch).

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

class WorldGenerationComputeGlDevice {
public:
  static bool isBuildEnabled() noexcept;

  // Creates (or reuses) the hidden context and reports capabilities. Never claims
  // gpuOpenGlCompute unless compute shaders are actually detected.
  static WorldGenerationComputeCapabilities probeCapabilities() noexcept;

  static WorldGenerationComputeGlProbeInfo const& lastProbeInfo() noexcept;

  // Exact-integer DenseUint32Xor / DenseUint32Add. Returns false on any GL error
  // or if compute is unavailable. Uses SSBO + fence; does not publish world data.
  static bool dispatch(WorldGenerationComputeKernel kernel, uint32_t kernelVersion,
      List<uint32_t> const& input, uint32_t parameter, List<uint32_t>& output);

  static void shutdown() noexcept;
};

}
