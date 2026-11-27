#include "StarWorldGenerationComputeGl.hpp"
#include "StarFormat.hpp"

#include <mutex>
#include <string>

#ifndef STAR_ENABLE_WORLDGEN_GL_COMPUTE

namespace Star {

namespace {
WorldGenerationComputeGlProbeInfo g_info{};
std::once_flag g_once;
}

bool WorldGenerationComputeGlDevice::isBuildEnabled() noexcept {
  return false;
}

WorldGenerationComputeCapabilities WorldGenerationComputeGlDevice::probeCapabilities() noexcept {
  std::call_once(g_once, [] {
    g_info.buildEnabled = false;
    g_info.failureReason = "STAR_ENABLE_WORLDGEN_GL_COMPUTE not enabled in this build";
  });
  WorldGenerationComputeCapabilities caps;
  caps.cpuReference = true;
  caps.gpuOpenGlCompute = false;
  caps.maxWorkGroupInvocations = 64;
  caps.maxInFlight = 8;
  caps.maxImmutableBufferBytes = 16ull * 1024ull * 1024ull;
  caps.maxStagingBytes = 32ull * 1024ull * 1024ull;
  return caps;
}

WorldGenerationComputeGlProbeInfo const& WorldGenerationComputeGlDevice::lastProbeInfo() noexcept {
  probeCapabilities();
  return g_info;
}

bool WorldGenerationComputeGlDevice::dispatch(WorldGenerationComputeKernel, uint32_t,
    List<uint32_t> const&, uint32_t, List<uint32_t>&) {
  return false;
}

void WorldGenerationComputeGlDevice::shutdown() noexcept {}

}

#else

#include "GL/glew.h"
#include <SDL3/SDL.h>

namespace Star {

namespace {

WorldGenerationComputeGlProbeInfo g_info{};
std::once_flag g_probeOnce;
std::mutex g_glMutex;
SDL_Window* g_window = nullptr;
SDL_GLContext g_context = nullptr;
GLuint g_xorProgram = 0;
GLuint g_addProgram = 0;
GLuint g_andProgram = 0;
GLuint g_orProgram = 0;
GLuint g_mulProgram = 0;
GLuint g_minProgram = 0;
GLuint g_maxProgram = 0;
GLuint g_subProgram = 0;
GLuint g_shlProgram = 0;
GLuint g_shrProgram = 0;
GLuint g_rotlProgram = 0;
GLuint g_rotrProgram = 0;
GLuint g_notProgram = 0;
GLuint g_clzProgram = 0;
GLuint g_ctzProgram = 0;
GLuint g_popcountProgram = 0;
GLuint g_eqProgram = 0;
GLuint g_neProgram = 0;
GLuint g_bitReverseProgram = 0;
GLuint g_ltProgram = 0;
GLuint g_leuProgram = 0;
GLuint g_absDiffProgram = 0;
GLuint g_gtProgram = 0;
GLuint g_geuProgram = 0;
GLuint g_xnorProgram = 0;
GLuint g_nandProgram = 0;
GLuint g_norProgram = 0;
GLuint g_mulHiProgram = 0;
GLuint g_andNotProgram = 0;
GLuint g_satAddProgram = 0;
GLuint g_satSubProgram = 0;
GLuint g_orNotProgram = 0;
GLuint g_avgProgram = 0;
GLuint g_isPow2Program = 0;
GLuint g_bswapProgram = 0;
GLuint g_parityProgram = 0;
GLuint g_bitTestProgram = 0;
GLuint g_hasZeroByteProgram = 0;
GLuint g_nextPow2Program = 0;
GLuint g_maskLowProgram = 0;
GLuint g_ilog2Program = 0;
GLuint g_broadcastByteProgram = 0;
GLuint g_byteSumProgram = 0;
GLuint g_sext8Program = 0;
GLuint g_sext16Program = 0;
GLuint g_extractByteProgram = 0;
GLuint g_bitSetProgram = 0;
GLuint g_bitClearProgram = 0;
GLuint g_bitToggleProgram = 0;
GLuint g_negProgram = 0;
GLuint g_absProgram = 0;
GLuint g_rolBytesProgram = 0;
GLuint g_rorBytesProgram = 0;
GLuint g_signMaskProgram = 0;
GLuint g_isZeroProgram = 0;
GLuint g_isNonzeroProgram = 0;
GLuint g_maskHighProgram = 0;
GLuint g_satMulProgram = 0;
GLuint g_ssbo = 0;
std::string g_version;
std::string g_vendor;
std::string g_renderer;
std::string g_glsl;
std::string g_failure;

char const* XorShader = R"GLSL(
#version 430
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Data { uint values[]; };
uniform uint uParameter;
uniform uint uCount;
void main() {
  uint i = gl_GlobalInvocationID.x;
  if (i >= uCount)
    return;
  values[i] = values[i] ^ uParameter;
}
)GLSL";

char const* AddShader = R"GLSL(
#version 430
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Data { uint values[]; };
uniform uint uParameter;
uniform uint uCount;
void main() {
  uint i = gl_GlobalInvocationID.x;
  if (i >= uCount)
    return;
  values[i] = values[i] + uParameter;
}
)GLSL";

char const* AndShader = R"GLSL(
#version 430
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Data { uint values[]; };
uniform uint uParameter;
uniform uint uCount;
void main() {
  uint i = gl_GlobalInvocationID.x;
  if (i >= uCount)
    return;
  values[i] = values[i] & uParameter;
}
)GLSL";

char const* OrShader = R"GLSL(
#version 430
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Data { uint values[]; };
uniform uint uParameter;
uniform uint uCount;
void main() {
  uint i = gl_GlobalInvocationID.x;
  if (i >= uCount)
    return;
  values[i] = values[i] | uParameter;
}
)GLSL";

char const* MulShader = R"GLSL(
#version 430
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Data { uint values[]; };
uniform uint uParameter;
uniform uint uCount;
void main() {
  uint i = gl_GlobalInvocationID.x;
  if (i >= uCount)
    return;
  values[i] = values[i] * uParameter;
}
)GLSL";

