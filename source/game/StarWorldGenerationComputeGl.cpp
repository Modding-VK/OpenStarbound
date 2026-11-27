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
GLuint g_swapHalvesProgram = 0;
GLuint g_cloProgram = 0;
GLuint g_ctoProgram = 0;
GLuint g_reverseNibblesProgram = 0;
GLuint g_byteMaxProgram = 0;
GLuint g_byteMinProgram = 0;
GLuint g_dupLow16Program = 0;
GLuint g_dupHigh16Program = 0;
GLuint g_reverseBitsInBytesProgram = 0;
GLuint g_isolateLsbProgram = 0;
GLuint g_isolateMsbProgram = 0;
GLuint g_lsbMaskProgram = 0;
GLuint g_clearLsbProgram = 0;
GLuint g_grayEncodeProgram = 0;
GLuint g_grayDecodeProgram = 0;
GLuint g_smearLsbProgram = 0;
GLuint g_clearMsbProgram = 0;
GLuint g_byteXorProgram = 0;
GLuint g_smearMsbProgram = 0;
GLuint g_byteOrProgram = 0;
GLuint g_byteAndProgram = 0;
GLuint g_keepOddBitsProgram = 0;
GLuint g_keepEvenBitsProgram = 0;
GLuint g_swapAdjacentBitsProgram = 0;
GLuint g_compressOddBitsProgram = 0;
GLuint g_compressEvenBitsProgram = 0;
GLuint g_expandLow16ToOddProgram = 0;
GLuint g_expandLow16ToEvenProgram = 0;
GLuint g_interleaveHalvesProgram = 0;
GLuint g_deinterleaveHalvesProgram = 0;
GLuint g_nibbleXorProgram = 0;
GLuint g_nibbleOrProgram = 0;
GLuint g_nibbleAndProgram = 0;
GLuint g_nibbleSumProgram = 0;
GLuint g_nibbleMaxProgram = 0;
GLuint g_nibbleMinProgram = 0;
GLuint g_extractNibbleProgram = 0;
GLuint g_insertNibbleProgram = 0;
GLuint g_broadcastNibbleProgram = 0;
GLuint g_swapAdjacentNibblesProgram = 0;
GLuint g_rolNibblesProgram = 0;
GLuint g_rorNibblesProgram = 0;
GLuint g_hasZeroNibbleProgram = 0;
GLuint g_sextNibbleProgram = 0;
GLuint g_countZeroNibblesProgram = 0;
GLuint g_packLowNibblesProgram = 0;
GLuint g_packHighNibblesProgram = 0;
GLuint g_expandLowNibblesProgram = 0;
GLuint g_expandHighNibblesProgram = 0;
GLuint g_replaceNibbleProgram = 0;
GLuint g_maskNibblesProgram = 0;
GLuint g_allNibblesEqualProgram = 0;
GLuint g_nibbleRangeProgram = 0;
GLuint g_broadcastNibbleAtProgram = 0;
GLuint g_halfSumProgram = 0;
GLuint g_halfMaxProgram = 0;
GLuint g_halfMinProgram = 0;
GLuint g_halfXorProgram = 0;
GLuint g_halfOrProgram = 0;
GLuint g_halfAndProgram = 0;
GLuint g_broadcastHalfProgram = 0;
GLuint g_halfEqProgram = 0;
GLuint g_halfRangeProgram = 0;
GLuint g_halfLtProgram = 0;
GLuint g_halfGtProgram = 0;
GLuint g_halfNeProgram = 0;
GLuint g_halfLeuProgram = 0;
GLuint g_halfGeuProgram = 0;
GLuint g_halfAvgProgram = 0;
GLuint g_broadcastHighHalfProgram = 0;
GLuint g_halfSubProgram = 0;
GLuint g_halfMulProgram = 0;
GLuint g_halfSatAddProgram = 0;
GLuint g_halfSatSubProgram = 0;
GLuint g_halfMulHiProgram = 0;
GLuint g_halfDivProgram = 0;
GLuint g_halfModProgram = 0;
GLuint g_halfCeilDivProgram = 0;
GLuint g_halfIsDivisibleProgram = 0;
GLuint g_halfGcdProgram = 0;
GLuint g_halfSelectProgram = 0;
GLuint g_halfLcmProgram = 0;
GLuint g_halfCoprimeProgram = 0;
GLuint g_halfAlignUpProgram = 0;
GLuint g_halfAlignDownProgram = 0;
GLuint g_halfRotlProgram = 0;
GLuint g_halfRotrProgram = 0;
GLuint g_halfPopcountProgram = 0;
GLuint g_halfClzProgram = 0;
GLuint g_halfCtzProgram = 0;
GLuint g_halfParityProgram = 0;
GLuint g_halfBitReverseProgram = 0;
GLuint g_halfBswapProgram = 0;
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

char const* SwapHalvesShader = R"GLSL(
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
  values[i] = (v << 16) | (v >> 16);
}
)GLSL";

char const* CloShader = R"GLSL(
#version 430
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Data { uint values[]; };
uniform uint uParameter;
uniform uint uCount;
void main() {
  uint i = gl_GlobalInvocationID.x;
  if (i >= uCount)
    return;
  values[i] = findMSB(~values[i]) == -1 ? 32u : (31u - uint(findMSB(~values[i])));
}
)GLSL";

char const* CtoShader = R"GLSL(
#version 430
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Data { uint values[]; };
uniform uint uParameter;
uniform uint uCount;
void main() {
  uint i = gl_GlobalInvocationID.x;
  if (i >= uCount)
    return;
  values[i] = findLSB(~values[i]) == -1 ? 32u : uint(findLSB(~values[i]));
}
)GLSL";

char const* ReverseNibblesShader = R"GLSL(
#version 430
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Data { uint values[]; };
uniform uint uParameter;
uniform uint uCount;
void main() {
  uint i = gl_GlobalInvocationID.x;
  if (i >= uCount)
    return;
  uint value = values[i];
  uint v = ((value & 0x000000ffu) << 24) | ((value & 0x0000ff00u) << 8)
      | ((value & 0x00ff0000u) >> 8) | ((value & 0xff000000u) >> 24);
  values[i] = ((v & 0x0f0f0f0fu) << 4) | ((v & 0xf0f0f0f0u) >> 4);
}
)GLSL";

char const* ByteMaxShader = R"GLSL(
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
  uint b0 = v & 0xffu;
  uint b1 = (v >> 8) & 0xffu;
  uint b2 = (v >> 16) & 0xffu;
  uint b3 = (v >> 24) & 0xffu;
  uint m = max(max(b0, b1), max(b2, b3));
  values[i] = m;
}
)GLSL";

char const* ByteMinShader = R"GLSL(
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
  uint b0 = v & 0xffu;
  uint b1 = (v >> 8) & 0xffu;
  uint b2 = (v >> 16) & 0xffu;
  uint b3 = (v >> 24) & 0xffu;
  uint m = min(min(b0, b1), min(b2, b3));
  values[i] = m;
}
)GLSL";

char const* DupLow16Shader = R"GLSL(
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
  values[i] = low | (low << 16);
}
)GLSL";

char const* DupHigh16Shader = R"GLSL(
#version 430
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Data { uint values[]; };
uniform uint uParameter;
uniform uint uCount;
void main() {
  uint i = gl_GlobalInvocationID.x;
  if (i >= uCount)
    return;
  uint high = values[i] & 0xffff0000u;
  values[i] = high | (high >> 16);
}
)GLSL";

char const* ReverseBitsInBytesShader = R"GLSL(
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
  v = ((v >> 1) & 0x55555555u) | ((v & 0x55555555u) << 1);
  v = ((v >> 2) & 0x33333333u) | ((v & 0x33333333u) << 2);
  v = ((v >> 4) & 0x0f0f0f0fu) | ((v & 0x0f0f0f0fu) << 4);
  values[i] = v;
}
)GLSL";

char const* IsolateLsbShader = R"GLSL(
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
  values[i] = v & uint(-int(v));
}
)GLSL";

char const* IsolateMsbShader = R"GLSL(
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
  values[i] = v == 0u ? 0u : (1u << uint(findMSB(v)));
}
)GLSL";

