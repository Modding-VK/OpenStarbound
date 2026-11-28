#include "StarWorldGenerationCompute.hpp"
#include "StarWorldGenerationComputeGl.hpp"
#include "StarFormat.hpp"

#include <chrono>
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

List<uint32_t> timingCorpus(size_t repeats) {
  auto base = edgeCorpus();
  List<uint32_t> out;
  out.reserve(base.size() * repeats);
  for (size_t r = 0; r < repeats; ++r) {
    for (uint32_t v : base)
      out.append(v);
  }
  return out;
}

bool valuesEqual(List<uint32_t> const& a, List<uint32_t> const& b) {
  if (a.size() != b.size())
    return false;
  for (size_t i = 0; i < a.size(); ++i) {
    if (a[i] != b[i])
      return false;
  }
  return true;
}

int runTiming(WorldGenerationComputeService& service, WorldGenerationComputeKernel kernel,
    uint32_t parameter, char const* name, size_t repeats, size_t iters) {
  auto input = timingCorpus(repeats);
  // Warmup (excluded from totals): one CPU + one GPU pass.
  auto warmCpu = WorldGenerationComputeService::runKernel(kernel, 1u, input, parameter);
  auto warmTicket = service.submit(makeIdentity(8000ull + (uint64_t)kernel, kernel), input, parameter,
      WorldGenerationComputeBackend::GpuOpenGlCompute);
  if (!warmTicket) {
    std::printf("timing %s: gpu submit refused\n", name);
    return 4;
  }
  WorldGenerationComputeResult warmResult;
  if (service.poll(*warmTicket, &warmResult) != WorldGenerationComputePollStatus::Ready
      || !valuesEqual(warmResult.values, warmCpu)) {
    std::printf("timing %s: warmup mismatch\n", name);
    return 5;
  }

  long long cpuUs = 0;
  long long gpuUs = 0;
  for (size_t i = 0; i < iters; ++i) {
    auto t0 = std::chrono::steady_clock::now();
    auto cpu = WorldGenerationComputeService::runKernel(kernel, 1u, input, parameter);
    auto t1 = std::chrono::steady_clock::now();
    auto ticket = service.submit(
        makeIdentity(9000ull + (uint64_t)kernel * 100ull + (uint64_t)i, kernel), input, parameter,
        WorldGenerationComputeBackend::GpuOpenGlCompute);
    if (!ticket) {
      std::printf("timing %s: gpu submit refused at iter %zu\n", name, i);
      return 4;
    }
    WorldGenerationComputeResult result;
    auto status = service.poll(*ticket, &result);
    auto t2 = std::chrono::steady_clock::now();
    if (status != WorldGenerationComputePollStatus::Ready
        || result.backend != WorldGenerationComputeBackend::GpuOpenGlCompute
        || !valuesEqual(result.values, cpu)) {
      std::printf("timing %s: iter %zu mismatch status=%u\n", name, i, (unsigned)status);
      return 5;
    }
    cpuUs += std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
    gpuUs += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
  }
  double ratio = gpuUs > 0 ? (double)cpuUs / (double)gpuUs : 0.0;
  int gpuFaster = (gpuUs > 0 && cpuUs > gpuUs) ? 1 : 0;
  std::printf(
      "timing %s: n=%zu iters=%zu cpu_us=%lld gpu_submit_poll_us=%lld ratio_cpu_over_gpu=%.3f "
      "gpu_faster=%d equal=1\n",
      name, input.size(), iters, cpuUs, gpuUs, ratio, gpuFaster);
  // Equality is the contract here; gpu_faster is evidence for the 50% gain gate, not a hard FAIL.
  return 0;
}

int runDriverEvidence(WorldGenerationComputeService& service) {
  // Multi-driver evidence line: stable fingerprints Desktop workers can compare across vendors.
  auto const& info = WorldGenerationComputeGlDevice::lastProbeInfo();
  auto caps = service.capabilities();
  auto capsFp = WorldGenerationComputeService::fingerprintCapabilities(caps);
  auto input = edgeCorpus();
  auto terrain = service.compareShadow(
      makeIdentity(9200, WorldGenerationComputeKernel::TerrainFieldMax2), input, 0u,
      WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::GpuOpenGlCompute);
  auto biome = service.compareShadow(
      makeIdentity(9201, WorldGenerationComputeKernel::BiomeIdBucket), input, 0x10u,
      WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::GpuOpenGlCompute);
  List<WorldGenerationComputeShadowReport> reports;
  reports.append(terrain);
  reports.append(biome);
  auto summary = WorldGenerationComputeService::summarizeShadowReports(reports);
  auto shadowFp = WorldGenerationComputeService::fingerprintShadowSummary(summary);
  std::printf(
      "driver_evidence vendor='%s' renderer='%s' version='%s' caps_fp=%llu shadow_fp=%llu "
      "terrain_verdict=%u biome_verdict=%u\n",
      info.glVendor, info.glRenderer, info.glVersion, (unsigned long long)capsFp,
      (unsigned long long)shadowFp, (unsigned)terrain.verdict, (unsigned)biome.verdict);
  if (terrain.verdict != WorldGenerationComputeShadowVerdict::Equal
      || biome.verdict != WorldGenerationComputeShadowVerdict::Equal)
    return 7;
  return 0;
}

List<uint32_t> runCpuTerrainPipeline(List<uint32_t> input, uint32_t passes) {
  // Same in-place terrain stack the GPU pipeline runs: Max2 → ClampU16 → BiomeIdBucket.
  for (uint32_t pass = 0; pass < passes; ++pass) {
    input = WorldGenerationComputeService::runKernel(
        WorldGenerationComputeKernel::TerrainFieldMax2, 1u, input, 0u);
    input = WorldGenerationComputeService::runKernel(
        WorldGenerationComputeKernel::TerrainFieldClampU16, 1u, input, 0xffffu);
    input = WorldGenerationComputeService::runKernel(
        WorldGenerationComputeKernel::BiomeIdBucket, 1u, input, 0x10u);
  }
  return input;
}