char const* MinShader = R"GLSL(
#version 430
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Data { uint values[]; };
uniform uint uParameter;
uniform uint uCount;
void main() {
  uint i = gl_GlobalInvocationID.x;
  if (i >= uCount)
    return;
  values[i] = min(values[i], uParameter);
}
)GLSL";

char const* MaxShader = R"GLSL(
#version 430
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Data { uint values[]; };
uniform uint uParameter;
uniform uint uCount;
void main() {
  uint i = gl_GlobalInvocationID.x;
  if (i >= uCount)
    return;
  values[i] = max(values[i], uParameter);
}
)GLSL";

char const* SubShader = R"GLSL(
#version 430
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Data { uint values[]; };
uniform uint uParameter;
uniform uint uCount;
void main() {
  uint i = gl_GlobalInvocationID.x;
  if (i >= uCount)
    return;
  values[i] = values[i] - uParameter;
}
)GLSL";

char const* ShlShader = R"GLSL(
#version 430
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Data { uint values[]; };
uniform uint uParameter;
uniform uint uCount;
void main() {
  uint i = gl_GlobalInvocationID.x;
  if (i >= uCount)
    return;
  values[i] = values[i] << (uParameter & 31u);
}
)GLSL";

char const* ShrShader = R"GLSL(
#version 430
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Data { uint values[]; };
uniform uint uParameter;
uniform uint uCount;
void main() {
  uint i = gl_GlobalInvocationID.x;
  if (i >= uCount)
    return;
  values[i] = values[i] >> (uParameter & 31u);
}
)GLSL";

char const* RotlShader = R"GLSL(
#version 430
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Data { uint values[]; };
uniform uint uParameter;
uniform uint uCount;
void main() {
  uint i = gl_GlobalInvocationID.x;
  if (i >= uCount)
    return;
  uint r = uParameter & 31u;
  values[i] = (values[i] << r) | (values[i] >> ((32u - r) & 31u));
}
)GLSL";

char const* RotrShader = R"GLSL(
#version 430
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Data { uint values[]; };
uniform uint uParameter;
uniform uint uCount;
void main() {
  uint i = gl_GlobalInvocationID.x;
  if (i >= uCount)
    return;
  uint r = uParameter & 31u;
  values[i] = (values[i] >> r) | (values[i] << ((32u - r) & 31u));
}
)GLSL";

char const* NotShader = R"GLSL(
#version 430
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Data { uint values[]; };
uniform uint uParameter;
uniform uint uCount;
void main() {
  uint i = gl_GlobalInvocationID.x;
  if (i >= uCount)
    return;
  values[i] = ~values[i];
}
)GLSL";

char const* ClzShader = R"GLSL(
#version 430
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Data { uint values[]; };
uniform uint uParameter;
uniform uint uCount;
void main() {
  uint i = gl_GlobalInvocationID.x;
  if (i >= uCount)
    return;
  uint v = values[i];
  values[i] = v == 0u ? 32u : (31u - uint(findMSB(v)));
}
)GLSL";

char const* CtzShader = R"GLSL(
#version 430
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Data { uint values[]; };
uniform uint uParameter;
uniform uint uCount;
void main() {
  uint i = gl_GlobalInvocationID.x;
  if (i >= uCount)
    return;
  uint v = values[i];
  values[i] = v == 0u ? 32u : uint(findLSB(v));
}
)GLSL";

char const* PopcountShader = R"GLSL(
#version 430
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Data { uint values[]; };
uniform uint uParameter;
uniform uint uCount;
void main() {
  uint i = gl_GlobalInvocationID.x;
  if (i >= uCount)
    return;
  values[i] = bitCount(values[i]);
}
)GLSL";

char const* EqShader = R"GLSL(
#version 430
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Data { uint values[]; };
uniform uint uParameter;
uniform uint uCount;
void main() {
  uint i = gl_GlobalInvocationID.x;
  if (i >= uCount)
    return;
  values[i] = values[i] == uParameter ? 0xffffffffu : 0u;
}
)GLSL";

char const* NeShader = R"GLSL(
#version 430
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Data { uint values[]; };
uniform uint uParameter;
uniform uint uCount;
void main() {
  uint i = gl_GlobalInvocationID.x;
  if (i >= uCount)
    return;
  values[i] = values[i] != uParameter ? 0xffffffffu : 0u;
}
)GLSL";

char const* BitReverseShader = R"GLSL(
#version 430
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Data { uint values[]; };
uniform uint uParameter;
uniform uint uCount;
void main() {
  uint i = gl_GlobalInvocationID.x;
  if (i >= uCount)
    return;
  values[i] = bitfieldReverse(values[i]);
}
)GLSL";

char const* LtShader = R"GLSL(
#version 430
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Data { uint values[]; };
uniform uint uParameter;
uniform uint uCount;
void main() {
  uint i = gl_GlobalInvocationID.x;
  if (i >= uCount)
    return;
  values[i] = values[i] < uParameter ? 0xffffffffu : 0u;
}
)GLSL";

char const* LeuShader = R"GLSL(
#version 430
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Data { uint values[]; };
uniform uint uParameter;
uniform uint uCount;
void main() {
  uint i = gl_GlobalInvocationID.x;
  if (i >= uCount)
    return;
  values[i] = values[i] <= uParameter ? 0xffffffffu : 0u;
}
)GLSL";

char const* AbsDiffShader = R"GLSL(
#version 430
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Data { uint values[]; };
uniform uint uParameter;
uniform uint uCount;
void main() {
  uint i = gl_GlobalInvocationID.x;
  if (i >= uCount)
    return;
  uint v = values[i];
  values[i] = v > uParameter ? (v - uParameter) : (uParameter - v);
}
)GLSL";

char const* GtShader = R"GLSL(
#version 430
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Data { uint values[]; };
uniform uint uParameter;
uniform uint uCount;
void main() {
  uint i = gl_GlobalInvocationID.x;
  if (i >= uCount)
    return;
  values[i] = values[i] > uParameter ? 0xffffffffu : 0u;
}
)GLSL";

char const* GeuShader = R"GLSL(
#version 430
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Data { uint values[]; };
uniform uint uParameter;
uniform uint uCount;
void main() {
  uint i = gl_GlobalInvocationID.x;
  if (i >= uCount)
    return;
  values[i] = values[i] >= uParameter ? 0xffffffffu : 0u;
}
)GLSL";