char const* LsbMaskShader = R"GLSL(
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
  values[i] = v ^ (v - 1u);
}
)GLSL";

char const* ClearLsbShader = R"GLSL(
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
  values[i] = v & (v - 1u);
}
)GLSL";

char const* GrayEncodeShader = R"GLSL(
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
  values[i] = v ^ (v >> 1);
}
)GLSL";

char const* GrayDecodeShader = R"GLSL(
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
  v ^= v >> 1;
  v ^= v >> 2;
  v ^= v >> 4;
  v ^= v >> 8;
  v ^= v >> 16;
  values[i] = v;
}
)GLSL";

char const* SmearLsbShader = R"GLSL(
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
  values[i] = v | (v - 1u);
}
)GLSL";

char const* ClearMsbShader = R"GLSL(
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
  values[i] = v ^ (1u << findMSB(v));
}
)GLSL";

char const* ByteXorShader = R"GLSL(
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
  values[i] = (v ^ (v >> 8) ^ (v >> 16) ^ (v >> 24)) & 0xffu;
}
)GLSL";

char const* SmearMsbShader = R"GLSL(
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
  v |= v >> 1;
  v |= v >> 2;
  v |= v >> 4;
  v |= v >> 8;
  v |= v >> 16;
  values[i] = v;
}
)GLSL";

char const* ByteOrShader = R"GLSL(
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
  values[i] = (v | (v >> 8) | (v >> 16) | (v >> 24)) & 0xffu;
}
)GLSL";

char const* ByteAndShader = R"GLSL(
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
  values[i] = (v & (v >> 8) & (v >> 16) & (v >> 24)) & 0xffu;
}
)GLSL";

char const* KeepOddBitsShader = R"GLSL(
#version 430
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Data { uint values[]; };
uniform uint uParameter;
uniform uint uCount;
void main() {
  uint i = gl_GlobalInvocationID.x;
  if (i >= uCount)
    return;
  values[i] = values[i] & 0x55555555u;
}
)GLSL";

char const* KeepEvenBitsShader = R"GLSL(
#version 430
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Data { uint values[]; };
uniform uint uParameter;
uniform uint uCount;
void main() {
  uint i = gl_GlobalInvocationID.x;
  if (i >= uCount)
    return;
  values[i] = values[i] & 0xaaaaaaaau;
}
)GLSL";

char const* SwapAdjacentBitsShader = R"GLSL(
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
  values[i] = ((v & 0x55555555u) << 1) | ((v & 0xaaaaaaaau) >> 1);
}
)GLSL";

char const* CompressOddBitsShader = R"GLSL(
#version 430
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Data { uint values[]; };
uniform uint uParameter;
uniform uint uCount;
void main() {
  uint i = gl_GlobalInvocationID.x;
  if (i >= uCount)
    return;
  uint x = values[i] & 0x55555555u;
  x = (x | (x >> 1)) & 0x33333333u;
  x = (x | (x >> 2)) & 0x0f0f0f0fu;
  x = (x | (x >> 4)) & 0x00ff00ffu;
  x = (x | (x >> 8)) & 0x0000ffffu;
  values[i] = x;
}
)GLSL";

char const* CompressEvenBitsShader = R"GLSL(
#version 430
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Data { uint values[]; };
uniform uint uParameter;
uniform uint uCount;
void main() {
  uint i = gl_GlobalInvocationID.x;
  if (i >= uCount)
    return;
  uint x = (values[i] >> 1) & 0x55555555u;
  x = (x | (x >> 1)) & 0x33333333u;
  x = (x | (x >> 2)) & 0x0f0f0f0fu;
  x = (x | (x >> 4)) & 0x00ff00ffu;
  x = (x | (x >> 8)) & 0x0000ffffu;
  values[i] = x;
}
)GLSL";

char const* ExpandLow16ToOddShader = R"GLSL(
#version 430
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Data { uint values[]; };
uniform uint uParameter;
uniform uint uCount;
void main() {
  uint i = gl_GlobalInvocationID.x;
  if (i >= uCount)
    return;
  uint x = values[i] & 0xffffu;
  x = (x | (x << 8)) & 0x00ff00ffu;
  x = (x | (x << 4)) & 0x0f0f0f0fu;
  x = (x | (x << 2)) & 0x33333333u;
  x = (x | (x << 1)) & 0x55555555u;
  values[i] = x;
}
)GLSL";

char const* ExpandLow16ToEvenShader = R"GLSL(
#version 430
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Data { uint values[]; };
uniform uint uParameter;
uniform uint uCount;
void main() {
  uint i = gl_GlobalInvocationID.x;
  if (i >= uCount)
    return;
  uint x = values[i] & 0xffffu;
  x = (x | (x << 8)) & 0x00ff00ffu;
  x = (x | (x << 4)) & 0x0f0f0f0fu;
  x = (x | (x << 2)) & 0x33333333u;
  x = (x | (x << 1)) & 0x55555555u;
  values[i] = x << 1;
}
)GLSL";

char const* InterleaveHalvesShader = R"GLSL(
#version 430
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Data { uint values[]; };
uniform uint uParameter;
uniform uint uCount;
uint expandOdd(uint x) {
  x &= 0xffffu;
  x = (x | (x << 8)) & 0x00ff00ffu;
  x = (x | (x << 4)) & 0x0f0f0f0fu;
  x = (x | (x << 2)) & 0x33333333u;
  x = (x | (x << 1)) & 0x55555555u;
  return x;
}
void main() {
  uint i = gl_GlobalInvocationID.x;
  if (i >= uCount)
    return;
  uint v = values[i];
  values[i] = (expandOdd(v >> 16) << 1) | expandOdd(v);
}
)GLSL";

char const* DeinterleaveHalvesShader = R"GLSL(
#version 430
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Data { uint values[]; };
uniform uint uParameter;
uniform uint uCount;
uint compressOdd(uint x) {
  x &= 0x55555555u;
  x = (x | (x >> 1)) & 0x33333333u;
  x = (x | (x >> 2)) & 0x0f0f0f0fu;
  x = (x | (x >> 4)) & 0x00ff00ffu;
  x = (x | (x >> 8)) & 0x0000ffffu;
  return x;
}
void main() {
  uint i = gl_GlobalInvocationID.x;
  if (i >= uCount)
    return;
  uint v = values[i];
  values[i] = (compressOdd(v >> 1) << 16) | compressOdd(v);
}
)GLSL";

char const* NibbleXorShader = R"GLSL(
#version 430
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Data { uint values[]; };
uniform uint uParameter;
uniform uint uCount;
void main() {
  uint i = gl_GlobalInvocationID.x;
  if (i >= uCount)
    return;
  uint x = values[i];
  x ^= x >> 4;
  x ^= x >> 8;
  x ^= x >> 16;
  values[i] = x & 0xfu;
}
)GLSL";

char const* NibbleOrShader = R"GLSL(
#version 430
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Data { uint values[]; };
uniform uint uParameter;
uniform uint uCount;
void main() {
  uint i = gl_GlobalInvocationID.x;
  if (i >= uCount)
    return;
  uint x = values[i];
  x |= x >> 4;
  x |= x >> 8;
  x |= x >> 16;
  values[i] = x & 0xfu;
}
)GLSL";

char const* NibbleAndShader = R"GLSL(
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
  uint m = (v & 0x0f0f0f0fu) & ((v >> 4) & 0x0f0f0f0fu);
  m &= m >> 8;
  m &= m >> 16;
  values[i] = m & 0xfu;
}
)GLSL";

char const* NibbleSumShader = R"GLSL(
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
  uint x = (v & 0x0f0f0f0fu) + ((v >> 4) & 0x0f0f0f0fu);
  x = (x & 0xffu) + ((x >> 8) & 0xffu) + ((x >> 16) & 0xffu) + ((x >> 24) & 0xffu);
  values[i] = x;
}
)GLSL";

char const* NibbleMaxShader = R"GLSL(
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
  uint m = v & 0xfu;
  m = max(m, (v >> 4) & 0xfu);
  m = max(m, (v >> 8) & 0xfu);
  m = max(m, (v >> 12) & 0xfu);
  m = max(m, (v >> 16) & 0xfu);
  m = max(m, (v >> 20) & 0xfu);
  m = max(m, (v >> 24) & 0xfu);
  m = max(m, (v >> 28) & 0xfu);
  values[i] = m;
}
)GLSL";

