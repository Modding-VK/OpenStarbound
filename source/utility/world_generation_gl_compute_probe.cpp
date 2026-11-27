#include "StarWorldGenerationCompute.hpp"
#include "StarWorldGenerationComputeGl.hpp"
#include "StarFormat.hpp"

#include <cstdio>
#include <cstdlib>

using namespace Star;

namespace {

List<uint32_t> edgeCorpus() {
  return WorldGenerationComputeService::shadowEdgeCorpus();
}

WorldGenerationComputeInputIdentity makeIdentity(uint64_t sequence,
    WorldGenerationComputeKernel kernel) {
  WorldGenerationComputeInputIdentity id;
  id.worldEpoch = 42;
  id.sequence = sequence;
  id.kernelVersion = 1;
  id.kernel = kernel;
  return id;
}

int runShadow(WorldGenerationComputeService& service, WorldGenerationComputeKernel kernel,
    uint32_t parameter, char const* name) {
  auto report = service.compareShadow(makeIdentity((uint64_t)kernel, kernel), edgeCorpus(),
      parameter, WorldGenerationComputeBackend::CpuReference,
      WorldGenerationComputeBackend::GpuOpenGlCompute);
  std::printf("shadow %s: verdict=%u compared=%zu divergences=%zu\n", name,
      (unsigned)report.verdict, report.comparedValues, report.divergenceCount);
  if (report.verdict == WorldGenerationComputeShadowVerdict::Equal)
    return 0;
  if (report.verdict == WorldGenerationComputeShadowVerdict::ShadowUnavailable) {
    std::printf("  shadow unavailable (probe/dispatch failed)\n");
    return 2;
  }
  std::printf("  firstDivergence=%zu primary=%u shadow=%u\n", report.firstDivergenceIndex,
      report.primarySample, report.shadowSample);
  return 3;
}

}