char const* XnorShader = R"GLSL(
#version 430
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Data { uint values[]; };
uniform uint uParameter;
uniform uint uCount;
void main() {
  uint i = gl_GlobalInvocationID.x;
  if (i >= uCount)
    return;
  values[i] = ~(values[i] ^ uParameter);
}
)GLSL";

char const* NandShader = R"GLSL(
#version 430
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Data { uint values[]; };
uniform uint uParameter;
uniform uint uCount;
void main() {
  uint i = gl_GlobalInvocationID.x;
  if (i >= uCount)
    return;
  values[i] = ~(values[i] & uParameter);
}
)GLSL";

char const* NorShader = R"GLSL(
#version 430
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Data { uint values[]; };
uniform uint uParameter;
uniform uint uCount;
void main() {
  uint i = gl_GlobalInvocationID.x;
  if (i >= uCount)
    return;
  values[i] = ~(values[i] | uParameter);
}
)GLSL";

char const* MulHiShader = R"GLSL(
#version 430
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Data { uint values[]; };
uniform uint uParameter;
uniform uint uCount;
void main() {
  uint i = gl_GlobalInvocationID.x;
  if (i >= uCount)
    return;
  uint msb;
  uint lsb;
  umulExtended(values[i], uParameter, msb, lsb);
  values[i] = msb;
}
)GLSL";

char const* AndNotShader = R"GLSL(
#version 430
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Data { uint values[]; };
uniform uint uParameter;
uniform uint uCount;
void main() {
  uint i = gl_GlobalInvocationID.x;
  if (i >= uCount)
    return;
  values[i] = values[i] & ~uParameter;
}
)GLSL";

char const* SatAddShader = R"GLSL(
#version 430
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Data { uint values[]; };
uniform uint uParameter;
uniform uint uCount;
void main() {
  uint i = gl_GlobalInvocationID.x;
  if (i >= uCount)
    return;
  uint v = values[i];
  values[i] = (0xffffffffu - v < uParameter) ? 0xffffffffu : (v + uParameter);
}
)GLSL";

char const* SatSubShader = R"GLSL(
#version 430
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Data { uint values[]; };
uniform uint uParameter;
uniform uint uCount;
void main() {
  uint i = gl_GlobalInvocationID.x;
  if (i >= uCount)
    return;
  uint v = values[i];
  values[i] = v > uParameter ? (v - uParameter) : 0u;
}
)GLSL";

char const* OrNotShader = R"GLSL(
#version 430
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Data { uint values[]; };
uniform uint uParameter;
uniform uint uCount;
void main() {
  uint i = gl_GlobalInvocationID.x;
  if (i >= uCount)
    return;
  values[i] = values[i] | ~uParameter;
}
)GLSL";

char const* AvgShader = R"GLSL(
#version 430
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Data { uint values[]; };
uniform uint uParameter;
uniform uint uCount;
void main() {
  uint i = gl_GlobalInvocationID.x;
  if (i >= uCount)
    return;
  uint v = values[i];
  values[i] = (v >> 1u) + (uParameter >> 1u) + (v & uParameter & 1u);
}
)GLSL";

char const* IsPow2Shader = R"GLSL(
#version 430
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Data { uint values[]; };
uniform uint uParameter;
uniform uint uCount;
void main() {
  uint i = gl_GlobalInvocationID.x;
  if (i >= uCount)
    return;
  uint v = values[i];
  values[i] = (v != 0u && (v & (v - 1u)) == 0u) ? 0xffffffffu : 0u;
}
)GLSL";

char const* BswapShader = R"GLSL(
#version 430
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Data { uint values[]; };
uniform uint uParameter;
uniform uint uCount;
void main() {
  uint i = gl_GlobalInvocationID.x;
  if (i >= uCount)
    return;
  uint v = values[i];
  values[i] = ((v & 0x000000ffu) << 24) | ((v & 0x0000ff00u) << 8)
      | ((v & 0x00ff0000u) >> 8) | ((v & 0xff000000u) >> 24);
}
)GLSL";

char const* ParityShader = R"GLSL(
#version 430
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Data { uint values[]; };
uniform uint uParameter;
uniform uint uCount;
void main() {
  uint i = gl_GlobalInvocationID.x;
  if (i >= uCount)
    return;
  values[i] = bitCount(values[i]) & 1u;
}
)GLSL";

char const* BitTestShader = R"GLSL(
#version 430
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Data { uint values[]; };
uniform uint uParameter;
uniform uint uCount;
void main() {
  uint i = gl_GlobalInvocationID.x;
  if (i >= uCount)
    return;
  uint bit = uParameter & 31u;
  values[i] = ((values[i] >> bit) & 1u) != 0u ? 0xffffffffu : 0u;
}
)GLSL";

char const* HasZeroByteShader = R"GLSL(
#version 430
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Data { uint values[]; };
uniform uint uParameter;
uniform uint uCount;
void main() {
  uint i = gl_GlobalInvocationID.x;
  if (i >= uCount)
    return;
  uint v = values[i];
  values[i] = ((v - 0x01010101u) & ~v & 0x80808080u) != 0u ? 0xffffffffu : 0u;
}
)GLSL";

char const* NextPow2Shader = R"GLSL(
#version 430
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Data { uint values[]; };
uniform uint uParameter;
uniform uint uCount;
void main() {
  uint i = gl_GlobalInvocationID.x;
  if (i >= uCount)
    return;
  uint v = values[i];
  if (v == 0u) {
    values[i] = 0u;
    return;
  }
  v = v - 1u;
  v |= v >> 1;
  v |= v >> 2;
  v |= v >> 4;
  v |= v >> 8;
  v |= v >> 16;
  values[i] = v + 1u;
}
)GLSL";

char const* MaskLowShader = R"GLSL(
#version 430
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Data { uint values[]; };
uniform uint uParameter;
uniform uint uCount;
void main() {
  uint i = gl_GlobalInvocationID.x;
  if (i >= uCount)
    return;
  uint bits = uParameter > 32u ? 32u : uParameter;
  uint mask = bits == 32u ? 0xffffffffu : (bits == 0u ? 0u : ((1u << bits) - 1u));
  values[i] = values[i] & mask;
}
)GLSL";

