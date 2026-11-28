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