int main() {
  std::printf("world_generation_gl_compute_probe\n");
  std::printf("build_enabled=%d\n", WorldGenerationComputeGlDevice::isBuildEnabled() ? 1 : 0);

  auto caps = WorldGenerationComputeService::probeCapabilities();
  auto const& info = WorldGenerationComputeGlDevice::lastProbeInfo();
  std::printf("probe: cpu=%d gpuOpenGlCompute=%d context=%d.%d created=%d\n",
      caps.cpuReference ? 1 : 0, caps.gpuOpenGlCompute ? 1 : 0, info.contextMajor,
      info.contextMinor, info.contextCreated ? 1 : 0);
  std::printf("gl: version='%s' vendor='%s' renderer='%s' glsl='%s'\n", info.glVersion,
      info.glVendor, info.glRenderer, info.glslVersion);
  if (info.failureReason && info.failureReason[0])
    std::printf("failure: %s\n", info.failureReason);

  if (!WorldGenerationComputeGlDevice::isBuildEnabled()) {
    std::printf("Rebuild with -DSTAR_ENABLE_WORLDGEN_GL_COMPUTE=ON (see Windows/RTX checklist).\n");
    return 1;
  }
  if (!caps.gpuOpenGlCompute) {
    std::printf("Probe did not claim OpenGL compute.\n");
    WorldGenerationComputeGlDevice::shutdown();
    return 2;
  }

  WorldGenerationComputeService service(caps);
  int rc = 0;
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32Xor, 0x11u, "DenseUint32Xor");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32Add, 3u, "DenseUint32Add");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32And, 0x0f0f0f0fu,
      "DenseUint32And");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32Or, 0x0f0f0f0fu,
      "DenseUint32Or");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32Mul, 3u, "DenseUint32Mul");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32Min, 0x80000000u,
      "DenseUint32Min");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32Max, 0x7fffffffu,
      "DenseUint32Max");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32Sub, 1u, "DenseUint32Sub");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32Shl, 3u, "DenseUint32Shl");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32Shr, 3u, "DenseUint32Shr");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32Rotl, 5u, "DenseUint32Rotl");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32Rotr, 5u, "DenseUint32Rotr");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32Not, 0u, "DenseUint32Not");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32Clz, 0u, "DenseUint32Clz");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32Ctz, 0u, "DenseUint32Ctz");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32Popcount, 0u,
      "DenseUint32Popcount");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32Eq, 0u, "DenseUint32Eq");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32Ne, 1u, "DenseUint32Ne");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32BitReverse, 0u,
      "DenseUint32BitReverse");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32Lt, 0x80000000u,
      "DenseUint32Lt");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32Leu, 0x80000000u,
      "DenseUint32Leu");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32AbsDiff, 1u,
      "DenseUint32AbsDiff");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32Gt, 0x7fffffffu,
      "DenseUint32Gt");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32Geu, 0x80000000u,
      "DenseUint32Geu");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32Xnor, 0x11u,
      "DenseUint32Xnor");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32Nand, 0x0f0f0f0fu,
      "DenseUint32Nand");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32Nor, 0x0f0f0f0fu,
      "DenseUint32Nor");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32MulHi, 0x10001u,
      "DenseUint32MulHi");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32AndNot, 0x0f0f0f0fu,
      "DenseUint32AndNot");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32SatAdd, 0xffffffffu,
      "DenseUint32SatAdd");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32SatSub, 1u,
      "DenseUint32SatSub");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32OrNot, 0x0f0f0f0fu,
      "DenseUint32OrNot");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32Avg, 3u, "DenseUint32Avg");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32IsPow2, 0u,
      "DenseUint32IsPow2");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32Bswap, 0u,
      "DenseUint32Bswap");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32Parity, 0u,
      "DenseUint32Parity");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32BitTest, 1u,
      "DenseUint32BitTest");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HasZeroByte, 0u,
      "DenseUint32HasZeroByte");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32NextPow2, 0u,
      "DenseUint32NextPow2");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32MaskLow, 8u,
      "DenseUint32MaskLow");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32Ilog2, 0u,
      "DenseUint32Ilog2");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32BroadcastByte, 0u,
      "DenseUint32BroadcastByte");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32ByteSum, 0u,
      "DenseUint32ByteSum");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32Sext8, 0u,
      "DenseUint32Sext8");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32Sext16, 0u,
      "DenseUint32Sext16");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32ExtractByte, 1u,
      "DenseUint32ExtractByte");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32BitSet, 1u,
      "DenseUint32BitSet");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32BitClear, 1u,
      "DenseUint32BitClear");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32BitToggle, 1u,
      "DenseUint32BitToggle");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32Neg, 0u,
      "DenseUint32Neg");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32Abs, 0u,
      "DenseUint32Abs");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32RolBytes, 1u,
      "DenseUint32RolBytes");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32RorBytes, 1u,
      "DenseUint32RorBytes");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32SignMask, 0u,
      "DenseUint32SignMask");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32IsZero, 0u,
      "DenseUint32IsZero");

  auto ticket = service.submit(makeIdentity(99, WorldGenerationComputeKernel::DenseUint32Xor),
      edgeCorpus(), 0x55aa55aau, WorldGenerationComputeBackend::GpuOpenGlCompute);
  if (!ticket) {
    std::printf("gpu submit refused after successful probe\n");
    rc |= 4;
  } else {
    WorldGenerationComputeResult result;
    auto status = service.poll(*ticket, &result);
    std::printf("gpu submit/poll: status=%u backend=%u values=%zu\n", (unsigned)status,
        (unsigned)result.backend, result.values.size());
    if (status != WorldGenerationComputePollStatus::Ready
        || result.backend != WorldGenerationComputeBackend::GpuOpenGlCompute)
      rc |= 4;
  }

  WorldGenerationComputeGlDevice::shutdown();
  if (rc == 0)
    std::printf("PASS: probe + DenseUint32* CPU↔GPU shadow equal\n");
  else
    std::printf("FAIL: rc=%d\n", rc);
  return rc == 0 ? 0 : 1;
}