char const* Ilog2Shader = R"GLSL(
#version 430
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Data { uint values[]; };
uniform uint uParameter;
uniform uint uCount;
void main() {
  uint i = gl_GlobalInvocationID.x;
  if (i >= uCount)
    return;
  uint v = values[i];
  // findMSB matches floor(log2) for nonzero values; define 0 -> 0 like the CPU path.
  values[i] = v == 0u ? 0u : uint(findMSB(v));
}
)GLSL";

char const* BroadcastByteShader = R"GLSL(
#version 430
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Data { uint values[]; };
uniform uint uParameter;
uniform uint uCount;
void main() {
  uint i = gl_GlobalInvocationID.x;
  if (i >= uCount)
    return;
  uint b = values[i] & 0xffu;
  values[i] = b * 0x01010101u;
}
)GLSL";

char const* ByteSumShader = R"GLSL(
#version 430
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Data { uint values[]; };
uniform uint uParameter;
uniform uint uCount;
void main() {
  uint i = gl_GlobalInvocationID.x;
  if (i >= uCount)
    return;
  uint v = values[i];
  values[i] = (v & 0xffu) + ((v >> 8) & 0xffu) + ((v >> 16) & 0xffu) + ((v >> 24) & 0xffu);
}
)GLSL";

char const* Sext8Shader = R"GLSL(
#version 430
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Data { uint values[]; };
uniform uint uParameter;
uniform uint uCount;
void main() {
  uint i = gl_GlobalInvocationID.x;
  if (i >= uCount)
    return;
  uint low = values[i] & 0xffu;
  values[i] = (low & 0x80u) != 0u ? (low | 0xffffff00u) : low;
}
)GLSL";

char const* Sext16Shader = R"GLSL(
#version 430
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Data { uint values[]; };
uniform uint uParameter;
uniform uint uCount;
void main() {
  uint i = gl_GlobalInvocationID.x;
  if (i >= uCount)
    return;
  uint low = values[i] & 0xffffu;
  values[i] = (low & 0x8000u) != 0u ? (low | 0xffff0000u) : low;
}
)GLSL";

char const* ExtractByteShader = R"GLSL(
#version 430
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Data { uint values[]; };
uniform uint uParameter;
uniform uint uCount;
void main() {
  uint i = gl_GlobalInvocationID.x;
  if (i >= uCount)
    return;
  uint shift = (uParameter & 3u) * 8u;
  values[i] = (values[i] >> shift) & 0xffu;
}
)GLSL";

char const* BitSetShader = R"GLSL(
#version 430
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Data { uint values[]; };
uniform uint uParameter;
uniform uint uCount;
void main() {
  uint i = gl_GlobalInvocationID.x;
  if (i >= uCount)
    return;
  uint mask = 1u << (uParameter & 31u);
  values[i] = values[i] | mask;
}
)GLSL";

char const* BitClearShader = R"GLSL(
#version 430
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Data { uint values[]; };
uniform uint uParameter;
uniform uint uCount;
void main() {
  uint i = gl_GlobalInvocationID.x;
  if (i >= uCount)
    return;
  uint mask = 1u << (uParameter & 31u);
  values[i] = values[i] & ~mask;
}
)GLSL";

char const* BitToggleShader = R"GLSL(
#version 430
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Data { uint values[]; };
uniform uint uParameter;
uniform uint uCount;
void main() {
  uint i = gl_GlobalInvocationID.x;
  if (i >= uCount)
    return;
  uint mask = 1u << (uParameter & 31u);
  values[i] = values[i] ^ mask;
}
)GLSL";

char const* NegShader = R"GLSL(
#version 430
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Data { uint values[]; };
uniform uint uParameter;
uniform uint uCount;
void main() {
  uint i = gl_GlobalInvocationID.x;
  if (i >= uCount)
    return;
  values[i] = uint(-int(values[i]));
}
)GLSL";

char const* AbsShader = R"GLSL(
#version 430
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Data { uint values[]; };
uniform uint uParameter;
uniform uint uCount;
void main() {
  uint i = gl_GlobalInvocationID.x;
  if (i >= uCount)
    return;
  uint v = values[i];
  values[i] = (v & 0x80000000u) != 0u ? uint(-int(v)) : v;
}
)GLSL";

char const* RolBytesShader = R"GLSL(
#version 430
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Data { uint values[]; };
uniform uint uParameter;
uniform uint uCount;
void main() {
  uint i = gl_GlobalInvocationID.x;
  if (i >= uCount)
    return;
  uint shift = (uParameter & 3u) * 8u;
  uint v = values[i];
  values[i] = shift == 0u ? v : ((v << shift) | (v >> (32u - shift)));
}
)GLSL";

char const* RorBytesShader = R"GLSL(
#version 430
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Data { uint values[]; };
uniform uint uParameter;
uniform uint uCount;
void main() {
  uint i = gl_GlobalInvocationID.x;
  if (i >= uCount)
    return;
  uint shift = (uParameter & 3u) * 8u;
  uint v = values[i];
  values[i] = shift == 0u ? v : ((v >> shift) | (v << (32u - shift)));
}
)GLSL";

char const* SignMaskShader = R"GLSL(
#version 430
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Data { uint values[]; };
uniform uint uParameter;
uniform uint uCount;
void main() {
  uint i = gl_GlobalInvocationID.x;
  if (i >= uCount)
    return;
  values[i] = (values[i] & 0x80000000u) != 0u ? 0xffffffffu : 0u;
}
)GLSL";

char const* IsZeroShader = R"GLSL(
#version 430
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Data { uint values[]; };
uniform uint uParameter;
uniform uint uCount;
void main() {
  uint i = gl_GlobalInvocationID.x;
  if (i >= uCount)
    return;
  values[i] = values[i] == 0u ? 0xffffffffu : 0u;
}
)GLSL";

char const* IsNonzeroShader = R"GLSL(
#version 430
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Data { uint values[]; };
uniform uint uParameter;
uniform uint uCount;
void main() {
  uint i = gl_GlobalInvocationID.x;
  if (i >= uCount)
    return;
  values[i] = values[i] != 0u ? 0xffffffffu : 0u;
}
)GLSL";