char const* NibbleMinShader = R"GLSL(
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
  uint m = v & 0xfu;
  m = min(m, (v >> 4) & 0xfu);
  m = min(m, (v >> 8) & 0xfu);
  m = min(m, (v >> 12) & 0xfu);
  m = min(m, (v >> 16) & 0xfu);
  m = min(m, (v >> 20) & 0xfu);
  m = min(m, (v >> 24) & 0xfu);
  m = min(m, (v >> 28) & 0xfu);
  values[i] = m;
}
)GLSL";

char const* ExtractNibbleShader = R"GLSL(
#version 430
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Data { uint values[]; };
uniform uint uParameter;
uniform uint uCount;
void main() {
  uint i = gl_GlobalInvocationID.x;
  if (i >= uCount)
    return;
  uint shift = (uParameter & 7u) * 4u;
  values[i] = (values[i] >> shift) & 0xfu;
}
)GLSL";

char const* InsertNibbleShader = R"GLSL(
#version 430
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Data { uint values[]; };
uniform uint uParameter;
uniform uint uCount;
void main() {
  uint i = gl_GlobalInvocationID.x;
  if (i >= uCount)
    return;
  uint shift = (uParameter & 7u) * 4u;
  uint nib = (uParameter >> 8) & 0xfu;
  uint mask = ~(0xfu << shift);
  values[i] = (values[i] & mask) | (nib << shift);
}
)GLSL";

char const* BroadcastNibbleShader = R"GLSL(
#version 430
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Data { uint values[]; };
uniform uint uParameter;
uniform uint uCount;
void main() {
  uint i = gl_GlobalInvocationID.x;
  if (i >= uCount)
    return;
  uint n = values[i] & 0xfu;
  values[i] = n * 0x11111111u;
}
)GLSL";

char const* SwapAdjacentNibblesShader = R"GLSL(
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
  values[i] = ((v & 0x0f0f0f0fu) << 4) | ((v & 0xf0f0f0f0u) >> 4);
}
)GLSL";

char const* RolNibblesShader = R"GLSL(
#version 430
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Data { uint values[]; };
uniform uint uParameter;
uniform uint uCount;
void main() {
  uint i = gl_GlobalInvocationID.x;
  if (i >= uCount)
    return;
  uint r = (uParameter & 7u) * 4u;
  values[i] = (values[i] << r) | (values[i] >> ((32u - r) & 31u));
}
)GLSL";

char const* RorNibblesShader = R"GLSL(
#version 430
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Data { uint values[]; };
uniform uint uParameter;
uniform uint uCount;
void main() {
  uint i = gl_GlobalInvocationID.x;
  if (i >= uCount)
    return;
  uint r = (uParameter & 7u) * 4u;
  values[i] = (values[i] >> r) | (values[i] << ((32u - r) & 31u));
}
)GLSL";

char const* HasZeroNibbleShader = R"GLSL(
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
  bool hasZero = ((v - 0x11111111u) & ~v & 0x88888888u) != 0u;
  values[i] = hasZero ? 0xffffffffu : 0u;
}
)GLSL";

char const* SextNibbleShader = R"GLSL(
#version 430
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Data { uint values[]; };
uniform uint uParameter;
uniform uint uCount;
void main() {
  uint i = gl_GlobalInvocationID.x;
  if (i >= uCount)
    return;
  uint shift = (uParameter & 7u) * 4u;
  uint n = (values[i] >> shift) & 0xfu;
  values[i] = (n & 0x8u) != 0u ? (n | 0xfffffff0u) : n;
}
)GLSL";

char const* CountZeroNibblesShader = R"GLSL(
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
  uint count = 0u;
  for (uint shift = 0u; shift < 32u; shift += 4u) {
    if (((v >> shift) & 0xfu) == 0u)
      count += 1u;
  }
  values[i] = count;
}
)GLSL";

char const* PackLowNibblesShader = R"GLSL(
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
  values[i] = (v & 0xfu) | (((v >> 8) & 0xfu) << 4) | (((v >> 16) & 0xfu) << 8)
      | (((v >> 24) & 0xfu) << 12);
}
)GLSL";

char const* PackHighNibblesShader = R"GLSL(
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
  values[i] = ((v >> 4) & 0xfu) | (((v >> 12) & 0xfu) << 4) | (((v >> 20) & 0xfu) << 8)
      | (((v >> 28) & 0xfu) << 12);
}
)GLSL";

char const* ExpandLowNibblesShader = R"GLSL(
#version 430
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Data { uint values[]; };
uniform uint uParameter;
uniform uint uCount;
void main() {
  uint i = gl_GlobalInvocationID.x;
  if (i >= uCount)
    return;
  uint p = values[i] & 0xffffu;
  values[i] = (p & 0xfu) | (((p >> 4) & 0xfu) << 8) | (((p >> 8) & 0xfu) << 16)
      | (((p >> 12) & 0xfu) << 24);
}
)GLSL";

char const* ExpandHighNibblesShader = R"GLSL(
#version 430
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Data { uint values[]; };
uniform uint uParameter;
uniform uint uCount;
void main() {
  uint i = gl_GlobalInvocationID.x;
  if (i >= uCount)
    return;
  uint p = values[i] & 0xffffu;
  values[i] = ((p & 0xfu) << 4) | (((p >> 4) & 0xfu) << 12) | (((p >> 8) & 0xfu) << 20)
      | (((p >> 12) & 0xfu) << 28);
}
)GLSL";

char const* ReplaceNibbleShader = R"GLSL(
#version 430
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Data { uint values[]; };
uniform uint uParameter;
uniform uint uCount;
void main() {
  uint i = gl_GlobalInvocationID.x;
  if (i >= uCount)
    return;
  uint from = uParameter & 0xfu;
  uint to = (uParameter >> 8) & 0xfu;
  uint v = values[i];
  uint result = 0u;
  for (uint n = 0u; n < 8u; ++n) {
    uint shift = n * 4u;
    uint nib = (v >> shift) & 0xfu;
    result |= (nib == from ? to : nib) << shift;
  }
  values[i] = result;
}
)GLSL";

char const* MaskNibblesShader = R"GLSL(
#version 430
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Data { uint values[]; };
uniform uint uParameter;
uniform uint uCount;
void main() {
  uint i = gl_GlobalInvocationID.x;
  if (i >= uCount)
    return;
  uint select = uParameter & 0xffu;
  uint v = values[i];
  uint result = 0u;
  for (uint n = 0u; n < 8u; ++n) {
    if ((select & (1u << n)) != 0u) {
      uint shift = n * 4u;
      result |= v & (0xfu << shift);
    }
  }
  values[i] = result;
}
)GLSL";

char const* AllNibblesEqualShader = R"GLSL(
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
  uint first = v & 0xfu;
  bool equal = true;
  for (uint shift = 4u; shift < 32u; shift += 4u) {
    if (((v >> shift) & 0xfu) != first) {
      equal = false;
      break;
    }
  }
  values[i] = equal ? 0xffffffffu : 0u;
}
)GLSL";

char const* NibbleRangeShader = R"GLSL(
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
  uint lo = v & 0xfu;
  uint hi = lo;
  for (uint shift = 4u; shift < 32u; shift += 4u) {
    uint n = (v >> shift) & 0xfu;
    lo = min(lo, n);
    hi = max(hi, n);
  }
  values[i] = hi - lo;
}
)GLSL";

char const* BroadcastNibbleAtShader = R"GLSL(
#version 430
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Data { uint values[]; };
uniform uint uParameter;
uniform uint uCount;
void main() {
  uint i = gl_GlobalInvocationID.x;
  if (i >= uCount)
    return;
  uint shift = (uParameter & 7u) * 4u;
  uint n = (values[i] >> shift) & 0xfu;
  values[i] = n * 0x11111111u;
}
)GLSL";

char const* HalfSumShader = R"GLSL(
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
  values[i] = (v & 0xffffu) + (v >> 16);
}
)GLSL";

char const* HalfMaxShader = R"GLSL(
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
  uint lo = v & 0xffffu;
  uint hi = v >> 16;
  values[i] = max(lo, hi);
}
)GLSL";

