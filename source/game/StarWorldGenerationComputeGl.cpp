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
      || !requireProgram(g_absDiffProgram, AbsDiffShader, "DenseUint32AbsDiff"))
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