char const* MaskHighShader = R"GLSL(
#version 430
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Data { uint values[]; };
uniform uint uParameter;
uniform uint uCount;
void main() {
  uint i = gl_GlobalInvocationID.x;
  if (i >= uCount)
    return;
  uint bits = uParameter > 32u ? 32u : uParameter;
  uint mask = bits == 0u ? 0u : (bits == 32u ? 0xffffffffu : (0xffffffffu << (32u - bits)));
  values[i] = values[i] & mask;
}
)GLSL";

char const* SatMulShader = R"GLSL(
#version 430
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Data { uint values[]; };
uniform uint uParameter;
uniform uint uCount;
void main() {
  uint i = gl_GlobalInvocationID.x;
  if (i >= uCount)
    return;
  uint msb;
  uint lsb;
  umulExtended(values[i], uParameter, msb, lsb);
  values[i] = msb != 0u ? 0xffffffffu : lsb;
}
)GLSL";

bool clearGlErrors() {
  bool had = false;
  while (glGetError() != GL_NO_ERROR)
    had = true;
  return had;
}

GLuint compileComputeProgram(char const* source, std::string& errorOut) {
  GLuint shader = glCreateShader(GL_COMPUTE_SHADER);
  glShaderSource(shader, 1, &source, nullptr);
  glCompileShader(shader);
  GLint ok = GL_FALSE;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    GLint len = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
    std::string log(len > 0 ? (size_t)len : 0, '\0');
    if (!log.empty())
      glGetShaderInfoLog(shader, len, nullptr, &log[0]);
    errorOut = log;
    glDeleteShader(shader);
    return 0;
  }
  GLuint program = glCreateProgram();
  glAttachShader(program, shader);
  glLinkProgram(program);
  glDeleteShader(shader);
  glGetProgramiv(program, GL_LINK_STATUS, &ok);
  if (!ok) {
    GLint len = 0;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &len);
    std::string log(len > 0 ? (size_t)len : 0, '\0');
    if (!log.empty())
      glGetProgramInfoLog(program, len, nullptr, &log[0]);
    errorOut = log;
    glDeleteProgram(program);
    return 0;
  }
  return program;
}

void destroyContextUnlocked() {
  if (g_context && g_window)
    SDL_GL_MakeCurrent(g_window, g_context);
  if (g_ssbo) {
    glDeleteBuffers(1, &g_ssbo);
    g_ssbo = 0;
  }
  if (g_xorProgram) {
    glDeleteProgram(g_xorProgram);
    g_xorProgram = 0;
  }
  if (g_addProgram) {
    glDeleteProgram(g_addProgram);
    g_addProgram = 0;
  }
  if (g_andProgram) {
    glDeleteProgram(g_andProgram);
    g_andProgram = 0;
  }
  if (g_orProgram) {
    glDeleteProgram(g_orProgram);
    g_orProgram = 0;
  }
  if (g_mulProgram) {
    glDeleteProgram(g_mulProgram);
    g_mulProgram = 0;
  }
  if (g_minProgram) {
    glDeleteProgram(g_minProgram);
    g_minProgram = 0;
  }
  if (g_maxProgram) {
    glDeleteProgram(g_maxProgram);
    g_maxProgram = 0;
  }
  if (g_subProgram) {
    glDeleteProgram(g_subProgram);
    g_subProgram = 0;
  }
  if (g_shlProgram) {
    glDeleteProgram(g_shlProgram);
    g_shlProgram = 0;
  }
  if (g_shrProgram) {
    glDeleteProgram(g_shrProgram);
    g_shrProgram = 0;
  }
  if (g_rotlProgram) {
    glDeleteProgram(g_rotlProgram);
    g_rotlProgram = 0;
  }
  if (g_rotrProgram) {
    glDeleteProgram(g_rotrProgram);
    g_rotrProgram = 0;
  }
  if (g_notProgram) {
    glDeleteProgram(g_notProgram);
    g_notProgram = 0;
  }
  if (g_clzProgram) {
    glDeleteProgram(g_clzProgram);
    g_clzProgram = 0;
  }
  if (g_ctzProgram) {
    glDeleteProgram(g_ctzProgram);
    g_ctzProgram = 0;
  }
  if (g_popcountProgram) {
    glDeleteProgram(g_popcountProgram);
    g_popcountProgram = 0;
  }
  if (g_eqProgram) {
    glDeleteProgram(g_eqProgram);
    g_eqProgram = 0;
  }
  if (g_neProgram) {
    glDeleteProgram(g_neProgram);
    g_neProgram = 0;
  }
  if (g_bitReverseProgram) {
    glDeleteProgram(g_bitReverseProgram);
    g_bitReverseProgram = 0;
  }
  if (g_ltProgram) {
    glDeleteProgram(g_ltProgram);
    g_ltProgram = 0;
  }
  if (g_leuProgram) {
    glDeleteProgram(g_leuProgram);
    g_leuProgram = 0;
  }
  if (g_absDiffProgram) {
    glDeleteProgram(g_absDiffProgram);
    g_absDiffProgram = 0;
  }
  if (g_gtProgram) {
    glDeleteProgram(g_gtProgram);
    g_gtProgram = 0;
  }
  if (g_geuProgram) {
    glDeleteProgram(g_geuProgram);
    g_geuProgram = 0;
  }
  if (g_xnorProgram) {
    glDeleteProgram(g_xnorProgram);
    g_xnorProgram = 0;
  }
  if (g_nandProgram) {
    glDeleteProgram(g_nandProgram);
    g_nandProgram = 0;
  }
  if (g_norProgram) {
    glDeleteProgram(g_norProgram);
    g_norProgram = 0;
  }
  if (g_mulHiProgram) {
    glDeleteProgram(g_mulHiProgram);
    g_mulHiProgram = 0;
  }
  if (g_andNotProgram) {
    glDeleteProgram(g_andNotProgram);
    g_andNotProgram = 0;
  }
  if (g_satAddProgram) {
    glDeleteProgram(g_satAddProgram);
    g_satAddProgram = 0;
  }
  if (g_satSubProgram) {
    glDeleteProgram(g_satSubProgram);
    g_satSubProgram = 0;
  }
  if (g_orNotProgram) {
    glDeleteProgram(g_orNotProgram);
    g_orNotProgram = 0;
  }
  if (g_avgProgram) {
    glDeleteProgram(g_avgProgram);
    g_avgProgram = 0;
  }
  if (g_isPow2Program) {
    glDeleteProgram(g_isPow2Program);
    g_isPow2Program = 0;
  }
  if (g_bswapProgram) {
    glDeleteProgram(g_bswapProgram);
    g_bswapProgram = 0;
  }
  if (g_parityProgram) {
    glDeleteProgram(g_parityProgram);
    g_parityProgram = 0;
  }
  if (g_bitTestProgram) {
    glDeleteProgram(g_bitTestProgram);
    g_bitTestProgram = 0;
  }
  if (g_hasZeroByteProgram) {
    glDeleteProgram(g_hasZeroByteProgram);
    g_hasZeroByteProgram = 0;
  }
  if (g_nextPow2Program) {
    glDeleteProgram(g_nextPow2Program);
    g_nextPow2Program = 0;
  }
  if (g_maskLowProgram) {
    glDeleteProgram(g_maskLowProgram);
    g_maskLowProgram = 0;
  }
  if (g_ilog2Program) {
    glDeleteProgram(g_ilog2Program);
    g_ilog2Program = 0;
  }
  if (g_broadcastByteProgram) {
    glDeleteProgram(g_broadcastByteProgram);
    g_broadcastByteProgram = 0;
  }
  if (g_byteSumProgram) {
    glDeleteProgram(g_byteSumProgram);
    g_byteSumProgram = 0;
  }
  if (g_sext8Program) {
    glDeleteProgram(g_sext8Program);
    g_sext8Program = 0;
  }
  if (g_sext16Program) {
    glDeleteProgram(g_sext16Program);
    g_sext16Program = 0;
  }
  if (g_extractByteProgram) {
    glDeleteProgram(g_extractByteProgram);
    g_extractByteProgram = 0;
  }
  if (g_bitSetProgram) {
    glDeleteProgram(g_bitSetProgram);
    g_bitSetProgram = 0;
  }
  if (g_bitClearProgram) {
    glDeleteProgram(g_bitClearProgram);
    g_bitClearProgram = 0;
  }
  if (g_bitToggleProgram) {
    glDeleteProgram(g_bitToggleProgram);
    g_bitToggleProgram = 0;
  }
  if (g_negProgram) {
    glDeleteProgram(g_negProgram);
    g_negProgram = 0;
  }
  if (g_absProgram) {
    glDeleteProgram(g_absProgram);
    g_absProgram = 0;
  }
  if (g_rolBytesProgram) {
    glDeleteProgram(g_rolBytesProgram);
    g_rolBytesProgram = 0;
  }
  if (g_rorBytesProgram) {
    glDeleteProgram(g_rorBytesProgram);
    g_rorBytesProgram = 0;
  }
  if (g_signMaskProgram) {
    glDeleteProgram(g_signMaskProgram);
    g_signMaskProgram = 0;
  }
  if (g_isZeroProgram) {
    glDeleteProgram(g_isZeroProgram);
    g_isZeroProgram = 0;
  }
  if (g_isNonzeroProgram) {
    glDeleteProgram(g_isNonzeroProgram);
    g_isNonzeroProgram = 0;
  }
  if (g_maskHighProgram) {
    glDeleteProgram(g_maskHighProgram);
    g_maskHighProgram = 0;
  }
  if (g_satMulProgram) {
    glDeleteProgram(g_satMulProgram);
    g_satMulProgram = 0;
  }
  if (g_context) {
    SDL_GL_DestroyContext(g_context);
    g_context = nullptr;
  }
  if (g_window) {
    SDL_DestroyWindow(g_window);
    g_window = nullptr;
  }
}