char const* HalfMinShader = R"GLSL(
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
  uint lo = v & 0xffffu;
  uint hi = v >> 16;
  values[i] = min(lo, hi);
}
)GLSL";

char const* HalfXorShader = R"GLSL(
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
  values[i] = (v & 0xffffu) ^ (v >> 16);
}
)GLSL";

char const* HalfOrShader = R"GLSL(
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
  values[i] = (v & 0xffffu) | (v >> 16);
}
)GLSL";

char const* HalfAndShader = R"GLSL(
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
  values[i] = (v & 0xffffu) & (v >> 16);
}
)GLSL";

char const* BroadcastHalfShader = R"GLSL(
#version 430
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Data { uint values[]; };
uniform uint uParameter;
uniform uint uCount;
void main() {
  uint i = gl_GlobalInvocationID.x;
  if (i >= uCount)
    return;
  uint lo = values[i] & 0xffffu;
  values[i] = lo | (lo << 16);
}
)GLSL";

char const* HalfEqShader = R"GLSL(
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
  uint lo = v & 0xffffu;
  uint hi = v >> 16;
  values[i] = (lo == hi) ? 0xffffffffu : 0u;
}
)GLSL";

char const* HalfRangeShader = R"GLSL(
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
  uint lo = v & 0xffffu;
  uint hi = v >> 16;
  values[i] = lo > hi ? (lo - hi) : (hi - lo);
}
)GLSL";

char const* HalfLtShader = R"GLSL(
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
  uint lo = v & 0xffffu;
  uint hi = v >> 16;
  values[i] = (lo < hi) ? 0xffffffffu : 0u;
}
)GLSL";

char const* HalfGtShader = R"GLSL(
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
  uint lo = v & 0xffffu;
  uint hi = v >> 16;
  values[i] = (lo > hi) ? 0xffffffffu : 0u;
}
)GLSL";

char const* HalfNeShader = R"GLSL(
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
  uint lo = v & 0xffffu;
  uint hi = v >> 16;
  values[i] = (lo != hi) ? 0xffffffffu : 0u;
}
)GLSL";

char const* HalfLeuShader = R"GLSL(
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
  uint lo = v & 0xffffu;
  uint hi = v >> 16;
  values[i] = (lo <= hi) ? 0xffffffffu : 0u;
}
)GLSL";

char const* HalfGeuShader = R"GLSL(
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
  uint lo = v & 0xffffu;
  uint hi = v >> 16;
  values[i] = (lo >= hi) ? 0xffffffffu : 0u;
}
)GLSL";

char const* HalfAvgShader = R"GLSL(
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
  uint lo = v & 0xffffu;
  uint hi = v >> 16;
  values[i] = (lo + hi) >> 1;
}
)GLSL";

char const* BroadcastHighHalfShader = R"GLSL(
#version 430
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Data { uint values[]; };
uniform uint uParameter;
uniform uint uCount;
void main() {
  uint i = gl_GlobalInvocationID.x;
  if (i >= uCount)
    return;
  uint hi = values[i] >> 16;
  values[i] = hi | (hi << 16);
}
)GLSL";

char const* HalfSubShader = R"GLSL(
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
  uint lo = v & 0xffffu;
  uint hi = v >> 16;
  values[i] = lo - hi;
}
)GLSL";

char const* HalfMulShader = R"GLSL(
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
  uint lo = v & 0xffffu;
  uint hi = v >> 16;
  values[i] = lo * hi;
}
)GLSL";

char const* HalfSatAddShader = R"GLSL(
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
  uint lo = v & 0xffffu;
  uint hi = v >> 16;
  uint sum = lo + hi;
  values[i] = sum > 0xffffu ? 0xffffu : sum;
}
)GLSL";

char const* HalfSatSubShader = R"GLSL(
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
  uint lo = v & 0xffffu;
  uint hi = v >> 16;
  values[i] = lo > hi ? (lo - hi) : 0u;
}
)GLSL";

char const* HalfMulHiShader = R"GLSL(
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
  uint lo = v & 0xffffu;
  uint hi = v >> 16;
  values[i] = (lo * hi) >> 16;
}
)GLSL";

char const* HalfDivShader = R"GLSL(
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
  uint lo = v & 0xffffu;
  uint hi = v >> 16;
  values[i] = (hi == 0u) ? 0u : (lo / hi);
}
)GLSL";

char const* HalfModShader = R"GLSL(
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
  uint lo = v & 0xffffu;
  uint hi = v >> 16;
  values[i] = (hi == 0u) ? 0u : (lo % hi);
}
)GLSL";

char const* HalfCeilDivShader = R"GLSL(
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
  uint lo = v & 0xffffu;
  uint hi = v >> 16;
  values[i] = (hi == 0u) ? 0u : ((lo + hi - 1u) / hi);
}
)GLSL";

char const* HalfIsDivisibleShader = R"GLSL(
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
  uint lo = v & 0xffffu;
  uint hi = v >> 16;
  values[i] = (hi != 0u && (lo % hi) == 0u) ? 0xffffffffu : 0u;
}
)GLSL";

char const* HalfGcdShader = R"GLSL(
#version 430
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Data { uint values[]; };
uniform uint uParameter;
uniform uint uCount;
void main() {
  uint i = gl_GlobalInvocationID.x;
  if (i >= uCount)
    return;
  uint a = values[i] & 0xffffu;
  uint b = values[i] >> 16;
  while (b != 0u) {
    uint t = a % b;
    a = b;
    b = t;
  }
  values[i] = a;
}
)GLSL";

char const* HalfSelectShader = R"GLSL(
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
  uint lo = v & 0xffffu;
  uint hi = v >> 16;
  values[i] = ((uParameter & 1u) != 0u) ? hi : lo;
}
)GLSL";

char const* HalfLcmShader = R"GLSL(
#version 430
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Data { uint values[]; };
uniform uint uParameter;
uniform uint uCount;
void main() {
  uint i = gl_GlobalInvocationID.x;
  if (i >= uCount)
    return;
  uint a = values[i] & 0xffffu;
  uint b = values[i] >> 16;
  if (a == 0u || b == 0u) {
    values[i] = 0u;
    return;
  }
  uint x = a;
  uint y = b;
  while (y != 0u) {
    uint t = x % y;
    x = y;
    y = t;
  }
  values[i] = (a / x) * b;
}
)GLSL";

char const* HalfCoprimeShader = R"GLSL(
#version 430
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Data { uint values[]; };
uniform uint uParameter;
uniform uint uCount;
void main() {
  uint i = gl_GlobalInvocationID.x;
  if (i >= uCount)
    return;
  uint a = values[i] & 0xffffu;
  uint b = values[i] >> 16;
  while (b != 0u) {
    uint t = a % b;
    a = b;
    b = t;
  }
  values[i] = (a == 1u) ? 0xffffffffu : 0u;
}
)GLSL";

char const* HalfAlignUpShader = R"GLSL(
#version 430
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Data { uint values[]; };
uniform uint uParameter;
uniform uint uCount;
void main() {
  uint i = gl_GlobalInvocationID.x;
  if (i >= uCount)
    return;
  uint lo = values[i] & 0xffffu;
  uint hi = values[i] >> 16;
  values[i] = (hi == 0u) ? lo : ((lo + hi - 1u) / hi) * hi;
}
)GLSL";

char const* HalfAlignDownShader = R"GLSL(
#version 430
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Data { uint values[]; };
uniform uint uParameter;
uniform uint uCount;
void main() {
  uint i = gl_GlobalInvocationID.x;
  if (i >= uCount)
    return;
  uint lo = values[i] & 0xffffu;
  uint hi = values[i] >> 16;
  values[i] = (hi == 0u) ? lo : (lo / hi) * hi;
}
)GLSL";

char const* HalfRotlShader = R"GLSL(
#version 430
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Data { uint values[]; };
uniform uint uParameter;
uniform uint uCount;
void main() {
  uint i = gl_GlobalInvocationID.x;
  if (i >= uCount)
    return;
  uint lo = values[i] & 0xffffu;
  uint rot = (values[i] >> 16) & 15u;
  values[i] = ((lo << rot) | (lo >> ((16u - rot) & 15u))) & 0xffffu;
}
)GLSL";

