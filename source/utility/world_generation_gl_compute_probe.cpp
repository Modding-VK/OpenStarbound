#include "StarWorldGenerationCompute.hpp"
#include "StarWorldGenerationComputeGl.hpp"
#include "StarFormat.hpp"

#include <cstdio>
#include <cstdlib>

using namespace Star;

namespace {

List<uint32_t> edgeCorpus() {
  return List<uint32_t>{0u, 1u, 0xffffffffu, 0x80000000u, 0x7fffffffu, 0xfffffffeu,
      0x55aa55aau, 0xa5a5a5a5u};
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