bool createContext(std::string& failure) {
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    failure = strf("SDL_Init(VIDEO) failed: {}", SDL_GetError());
    return false;
  }

  g_window = SDL_CreateWindow("worldgen-gl-compute-probe", 64, 64,
      SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  if (!g_window) {
    failure = strf("SDL_CreateWindow failed: {}", SDL_GetError());
    return false;
  }

  static const std::pair<int, int> versions[] = {
      {4, 6}, {4, 5}, {4, 4}, {4, 3}};
  for (auto const& ver : versions) {
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, ver.first);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, ver.second);
    g_context = SDL_GL_CreateContext(g_window);
    if (g_context) {
      g_info.contextMajor = ver.first;
      g_info.contextMinor = ver.second;
      break;
    }
  }
  if (!g_context) {
    failure = strf("SDL_GL_CreateContext failed (need GL 4.3+): {}", SDL_GetError());
    destroyContextUnlocked();
    return false;
  }
  if (!SDL_GL_MakeCurrent(g_window, g_context)) {
    failure = strf("SDL_GL_MakeCurrent failed: {}", SDL_GetError());
    destroyContextUnlocked();
    return false;
  }

  glewExperimental = GL_TRUE;
  GLenum glewResult = glewInit();
  if (glewResult != GLEW_OK && glewResult != GLEW_ERROR_NO_GLX_DISPLAY) {
    failure = strf("glewInit failed: {}", (char const*)glewGetErrorString(glewResult));
    destroyContextUnlocked();
    return false;
  }
  clearGlErrors();

  auto asCStr = [](GLubyte const* p) -> char const* {
    return p ? (char const*)p : "";
  };
  g_version = asCStr(glGetString(GL_VERSION));
  g_vendor = asCStr(glGetString(GL_VENDOR));
  g_renderer = asCStr(glGetString(GL_RENDERER));
  g_glsl = asCStr(glGetString(GL_SHADING_LANGUAGE_VERSION));
  g_info.glVersion = g_version.c_str();
  g_info.glVendor = g_vendor.c_str();
  g_info.glRenderer = g_renderer.c_str();
  g_info.glslVersion = g_glsl.c_str();
  g_info.contextCreated = true;

  bool compute = (GLEW_VERSION_4_3 != 0) || (GLEW_ARB_compute_shader != 0);
  if (!compute || !glDispatchCompute || !glFenceSync || !glBindBufferBase) {
    failure = "OpenGL compute entry points unavailable (need 4.3+ or ARB_compute_shader)";
    destroyContextUnlocked();
    return false;
  }

  std::string shaderError;
  auto requireProgram = [&](GLuint& slot, char const* source, char const* label) -> bool {
    slot = compileComputeProgram(source, shaderError);
    if (!slot) {
      failure = strf("{} shader failed: {}", label, shaderError);
      destroyContextUnlocked();
      return false;
    }
    return true;
  };
  if (!requireProgram(g_xorProgram, XorShader, "DenseUint32Xor")
      || !requireProgram(g_addProgram, AddShader, "DenseUint32Add")
      || !requireProgram(g_andProgram, AndShader, "DenseUint32And")
      || !requireProgram(g_orProgram, OrShader, "DenseUint32Or")
      || !requireProgram(g_mulProgram, MulShader, "DenseUint32Mul")
      || !requireProgram(g_minProgram, MinShader, "DenseUint32Min")
      || !requireProgram(g_maxProgram, MaxShader, "DenseUint32Max")
      || !requireProgram(g_subProgram, SubShader, "DenseUint32Sub")
      || !requireProgram(g_shlProgram, ShlShader, "DenseUint32Shl")
      || !requireProgram(g_shrProgram, ShrShader, "DenseUint32Shr")
      || !requireProgram(g_rotlProgram, RotlShader, "DenseUint32Rotl")
      || !requireProgram(g_rotrProgram, RotrShader, "DenseUint32Rotr")
      || !requireProgram(g_notProgram, NotShader, "DenseUint32Not")
      || !requireProgram(g_clzProgram, ClzShader, "DenseUint32Clz")
      || !requireProgram(g_ctzProgram, CtzShader, "DenseUint32Ctz")
      || !requireProgram(g_popcountProgram, PopcountShader, "DenseUint32Popcount")
      || !requireProgram(g_eqProgram, EqShader, "DenseUint32Eq")
      || !requireProgram(g_neProgram, NeShader, "DenseUint32Ne")
      || !requireProgram(g_bitReverseProgram, BitReverseShader, "DenseUint32BitReverse")
      || !requireProgram(g_ltProgram, LtShader, "DenseUint32Lt")
      || !requireProgram(g_leuProgram, LeuShader, "DenseUint32Leu")
      || !requireProgram(g_absDiffProgram, AbsDiffShader, "DenseUint32AbsDiff")
      || !requireProgram(g_gtProgram, GtShader, "DenseUint32Gt")
      || !requireProgram(g_geuProgram, GeuShader, "DenseUint32Geu")
      || !requireProgram(g_xnorProgram, XnorShader, "DenseUint32Xnor")
      || !requireProgram(g_nandProgram, NandShader, "DenseUint32Nand")
      || !requireProgram(g_norProgram, NorShader, "DenseUint32Nor")
      || !requireProgram(g_mulHiProgram, MulHiShader, "DenseUint32MulHi")
      || !requireProgram(g_andNotProgram, AndNotShader, "DenseUint32AndNot")
      || !requireProgram(g_satAddProgram, SatAddShader, "DenseUint32SatAdd")
      || !requireProgram(g_satSubProgram, SatSubShader, "DenseUint32SatSub")
      || !requireProgram(g_orNotProgram, OrNotShader, "DenseUint32OrNot")
      || !requireProgram(g_avgProgram, AvgShader, "DenseUint32Avg")
      || !requireProgram(g_isPow2Program, IsPow2Shader, "DenseUint32IsPow2")
      || !requireProgram(g_bswapProgram, BswapShader, "DenseUint32Bswap")
      || !requireProgram(g_parityProgram, ParityShader, "DenseUint32Parity")
      || !requireProgram(g_bitTestProgram, BitTestShader, "DenseUint32BitTest")
      || !requireProgram(g_hasZeroByteProgram, HasZeroByteShader, "DenseUint32HasZeroByte")
      || !requireProgram(g_nextPow2Program, NextPow2Shader, "DenseUint32NextPow2")
      || !requireProgram(g_maskLowProgram, MaskLowShader, "DenseUint32MaskLow")
      || !requireProgram(g_ilog2Program, Ilog2Shader, "DenseUint32Ilog2")
      || !requireProgram(g_broadcastByteProgram, BroadcastByteShader, "DenseUint32BroadcastByte")
      || !requireProgram(g_byteSumProgram, ByteSumShader, "DenseUint32ByteSum")
      || !requireProgram(g_sext8Program, Sext8Shader, "DenseUint32Sext8")
      || !requireProgram(g_sext16Program, Sext16Shader, "DenseUint32Sext16")
      || !requireProgram(g_extractByteProgram, ExtractByteShader, "DenseUint32ExtractByte")
      || !requireProgram(g_bitSetProgram, BitSetShader, "DenseUint32BitSet")
      || !requireProgram(g_bitClearProgram, BitClearShader, "DenseUint32BitClear")
      || !requireProgram(g_bitToggleProgram, BitToggleShader, "DenseUint32BitToggle")
      || !requireProgram(g_negProgram, NegShader, "DenseUint32Neg")
      || !requireProgram(g_absProgram, AbsShader, "DenseUint32Abs")
      || !requireProgram(g_rolBytesProgram, RolBytesShader, "DenseUint32RolBytes")
      || !requireProgram(g_rorBytesProgram, RorBytesShader, "DenseUint32RorBytes")
      || !requireProgram(g_signMaskProgram, SignMaskShader, "DenseUint32SignMask")
      || !requireProgram(g_isZeroProgram, IsZeroShader, "DenseUint32IsZero")
      || !requireProgram(g_isNonzeroProgram, IsNonzeroShader, "DenseUint32IsNonzero")
      || !requireProgram(g_maskHighProgram, MaskHighShader, "DenseUint32MaskHigh")
      || !requireProgram(g_satMulProgram, SatMulShader, "DenseUint32SatMul"))
    return false;
  glGenBuffers(1, &g_ssbo);
  if (!g_ssbo) {
    failure = "glGenBuffers failed for SSBO";
    destroyContextUnlocked();
    return false;
  }
  g_info.computeAvailable = true;
  return true;
}