char const* HalfRotrShader = R"GLSL(
#version 430
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Data { uint values[]; };
uniform uint uParameter;
uniform uint uCount;
void main() {
  uint i = gl_GlobalInvocationID.x;
  if (i >= uCount)
    return;
  uint lo = values[i] & 0xffffu;
  uint rot = (values[i] >> 16) & 15u;
  values[i] = ((lo >> rot) | (lo << ((16u - rot) & 15u))) & 0xffffu;
}
)GLSL";

char const* HalfPopcountShader = R"GLSL(
#version 430
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Data { uint values[]; };
uniform uint uParameter;
uniform uint uCount;
void main() {
  uint i = gl_GlobalInvocationID.x;
  if (i >= uCount)
    return;
  values[i] = bitCount(values[i] & 0xffffu);
}
)GLSL";

char const* HalfClzShader = R"GLSL(
#version 430
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Data { uint values[]; };
uniform uint uParameter;
uniform uint uCount;
void main() {
  uint i = gl_GlobalInvocationID.x;
  if (i >= uCount)
    return;
  uint lo = values[i] & 0xffffu;
  values[i] = lo == 0u ? 16u : (15u - uint(findMSB(lo)));
}
)GLSL";

char const* HalfCtzShader = R"GLSL(
#version 430
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Data { uint values[]; };
uniform uint uParameter;
uniform uint uCount;
void main() {
  uint i = gl_GlobalInvocationID.x;
  if (i >= uCount)
    return;
  uint lo = values[i] & 0xffffu;
  values[i] = lo == 0u ? 16u : uint(findLSB(lo));
}
)GLSL";

char const* HalfParityShader = R"GLSL(
#version 430
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Data { uint values[]; };
uniform uint uParameter;
uniform uint uCount;
void main() {
  uint i = gl_GlobalInvocationID.x;
  if (i >= uCount)
    return;
  values[i] = bitCount(values[i] & 0xffffu) & 1u;
}
)GLSL";

char const* HalfBitReverseShader = R"GLSL(
#version 430
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Data { uint values[]; };
uniform uint uParameter;
uniform uint uCount;
void main() {
  uint i = gl_GlobalInvocationID.x;
  if (i >= uCount)
    return;
  values[i] = bitfieldReverse(values[i] & 0xffffu) >> 16;
}
)GLSL";