int runGainPipeline(size_t repeats, size_t iters, uint32_t passes) {
  // Gain harness: one upload + multi-kernel terrain stack × passes + one readback.
  // Sync per-kernel submit/poll loses to CPU on small ALU; pipeline amortizes transfer.
  std::printf("--- m50 gain pipeline ---\n");
  auto input = timingCorpus(repeats);
  List<WorldGenerationComputeKernel> kernels;
  kernels.append(WorldGenerationComputeKernel::TerrainFieldMax2);
  kernels.append(WorldGenerationComputeKernel::TerrainFieldClampU16);
  kernels.append(WorldGenerationComputeKernel::BiomeIdBucket);
  List<uint32_t> parameters;
  parameters.append(0u);
  parameters.append(0xffffu);
  parameters.append(0x10u);

  // Warmup
  auto warmCpu = runCpuTerrainPipeline(input, passes);
  List<uint32_t> warmGpu;
  if (!WorldGenerationComputeGlDevice::dispatchPipeline(
          kernels, parameters, 1u, input, warmGpu, passes)
      || !valuesEqual(warmCpu, warmGpu)) {
    std::printf("gain_pipeline: warmup mismatch or dispatch failed\n");
    return 8;
  }

  long long cpuUs = 0;
  long long gpuUs = 0;
  for (size_t i = 0; i < iters; ++i) {
    auto t0 = std::chrono::steady_clock::now();
    auto cpu = runCpuTerrainPipeline(input, passes);
    auto t1 = std::chrono::steady_clock::now();
    List<uint32_t> gpu;
    if (!WorldGenerationComputeGlDevice::dispatchPipeline(
            kernels, parameters, 1u, input, gpu, passes)
        || !valuesEqual(cpu, gpu)) {
      std::printf("gain_pipeline: iter %zu mismatch\n", i);
      return 8;
    }
    auto t2 = std::chrono::steady_clock::now();
    cpuUs += std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
    gpuUs += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
  }
  double ratio = gpuUs > 0 ? (double)cpuUs / (double)gpuUs : 0.0;
  int gpuFaster = (gpuUs > 0 && cpuUs > gpuUs) ? 1 : 0;
  std::printf(
      "gain_pipeline: n=%zu iters=%zu passes=%u kernels=3 cpu_us=%lld gpu_pipeline_us=%lld "
      "ratio_cpu_over_gpu=%.3f gpu_faster=%d equal=1\n",
      input.size(), iters, passes, cpuUs, gpuUs, ratio, gpuFaster);
  return 0;
}