void runProbe() {
  g_info = {};
  g_info.buildEnabled = true;
  std::string failure;
  if (!createContext(failure)) {
    g_failure = failure;
    g_info.failureReason = g_failure.c_str();
    g_info.computeAvailable = false;
    return;
  }
  g_info.failureReason = "";
}

} // namespace

bool WorldGenerationComputeGlDevice::isBuildEnabled() noexcept {
  return true;
}

WorldGenerationComputeCapabilities WorldGenerationComputeGlDevice::probeCapabilities() noexcept {
  std::lock_guard<std::mutex> lock(g_glMutex);
  std::call_once(g_probeOnce, runProbe);
  WorldGenerationComputeCapabilities caps;
  caps.cpuReference = true;
  caps.gpuOpenGlCompute = g_info.computeAvailable;
  caps.maxWorkGroupInvocations = 64;
  caps.maxInFlight = 8;
  caps.maxImmutableBufferBytes = 16ull * 1024ull * 1024ull;
  caps.maxStagingBytes = 32ull * 1024ull * 1024ull;
  if (g_info.computeAvailable) {
    GLint invocations = 0;
    glGetIntegerv(GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS, &invocations);
    if (invocations > 0)
      caps.maxWorkGroupInvocations = (uint32_t)invocations;
  }
  return caps;
}