char const* HalfBswapShader = R"GLSL(
#version 430
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Data { uint values[]; };
uniform uint uParameter;
uniform uint uCount;
void main() {
  uint i = gl_GlobalInvocationID.x;
  if (i >= uCount)
    return;
  uint lo = values[i] & 0xffffu;
  values[i] = ((lo & 0xffu) << 8) | ((lo >> 8) & 0xffu);
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
  if (g_swapHalvesProgram) {
    glDeleteProgram(g_swapHalvesProgram);
    g_swapHalvesProgram = 0;
  }
  if (g_cloProgram) {
    glDeleteProgram(g_cloProgram);
    g_cloProgram = 0;
  }
  if (g_ctoProgram) {
    glDeleteProgram(g_ctoProgram);
    g_ctoProgram = 0;
  }
  if (g_reverseNibblesProgram) {
    glDeleteProgram(g_reverseNibblesProgram);
    g_reverseNibblesProgram = 0;
  }
  if (g_byteMaxProgram) {
    glDeleteProgram(g_byteMaxProgram);
    g_byteMaxProgram = 0;
  }
  if (g_byteMinProgram) {
    glDeleteProgram(g_byteMinProgram);
    g_byteMinProgram = 0;
  }
  if (g_dupLow16Program) {
    glDeleteProgram(g_dupLow16Program);
    g_dupLow16Program = 0;
  }
  if (g_dupHigh16Program) {
    glDeleteProgram(g_dupHigh16Program);
    g_dupHigh16Program = 0;
  }
  if (g_reverseBitsInBytesProgram) {
    glDeleteProgram(g_reverseBitsInBytesProgram);
    g_reverseBitsInBytesProgram = 0;
  }
  if (g_isolateLsbProgram) {
    glDeleteProgram(g_isolateLsbProgram);
    g_isolateLsbProgram = 0;
  }
  if (g_isolateMsbProgram) {
    glDeleteProgram(g_isolateMsbProgram);
    g_isolateMsbProgram = 0;
  }
  if (g_lsbMaskProgram) {
    glDeleteProgram(g_lsbMaskProgram);
    g_lsbMaskProgram = 0;
  }
  if (g_clearLsbProgram) {
    glDeleteProgram(g_clearLsbProgram);
    g_clearLsbProgram = 0;
  }
  if (g_grayEncodeProgram) {
    glDeleteProgram(g_grayEncodeProgram);
    g_grayEncodeProgram = 0;
  }
  if (g_grayDecodeProgram) {
    glDeleteProgram(g_grayDecodeProgram);
    g_grayDecodeProgram = 0;
  }
  if (g_smearLsbProgram) {
    glDeleteProgram(g_smearLsbProgram);
    g_smearLsbProgram = 0;
  }
  if (g_clearMsbProgram) {
    glDeleteProgram(g_clearMsbProgram);
    g_clearMsbProgram = 0;
  }
  if (g_byteXorProgram) {
    glDeleteProgram(g_byteXorProgram);
    g_byteXorProgram = 0;
  }
  if (g_smearMsbProgram) {
    glDeleteProgram(g_smearMsbProgram);
    g_smearMsbProgram = 0;
  }
  if (g_byteOrProgram) {
    glDeleteProgram(g_byteOrProgram);
    g_byteOrProgram = 0;
  }
  if (g_byteAndProgram) {
    glDeleteProgram(g_byteAndProgram);
    g_byteAndProgram = 0;
  }
  if (g_keepOddBitsProgram) {
    glDeleteProgram(g_keepOddBitsProgram);
    g_keepOddBitsProgram = 0;
  }
  if (g_keepEvenBitsProgram) {
    glDeleteProgram(g_keepEvenBitsProgram);
    g_keepEvenBitsProgram = 0;
  }
  if (g_swapAdjacentBitsProgram) {
    glDeleteProgram(g_swapAdjacentBitsProgram);
    g_swapAdjacentBitsProgram = 0;
  }
  if (g_compressOddBitsProgram) {
    glDeleteProgram(g_compressOddBitsProgram);
    g_compressOddBitsProgram = 0;
  }
  if (g_compressEvenBitsProgram) {
    glDeleteProgram(g_compressEvenBitsProgram);
    g_compressEvenBitsProgram = 0;
  }
  if (g_expandLow16ToOddProgram) {
    glDeleteProgram(g_expandLow16ToOddProgram);
    g_expandLow16ToOddProgram = 0;
  }
  if (g_expandLow16ToEvenProgram) {
    glDeleteProgram(g_expandLow16ToEvenProgram);
    g_expandLow16ToEvenProgram = 0;
  }
  if (g_interleaveHalvesProgram) {
    glDeleteProgram(g_interleaveHalvesProgram);
    g_interleaveHalvesProgram = 0;
  }
  if (g_deinterleaveHalvesProgram) {
    glDeleteProgram(g_deinterleaveHalvesProgram);
    g_deinterleaveHalvesProgram = 0;
  }
  if (g_nibbleXorProgram) {
    glDeleteProgram(g_nibbleXorProgram);
    g_nibbleXorProgram = 0;
  }
  if (g_nibbleOrProgram) {
    glDeleteProgram(g_nibbleOrProgram);
    g_nibbleOrProgram = 0;
  }
  if (g_nibbleAndProgram) {
    glDeleteProgram(g_nibbleAndProgram);
    g_nibbleAndProgram = 0;
  }
  if (g_nibbleSumProgram) {
    glDeleteProgram(g_nibbleSumProgram);
    g_nibbleSumProgram = 0;
  }
  if (g_nibbleMaxProgram) {
    glDeleteProgram(g_nibbleMaxProgram);
    g_nibbleMaxProgram = 0;
  }
  if (g_nibbleMinProgram) {
    glDeleteProgram(g_nibbleMinProgram);
    g_nibbleMinProgram = 0;
  }
  if (g_extractNibbleProgram) {
    glDeleteProgram(g_extractNibbleProgram);
    g_extractNibbleProgram = 0;
  }
  if (g_insertNibbleProgram) {
    glDeleteProgram(g_insertNibbleProgram);
    g_insertNibbleProgram = 0;
  }
  if (g_broadcastNibbleProgram) {
    glDeleteProgram(g_broadcastNibbleProgram);
    g_broadcastNibbleProgram = 0;
  }
  if (g_swapAdjacentNibblesProgram) {
    glDeleteProgram(g_swapAdjacentNibblesProgram);
    g_swapAdjacentNibblesProgram = 0;
  }
  if (g_rolNibblesProgram) {
    glDeleteProgram(g_rolNibblesProgram);
    g_rolNibblesProgram = 0;
  }
  if (g_rorNibblesProgram) {
    glDeleteProgram(g_rorNibblesProgram);
    g_rorNibblesProgram = 0;
  }
  if (g_hasZeroNibbleProgram) {
    glDeleteProgram(g_hasZeroNibbleProgram);
    g_hasZeroNibbleProgram = 0;
  }
  if (g_sextNibbleProgram) {
    glDeleteProgram(g_sextNibbleProgram);
    g_sextNibbleProgram = 0;
  }
  if (g_countZeroNibblesProgram) {
    glDeleteProgram(g_countZeroNibblesProgram);
    g_countZeroNibblesProgram = 0;
  }
  if (g_packLowNibblesProgram) {
    glDeleteProgram(g_packLowNibblesProgram);
    g_packLowNibblesProgram = 0;
  }
  if (g_packHighNibblesProgram) {
    glDeleteProgram(g_packHighNibblesProgram);
    g_packHighNibblesProgram = 0;
  }
  if (g_expandLowNibblesProgram) {
    glDeleteProgram(g_expandLowNibblesProgram);
    g_expandLowNibblesProgram = 0;
  }
  if (g_expandHighNibblesProgram) {
    glDeleteProgram(g_expandHighNibblesProgram);
    g_expandHighNibblesProgram = 0;
  }
  if (g_replaceNibbleProgram) {
    glDeleteProgram(g_replaceNibbleProgram);
    g_replaceNibbleProgram = 0;
  }
  if (g_maskNibblesProgram) {
    glDeleteProgram(g_maskNibblesProgram);
    g_maskNibblesProgram = 0;
  }
  if (g_allNibblesEqualProgram) {
    glDeleteProgram(g_allNibblesEqualProgram);
    g_allNibblesEqualProgram = 0;
  }
  if (g_nibbleRangeProgram) {
    glDeleteProgram(g_nibbleRangeProgram);
    g_nibbleRangeProgram = 0;
  }
  if (g_broadcastNibbleAtProgram) {
    glDeleteProgram(g_broadcastNibbleAtProgram);
    g_broadcastNibbleAtProgram = 0;
  }
  if (g_halfSumProgram) {
    glDeleteProgram(g_halfSumProgram);
    g_halfSumProgram = 0;
  }
  if (g_halfMaxProgram) {
    glDeleteProgram(g_halfMaxProgram);
    g_halfMaxProgram = 0;
  }
  if (g_halfMinProgram) {
    glDeleteProgram(g_halfMinProgram);
    g_halfMinProgram = 0;
  }
  if (g_halfXorProgram) {
    glDeleteProgram(g_halfXorProgram);
    g_halfXorProgram = 0;
  }
  if (g_halfOrProgram) {
    glDeleteProgram(g_halfOrProgram);
    g_halfOrProgram = 0;
  }
  if (g_halfAndProgram) {
    glDeleteProgram(g_halfAndProgram);
    g_halfAndProgram = 0;
  }
  if (g_broadcastHalfProgram) {
    glDeleteProgram(g_broadcastHalfProgram);
    g_broadcastHalfProgram = 0;
  }
  if (g_halfEqProgram) {
    glDeleteProgram(g_halfEqProgram);
    g_halfEqProgram = 0;
  }
  if (g_halfRangeProgram) {
    glDeleteProgram(g_halfRangeProgram);
    g_halfRangeProgram = 0;
  }
  if (g_halfLtProgram) {
    glDeleteProgram(g_halfLtProgram);
    g_halfLtProgram = 0;
  }
  if (g_halfGtProgram) {
    glDeleteProgram(g_halfGtProgram);
    g_halfGtProgram = 0;
  }
  if (g_halfNeProgram) {
    glDeleteProgram(g_halfNeProgram);
    g_halfNeProgram = 0;
  }
  if (g_halfLeuProgram) {
    glDeleteProgram(g_halfLeuProgram);
    g_halfLeuProgram = 0;
  }
  if (g_halfGeuProgram) {
    glDeleteProgram(g_halfGeuProgram);
    g_halfGeuProgram = 0;
  }
  if (g_halfAvgProgram) {
    glDeleteProgram(g_halfAvgProgram);
    g_halfAvgProgram = 0;
  }
  if (g_broadcastHighHalfProgram) {
    glDeleteProgram(g_broadcastHighHalfProgram);
    g_broadcastHighHalfProgram = 0;
  }
  if (g_halfSubProgram) {
    glDeleteProgram(g_halfSubProgram);
    g_halfSubProgram = 0;
  }
  if (g_halfMulProgram) {
    glDeleteProgram(g_halfMulProgram);
    g_halfMulProgram = 0;
  }
  if (g_halfSatAddProgram) {
    glDeleteProgram(g_halfSatAddProgram);
    g_halfSatAddProgram = 0;
  }
  if (g_halfSatSubProgram) {
    glDeleteProgram(g_halfSatSubProgram);
    g_halfSatSubProgram = 0;
  }
  if (g_halfMulHiProgram) {
    glDeleteProgram(g_halfMulHiProgram);
    g_halfMulHiProgram = 0;
  }
  if (g_halfDivProgram) {
    glDeleteProgram(g_halfDivProgram);
    g_halfDivProgram = 0;
  }
  if (g_halfModProgram) {
    glDeleteProgram(g_halfModProgram);
    g_halfModProgram = 0;
  }
  if (g_halfCeilDivProgram) {
    glDeleteProgram(g_halfCeilDivProgram);
    g_halfCeilDivProgram = 0;
  }
  if (g_halfIsDivisibleProgram) {
    glDeleteProgram(g_halfIsDivisibleProgram);
    g_halfIsDivisibleProgram = 0;
  }
  if (g_halfGcdProgram) {
    glDeleteProgram(g_halfGcdProgram);
    g_halfGcdProgram = 0;
  }
  if (g_halfSelectProgram) {
    glDeleteProgram(g_halfSelectProgram);
    g_halfSelectProgram = 0;
  }
  if (g_halfLcmProgram) {
    glDeleteProgram(g_halfLcmProgram);
    g_halfLcmProgram = 0;
  }
  if (g_halfCoprimeProgram) {
    glDeleteProgram(g_halfCoprimeProgram);
    g_halfCoprimeProgram = 0;
  }
  if (g_halfAlignUpProgram) {
    glDeleteProgram(g_halfAlignUpProgram);
    g_halfAlignUpProgram = 0;
  }
  if (g_halfAlignDownProgram) {
    glDeleteProgram(g_halfAlignDownProgram);
    g_halfAlignDownProgram = 0;
  }
  if (g_halfRotlProgram) {
    glDeleteProgram(g_halfRotlProgram);
    g_halfRotlProgram = 0;
  }
  if (g_halfRotrProgram) {
    glDeleteProgram(g_halfRotrProgram);
    g_halfRotrProgram = 0;
  }
  if (g_halfPopcountProgram) {
    glDeleteProgram(g_halfPopcountProgram);
    g_halfPopcountProgram = 0;
  }
  if (g_halfClzProgram) {
    glDeleteProgram(g_halfClzProgram);
    g_halfClzProgram = 0;
  }
  if (g_halfCtzProgram) {
    glDeleteProgram(g_halfCtzProgram);
    g_halfCtzProgram = 0;
  }
  if (g_halfParityProgram) {
    glDeleteProgram(g_halfParityProgram);
    g_halfParityProgram = 0;
  }
  if (g_halfBitReverseProgram) {
    glDeleteProgram(g_halfBitReverseProgram);
    g_halfBitReverseProgram = 0;
  }
  if (g_halfBswapProgram) {
    glDeleteProgram(g_halfBswapProgram);
    g_halfBswapProgram = 0;
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
      || !requireProgram(g_satMulProgram, SatMulShader, "DenseUint32SatMul")
      || !requireProgram(g_swapHalvesProgram, SwapHalvesShader, "DenseUint32SwapHalves")
      || !requireProgram(g_cloProgram, CloShader, "DenseUint32Clo")
      || !requireProgram(g_ctoProgram, CtoShader, "DenseUint32Cto")
      || !requireProgram(g_reverseNibblesProgram, ReverseNibblesShader, "DenseUint32ReverseNibbles")
      || !requireProgram(g_byteMaxProgram, ByteMaxShader, "DenseUint32ByteMax")
      || !requireProgram(g_byteMinProgram, ByteMinShader, "DenseUint32ByteMin")
      || !requireProgram(g_dupLow16Program, DupLow16Shader, "DenseUint32DupLow16")
      || !requireProgram(g_dupHigh16Program, DupHigh16Shader, "DenseUint32DupHigh16")
      || !requireProgram(g_reverseBitsInBytesProgram, ReverseBitsInBytesShader,
            "DenseUint32ReverseBitsInBytes")
      || !requireProgram(g_isolateLsbProgram, IsolateLsbShader, "DenseUint32IsolateLsb")
      || !requireProgram(g_isolateMsbProgram, IsolateMsbShader, "DenseUint32IsolateMsb")
      || !requireProgram(g_lsbMaskProgram, LsbMaskShader, "DenseUint32LsbMask")
      || !requireProgram(g_clearLsbProgram, ClearLsbShader, "DenseUint32ClearLsb")
      || !requireProgram(g_grayEncodeProgram, GrayEncodeShader, "DenseUint32GrayEncode")
      || !requireProgram(g_grayDecodeProgram, GrayDecodeShader, "DenseUint32GrayDecode")
      || !requireProgram(g_smearLsbProgram, SmearLsbShader, "DenseUint32SmearLsb")
      || !requireProgram(g_clearMsbProgram, ClearMsbShader, "DenseUint32ClearMsb")
      || !requireProgram(g_byteXorProgram, ByteXorShader, "DenseUint32ByteXor")
      || !requireProgram(g_smearMsbProgram, SmearMsbShader, "DenseUint32SmearMsb")
      || !requireProgram(g_byteOrProgram, ByteOrShader, "DenseUint32ByteOr")
      || !requireProgram(g_byteAndProgram, ByteAndShader, "DenseUint32ByteAnd")
      || !requireProgram(g_keepOddBitsProgram, KeepOddBitsShader, "DenseUint32KeepOddBits")
      || !requireProgram(g_keepEvenBitsProgram, KeepEvenBitsShader, "DenseUint32KeepEvenBits")
      || !requireProgram(g_swapAdjacentBitsProgram, SwapAdjacentBitsShader,
            "DenseUint32SwapAdjacentBits")
      || !requireProgram(g_compressOddBitsProgram, CompressOddBitsShader,
            "DenseUint32CompressOddBits")
      || !requireProgram(g_compressEvenBitsProgram, CompressEvenBitsShader,
            "DenseUint32CompressEvenBits")
      || !requireProgram(g_expandLow16ToOddProgram, ExpandLow16ToOddShader,
            "DenseUint32ExpandLow16ToOdd")
      || !requireProgram(g_expandLow16ToEvenProgram, ExpandLow16ToEvenShader,
            "DenseUint32ExpandLow16ToEven")
      || !requireProgram(g_interleaveHalvesProgram, InterleaveHalvesShader,
            "DenseUint32InterleaveHalves")
      || !requireProgram(g_deinterleaveHalvesProgram, DeinterleaveHalvesShader,
            "DenseUint32DeinterleaveHalves")
      || !requireProgram(g_nibbleXorProgram, NibbleXorShader, "DenseUint32NibbleXor")
      || !requireProgram(g_nibbleOrProgram, NibbleOrShader, "DenseUint32NibbleOr")
      || !requireProgram(g_nibbleAndProgram, NibbleAndShader, "DenseUint32NibbleAnd")
      || !requireProgram(g_nibbleSumProgram, NibbleSumShader, "DenseUint32NibbleSum")
      || !requireProgram(g_nibbleMaxProgram, NibbleMaxShader, "DenseUint32NibbleMax")
      || !requireProgram(g_nibbleMinProgram, NibbleMinShader, "DenseUint32NibbleMin")
      || !requireProgram(g_extractNibbleProgram, ExtractNibbleShader, "DenseUint32ExtractNibble")
      || !requireProgram(g_insertNibbleProgram, InsertNibbleShader, "DenseUint32InsertNibble")
      || !requireProgram(g_broadcastNibbleProgram, BroadcastNibbleShader,
            "DenseUint32BroadcastNibble")
      || !requireProgram(g_swapAdjacentNibblesProgram, SwapAdjacentNibblesShader,
            "DenseUint32SwapAdjacentNibbles")
      || !requireProgram(g_rolNibblesProgram, RolNibblesShader, "DenseUint32RolNibbles")
      || !requireProgram(g_rorNibblesProgram, RorNibblesShader, "DenseUint32RorNibbles")
      || !requireProgram(g_hasZeroNibbleProgram, HasZeroNibbleShader, "DenseUint32HasZeroNibble")
      || !requireProgram(g_sextNibbleProgram, SextNibbleShader, "DenseUint32SextNibble")
      || !requireProgram(g_countZeroNibblesProgram, CountZeroNibblesShader,
            "DenseUint32CountZeroNibbles")
      || !requireProgram(g_packLowNibblesProgram, PackLowNibblesShader, "DenseUint32PackLowNibbles")
      || !requireProgram(g_packHighNibblesProgram, PackHighNibblesShader,
            "DenseUint32PackHighNibbles")
      || !requireProgram(g_expandLowNibblesProgram, ExpandLowNibblesShader,
            "DenseUint32ExpandLowNibbles")
      || !requireProgram(g_expandHighNibblesProgram, ExpandHighNibblesShader,
            "DenseUint32ExpandHighNibbles")
      || !requireProgram(g_replaceNibbleProgram, ReplaceNibbleShader, "DenseUint32ReplaceNibble")
      || !requireProgram(g_maskNibblesProgram, MaskNibblesShader, "DenseUint32MaskNibbles")
      || !requireProgram(g_allNibblesEqualProgram, AllNibblesEqualShader,
            "DenseUint32AllNibblesEqual")
      || !requireProgram(g_nibbleRangeProgram, NibbleRangeShader, "DenseUint32NibbleRange")
      || !requireProgram(g_broadcastNibbleAtProgram, BroadcastNibbleAtShader,
            "DenseUint32BroadcastNibbleAt")
      || !requireProgram(g_halfSumProgram, HalfSumShader, "DenseUint32HalfSum")
      || !requireProgram(g_halfMaxProgram, HalfMaxShader, "DenseUint32HalfMax")
      || !requireProgram(g_halfMinProgram, HalfMinShader, "DenseUint32HalfMin")
      || !requireProgram(g_halfXorProgram, HalfXorShader, "DenseUint32HalfXor")
      || !requireProgram(g_halfOrProgram, HalfOrShader, "DenseUint32HalfOr")
      || !requireProgram(g_halfAndProgram, HalfAndShader, "DenseUint32HalfAnd")
      || !requireProgram(g_broadcastHalfProgram, BroadcastHalfShader, "DenseUint32BroadcastHalf")
      || !requireProgram(g_halfEqProgram, HalfEqShader, "DenseUint32HalfEq")
      || !requireProgram(g_halfRangeProgram, HalfRangeShader, "DenseUint32HalfRange")
      || !requireProgram(g_halfLtProgram, HalfLtShader, "DenseUint32HalfLt")
      || !requireProgram(g_halfGtProgram, HalfGtShader, "DenseUint32HalfGt")
      || !requireProgram(g_halfNeProgram, HalfNeShader, "DenseUint32HalfNe")
      || !requireProgram(g_halfLeuProgram, HalfLeuShader, "DenseUint32HalfLeu")
      || !requireProgram(g_halfGeuProgram, HalfGeuShader, "DenseUint32HalfGeu")
      || !requireProgram(g_halfAvgProgram, HalfAvgShader, "DenseUint32HalfAvg")
      || !requireProgram(g_broadcastHighHalfProgram, BroadcastHighHalfShader, "DenseUint32BroadcastHighHalf")
      || !requireProgram(g_halfSubProgram, HalfSubShader, "DenseUint32HalfSub")
      || !requireProgram(g_halfMulProgram, HalfMulShader, "DenseUint32HalfMul")
      || !requireProgram(g_halfSatAddProgram, HalfSatAddShader, "DenseUint32HalfSatAdd")
      || !requireProgram(g_halfSatSubProgram, HalfSatSubShader, "DenseUint32HalfSatSub")
      || !requireProgram(g_halfMulHiProgram, HalfMulHiShader, "DenseUint32HalfMulHi")
      || !requireProgram(g_halfDivProgram, HalfDivShader, "DenseUint32HalfDiv")
      || !requireProgram(g_halfModProgram, HalfModShader, "DenseUint32HalfMod")
      || !requireProgram(g_halfCeilDivProgram, HalfCeilDivShader, "DenseUint32HalfCeilDiv")
      || !requireProgram(g_halfIsDivisibleProgram, HalfIsDivisibleShader, "DenseUint32HalfIsDivisible")
      || !requireProgram(g_halfGcdProgram, HalfGcdShader, "DenseUint32HalfGcd")
      || !requireProgram(g_halfSelectProgram, HalfSelectShader, "DenseUint32HalfSelect")
      || !requireProgram(g_halfLcmProgram, HalfLcmShader, "DenseUint32HalfLcm")
      || !requireProgram(g_halfCoprimeProgram, HalfCoprimeShader, "DenseUint32HalfCoprime")
      || !requireProgram(g_halfAlignUpProgram, HalfAlignUpShader, "DenseUint32HalfAlignUp")
      || !requireProgram(g_halfAlignDownProgram, HalfAlignDownShader, "DenseUint32HalfAlignDown")
      || !requireProgram(g_halfRotlProgram, HalfRotlShader, "DenseUint32HalfRotl")
      || !requireProgram(g_halfRotrProgram, HalfRotrShader, "DenseUint32HalfRotr")
      || !requireProgram(g_halfPopcountProgram, HalfPopcountShader, "DenseUint32HalfPopcount")
      || !requireProgram(g_halfClzProgram, HalfClzShader, "DenseUint32HalfClz")
      || !requireProgram(g_halfCtzProgram, HalfCtzShader, "DenseUint32HalfCtz")
      || !requireProgram(g_halfParityProgram, HalfParityShader, "DenseUint32HalfParity")
      || !requireProgram(g_halfBitReverseProgram, HalfBitReverseShader, "DenseUint32HalfBitReverse")
      || !requireProgram(g_halfBswapProgram, HalfBswapShader, "DenseUint32HalfBswap"))
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
  else if (kernel == WorldGenerationComputeKernel::DenseUint32SwapHalves)
    program = g_swapHalvesProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32Clo)
    program = g_cloProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32Cto)
    program = g_ctoProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32ReverseNibbles)
    program = g_reverseNibblesProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32ByteMax)
    program = g_byteMaxProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32ByteMin)
    program = g_byteMinProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32DupLow16)
    program = g_dupLow16Program;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32DupHigh16)
    program = g_dupHigh16Program;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32ReverseBitsInBytes)
    program = g_reverseBitsInBytesProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32IsolateLsb)
    program = g_isolateLsbProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32IsolateMsb)
    program = g_isolateMsbProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32LsbMask)
    program = g_lsbMaskProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32ClearLsb)
    program = g_clearLsbProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32GrayEncode)
    program = g_grayEncodeProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32GrayDecode)
    program = g_grayDecodeProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32SmearLsb)
    program = g_smearLsbProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32ClearMsb)
    program = g_clearMsbProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32ByteXor)
    program = g_byteXorProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32SmearMsb)
    program = g_smearMsbProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32ByteOr)
    program = g_byteOrProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32ByteAnd)
    program = g_byteAndProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32KeepOddBits)
    program = g_keepOddBitsProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32KeepEvenBits)
    program = g_keepEvenBitsProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32SwapAdjacentBits)
    program = g_swapAdjacentBitsProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32CompressOddBits)
    program = g_compressOddBitsProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32CompressEvenBits)
    program = g_compressEvenBitsProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32ExpandLow16ToOdd)
    program = g_expandLow16ToOddProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32ExpandLow16ToEven)
    program = g_expandLow16ToEvenProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32InterleaveHalves)
    program = g_interleaveHalvesProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32DeinterleaveHalves)
    program = g_deinterleaveHalvesProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32NibbleXor)
    program = g_nibbleXorProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32NibbleOr)
    program = g_nibbleOrProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32NibbleAnd)
    program = g_nibbleAndProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32NibbleSum)
    program = g_nibbleSumProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32NibbleMax)
    program = g_nibbleMaxProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32NibbleMin)
    program = g_nibbleMinProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32ExtractNibble)
    program = g_extractNibbleProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32InsertNibble)
    program = g_insertNibbleProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32BroadcastNibble)
    program = g_broadcastNibbleProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32SwapAdjacentNibbles)
    program = g_swapAdjacentNibblesProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32RolNibbles)
    program = g_rolNibblesProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32RorNibbles)
    program = g_rorNibblesProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32HasZeroNibble)
    program = g_hasZeroNibbleProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32SextNibble)
    program = g_sextNibbleProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32CountZeroNibbles)
    program = g_countZeroNibblesProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32PackLowNibbles)
    program = g_packLowNibblesProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32PackHighNibbles)
    program = g_packHighNibblesProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32ExpandLowNibbles)
    program = g_expandLowNibblesProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32ExpandHighNibbles)
    program = g_expandHighNibblesProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32ReplaceNibble)
    program = g_replaceNibbleProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32MaskNibbles)
    program = g_maskNibblesProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32AllNibblesEqual)
    program = g_allNibblesEqualProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32NibbleRange)
    program = g_nibbleRangeProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32BroadcastNibbleAt)
    program = g_broadcastNibbleAtProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfSum)
    program = g_halfSumProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfMax)
    program = g_halfMaxProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfMin)
    program = g_halfMinProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfXor)
    program = g_halfXorProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfOr)
    program = g_halfOrProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfAnd)
    program = g_halfAndProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32BroadcastHalf)
    program = g_broadcastHalfProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfEq)
    program = g_halfEqProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfRange)
    program = g_halfRangeProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfLt)
    program = g_halfLtProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfGt)
    program = g_halfGtProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfNe)
    program = g_halfNeProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfLeu)
    program = g_halfLeuProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfGeu)
    program = g_halfGeuProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfAvg)
    program = g_halfAvgProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32BroadcastHighHalf)
    program = g_broadcastHighHalfProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfSub)
    program = g_halfSubProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfMul)
    program = g_halfMulProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfSatAdd)
    program = g_halfSatAddProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfSatSub)
    program = g_halfSatSubProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfMulHi)
    program = g_halfMulHiProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfDiv)
    program = g_halfDivProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfMod)
    program = g_halfModProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfCeilDiv)
    program = g_halfCeilDivProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfIsDivisible)
    program = g_halfIsDivisibleProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfGcd)
    program = g_halfGcdProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfSelect)
    program = g_halfSelectProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfLcm)
    program = g_halfLcmProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfCoprime)
    program = g_halfCoprimeProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfAlignUp)
    program = g_halfAlignUpProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfAlignDown)
    program = g_halfAlignDownProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfRotl)
    program = g_halfRotlProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfRotr)
    program = g_halfRotrProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfPopcount)
    program = g_halfPopcountProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfClz)
    program = g_halfClzProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfCtz)
    program = g_halfCtzProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfParity)
    program = g_halfParityProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfBitReverse)
    program = g_halfBitReverseProgram;
  else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfBswap)
    program = g_halfBswapProgram;
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