int runDeferredGpuPump(WorldGenerationComputeService& service) {
  // Real-GL deferred path: fence arms on submit; pump advances non-blocking then dispatches.
  service.setDeferredCpuCompletion(true);
  WorldGenerationComputeGlDevice::adoptOwnerThread();
  service.adoptOwnerThread();

  auto ticket = service.submit(makeIdentity(9100, WorldGenerationComputeKernel::TerrainFieldMax2),
      List<uint32_t>{0x00200010u, 0x00010002u}, 0u,
      WorldGenerationComputeBackend::GpuOpenGlCompute);
  if (!ticket) {
    std::printf("deferred TerrainFieldMax2: submit refused\n");
    service.clearOwnerThread();
    WorldGenerationComputeGlDevice::clearOwnerThread();
    return 6;
  }

  WorldGenerationComputeResult early;
  auto earlyStatus = service.poll(*ticket, &early);
  std::printf("deferred TerrainFieldMax2: early_status=%u pending_fences=%zu\n",
      (unsigned)earlyStatus, WorldGenerationComputeGlDevice::pendingFenceCount());
  if (earlyStatus != WorldGenerationComputePollStatus::Pending) {
    service.clearOwnerThread();
    WorldGenerationComputeGlDevice::clearOwnerThread();
    return 6;
  }

  size_t pumped = 0;
  for (int i = 0; i < 64 && earlyStatus == WorldGenerationComputePollStatus::Pending; ++i) {
    pumped += service.pump();
    earlyStatus = service.poll(*ticket, &early);
  }
  std::printf("deferred TerrainFieldMax2: pumped=%zu final_status=%u values=%zu\n", pumped,
      (unsigned)earlyStatus, early.values.size());
  int rc = 0;
  if (earlyStatus != WorldGenerationComputePollStatus::Ready || early.values.size() != 2
      || early.values[0] != 0x0020u || early.values[1] != 0x0002u
      || early.backend != WorldGenerationComputeBackend::GpuOpenGlCompute)
    rc = 6;

  service.clearOwnerThread();
  WorldGenerationComputeGlDevice::clearOwnerThread();
  service.setDeferredCpuCompletion(false);
  return rc;
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
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32IsNonzero, 0u,
      "DenseUint32IsNonzero");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32MaskHigh, 8u,
      "DenseUint32MaskHigh");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32SatMul, 0x10000u,
      "DenseUint32SatMul");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32SwapHalves, 0u,
      "DenseUint32SwapHalves");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32Clo, 0u,
      "DenseUint32Clo");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32Cto, 0u,
      "DenseUint32Cto");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32ReverseNibbles, 0u,
      "DenseUint32ReverseNibbles");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32ByteMax, 0u,
      "DenseUint32ByteMax");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32ByteMin, 0u,
      "DenseUint32ByteMin");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32DupLow16, 0u,
      "DenseUint32DupLow16");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32DupHigh16, 0u,
      "DenseUint32DupHigh16");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32ReverseBitsInBytes, 0u,
      "DenseUint32ReverseBitsInBytes");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32IsolateLsb, 0u,
      "DenseUint32IsolateLsb");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32IsolateMsb, 0u,
      "DenseUint32IsolateMsb");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32LsbMask, 0u,
      "DenseUint32LsbMask");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32ClearLsb, 0u,
      "DenseUint32ClearLsb");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32GrayEncode, 0u,
      "DenseUint32GrayEncode");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32GrayDecode, 0u,
      "DenseUint32GrayDecode");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32SmearLsb, 0u,
      "DenseUint32SmearLsb");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32ClearMsb, 0u,
      "DenseUint32ClearMsb");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32ByteXor, 0u,
      "DenseUint32ByteXor");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32SmearMsb, 0u,
      "DenseUint32SmearMsb");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32ByteOr, 0u,
      "DenseUint32ByteOr");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32ByteAnd, 0u,
      "DenseUint32ByteAnd");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32KeepOddBits, 0u,
      "DenseUint32KeepOddBits");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32KeepEvenBits, 0u,
      "DenseUint32KeepEvenBits");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32SwapAdjacentBits, 0u,
      "DenseUint32SwapAdjacentBits");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32CompressOddBits, 0u,
      "DenseUint32CompressOddBits");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32CompressEvenBits, 0u,
      "DenseUint32CompressEvenBits");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32ExpandLow16ToOdd, 0u,
      "DenseUint32ExpandLow16ToOdd");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32ExpandLow16ToEven, 0u,
      "DenseUint32ExpandLow16ToEven");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32InterleaveHalves, 0u,
      "DenseUint32InterleaveHalves");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32DeinterleaveHalves, 0u,
      "DenseUint32DeinterleaveHalves");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32NibbleXor, 0u,
      "DenseUint32NibbleXor");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32NibbleOr, 0u,
      "DenseUint32NibbleOr");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32NibbleAnd, 0u,
      "DenseUint32NibbleAnd");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32NibbleSum, 0u,
      "DenseUint32NibbleSum");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32NibbleMax, 0u,
      "DenseUint32NibbleMax");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32NibbleMin, 0u,
      "DenseUint32NibbleMin");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32ExtractNibble, 1u,
      "DenseUint32ExtractNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32InsertNibble, 0x0a01u,
      "DenseUint32InsertNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32BroadcastNibble, 0u,
      "DenseUint32BroadcastNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32SwapAdjacentNibbles, 0u,
      "DenseUint32SwapAdjacentNibbles");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32RolNibbles, 1u,
      "DenseUint32RolNibbles");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32RorNibbles, 1u,
      "DenseUint32RorNibbles");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HasZeroNibble, 0u,
      "DenseUint32HasZeroNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32SextNibble, 0u,
      "DenseUint32SextNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32CountZeroNibbles, 0u,
      "DenseUint32CountZeroNibbles");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32PackLowNibbles, 0u,
      "DenseUint32PackLowNibbles");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32PackHighNibbles, 0u,
      "DenseUint32PackHighNibbles");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32ExpandLowNibbles, 0u,
      "DenseUint32ExpandLowNibbles");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32ExpandHighNibbles, 0u,
      "DenseUint32ExpandHighNibbles");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32ReplaceNibble, 0x0a08u,
      "DenseUint32ReplaceNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32MaskNibbles, 0x55u,
      "DenseUint32MaskNibbles");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32AllNibblesEqual, 0u,
      "DenseUint32AllNibblesEqual");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32NibbleRange, 0u,
      "DenseUint32NibbleRange");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32BroadcastNibbleAt, 0u,
      "DenseUint32BroadcastNibbleAt");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfSum, 0u,
      "DenseUint32HalfSum");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfMax, 0u,
      "DenseUint32HalfMax");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfMin, 0u,
      "DenseUint32HalfMin");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfXor, 0u,
      "DenseUint32HalfXor");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfOr, 0u,
      "DenseUint32HalfOr");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfAnd, 0u,
      "DenseUint32HalfAnd");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32BroadcastHalf, 0u,
      "DenseUint32BroadcastHalf");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfEq, 0u,
      "DenseUint32HalfEq");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfRange, 0u,
      "DenseUint32HalfRange");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfLt, 0u,
      "DenseUint32HalfLt");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfGt, 0u,
      "DenseUint32HalfGt");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfNe, 0u,
      "DenseUint32HalfNe");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfLeu, 0u,
      "DenseUint32HalfLeu");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfGeu, 0u,
      "DenseUint32HalfGeu");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfAvg, 0u,
      "DenseUint32HalfAvg");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32BroadcastHighHalf, 0u,
      "DenseUint32BroadcastHighHalf");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfSub, 0u,
      "DenseUint32HalfSub");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfMul, 0u,
      "DenseUint32HalfMul");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfSatAdd, 0u,
      "DenseUint32HalfSatAdd");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfSatSub, 0u,
      "DenseUint32HalfSatSub");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfMulHi, 0u,
      "DenseUint32HalfMulHi");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfDiv, 0u,
      "DenseUint32HalfDiv");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfMod, 0u,
      "DenseUint32HalfMod");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfCeilDiv, 0u,
      "DenseUint32HalfCeilDiv");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfIsDivisible, 0u,
      "DenseUint32HalfIsDivisible");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfGcd, 0u,
      "DenseUint32HalfGcd");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfSelect, 0u,
      "DenseUint32HalfSelect");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfLcm, 0u,
      "DenseUint32HalfLcm");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfCoprime, 0u,
      "DenseUint32HalfCoprime");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfAlignUp, 0u,
      "DenseUint32HalfAlignUp");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfAlignDown, 0u,
      "DenseUint32HalfAlignDown");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfRotl, 0u,
      "DenseUint32HalfRotl");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfRotr, 0u,
      "DenseUint32HalfRotr");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfPopcount, 0u,
      "DenseUint32HalfPopcount");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfClz, 0u,
      "DenseUint32HalfClz");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfCtz, 0u,
      "DenseUint32HalfCtz");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfParity, 0u,
      "DenseUint32HalfParity");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfBitReverse, 0u,
      "DenseUint32HalfBitReverse");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfBswap, 0u,
      "DenseUint32HalfBswap");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfNot, 0u,
      "DenseUint32HalfNot");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfNeg, 0u,
      "DenseUint32HalfNeg");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfAbs, 0u,
      "DenseUint32HalfAbs");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfSignMask, 0u,
      "DenseUint32HalfSignMask");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfIsZero, 0u,
      "DenseUint32HalfIsZero");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfIsNonzero, 0u,
      "DenseUint32HalfIsNonzero");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfClo, 0u,
      "DenseUint32HalfClo");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfCto, 0u,
      "DenseUint32HalfCto");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfIsPow2, 0u,
      "DenseUint32HalfIsPow2");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfIlog2, 0u,
      "DenseUint32HalfIlog2");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfNextPow2, 0u,
      "DenseUint32HalfNextPow2");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfMaskLow, 8u,
      "DenseUint32HalfMaskLow");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfMaskHigh, 8u,
      "DenseUint32HalfMaskHigh");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfBitTest, 3u,
      "DenseUint32HalfBitTest");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfIsolateLsb, 0u,
      "DenseUint32HalfIsolateLsb");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfIsolateMsb, 0u,
      "DenseUint32HalfIsolateMsb");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfClearLsb, 0u,
      "DenseUint32HalfClearLsb");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfLsbMask, 0u,
      "DenseUint32HalfLsbMask");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfGrayEncode, 0u,
      "DenseUint32HalfGrayEncode");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfGrayDecode, 0u,
      "DenseUint32HalfGrayDecode");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfSmearLsb, 0u,
      "DenseUint32HalfSmearLsb");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfClearMsb, 0u,
      "DenseUint32HalfClearMsb");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfSmearMsb, 0u,
      "DenseUint32HalfSmearMsb");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfKeepOddBits, 0u,
      "DenseUint32HalfKeepOddBits");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfKeepEvenBits, 0u,
      "DenseUint32HalfKeepEvenBits");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfSwapAdjacentBits, 0u,
      "DenseUint32HalfSwapAdjacentBits");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfCompressOddBits, 0u,
      "DenseUint32HalfCompressOddBits");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfCompressEvenBits, 0u,
      "DenseUint32HalfCompressEvenBits");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfExpandLow8ToOdd, 0u,
      "DenseUint32HalfExpandLow8ToOdd");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfExpandLow8ToEven, 0u,
      "DenseUint32HalfExpandLow8ToEven");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfInterleaveBytes, 0u,
      "DenseUint32HalfInterleaveBytes");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfDeinterleaveBytes, 0u,
      "DenseUint32HalfDeinterleaveBytes");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfSwapBytes, 0u,
      "DenseUint32HalfSwapBytes");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfNibbleXor, 0u,
      "DenseUint32HalfNibbleXor");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfNibbleOr, 0u,
      "DenseUint32HalfNibbleOr");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfNibbleAnd, 0u,
      "DenseUint32HalfNibbleAnd");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfNibbleSum, 0u,
      "DenseUint32HalfNibbleSum");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfNibbleMax, 0u,
      "DenseUint32HalfNibbleMax");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfNibbleMin, 0u,
      "DenseUint32HalfNibbleMin");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfSwapAdjacentNibbles, 0u,
      "DenseUint32HalfSwapAdjacentNibbles");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfRolNibbles, 1u,
      "DenseUint32HalfRolNibbles");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfRorNibbles, 1u,
      "DenseUint32HalfRorNibbles");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfHasZeroNibble, 0u,
      "DenseUint32HalfHasZeroNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfCountZeroNibbles, 0u,
      "DenseUint32HalfCountZeroNibbles");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfBroadcastNibble, 0u,
      "DenseUint32HalfBroadcastNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfSextNibble, 0u,
      "DenseUint32HalfSextNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfExtractNibble, 1u,
      "DenseUint32HalfExtractNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfInsertNibble, 0x0a01u,
      "DenseUint32HalfInsertNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfAllNibblesEqual, 0u,
      "DenseUint32HalfAllNibblesEqual");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfNibbleRange, 0u,
      "DenseUint32HalfNibbleRange");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfMaskNibbles, 0x5u,
      "DenseUint32HalfMaskNibbles");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfBroadcastNibbleAt, 1u,
      "DenseUint32HalfBroadcastNibbleAt");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfReplaceNibble, 0x0a06u,
      "DenseUint32HalfReplaceNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfReverseNibbles, 0u,
      "DenseUint32HalfReverseNibbles");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfWidenNibbles, 0u,
      "DenseUint32HalfWidenNibbles");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfDupNibbles, 0u,
      "DenseUint32HalfDupNibbles");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfFoldAdjacentNibbles, 0u,
      "DenseUint32HalfFoldAdjacentNibbles");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfCountNonzeroNibbles, 0u,
      "DenseUint32HalfCountNonzeroNibbles");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfAddNibble, 1u,
      "DenseUint32HalfAddNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfSatAddNibble, 9u,
      "DenseUint32HalfSatAddNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfSubNibble, 1u,
      "DenseUint32HalfSubNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfSatSubNibble, 9u,
      "DenseUint32HalfSatSubNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfMulNibble, 2u,
      "DenseUint32HalfMulNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfSatMulNibble, 2u,
      "DenseUint32HalfSatMulNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfDivNibble, 2u,
      "DenseUint32HalfDivNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfModNibble, 3u,
      "DenseUint32HalfModNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfMinNibble, 6u,
      "DenseUint32HalfMinNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfMaxNibble, 6u,
      "DenseUint32HalfMaxNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfClampNibble, 0x0603u,
      "DenseUint32HalfClampNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfAndNibble, 5u,
      "DenseUint32HalfAndNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfOrNibble, 5u,
      "DenseUint32HalfOrNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfXorNibble, 5u,
      "DenseUint32HalfXorNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfShlNibble, 1u,
      "DenseUint32HalfShlNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfShrNibble, 1u,
      "DenseUint32HalfShrNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfRolNibble, 1u,
      "DenseUint32HalfRolNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfRorNibble, 1u,
      "DenseUint32HalfRorNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfNotNibble, 0u,
      "DenseUint32HalfNotNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfNegNibble, 0u,
      "DenseUint32HalfNegNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfAbsNibble, 0u,
      "DenseUint32HalfAbsNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfSignMaskNibble, 0u,
      "DenseUint32HalfSignMaskNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfIsZeroNibble, 0u,
      "DenseUint32HalfIsZeroNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfIsNonzeroNibble, 0u,
      "DenseUint32HalfIsNonzeroNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfEqNibble, 6u,
      "DenseUint32HalfEqNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfNeNibble, 6u,
      "DenseUint32HalfNeNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfLtNibble, 6u,
      "DenseUint32HalfLtNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfLeNibble, 6u,
      "DenseUint32HalfLeNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfGtNibble, 6u,
      "DenseUint32HalfGtNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfGeNibble, 6u,
      "DenseUint32HalfGeNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfBetweenNibble, 0x0605u,
      "DenseUint32HalfBetweenNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfOutsideNibble, 0x0605u,
      "DenseUint32HalfOutsideNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfPopcountNibble, 0u,
      "DenseUint32HalfPopcountNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfParityNibble, 0u,
      "DenseUint32HalfParityNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfReverseBitsNibble, 0u,
      "DenseUint32HalfReverseBitsNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfIsolateLsbNibble, 0u,
      "DenseUint32HalfIsolateLsbNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfClearLsbNibble, 0u,
      "DenseUint32HalfClearLsbNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfSmearLsbNibble, 0u,
      "DenseUint32HalfSmearLsbNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfIsolateMsbNibble, 0u,
      "DenseUint32HalfIsolateMsbNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfClearMsbNibble, 0u,
      "DenseUint32HalfClearMsbNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfSmearMsbNibble, 0u,
      "DenseUint32HalfSmearMsbNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfLsbMaskNibble, 0u,
      "DenseUint32HalfLsbMaskNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfKeepOddBitsNibble, 0u,
      "DenseUint32HalfKeepOddBitsNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfKeepEvenBitsNibble, 0u,
      "DenseUint32HalfKeepEvenBitsNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfSwapAdjacentBitsNibble, 0u,
      "DenseUint32HalfSwapAdjacentBitsNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfGrayEncodeNibble, 0u,
      "DenseUint32HalfGrayEncodeNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfGrayDecodeNibble, 0u,
      "DenseUint32HalfGrayDecodeNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfClzNibble, 0u,
      "DenseUint32HalfClzNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfCtzNibble, 0u,
      "DenseUint32HalfCtzNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfCloNibble, 0u,
      "DenseUint32HalfCloNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfCtoNibble, 0u,
      "DenseUint32HalfCtoNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfBitLengthNibble, 0u,
      "DenseUint32HalfBitLengthNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfIsPowerOfTwoNibble, 0u,
      "DenseUint32HalfIsPowerOfTwoNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfNextPow2Nibble, 0u,
      "DenseUint32HalfNextPow2Nibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfPrevPow2Nibble, 0u,
      "DenseUint32HalfPrevPow2Nibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfBroadcastLsbNibble, 0u,
      "DenseUint32HalfBroadcastLsbNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfBroadcastMsbNibble, 0u,
      "DenseUint32HalfBroadcastMsbNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfParityFillNibble, 0u,
      "DenseUint32HalfParityFillNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfRotl1Nibble, 0u,
      "DenseUint32HalfRotl1Nibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfRotr1Nibble, 0u,
      "DenseUint32HalfRotr1Nibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfNegateNibble, 0u,
      "DenseUint32HalfNegateNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfIncWrapNibble, 0u,
      "DenseUint32HalfIncWrapNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfDecWrapNibble, 0u,
      "DenseUint32HalfDecWrapNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfNandNibble, 0xau,
      "DenseUint32HalfNandNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfNorNibble, 0xau,
      "DenseUint32HalfNorNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfXnorNibble, 0xau,
      "DenseUint32HalfXnorNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfMajNibble, 0x5au,
      "DenseUint32HalfMajNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfMuxNibble, 0x5au,
      "DenseUint32HalfMuxNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfImpliesNibble, 0xau,
      "DenseUint32HalfImpliesNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfNeqFillNibble, 0x6u,
      "DenseUint32HalfNeqFillNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfAbsDiffNibble, 0x3u,
      "DenseUint32HalfAbsDiffNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfEqFillNibble, 0x6u,
      "DenseUint32HalfEqFillNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfOddFillNibble, 0u,
      "DenseUint32HalfOddFillNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfEvenFillNibble, 0u,
      "DenseUint32HalfEvenFillNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfLtFillNibble, 0x6u,
      "DenseUint32HalfLtFillNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfLeFillNibble, 0x6u,
      "DenseUint32HalfLeFillNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfGtFillNibble, 0x6u,
      "DenseUint32HalfGtFillNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfGeFillNibble, 0x6u,
      "DenseUint32HalfGeFillNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfSatIncNibble, 0u,
      "DenseUint32HalfSatIncNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfSatDecNibble, 0u,
      "DenseUint32HalfSatDecNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfRotl2Nibble, 0u,
      "DenseUint32HalfRotl2Nibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfRotl3Nibble, 0u,
      "DenseUint32HalfRotl3Nibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfAvgNibble, 0x4u,
      "DenseUint32HalfAvgNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfMidNibble, 0x5au,
      "DenseUint32HalfMidNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfMin3Nibble, 0x5au,
      "DenseUint32HalfMin3Nibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfMax3Nibble, 0x5au,
      "DenseUint32HalfMax3Nibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfRange3Nibble, 0x5au,
      "DenseUint32HalfRange3Nibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfSum3Nibble, 0x5au,
      "DenseUint32HalfSum3Nibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfSatSum3Nibble, 0x5au,
      "DenseUint32HalfSatSum3Nibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfAvg3Nibble, 0x5au,
      "DenseUint32HalfAvg3Nibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfXor3Nibble, 0x5au,
      "DenseUint32HalfXor3Nibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfMulAddNibble, 0x5au,
      "DenseUint32HalfMulAddNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfSatMulAddNibble, 0x12u,
      "DenseUint32HalfSatMulAddNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfMulSubNibble, 0x5au,
      "DenseUint32HalfMulSubNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfEqual3FillNibble, 0x88u,
      "DenseUint32HalfEqual3FillNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfNeq3FillNibble, 0x88u,
      "DenseUint32HalfNeq3FillNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfAnyEqFillNibble, 0x5au,
      "DenseUint32HalfAnyEqFillNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfAllDistinctFillNibble, 0x5au,
      "DenseUint32HalfAllDistinctFillNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfBlendNibble, 0xa5u,
      "DenseUint32HalfBlendNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfWeightedAddNibble, 0xa5u,
      "DenseUint32HalfWeightedAddNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfBitMergeNibble, 0xc3u,
      "DenseUint32HalfBitMergeNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfSatWeightedAddNibble, 0xa5u,
      "DenseUint32HalfSatWeightedAddNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfMaskSelectNibble, 0xb4u,
      "DenseUint32HalfMaskSelectNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfIfEqPickNibble, 0xd6u,
      "DenseUint32HalfIfEqPickNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfIfNePickNibble, 0xd6u,
      "DenseUint32HalfIfNePickNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfThresholdPickNibble, 0xd5u,
      "DenseUint32HalfThresholdPickNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfModMulNibble, 0x35u,
      "DenseUint32HalfModMulNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfModAddNibble, 0x35u,
      "DenseUint32HalfModAddNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfModSubNibble, 0x35u,
      "DenseUint32HalfModSubNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfCeilDivNibble, 0x3u,
      "DenseUint32HalfCeilDivNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfFloorDivNibble, 0x3u,
      "DenseUint32HalfFloorDivNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfRoundDivNibble, 0x3u,
      "DenseUint32HalfRoundDivNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfRemNibble, 0x3u,
      "DenseUint32HalfRemNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfShlWrapNibble, 0x2u,
      "DenseUint32HalfShlWrapNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfShrWrapNibble, 0x2u,
      "DenseUint32HalfShrWrapNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfFunnelShiftNibble, 0xa2u,
      "DenseUint32HalfFunnelShiftNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfRotlWrapNibble, 0x2u,
      "DenseUint32HalfRotlWrapNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfRotrWrapNibble, 0x1u,
      "DenseUint32HalfRotrWrapNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfBitPairSwapNibble, 0u,
      "DenseUint32HalfBitPairSwapNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfNibbleMirror, 0u,
      "DenseUint32HalfNibbleMirror");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfXorShiftNibble, 0x2u,
      "DenseUint32HalfXorShiftNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfAddCarryFillNibble, 0xau,
      "DenseUint32HalfAddCarryFillNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfSubBorrowFillNibble, 0xau,
      "DenseUint32HalfSubBorrowFillNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfMulOverflowFillNibble, 0x3u,
      "DenseUint32HalfMulOverflowFillNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfInterleaveLowNibble, 0x5u,
      "DenseUint32HalfInterleaveLowNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfDeinterleaveEvenNibble, 0u,
      "DenseUint32HalfDeinterleaveEvenNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfDeinterleaveOddNibble, 0u,
      "DenseUint32HalfDeinterleaveOddNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfBitDepositNibble, 0x5u,
      "DenseUint32HalfBitDepositNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfBitExtractNibble, 0x5u,
      "DenseUint32HalfBitExtractNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfReversePairsNibble, 0u,
      "DenseUint32HalfReversePairsNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfSatShlNibble, 1u,
      "DenseUint32HalfSatShlNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfRoundShrNibble, 1u,
      "DenseUint32HalfRoundShrNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfMinParamNibble, 0xau,
      "DenseUint32HalfMinParamNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfAvgTruncNibble, 0xau,
      "DenseUint32HalfAvgTruncNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfMaxParamNibble, 0xau,
      "DenseUint32HalfMaxParamNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfAbsDiffParamNibble, 0xau,
      "DenseUint32HalfAbsDiffParamNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfMulHiNibble, 0xau,
      "DenseUint32HalfMulHiNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfSquareNibble, 0u,
      "DenseUint32HalfSquareNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfIsOneFillNibble, 0u,
      "DenseUint32HalfIsOneFillNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfRevBitsNibble, 0u,
      "DenseUint32HalfRevBitsNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfCubeNibble, 0u,
      "DenseUint32HalfCubeNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfSatSquareNibble, 0u,
      "DenseUint32HalfSatSquareNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfOnesCompNibble, 0u,
      "DenseUint32HalfOnesCompNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfTwosCompNibble, 0u,
      "DenseUint32HalfTwosCompNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfIncNibble, 0u,
      "DenseUint32HalfIncNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfToggleBitNibble, 1u,
      "DenseUint32HalfToggleBitNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfDecNibble, 0u,
      "DenseUint32HalfDecNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfSetBitNibble, 1u,
      "DenseUint32HalfSetBitNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfClearBitNibble, 1u,
      "DenseUint32HalfClearBitNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfTestBitNibble, 1u,
      "DenseUint32HalfTestBitNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfExtractBitNibble, 1u,
      "DenseUint32HalfExtractBitNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfKeepLowBitsNibble, 1u,
      "DenseUint32HalfKeepLowBitsNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfAndNotNibble, 0x5u,
      "DenseUint32HalfAndNotNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfOrNotNibble, 0x5u,
      "DenseUint32HalfOrNotNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfXorNotNibble, 0x5u,
      "DenseUint32HalfXorNotNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfKeepHighBitsNibble, 1u,
      "DenseUint32HalfKeepHighBitsNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfScatterLsbNibble, 1u,
      "DenseUint32HalfScatterLsbNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfInsertBitNibble, 0x5u,
      "DenseUint32HalfInsertBitNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfAvgRoundNibble, 0xau,
      "DenseUint32HalfAvgRoundNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfMulLoNibble, 0xau,
      "DenseUint32HalfMulLoNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfAddSatParamNibble, 0xau,
      "DenseUint32HalfAddSatParamNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfSubSatParamNibble, 0xau,
      "DenseUint32HalfSubSatParamNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfNextMultipleNibble, 0xau,
      "DenseUint32HalfNextMultipleNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfPrevMultipleNibble, 0xau,
      "DenseUint32HalfPrevMultipleNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfMulSatNibble, 0x6u,
      "DenseUint32HalfMulSatNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfStepNibble, 0x6u,
      "DenseUint32HalfStepNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfGateNibble, 0x6u,
      "DenseUint32HalfGateNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfPassLtNibble, 0x6u,
      "DenseUint32HalfPassLtNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfPassGtNibble, 0x6u,
      "DenseUint32HalfPassGtNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfStepLtNibble, 0x6u,
      "DenseUint32HalfStepLtNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfPassLeNibble, 0x6u,
      "DenseUint32HalfPassLeNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfStepGtNibble, 0x6u,
      "DenseUint32HalfStepGtNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfStepLeNibble, 0x6u,
      "DenseUint32HalfStepLeNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfPassEqNibble, 0x6u,
      "DenseUint32HalfPassEqNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfPassNeNibble, 0x6u,
      "DenseUint32HalfPassNeNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfStepNeNibble, 0x6u,
      "DenseUint32HalfStepNeNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfStepEqNibble, 0x6u,
      "DenseUint32HalfStepEqNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfMirrorNibble, 0x6u,
      "DenseUint32HalfMirrorNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfSatMirrorNibble, 0x6u,
      "DenseUint32HalfSatMirrorNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfScale15Nibble, 0x6u,
      "DenseUint32HalfScale15Nibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfInvScale15Nibble, 0x6u,
      "DenseUint32HalfInvScale15Nibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfSquareDiffNibble, 0x6u,
      "DenseUint32HalfSquareDiffNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfSameSignNibble, 0x6u,
      "DenseUint32HalfSameSignNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfDiffSignNibble, 0x6u,
      "DenseUint32HalfDiffSignNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfParityMatchNibble, 0x6u,
      "DenseUint32HalfParityMatchNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfSignCopyNibble, 0x8u,
      "DenseUint32HalfSignCopyNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfMagnitudeNibble, 0x0u,
      "DenseUint32HalfMagnitudeNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfNegIfSignNibble, 0x8u,
      "DenseUint32HalfNegIfSignNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfIsqrtNibble, 0x0u,
      "DenseUint32HalfIsqrtNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfPow2FillNibble, 0x0u,
      "DenseUint32HalfPow2FillNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfLog2FloorNibble, 0x0u,
      "DenseUint32HalfLog2FloorNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfCeilPow2Nibble, 0x0u,
      "DenseUint32HalfCeilPow2Nibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfFloorPow2Nibble, 0x0u,
      "DenseUint32HalfFloorPow2Nibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfToggleLowNibble, 0x0u,
      "DenseUint32HalfToggleLowNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfAsrNibble, 0x1u,
      "DenseUint32HalfAsrNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfMod3Nibble, 0x0u,
      "DenseUint32HalfMod3Nibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfMod5Nibble, 0x0u,
      "DenseUint32HalfMod5Nibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfGcdNibble, 0x6u,
      "DenseUint32HalfGcdNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfLcmNibble, 0x6u,
      "DenseUint32HalfLcmNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfCoprimeFillNibble, 0x6u,
      "DenseUint32HalfCoprimeFillNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfIsDivisibleNibble, 0x6u,
      "DenseUint32HalfIsDivisibleNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfShareNibble, 0x6u,
      "DenseUint32HalfShareNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfReduceNibble, 0x6u,
      "DenseUint32HalfReduceNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfBitAndFillNibble, 0x6u,
      "DenseUint32HalfBitAndFillNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfSubsetMaskNibble, 0x6u,
      "DenseUint32HalfSubsetMaskNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfSupersetMaskNibble, 0x6u,
      "DenseUint32HalfSupersetMaskNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfHammingNibble, 0x6u,
      "DenseUint32HalfHammingNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfIntersectPopNibble, 0x6u,
      "DenseUint32HalfIntersectPopNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfUnionPopNibble, 0x6u,
      "DenseUint32HalfUnionPopNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfDisjointFillNibble, 0x6u,
      "DenseUint32HalfDisjointFillNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfEqualPopNibble, 0x6u,
      "DenseUint32HalfEqualPopNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfGtPopNibble, 0x6u,
      "DenseUint32HalfGtPopNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfLtPopNibble, 0x6u,
      "DenseUint32HalfLtPopNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfGePopNibble, 0x6u,
      "DenseUint32HalfGePopNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfLePopNibble, 0x6u,
      "DenseUint32HalfLePopNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfAvgBitsNibble, 0x6u,
      "DenseUint32HalfAvgBitsNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfMaxPopPickNibble, 0x6u,
      "DenseUint32HalfMaxPopPickNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfMinPopPickNibble, 0x6u,
      "DenseUint32HalfMinPopPickNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfPopDeltaNibble, 0x6u,
      "DenseUint32HalfPopDeltaNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfPopSumNibble, 0x6u,
      "DenseUint32HalfPopSumNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfPopParityFillNibble, 0x6u,
      "DenseUint32HalfPopParityFillNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfPopSubSatNibble, 0x6u,
      "DenseUint32HalfPopSubSatNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfPopMulNibble, 0x6u,
      "DenseUint32HalfPopMulNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfEvenPopFillNibble, 0x6u,
      "DenseUint32HalfEvenPopFillNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfNePopFillNibble, 0x6u,
      "DenseUint32HalfNePopFillNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfPopMinNibble, 0x6u,
      "DenseUint32HalfPopMinNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfPopMaxNibble, 0x6u,
      "DenseUint32HalfPopMaxNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfOddPopFillNibble, 0x6u,
      "DenseUint32HalfOddPopFillNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfPopMod3Nibble, 0x6u,
      "DenseUint32HalfPopMod3Nibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfMaj3FillNibble, 0x6u,
      "DenseUint32HalfMaj3FillNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfZeroPopFillNibble, 0x6u,
      "DenseUint32HalfZeroPopFillNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfFullPopFillNibble, 0x6u,
      "DenseUint32HalfFullPopFillNibble");
  rc |= runShadow(service, WorldGenerationComputeKernel::DenseUint32HalfPopEqCountFillNibble, 0x2u,
      "DenseUint32HalfPopEqCountFillNibble");

  // Milestone-50 focus (no further DenseUint32 horizontal): terrain/biome GL shadow.
  std::printf("--- m50 terrain/biome ---\n");
  rc |= runShadow(service, WorldGenerationComputeKernel::TerrainFieldMax2, 0u, "TerrainFieldMax2");
  rc |= runShadow(service, WorldGenerationComputeKernel::TerrainFieldMin2, 0u, "TerrainFieldMin2");
  rc |= runShadow(service, WorldGenerationComputeKernel::TerrainFieldClampU16, 0x00ffu,
      "TerrainFieldClampU16");
  rc |= runShadow(service, WorldGenerationComputeKernel::BiomeIdBucket, 0x10u, "BiomeIdBucket");

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

  // Wall timing (informative; not a 50% claim). Larger corpus + iters for gain evidence.
  // Env overrides: STAR_WORLDGEN_GL_TIMING_REPEATS / STAR_WORLDGEN_GL_TIMING_ITERS
  size_t timingRepeats = 16384u;
  size_t timingIters = 8u;
  if (char const* envRep = std::getenv("STAR_WORLDGEN_GL_TIMING_REPEATS")) {
    unsigned long v = std::strtoul(envRep, nullptr, 10);
    if (v >= 1ul && v <= 1000000ul)
      timingRepeats = (size_t)v;
  }
  if (char const* envIt = std::getenv("STAR_WORLDGEN_GL_TIMING_ITERS")) {
    unsigned long v = std::strtoul(envIt, nullptr, 10);
    if (v >= 1ul && v <= 1000ul)
      timingIters = (size_t)v;
  }
  rc |= runTiming(service, WorldGenerationComputeKernel::TerrainFieldMax2, 0u, "TerrainFieldMax2",
      timingRepeats, timingIters);
  rc |= runTiming(service, WorldGenerationComputeKernel::BiomeIdBucket, 0x10u, "BiomeIdBucket",
      timingRepeats, timingIters);

  // Gain pipeline: larger default corpus + multi-pass terrain stack (transfer amortized).
  // Env: STAR_WORLDGEN_GL_GAIN_REPEATS / STAR_WORLDGEN_GL_GAIN_ITERS / STAR_WORLDGEN_GL_GAIN_PASSES
  size_t gainRepeats = 65536u;
  size_t gainIters = 8u;
  uint32_t gainPasses = 16u;
  if (char const* envRep = std::getenv("STAR_WORLDGEN_GL_GAIN_REPEATS")) {
    unsigned long v = std::strtoul(envRep, nullptr, 10);
    if (v >= 1ul && v <= 1000000ul)
      gainRepeats = (size_t)v;
  }
  if (char const* envIt = std::getenv("STAR_WORLDGEN_GL_GAIN_ITERS")) {
    unsigned long v = std::strtoul(envIt, nullptr, 10);
    if (v >= 1ul && v <= 1000ul)
      gainIters = (size_t)v;
  }
  if (char const* envPass = std::getenv("STAR_WORLDGEN_GL_GAIN_PASSES")) {
    unsigned long v = std::strtoul(envPass, nullptr, 10);
    if (v >= 1ul && v <= 10000ul)
      gainPasses = (uint32_t)v;
  }
  rc |= runGainPipeline(gainRepeats, gainIters, gainPasses);

  rc |= runDeferredGpuPump(service);
  rc |= runDriverEvidence(service);

  WorldGenerationComputeGlDevice::shutdown();
  if (rc == 0)
    std::printf(
        "PASS: probe + DenseUint32* + TerrainField/Biome CPU↔GPU shadow + deferred pump + "
        "driver_evidence + gain_pipeline\n");
  else
    std::printf("FAIL: rc=%d\n", rc);
  return rc == 0 ? 0 : 1;
}