WorldGenerationComputeGlProbeInfo const& WorldGenerationComputeGlDevice::lastProbeInfo() noexcept {
  probeCapabilities();
  return g_info;
}

bool WorldGenerationComputeGlDevice::dispatch(WorldGenerationComputeKernel kernel,
    uint32_t kernelVersion, List<uint32_t> const& input, uint32_t parameter,
    List<uint32_t>& output) {
  std::lock_guard<std::mutex> lock(g_glMutex);
  std::call_once(g_probeOnce, runProbe);
  if (!g_info.computeAvailable || !g_context || !g_window)
    return false;
  if (kernelVersion != 1u)
    return false;
  GLuint program = 0;
  if (kernel == WorldGenerationComputeKernel::DenseUint32Xor)
    program = g_xorProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32Add)
    program = g_addProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32And)
    program = g_andProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32Or)
    program = g_orProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32Mul)
    program = g_mulProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32Min)
    program = g_minProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32Max)
    program = g_maxProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32Sub)
    program = g_subProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32Shl)
    program = g_shlProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32Shr)
    program = g_shrProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32Rotl)
    program = g_rotlProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32Rotr)
    program = g_rotrProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32Not)
    program = g_notProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32Clz)
    program = g_clzProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32Ctz)
    program = g_ctzProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32Popcount)
    program = g_popcountProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32Eq)
    program = g_eqProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32Ne)
    program = g_neProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32BitReverse)
    program = g_bitReverseProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32Lt)
    program = g_ltProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32Leu)
    program = g_leuProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32AbsDiff)
    program = g_absDiffProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32Gt)
    program = g_gtProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32Geu)
    program = g_geuProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32Xnor)
    program = g_xnorProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32Nand)
    program = g_nandProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32Nor)
    program = g_norProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32MulHi)
    program = g_mulHiProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32AndNot)
    program = g_andNotProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32SatAdd)
    program = g_satAddProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32SatSub)
    program = g_satSubProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32OrNot)
    program = g_orNotProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32Avg)
    program = g_avgProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32IsPow2)
    program = g_isPow2Program;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32Bswap)
    program = g_bswapProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32Parity)
    program = g_parityProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32BitTest)
    program = g_bitTestProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32HasZeroByte)
    program = g_hasZeroByteProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32NextPow2)
    program = g_nextPow2Program;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32MaskLow)
    program = g_maskLowProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32Ilog2)
    program = g_ilog2Program;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32BroadcastByte)
    program = g_broadcastByteProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32ByteSum)
    program = g_byteSumProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32Sext8)
    program = g_sext8Program;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32Sext16)
    program = g_sext16Program;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32ExtractByte)
    program = g_extractByteProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32BitSet)
    program = g_bitSetProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32BitClear)
    program = g_bitClearProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32BitToggle)
    program = g_bitToggleProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32Neg)
    program = g_negProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32Abs)
    program = g_absProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32RolBytes)
    program = g_rolBytesProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32RorBytes)
    program = g_rorBytesProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32SignMask)
    program = g_signMaskProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32IsZero)
    program = g_isZeroProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32IsNonzero)
    program = g_isNonzeroProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32MaskHigh)
    program = g_maskHighProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32SatMul)
    program = g_satMulProgram;
  else
    return false;
  if (!program || !SDL_GL_MakeCurrent(g_window, g_context))
    return false;

  clearGlErrors();
  uint32_t count = (uint32_t)input.size();
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, g_ssbo);
  glBufferData(GL_SHADER_STORAGE_BUFFER, (GLsizeiptr)(count * sizeof(uint32_t)),
      count ? input.ptr() : nullptr, GL_DYNAMIC_COPY);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, g_ssbo);

  glUseProgram(program);
  GLint locParam = glGetUniformLocation(program, "uParameter");
  GLint locCount = glGetUniformLocation(program, "uCount");
  if (locParam >= 0)
    glUniform1ui(locParam, parameter);
  if (locCount >= 0)
    glUniform1ui(locCount, count);

  GLuint groups = count == 0 ? 1u : (count + 63u) / 64u;
  glDispatchCompute(groups, 1, 1);
  glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);

  GLsync fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
  if (!fence)
    return false;
  GLenum wait = glClientWaitSync(fence, GL_SYNC_FLUSH_COMMANDS_BIT, 5'000'000'000ULL);
  glDeleteSync(fence);
  if (wait != GL_ALREADY_SIGNALED && wait != GL_CONDITION_SATISFIED)
    return false;

  output = List<uint32_t>(count, 0u);
  if (count)
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, (GLsizeiptr)(count * sizeof(uint32_t)),
        output.ptr());
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
  glUseProgram(0);
  return !clearGlErrors();
}

void WorldGenerationComputeGlDevice::shutdown() noexcept {
  std::lock_guard<std::mutex> lock(g_glMutex);
  destroyContextUnlocked();
  g_info.computeAvailable = false;
  g_info.contextCreated = false;
}

}

#endif
