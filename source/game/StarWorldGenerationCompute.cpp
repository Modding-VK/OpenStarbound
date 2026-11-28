#include "StarWorldGenerationCompute.hpp"
#include "StarWorldGenerationComputeGl.hpp"

#include <algorithm>
#include <cstring>

namespace Star {

bool WorldGenerationComputeInputIdentity::operator==(
    WorldGenerationComputeInputIdentity const& rhs) const {
  return worldEpoch == rhs.worldEpoch && sequence == rhs.sequence
      && kernelVersion == rhs.kernelVersion && kernel == rhs.kernel;
}

bool WorldGenerationComputeInputIdentity::operator!=(
    WorldGenerationComputeInputIdentity const& rhs) const {
  return !(*this == rhs);
}

WorldGenerationComputeCapabilities WorldGenerationComputeService::probeCapabilities() noexcept {
  // Normal builds: GL device stub keeps gpuOpenGlCompute false.
  // STAR_ENABLE_WORLDGEN_GL_COMPUTE: real Windows/SDL GL 4.3+/ARB_compute probe.
  return WorldGenerationComputeGlDevice::probeCapabilities();
}

WorldGenerationComputeService::WorldGenerationComputeService(
    WorldGenerationComputeCapabilities capabilities)
  : m_capabilities(std::move(capabilities)) {
  if (!m_capabilities.cpuReference)
    m_capabilities.cpuReference = true;
  if (!m_capabilities.maxInFlight)
    m_capabilities.maxInFlight = 1;
  if (!m_capabilities.maxWorkGroupInvocations)
    m_capabilities.maxWorkGroupInvocations = 1;
}

WorldGenerationComputeService::~WorldGenerationComputeService() = default;

bool WorldGenerationComputeService::supported(WorldGenerationComputeKernel kernel,
    uint32_t version) noexcept {
  return isKernelSupported(kernel, version);
}

bool WorldGenerationComputeService::isKernelSupported(WorldGenerationComputeKernel kernel,
    uint32_t kernelVersion) noexcept {
  if (kernelVersion != 1u)
    return false;
  return kernel == WorldGenerationComputeKernel::DenseUint32Xor
      || kernel == WorldGenerationComputeKernel::DenseUint32Add
      || kernel == WorldGenerationComputeKernel::DenseUint32And
      || kernel == WorldGenerationComputeKernel::DenseUint32Or
      || kernel == WorldGenerationComputeKernel::DenseUint32Mul
      || kernel == WorldGenerationComputeKernel::DenseUint32Min
      || kernel == WorldGenerationComputeKernel::DenseUint32Max
      || kernel == WorldGenerationComputeKernel::DenseUint32Sub
      || kernel == WorldGenerationComputeKernel::DenseUint32Shl
      || kernel == WorldGenerationComputeKernel::DenseUint32Shr
      || kernel == WorldGenerationComputeKernel::DenseUint32Rotl
      || kernel == WorldGenerationComputeKernel::DenseUint32Rotr
      || kernel == WorldGenerationComputeKernel::DenseUint32Not
      || kernel == WorldGenerationComputeKernel::DenseUint32Clz
      || kernel == WorldGenerationComputeKernel::DenseUint32Ctz
      || kernel == WorldGenerationComputeKernel::DenseUint32Popcount
      || kernel == WorldGenerationComputeKernel::DenseUint32Eq
      || kernel == WorldGenerationComputeKernel::DenseUint32Ne
      || kernel == WorldGenerationComputeKernel::DenseUint32BitReverse
      || kernel == WorldGenerationComputeKernel::DenseUint32Lt
      || kernel == WorldGenerationComputeKernel::DenseUint32Leu
      || kernel == WorldGenerationComputeKernel::DenseUint32AbsDiff
      || kernel == WorldGenerationComputeKernel::DenseUint32Gt
      || kernel == WorldGenerationComputeKernel::DenseUint32Geu
      || kernel == WorldGenerationComputeKernel::DenseUint32Xnor
      || kernel == WorldGenerationComputeKernel::DenseUint32Nand
      || kernel == WorldGenerationComputeKernel::DenseUint32Nor
      || kernel == WorldGenerationComputeKernel::DenseUint32MulHi
      || kernel == WorldGenerationComputeKernel::DenseUint32AndNot
      || kernel == WorldGenerationComputeKernel::DenseUint32SatAdd
      || kernel == WorldGenerationComputeKernel::DenseUint32SatSub
      || kernel == WorldGenerationComputeKernel::DenseUint32OrNot
      || kernel == WorldGenerationComputeKernel::DenseUint32Avg
      || kernel == WorldGenerationComputeKernel::DenseUint32IsPow2
      || kernel == WorldGenerationComputeKernel::DenseUint32Bswap
      || kernel == WorldGenerationComputeKernel::DenseUint32Parity
      || kernel == WorldGenerationComputeKernel::DenseUint32BitTest
      || kernel == WorldGenerationComputeKernel::DenseUint32HasZeroByte
      || kernel == WorldGenerationComputeKernel::DenseUint32NextPow2
      || kernel == WorldGenerationComputeKernel::DenseUint32MaskLow
      || kernel == WorldGenerationComputeKernel::DenseUint32Ilog2
      || kernel == WorldGenerationComputeKernel::DenseUint32BroadcastByte
      || kernel == WorldGenerationComputeKernel::DenseUint32ByteSum
      || kernel == WorldGenerationComputeKernel::DenseUint32Sext8
      || kernel == WorldGenerationComputeKernel::DenseUint32Sext16
      || kernel == WorldGenerationComputeKernel::DenseUint32ExtractByte
      || kernel == WorldGenerationComputeKernel::DenseUint32BitSet
      || kernel == WorldGenerationComputeKernel::DenseUint32BitClear
      || kernel == WorldGenerationComputeKernel::DenseUint32BitToggle
      || kernel == WorldGenerationComputeKernel::DenseUint32Neg
      || kernel == WorldGenerationComputeKernel::DenseUint32Abs
      || kernel == WorldGenerationComputeKernel::DenseUint32RolBytes
      || kernel == WorldGenerationComputeKernel::DenseUint32RorBytes
      || kernel == WorldGenerationComputeKernel::DenseUint32SignMask
      || kernel == WorldGenerationComputeKernel::DenseUint32IsZero
      || kernel == WorldGenerationComputeKernel::DenseUint32IsNonzero
      || kernel == WorldGenerationComputeKernel::DenseUint32MaskHigh
      || kernel == WorldGenerationComputeKernel::DenseUint32SatMul
      || kernel == WorldGenerationComputeKernel::DenseUint32SwapHalves
      || kernel == WorldGenerationComputeKernel::DenseUint32Clo
      || kernel == WorldGenerationComputeKernel::DenseUint32Cto
      || kernel == WorldGenerationComputeKernel::DenseUint32ReverseNibbles
      || kernel == WorldGenerationComputeKernel::DenseUint32ByteMax
      || kernel == WorldGenerationComputeKernel::DenseUint32ByteMin
      || kernel == WorldGenerationComputeKernel::DenseUint32DupLow16
      || kernel == WorldGenerationComputeKernel::DenseUint32DupHigh16
      || kernel == WorldGenerationComputeKernel::DenseUint32ReverseBitsInBytes
      || kernel == WorldGenerationComputeKernel::DenseUint32IsolateLsb
      || kernel == WorldGenerationComputeKernel::DenseUint32IsolateMsb
      || kernel == WorldGenerationComputeKernel::DenseUint32LsbMask
      || kernel == WorldGenerationComputeKernel::DenseUint32ClearLsb
      || kernel == WorldGenerationComputeKernel::DenseUint32GrayEncode
      || kernel == WorldGenerationComputeKernel::DenseUint32GrayDecode
      || kernel == WorldGenerationComputeKernel::DenseUint32SmearLsb
      || kernel == WorldGenerationComputeKernel::DenseUint32ClearMsb
      || kernel == WorldGenerationComputeKernel::DenseUint32ByteXor
      || kernel == WorldGenerationComputeKernel::DenseUint32SmearMsb
      || kernel == WorldGenerationComputeKernel::DenseUint32ByteOr
      || kernel == WorldGenerationComputeKernel::DenseUint32ByteAnd
      || kernel == WorldGenerationComputeKernel::DenseUint32KeepOddBits
      || kernel == WorldGenerationComputeKernel::DenseUint32KeepEvenBits
      || kernel == WorldGenerationComputeKernel::DenseUint32SwapAdjacentBits
      || kernel == WorldGenerationComputeKernel::DenseUint32CompressOddBits
      || kernel == WorldGenerationComputeKernel::DenseUint32CompressEvenBits
      || kernel == WorldGenerationComputeKernel::DenseUint32ExpandLow16ToOdd
      || kernel == WorldGenerationComputeKernel::DenseUint32ExpandLow16ToEven
      || kernel == WorldGenerationComputeKernel::DenseUint32InterleaveHalves
      || kernel == WorldGenerationComputeKernel::DenseUint32DeinterleaveHalves
      || kernel == WorldGenerationComputeKernel::DenseUint32NibbleXor
      || kernel == WorldGenerationComputeKernel::DenseUint32NibbleOr
      || kernel == WorldGenerationComputeKernel::DenseUint32NibbleAnd
      || kernel == WorldGenerationComputeKernel::DenseUint32NibbleSum
      || kernel == WorldGenerationComputeKernel::DenseUint32NibbleMax
      || kernel == WorldGenerationComputeKernel::DenseUint32NibbleMin
      || kernel == WorldGenerationComputeKernel::DenseUint32ExtractNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32InsertNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32BroadcastNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32SwapAdjacentNibbles
      || kernel == WorldGenerationComputeKernel::DenseUint32RolNibbles
      || kernel == WorldGenerationComputeKernel::DenseUint32RorNibbles
      || kernel == WorldGenerationComputeKernel::DenseUint32HasZeroNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32SextNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32CountZeroNibbles
      || kernel == WorldGenerationComputeKernel::DenseUint32PackLowNibbles
      || kernel == WorldGenerationComputeKernel::DenseUint32PackHighNibbles
      || kernel == WorldGenerationComputeKernel::DenseUint32ExpandLowNibbles
      || kernel == WorldGenerationComputeKernel::DenseUint32ExpandHighNibbles
      || kernel == WorldGenerationComputeKernel::DenseUint32ReplaceNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32MaskNibbles
      || kernel == WorldGenerationComputeKernel::DenseUint32AllNibblesEqual
      || kernel == WorldGenerationComputeKernel::DenseUint32NibbleRange
      || kernel == WorldGenerationComputeKernel::DenseUint32BroadcastNibbleAt
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfSum
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfMax
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfMin
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfXor
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfOr
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfAnd
      || kernel == WorldGenerationComputeKernel::DenseUint32BroadcastHalf
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfEq
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfRange
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfLt
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfGt
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfNe
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfLeu
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfGeu
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfAvg
      || kernel == WorldGenerationComputeKernel::DenseUint32BroadcastHighHalf
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfSub
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfMul
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfSatAdd
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfSatSub
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfMulHi
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfDiv
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfMod
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfCeilDiv
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfIsDivisible
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfGcd
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfSelect
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfLcm
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfCoprime
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfAlignUp
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfAlignDown
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfRotl
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfRotr
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfPopcount
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfClz
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfCtz
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfParity
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfBitReverse
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfBswap
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfNot
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfNeg
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfAbs
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfSignMask
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfIsZero
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfIsNonzero
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfClo
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfCto
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfIsPow2
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfIlog2
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfNextPow2
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfMaskLow
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfMaskHigh
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfBitTest
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfIsolateLsb
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfIsolateMsb
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfClearLsb
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfLsbMask
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfGrayEncode
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfGrayDecode
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfSmearLsb
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfClearMsb
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfSmearMsb
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfKeepOddBits
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfKeepEvenBits
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfSwapAdjacentBits
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfCompressOddBits
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfCompressEvenBits
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfExpandLow8ToOdd
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfExpandLow8ToEven
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfInterleaveBytes
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfDeinterleaveBytes
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfSwapBytes
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfNibbleXor
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfNibbleOr
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfNibbleAnd
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfNibbleSum
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfNibbleMax
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfNibbleMin
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfSwapAdjacentNibbles
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfRolNibbles
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfRorNibbles
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfHasZeroNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfCountZeroNibbles
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfBroadcastNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfSextNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfExtractNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfInsertNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfAllNibblesEqual
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfNibbleRange
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfMaskNibbles
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfBroadcastNibbleAt
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfReplaceNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfReverseNibbles
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfWidenNibbles
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfDupNibbles
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfFoldAdjacentNibbles
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfCountNonzeroNibbles
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfAddNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfSatAddNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfSubNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfSatSubNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfMulNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfSatMulNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfDivNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfModNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfMinNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfMaxNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfClampNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfAndNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfOrNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfXorNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfShlNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfShrNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfRolNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfRorNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfNotNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfNegNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfAbsNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfSignMaskNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfIsZeroNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfIsNonzeroNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfEqNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfNeNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfLtNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfLeNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfGtNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfGeNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfBetweenNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfOutsideNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfPopcountNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfParityNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfReverseBitsNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfIsolateLsbNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfClearLsbNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfSmearLsbNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfIsolateMsbNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfClearMsbNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfSmearMsbNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfLsbMaskNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfKeepOddBitsNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfKeepEvenBitsNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfSwapAdjacentBitsNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfGrayEncodeNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfGrayDecodeNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfClzNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfCtzNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfCloNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfCtoNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfBitLengthNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfIsPowerOfTwoNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfNextPow2Nibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfPrevPow2Nibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfBroadcastLsbNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfBroadcastMsbNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfParityFillNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfRotl1Nibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfRotr1Nibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfNegateNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfIncWrapNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfDecWrapNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfNandNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfNorNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfXnorNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfMajNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfMuxNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfImpliesNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfNeqFillNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfAbsDiffNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfEqFillNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfOddFillNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfEvenFillNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfLtFillNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfLeFillNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfGtFillNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfGeFillNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfSatIncNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfSatDecNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfRotl2Nibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfRotl3Nibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfAvgNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfMidNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfMin3Nibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfMax3Nibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfRange3Nibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfSum3Nibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfSatSum3Nibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfAvg3Nibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfXor3Nibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfMulAddNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfSatMulAddNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfMulSubNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfEqual3FillNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfNeq3FillNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfAnyEqFillNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfAllDistinctFillNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfBlendNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfWeightedAddNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfBitMergeNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfSatWeightedAddNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfMaskSelectNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfIfEqPickNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfIfNePickNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfThresholdPickNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfModMulNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfModAddNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfModSubNibble
      || kernel == WorldGenerationComputeKernel::DenseUint32HalfCeilDivNibble;
}

List<WorldGenerationComputeKernel> WorldGenerationComputeService::supportedKernels(
    uint32_t kernelVersion) {
  List<WorldGenerationComputeKernel> out;
  if (kernelVersion != 1u)
    return out;
  WorldGenerationComputeKernel const all[] = {
      WorldGenerationComputeKernel::DenseUint32Xor,
      WorldGenerationComputeKernel::DenseUint32Add,
      WorldGenerationComputeKernel::DenseUint32And,
      WorldGenerationComputeKernel::DenseUint32Or,
      WorldGenerationComputeKernel::DenseUint32Mul,
      WorldGenerationComputeKernel::DenseUint32Min,
      WorldGenerationComputeKernel::DenseUint32Max,
      WorldGenerationComputeKernel::DenseUint32Sub,
      WorldGenerationComputeKernel::DenseUint32Shl,
      WorldGenerationComputeKernel::DenseUint32Shr,
      WorldGenerationComputeKernel::DenseUint32Rotl,
      WorldGenerationComputeKernel::DenseUint32Rotr,
      WorldGenerationComputeKernel::DenseUint32Not,
      WorldGenerationComputeKernel::DenseUint32Clz,
      WorldGenerationComputeKernel::DenseUint32Ctz,
      WorldGenerationComputeKernel::DenseUint32Popcount,
      WorldGenerationComputeKernel::DenseUint32Eq,
      WorldGenerationComputeKernel::DenseUint32Ne,
      WorldGenerationComputeKernel::DenseUint32BitReverse,
      WorldGenerationComputeKernel::DenseUint32Lt,
      WorldGenerationComputeKernel::DenseUint32Leu,
      WorldGenerationComputeKernel::DenseUint32AbsDiff,
      WorldGenerationComputeKernel::DenseUint32Gt,
      WorldGenerationComputeKernel::DenseUint32Geu,
      WorldGenerationComputeKernel::DenseUint32Xnor,
      WorldGenerationComputeKernel::DenseUint32Nand,
      WorldGenerationComputeKernel::DenseUint32Nor,
      WorldGenerationComputeKernel::DenseUint32MulHi,
      WorldGenerationComputeKernel::DenseUint32AndNot,
      WorldGenerationComputeKernel::DenseUint32SatAdd,
      WorldGenerationComputeKernel::DenseUint32SatSub,
      WorldGenerationComputeKernel::DenseUint32OrNot,
      WorldGenerationComputeKernel::DenseUint32Avg,
      WorldGenerationComputeKernel::DenseUint32IsPow2,
      WorldGenerationComputeKernel::DenseUint32Bswap,
      WorldGenerationComputeKernel::DenseUint32Parity,
      WorldGenerationComputeKernel::DenseUint32BitTest,
      WorldGenerationComputeKernel::DenseUint32HasZeroByte,
      WorldGenerationComputeKernel::DenseUint32NextPow2,
      WorldGenerationComputeKernel::DenseUint32MaskLow,
      WorldGenerationComputeKernel::DenseUint32Ilog2,
      WorldGenerationComputeKernel::DenseUint32BroadcastByte,
      WorldGenerationComputeKernel::DenseUint32ByteSum,
      WorldGenerationComputeKernel::DenseUint32Sext8,
      WorldGenerationComputeKernel::DenseUint32Sext16,
      WorldGenerationComputeKernel::DenseUint32ExtractByte,
      WorldGenerationComputeKernel::DenseUint32BitSet,
      WorldGenerationComputeKernel::DenseUint32BitClear,
      WorldGenerationComputeKernel::DenseUint32BitToggle,
      WorldGenerationComputeKernel::DenseUint32Neg,
      WorldGenerationComputeKernel::DenseUint32Abs,
      WorldGenerationComputeKernel::DenseUint32RolBytes,
      WorldGenerationComputeKernel::DenseUint32RorBytes,
      WorldGenerationComputeKernel::DenseUint32SignMask,
      WorldGenerationComputeKernel::DenseUint32IsZero,
      WorldGenerationComputeKernel::DenseUint32IsNonzero,
      WorldGenerationComputeKernel::DenseUint32MaskHigh,
      WorldGenerationComputeKernel::DenseUint32SatMul,
      WorldGenerationComputeKernel::DenseUint32SwapHalves,
      WorldGenerationComputeKernel::DenseUint32Clo,
      WorldGenerationComputeKernel::DenseUint32Cto,
      WorldGenerationComputeKernel::DenseUint32ReverseNibbles,
      WorldGenerationComputeKernel::DenseUint32ByteMax,
      WorldGenerationComputeKernel::DenseUint32ByteMin,
      WorldGenerationComputeKernel::DenseUint32DupLow16,
      WorldGenerationComputeKernel::DenseUint32DupHigh16,
      WorldGenerationComputeKernel::DenseUint32ReverseBitsInBytes,
      WorldGenerationComputeKernel::DenseUint32IsolateLsb,
      WorldGenerationComputeKernel::DenseUint32IsolateMsb,
      WorldGenerationComputeKernel::DenseUint32LsbMask,
      WorldGenerationComputeKernel::DenseUint32ClearLsb,
      WorldGenerationComputeKernel::DenseUint32GrayEncode,
      WorldGenerationComputeKernel::DenseUint32GrayDecode,
      WorldGenerationComputeKernel::DenseUint32SmearLsb,
      WorldGenerationComputeKernel::DenseUint32ClearMsb,
      WorldGenerationComputeKernel::DenseUint32ByteXor,
      WorldGenerationComputeKernel::DenseUint32SmearMsb,
      WorldGenerationComputeKernel::DenseUint32ByteOr,
      WorldGenerationComputeKernel::DenseUint32ByteAnd,
      WorldGenerationComputeKernel::DenseUint32KeepOddBits,
      WorldGenerationComputeKernel::DenseUint32KeepEvenBits,
      WorldGenerationComputeKernel::DenseUint32SwapAdjacentBits,
      WorldGenerationComputeKernel::DenseUint32CompressOddBits,
      WorldGenerationComputeKernel::DenseUint32CompressEvenBits,
      WorldGenerationComputeKernel::DenseUint32ExpandLow16ToOdd,
      WorldGenerationComputeKernel::DenseUint32ExpandLow16ToEven,
      WorldGenerationComputeKernel::DenseUint32InterleaveHalves,
      WorldGenerationComputeKernel::DenseUint32DeinterleaveHalves,
      WorldGenerationComputeKernel::DenseUint32NibbleXor,
      WorldGenerationComputeKernel::DenseUint32NibbleOr,
      WorldGenerationComputeKernel::DenseUint32NibbleAnd,
      WorldGenerationComputeKernel::DenseUint32NibbleSum,
      WorldGenerationComputeKernel::DenseUint32NibbleMax,
      WorldGenerationComputeKernel::DenseUint32NibbleMin,
      WorldGenerationComputeKernel::DenseUint32ExtractNibble,
      WorldGenerationComputeKernel::DenseUint32InsertNibble,
      WorldGenerationComputeKernel::DenseUint32BroadcastNibble,
      WorldGenerationComputeKernel::DenseUint32SwapAdjacentNibbles,
      WorldGenerationComputeKernel::DenseUint32RolNibbles,
      WorldGenerationComputeKernel::DenseUint32RorNibbles,
      WorldGenerationComputeKernel::DenseUint32HasZeroNibble,
      WorldGenerationComputeKernel::DenseUint32SextNibble,
      WorldGenerationComputeKernel::DenseUint32CountZeroNibbles,
      WorldGenerationComputeKernel::DenseUint32PackLowNibbles,
      WorldGenerationComputeKernel::DenseUint32PackHighNibbles,
      WorldGenerationComputeKernel::DenseUint32ExpandLowNibbles,
      WorldGenerationComputeKernel::DenseUint32ExpandHighNibbles,
      WorldGenerationComputeKernel::DenseUint32ReplaceNibble,
      WorldGenerationComputeKernel::DenseUint32MaskNibbles,
      WorldGenerationComputeKernel::DenseUint32AllNibblesEqual,
      WorldGenerationComputeKernel::DenseUint32NibbleRange,
      WorldGenerationComputeKernel::DenseUint32BroadcastNibbleAt,
      WorldGenerationComputeKernel::DenseUint32HalfSum,
      WorldGenerationComputeKernel::DenseUint32HalfMax,
      WorldGenerationComputeKernel::DenseUint32HalfMin,
      WorldGenerationComputeKernel::DenseUint32HalfXor,
      WorldGenerationComputeKernel::DenseUint32HalfOr,
      WorldGenerationComputeKernel::DenseUint32HalfAnd,
      WorldGenerationComputeKernel::DenseUint32BroadcastHalf,
      WorldGenerationComputeKernel::DenseUint32HalfEq,
      WorldGenerationComputeKernel::DenseUint32HalfRange,
      WorldGenerationComputeKernel::DenseUint32HalfLt,
      WorldGenerationComputeKernel::DenseUint32HalfGt,
      WorldGenerationComputeKernel::DenseUint32HalfNe,
      WorldGenerationComputeKernel::DenseUint32HalfLeu,
      WorldGenerationComputeKernel::DenseUint32HalfGeu,
      WorldGenerationComputeKernel::DenseUint32HalfAvg,
      WorldGenerationComputeKernel::DenseUint32BroadcastHighHalf,
      WorldGenerationComputeKernel::DenseUint32HalfSub,
      WorldGenerationComputeKernel::DenseUint32HalfMul,
      WorldGenerationComputeKernel::DenseUint32HalfSatAdd,
      WorldGenerationComputeKernel::DenseUint32HalfSatSub,
      WorldGenerationComputeKernel::DenseUint32HalfMulHi,
      WorldGenerationComputeKernel::DenseUint32HalfDiv,
      WorldGenerationComputeKernel::DenseUint32HalfMod,
      WorldGenerationComputeKernel::DenseUint32HalfCeilDiv,
      WorldGenerationComputeKernel::DenseUint32HalfIsDivisible,
      WorldGenerationComputeKernel::DenseUint32HalfGcd,
      WorldGenerationComputeKernel::DenseUint32HalfSelect,
      WorldGenerationComputeKernel::DenseUint32HalfLcm,
      WorldGenerationComputeKernel::DenseUint32HalfCoprime,
      WorldGenerationComputeKernel::DenseUint32HalfAlignUp,
      WorldGenerationComputeKernel::DenseUint32HalfAlignDown,
      WorldGenerationComputeKernel::DenseUint32HalfRotl,
      WorldGenerationComputeKernel::DenseUint32HalfRotr,
      WorldGenerationComputeKernel::DenseUint32HalfPopcount,
      WorldGenerationComputeKernel::DenseUint32HalfClz,
      WorldGenerationComputeKernel::DenseUint32HalfCtz,
      WorldGenerationComputeKernel::DenseUint32HalfParity,
      WorldGenerationComputeKernel::DenseUint32HalfBitReverse,
      WorldGenerationComputeKernel::DenseUint32HalfBswap,
      WorldGenerationComputeKernel::DenseUint32HalfNot,
      WorldGenerationComputeKernel::DenseUint32HalfNeg,
      WorldGenerationComputeKernel::DenseUint32HalfAbs,
      WorldGenerationComputeKernel::DenseUint32HalfSignMask,
      WorldGenerationComputeKernel::DenseUint32HalfIsZero,
      WorldGenerationComputeKernel::DenseUint32HalfIsNonzero,
      WorldGenerationComputeKernel::DenseUint32HalfClo,
      WorldGenerationComputeKernel::DenseUint32HalfCto,
      WorldGenerationComputeKernel::DenseUint32HalfIsPow2,
      WorldGenerationComputeKernel::DenseUint32HalfIlog2,
      WorldGenerationComputeKernel::DenseUint32HalfNextPow2,
      WorldGenerationComputeKernel::DenseUint32HalfMaskLow,
      WorldGenerationComputeKernel::DenseUint32HalfMaskHigh,
      WorldGenerationComputeKernel::DenseUint32HalfBitTest,
      WorldGenerationComputeKernel::DenseUint32HalfIsolateLsb,
      WorldGenerationComputeKernel::DenseUint32HalfIsolateMsb,
      WorldGenerationComputeKernel::DenseUint32HalfClearLsb,
      WorldGenerationComputeKernel::DenseUint32HalfLsbMask,
      WorldGenerationComputeKernel::DenseUint32HalfGrayEncode,
      WorldGenerationComputeKernel::DenseUint32HalfGrayDecode,
      WorldGenerationComputeKernel::DenseUint32HalfSmearLsb,
      WorldGenerationComputeKernel::DenseUint32HalfClearMsb,
      WorldGenerationComputeKernel::DenseUint32HalfSmearMsb,
      WorldGenerationComputeKernel::DenseUint32HalfKeepOddBits,
      WorldGenerationComputeKernel::DenseUint32HalfKeepEvenBits,
      WorldGenerationComputeKernel::DenseUint32HalfSwapAdjacentBits,
      WorldGenerationComputeKernel::DenseUint32HalfCompressOddBits,
      WorldGenerationComputeKernel::DenseUint32HalfCompressEvenBits,
      WorldGenerationComputeKernel::DenseUint32HalfExpandLow8ToOdd,
      WorldGenerationComputeKernel::DenseUint32HalfExpandLow8ToEven,
      WorldGenerationComputeKernel::DenseUint32HalfInterleaveBytes,
      WorldGenerationComputeKernel::DenseUint32HalfDeinterleaveBytes,
      WorldGenerationComputeKernel::DenseUint32HalfSwapBytes,
      WorldGenerationComputeKernel::DenseUint32HalfNibbleXor,
      WorldGenerationComputeKernel::DenseUint32HalfNibbleOr,
      WorldGenerationComputeKernel::DenseUint32HalfNibbleAnd,
      WorldGenerationComputeKernel::DenseUint32HalfNibbleSum,
      WorldGenerationComputeKernel::DenseUint32HalfNibbleMax,
      WorldGenerationComputeKernel::DenseUint32HalfNibbleMin,
      WorldGenerationComputeKernel::DenseUint32HalfSwapAdjacentNibbles,
      WorldGenerationComputeKernel::DenseUint32HalfRolNibbles,
      WorldGenerationComputeKernel::DenseUint32HalfRorNibbles,
      WorldGenerationComputeKernel::DenseUint32HalfHasZeroNibble,
      WorldGenerationComputeKernel::DenseUint32HalfCountZeroNibbles,
      WorldGenerationComputeKernel::DenseUint32HalfBroadcastNibble,
      WorldGenerationComputeKernel::DenseUint32HalfSextNibble,
      WorldGenerationComputeKernel::DenseUint32HalfExtractNibble,
      WorldGenerationComputeKernel::DenseUint32HalfInsertNibble,
      WorldGenerationComputeKernel::DenseUint32HalfAllNibblesEqual,
      WorldGenerationComputeKernel::DenseUint32HalfNibbleRange,
      WorldGenerationComputeKernel::DenseUint32HalfMaskNibbles,
      WorldGenerationComputeKernel::DenseUint32HalfBroadcastNibbleAt,
      WorldGenerationComputeKernel::DenseUint32HalfReplaceNibble,
      WorldGenerationComputeKernel::DenseUint32HalfReverseNibbles,
      WorldGenerationComputeKernel::DenseUint32HalfWidenNibbles,
      WorldGenerationComputeKernel::DenseUint32HalfDupNibbles,
      WorldGenerationComputeKernel::DenseUint32HalfFoldAdjacentNibbles,
      WorldGenerationComputeKernel::DenseUint32HalfCountNonzeroNibbles,
      WorldGenerationComputeKernel::DenseUint32HalfAddNibble,
      WorldGenerationComputeKernel::DenseUint32HalfSatAddNibble,
      WorldGenerationComputeKernel::DenseUint32HalfSubNibble,
      WorldGenerationComputeKernel::DenseUint32HalfSatSubNibble,
      WorldGenerationComputeKernel::DenseUint32HalfMulNibble,
      WorldGenerationComputeKernel::DenseUint32HalfSatMulNibble,
      WorldGenerationComputeKernel::DenseUint32HalfDivNibble,
      WorldGenerationComputeKernel::DenseUint32HalfModNibble,
      WorldGenerationComputeKernel::DenseUint32HalfMinNibble,
      WorldGenerationComputeKernel::DenseUint32HalfMaxNibble,
      WorldGenerationComputeKernel::DenseUint32HalfClampNibble,
      WorldGenerationComputeKernel::DenseUint32HalfAndNibble,
      WorldGenerationComputeKernel::DenseUint32HalfOrNibble,
      WorldGenerationComputeKernel::DenseUint32HalfXorNibble,
      WorldGenerationComputeKernel::DenseUint32HalfShlNibble,
      WorldGenerationComputeKernel::DenseUint32HalfShrNibble,
      WorldGenerationComputeKernel::DenseUint32HalfRolNibble,
      WorldGenerationComputeKernel::DenseUint32HalfRorNibble,
      WorldGenerationComputeKernel::DenseUint32HalfNotNibble,
      WorldGenerationComputeKernel::DenseUint32HalfNegNibble,
      WorldGenerationComputeKernel::DenseUint32HalfAbsNibble,
      WorldGenerationComputeKernel::DenseUint32HalfSignMaskNibble,
      WorldGenerationComputeKernel::DenseUint32HalfIsZeroNibble,
      WorldGenerationComputeKernel::DenseUint32HalfIsNonzeroNibble,
      WorldGenerationComputeKernel::DenseUint32HalfEqNibble,
      WorldGenerationComputeKernel::DenseUint32HalfNeNibble,
      WorldGenerationComputeKernel::DenseUint32HalfLtNibble,
      WorldGenerationComputeKernel::DenseUint32HalfLeNibble,
      WorldGenerationComputeKernel::DenseUint32HalfGtNibble,
      WorldGenerationComputeKernel::DenseUint32HalfGeNibble,
      WorldGenerationComputeKernel::DenseUint32HalfBetweenNibble,
      WorldGenerationComputeKernel::DenseUint32HalfOutsideNibble,
      WorldGenerationComputeKernel::DenseUint32HalfPopcountNibble,
      WorldGenerationComputeKernel::DenseUint32HalfParityNibble,
      WorldGenerationComputeKernel::DenseUint32HalfReverseBitsNibble,
      WorldGenerationComputeKernel::DenseUint32HalfIsolateLsbNibble,
      WorldGenerationComputeKernel::DenseUint32HalfClearLsbNibble,
      WorldGenerationComputeKernel::DenseUint32HalfSmearLsbNibble,
      WorldGenerationComputeKernel::DenseUint32HalfIsolateMsbNibble,
      WorldGenerationComputeKernel::DenseUint32HalfClearMsbNibble,
      WorldGenerationComputeKernel::DenseUint32HalfSmearMsbNibble,
      WorldGenerationComputeKernel::DenseUint32HalfLsbMaskNibble,
      WorldGenerationComputeKernel::DenseUint32HalfKeepOddBitsNibble,
      WorldGenerationComputeKernel::DenseUint32HalfKeepEvenBitsNibble,
      WorldGenerationComputeKernel::DenseUint32HalfSwapAdjacentBitsNibble,
      WorldGenerationComputeKernel::DenseUint32HalfGrayEncodeNibble,
      WorldGenerationComputeKernel::DenseUint32HalfGrayDecodeNibble,
      WorldGenerationComputeKernel::DenseUint32HalfClzNibble,
      WorldGenerationComputeKernel::DenseUint32HalfCtzNibble,
      WorldGenerationComputeKernel::DenseUint32HalfCloNibble,
      WorldGenerationComputeKernel::DenseUint32HalfCtoNibble,
      WorldGenerationComputeKernel::DenseUint32HalfBitLengthNibble,
      WorldGenerationComputeKernel::DenseUint32HalfIsPowerOfTwoNibble,
      WorldGenerationComputeKernel::DenseUint32HalfNextPow2Nibble,
      WorldGenerationComputeKernel::DenseUint32HalfPrevPow2Nibble,
      WorldGenerationComputeKernel::DenseUint32HalfBroadcastLsbNibble,
      WorldGenerationComputeKernel::DenseUint32HalfBroadcastMsbNibble,
      WorldGenerationComputeKernel::DenseUint32HalfParityFillNibble,
      WorldGenerationComputeKernel::DenseUint32HalfRotl1Nibble,
      WorldGenerationComputeKernel::DenseUint32HalfRotr1Nibble,
      WorldGenerationComputeKernel::DenseUint32HalfNegateNibble,
      WorldGenerationComputeKernel::DenseUint32HalfIncWrapNibble,
      WorldGenerationComputeKernel::DenseUint32HalfDecWrapNibble,
      WorldGenerationComputeKernel::DenseUint32HalfNandNibble,
      WorldGenerationComputeKernel::DenseUint32HalfNorNibble,
      WorldGenerationComputeKernel::DenseUint32HalfXnorNibble,
      WorldGenerationComputeKernel::DenseUint32HalfMajNibble,
      WorldGenerationComputeKernel::DenseUint32HalfMuxNibble,
      WorldGenerationComputeKernel::DenseUint32HalfImpliesNibble,
      WorldGenerationComputeKernel::DenseUint32HalfNeqFillNibble,
      WorldGenerationComputeKernel::DenseUint32HalfAbsDiffNibble,
      WorldGenerationComputeKernel::DenseUint32HalfEqFillNibble,
      WorldGenerationComputeKernel::DenseUint32HalfOddFillNibble,
      WorldGenerationComputeKernel::DenseUint32HalfEvenFillNibble,
      WorldGenerationComputeKernel::DenseUint32HalfLtFillNibble,
      WorldGenerationComputeKernel::DenseUint32HalfLeFillNibble,
      WorldGenerationComputeKernel::DenseUint32HalfGtFillNibble,
      WorldGenerationComputeKernel::DenseUint32HalfGeFillNibble,
      WorldGenerationComputeKernel::DenseUint32HalfSatIncNibble,
      WorldGenerationComputeKernel::DenseUint32HalfSatDecNibble,
      WorldGenerationComputeKernel::DenseUint32HalfRotl2Nibble,
      WorldGenerationComputeKernel::DenseUint32HalfRotl3Nibble,
      WorldGenerationComputeKernel::DenseUint32HalfAvgNibble,
      WorldGenerationComputeKernel::DenseUint32HalfMidNibble,
      WorldGenerationComputeKernel::DenseUint32HalfMin3Nibble,
      WorldGenerationComputeKernel::DenseUint32HalfMax3Nibble,
      WorldGenerationComputeKernel::DenseUint32HalfRange3Nibble,
      WorldGenerationComputeKernel::DenseUint32HalfSum3Nibble,
      WorldGenerationComputeKernel::DenseUint32HalfSatSum3Nibble,
      WorldGenerationComputeKernel::DenseUint32HalfAvg3Nibble,
      WorldGenerationComputeKernel::DenseUint32HalfXor3Nibble,
      WorldGenerationComputeKernel::DenseUint32HalfMulAddNibble,
      WorldGenerationComputeKernel::DenseUint32HalfSatMulAddNibble,
      WorldGenerationComputeKernel::DenseUint32HalfMulSubNibble,
      WorldGenerationComputeKernel::DenseUint32HalfEqual3FillNibble,
      WorldGenerationComputeKernel::DenseUint32HalfNeq3FillNibble,
      WorldGenerationComputeKernel::DenseUint32HalfAnyEqFillNibble,
      WorldGenerationComputeKernel::DenseUint32HalfAllDistinctFillNibble,
      WorldGenerationComputeKernel::DenseUint32HalfBlendNibble,
      WorldGenerationComputeKernel::DenseUint32HalfWeightedAddNibble,
      WorldGenerationComputeKernel::DenseUint32HalfBitMergeNibble,
      WorldGenerationComputeKernel::DenseUint32HalfSatWeightedAddNibble,
      WorldGenerationComputeKernel::DenseUint32HalfMaskSelectNibble,
      WorldGenerationComputeKernel::DenseUint32HalfIfEqPickNibble,
      WorldGenerationComputeKernel::DenseUint32HalfIfNePickNibble,
      WorldGenerationComputeKernel::DenseUint32HalfThresholdPickNibble,
      WorldGenerationComputeKernel::DenseUint32HalfModMulNibble,
      WorldGenerationComputeKernel::DenseUint32HalfModAddNibble,
      WorldGenerationComputeKernel::DenseUint32HalfModSubNibble,
      WorldGenerationComputeKernel::DenseUint32HalfCeilDivNibble};
  for (auto kernel : all)
    out.append(kernel);
  return out;
}

size_t WorldGenerationComputeService::supportedKernelCount(uint32_t kernelVersion) noexcept {
  if (kernelVersion != 1u)
    return 0;
  // Keep in sync with the catalog array in supportedKernels().
  return 319u;
}

List<uint32_t> WorldGenerationComputeService::runNamedKernel(char const* name,
    uint32_t kernelVersion, List<uint32_t> const& input, uint32_t parameter) {
  auto kernel = parseKernelName(name);
  if (!kernel)
    return {};
  return runKernel(*kernel, kernelVersion, input, parameter);
}

bool WorldGenerationComputeService::isBackendSupported(WorldGenerationComputeBackend backend,
    WorldGenerationComputeCapabilities const& capabilities) noexcept {
  if (backend == WorldGenerationComputeBackend::CpuReference)
    return capabilities.cpuReference;
  if (backend == WorldGenerationComputeBackend::GpuOpenGlCompute)
    return capabilities.gpuOpenGlCompute;
  return false;
}

char const* WorldGenerationComputeService::kernelName(WorldGenerationComputeKernel kernel) noexcept {
  switch (kernel) {
    case WorldGenerationComputeKernel::DenseUint32Xor: return "DenseUint32Xor";
    case WorldGenerationComputeKernel::DenseUint32Add: return "DenseUint32Add";
    case WorldGenerationComputeKernel::DenseUint32And: return "DenseUint32And";
    case WorldGenerationComputeKernel::DenseUint32Or: return "DenseUint32Or";
    case WorldGenerationComputeKernel::DenseUint32Mul: return "DenseUint32Mul";
    case WorldGenerationComputeKernel::DenseUint32Min: return "DenseUint32Min";
    case WorldGenerationComputeKernel::DenseUint32Max: return "DenseUint32Max";
    case WorldGenerationComputeKernel::DenseUint32Sub: return "DenseUint32Sub";
    case WorldGenerationComputeKernel::DenseUint32Shl: return "DenseUint32Shl";
    case WorldGenerationComputeKernel::DenseUint32Shr: return "DenseUint32Shr";
    case WorldGenerationComputeKernel::DenseUint32Rotl: return "DenseUint32Rotl";
    case WorldGenerationComputeKernel::DenseUint32Rotr: return "DenseUint32Rotr";
    case WorldGenerationComputeKernel::DenseUint32Not: return "DenseUint32Not";
    case WorldGenerationComputeKernel::DenseUint32Clz: return "DenseUint32Clz";
    case WorldGenerationComputeKernel::DenseUint32Ctz: return "DenseUint32Ctz";
    case WorldGenerationComputeKernel::DenseUint32Popcount: return "DenseUint32Popcount";
    case WorldGenerationComputeKernel::DenseUint32Eq: return "DenseUint32Eq";
    case WorldGenerationComputeKernel::DenseUint32Ne: return "DenseUint32Ne";
    case WorldGenerationComputeKernel::DenseUint32BitReverse: return "DenseUint32BitReverse";
    case WorldGenerationComputeKernel::DenseUint32Lt: return "DenseUint32Lt";
    case WorldGenerationComputeKernel::DenseUint32Leu: return "DenseUint32Leu";
    case WorldGenerationComputeKernel::DenseUint32AbsDiff: return "DenseUint32AbsDiff";
    case WorldGenerationComputeKernel::DenseUint32Gt: return "DenseUint32Gt";
    case WorldGenerationComputeKernel::DenseUint32Geu: return "DenseUint32Geu";
    case WorldGenerationComputeKernel::DenseUint32Xnor: return "DenseUint32Xnor";
    case WorldGenerationComputeKernel::DenseUint32Nand: return "DenseUint32Nand";
    case WorldGenerationComputeKernel::DenseUint32Nor: return "DenseUint32Nor";
    case WorldGenerationComputeKernel::DenseUint32MulHi: return "DenseUint32MulHi";
    case WorldGenerationComputeKernel::DenseUint32AndNot: return "DenseUint32AndNot";
    case WorldGenerationComputeKernel::DenseUint32SatAdd: return "DenseUint32SatAdd";
    case WorldGenerationComputeKernel::DenseUint32SatSub: return "DenseUint32SatSub";
    case WorldGenerationComputeKernel::DenseUint32OrNot: return "DenseUint32OrNot";
    case WorldGenerationComputeKernel::DenseUint32Avg: return "DenseUint32Avg";
    case WorldGenerationComputeKernel::DenseUint32IsPow2: return "DenseUint32IsPow2";
    case WorldGenerationComputeKernel::DenseUint32Bswap: return "DenseUint32Bswap";
    case WorldGenerationComputeKernel::DenseUint32Parity: return "DenseUint32Parity";
    case WorldGenerationComputeKernel::DenseUint32BitTest: return "DenseUint32BitTest";
    case WorldGenerationComputeKernel::DenseUint32HasZeroByte: return "DenseUint32HasZeroByte";
    case WorldGenerationComputeKernel::DenseUint32NextPow2: return "DenseUint32NextPow2";
    case WorldGenerationComputeKernel::DenseUint32MaskLow: return "DenseUint32MaskLow";
    case WorldGenerationComputeKernel::DenseUint32Ilog2: return "DenseUint32Ilog2";
    case WorldGenerationComputeKernel::DenseUint32BroadcastByte: return "DenseUint32BroadcastByte";
    case WorldGenerationComputeKernel::DenseUint32ByteSum: return "DenseUint32ByteSum";
    case WorldGenerationComputeKernel::DenseUint32Sext8: return "DenseUint32Sext8";
    case WorldGenerationComputeKernel::DenseUint32Sext16: return "DenseUint32Sext16";
    case WorldGenerationComputeKernel::DenseUint32ExtractByte: return "DenseUint32ExtractByte";
    case WorldGenerationComputeKernel::DenseUint32BitSet: return "DenseUint32BitSet";
    case WorldGenerationComputeKernel::DenseUint32BitClear: return "DenseUint32BitClear";
    case WorldGenerationComputeKernel::DenseUint32BitToggle: return "DenseUint32BitToggle";
    case WorldGenerationComputeKernel::DenseUint32Neg: return "DenseUint32Neg";
    case WorldGenerationComputeKernel::DenseUint32Abs: return "DenseUint32Abs";
    case WorldGenerationComputeKernel::DenseUint32RolBytes: return "DenseUint32RolBytes";
    case WorldGenerationComputeKernel::DenseUint32RorBytes: return "DenseUint32RorBytes";
    case WorldGenerationComputeKernel::DenseUint32SignMask: return "DenseUint32SignMask";
    case WorldGenerationComputeKernel::DenseUint32IsZero: return "DenseUint32IsZero";
    case WorldGenerationComputeKernel::DenseUint32IsNonzero: return "DenseUint32IsNonzero";
    case WorldGenerationComputeKernel::DenseUint32MaskHigh: return "DenseUint32MaskHigh";
    case WorldGenerationComputeKernel::DenseUint32SatMul: return "DenseUint32SatMul";
    case WorldGenerationComputeKernel::DenseUint32SwapHalves: return "DenseUint32SwapHalves";
    case WorldGenerationComputeKernel::DenseUint32Clo: return "DenseUint32Clo";
    case WorldGenerationComputeKernel::DenseUint32Cto: return "DenseUint32Cto";
    case WorldGenerationComputeKernel::DenseUint32ReverseNibbles: return "DenseUint32ReverseNibbles";
    case WorldGenerationComputeKernel::DenseUint32ByteMax: return "DenseUint32ByteMax";
    case WorldGenerationComputeKernel::DenseUint32ByteMin: return "DenseUint32ByteMin";
    case WorldGenerationComputeKernel::DenseUint32DupLow16: return "DenseUint32DupLow16";
    case WorldGenerationComputeKernel::DenseUint32DupHigh16: return "DenseUint32DupHigh16";
    case WorldGenerationComputeKernel::DenseUint32ReverseBitsInBytes: return "DenseUint32ReverseBitsInBytes";
    case WorldGenerationComputeKernel::DenseUint32IsolateLsb: return "DenseUint32IsolateLsb";
    case WorldGenerationComputeKernel::DenseUint32IsolateMsb: return "DenseUint32IsolateMsb";
    case WorldGenerationComputeKernel::DenseUint32LsbMask: return "DenseUint32LsbMask";
    case WorldGenerationComputeKernel::DenseUint32ClearLsb: return "DenseUint32ClearLsb";
    case WorldGenerationComputeKernel::DenseUint32GrayEncode: return "DenseUint32GrayEncode";
    case WorldGenerationComputeKernel::DenseUint32GrayDecode: return "DenseUint32GrayDecode";
    case WorldGenerationComputeKernel::DenseUint32SmearLsb: return "DenseUint32SmearLsb";
    case WorldGenerationComputeKernel::DenseUint32ClearMsb: return "DenseUint32ClearMsb";
    case WorldGenerationComputeKernel::DenseUint32ByteXor: return "DenseUint32ByteXor";
    case WorldGenerationComputeKernel::DenseUint32SmearMsb: return "DenseUint32SmearMsb";
    case WorldGenerationComputeKernel::DenseUint32ByteOr: return "DenseUint32ByteOr";
    case WorldGenerationComputeKernel::DenseUint32ByteAnd: return "DenseUint32ByteAnd";
    case WorldGenerationComputeKernel::DenseUint32KeepOddBits: return "DenseUint32KeepOddBits";
    case WorldGenerationComputeKernel::DenseUint32KeepEvenBits: return "DenseUint32KeepEvenBits";
    case WorldGenerationComputeKernel::DenseUint32SwapAdjacentBits: return "DenseUint32SwapAdjacentBits";
    case WorldGenerationComputeKernel::DenseUint32CompressOddBits: return "DenseUint32CompressOddBits";
    case WorldGenerationComputeKernel::DenseUint32CompressEvenBits: return "DenseUint32CompressEvenBits";
    case WorldGenerationComputeKernel::DenseUint32ExpandLow16ToOdd: return "DenseUint32ExpandLow16ToOdd";
    case WorldGenerationComputeKernel::DenseUint32ExpandLow16ToEven: return "DenseUint32ExpandLow16ToEven";
    case WorldGenerationComputeKernel::DenseUint32InterleaveHalves: return "DenseUint32InterleaveHalves";
    case WorldGenerationComputeKernel::DenseUint32DeinterleaveHalves: return "DenseUint32DeinterleaveHalves";
    case WorldGenerationComputeKernel::DenseUint32NibbleXor: return "DenseUint32NibbleXor";
    case WorldGenerationComputeKernel::DenseUint32NibbleOr: return "DenseUint32NibbleOr";
    case WorldGenerationComputeKernel::DenseUint32NibbleAnd: return "DenseUint32NibbleAnd";
    case WorldGenerationComputeKernel::DenseUint32NibbleSum: return "DenseUint32NibbleSum";
    case WorldGenerationComputeKernel::DenseUint32NibbleMax: return "DenseUint32NibbleMax";
    case WorldGenerationComputeKernel::DenseUint32NibbleMin: return "DenseUint32NibbleMin";
    case WorldGenerationComputeKernel::DenseUint32ExtractNibble: return "DenseUint32ExtractNibble";
    case WorldGenerationComputeKernel::DenseUint32InsertNibble: return "DenseUint32InsertNibble";
    case WorldGenerationComputeKernel::DenseUint32BroadcastNibble: return "DenseUint32BroadcastNibble";
    case WorldGenerationComputeKernel::DenseUint32SwapAdjacentNibbles: return "DenseUint32SwapAdjacentNibbles";
    case WorldGenerationComputeKernel::DenseUint32RolNibbles: return "DenseUint32RolNibbles";
    case WorldGenerationComputeKernel::DenseUint32RorNibbles: return "DenseUint32RorNibbles";
    case WorldGenerationComputeKernel::DenseUint32HasZeroNibble: return "DenseUint32HasZeroNibble";
    case WorldGenerationComputeKernel::DenseUint32SextNibble: return "DenseUint32SextNibble";
    case WorldGenerationComputeKernel::DenseUint32CountZeroNibbles: return "DenseUint32CountZeroNibbles";
    case WorldGenerationComputeKernel::DenseUint32PackLowNibbles: return "DenseUint32PackLowNibbles";
    case WorldGenerationComputeKernel::DenseUint32PackHighNibbles: return "DenseUint32PackHighNibbles";
    case WorldGenerationComputeKernel::DenseUint32ExpandLowNibbles: return "DenseUint32ExpandLowNibbles";
    case WorldGenerationComputeKernel::DenseUint32ExpandHighNibbles: return "DenseUint32ExpandHighNibbles";
    case WorldGenerationComputeKernel::DenseUint32ReplaceNibble: return "DenseUint32ReplaceNibble";
    case WorldGenerationComputeKernel::DenseUint32MaskNibbles: return "DenseUint32MaskNibbles";
    case WorldGenerationComputeKernel::DenseUint32AllNibblesEqual: return "DenseUint32AllNibblesEqual";
    case WorldGenerationComputeKernel::DenseUint32NibbleRange: return "DenseUint32NibbleRange";
    case WorldGenerationComputeKernel::DenseUint32BroadcastNibbleAt: return "DenseUint32BroadcastNibbleAt";
    case WorldGenerationComputeKernel::DenseUint32HalfSum: return "DenseUint32HalfSum";
    case WorldGenerationComputeKernel::DenseUint32HalfMax: return "DenseUint32HalfMax";
    case WorldGenerationComputeKernel::DenseUint32HalfMin: return "DenseUint32HalfMin";
    case WorldGenerationComputeKernel::DenseUint32HalfXor: return "DenseUint32HalfXor";
    case WorldGenerationComputeKernel::DenseUint32HalfOr: return "DenseUint32HalfOr";
    case WorldGenerationComputeKernel::DenseUint32HalfAnd: return "DenseUint32HalfAnd";
    case WorldGenerationComputeKernel::DenseUint32BroadcastHalf: return "DenseUint32BroadcastHalf";
    case WorldGenerationComputeKernel::DenseUint32HalfEq: return "DenseUint32HalfEq";
    case WorldGenerationComputeKernel::DenseUint32HalfRange: return "DenseUint32HalfRange";
    case WorldGenerationComputeKernel::DenseUint32HalfLt: return "DenseUint32HalfLt";
    case WorldGenerationComputeKernel::DenseUint32HalfGt: return "DenseUint32HalfGt";
    case WorldGenerationComputeKernel::DenseUint32HalfNe: return "DenseUint32HalfNe";
    case WorldGenerationComputeKernel::DenseUint32HalfLeu: return "DenseUint32HalfLeu";
    case WorldGenerationComputeKernel::DenseUint32HalfGeu: return "DenseUint32HalfGeu";
    case WorldGenerationComputeKernel::DenseUint32HalfAvg: return "DenseUint32HalfAvg";
    case WorldGenerationComputeKernel::DenseUint32BroadcastHighHalf: return "DenseUint32BroadcastHighHalf";
    case WorldGenerationComputeKernel::DenseUint32HalfSub: return "DenseUint32HalfSub";
    case WorldGenerationComputeKernel::DenseUint32HalfMul: return "DenseUint32HalfMul";
    case WorldGenerationComputeKernel::DenseUint32HalfSatAdd: return "DenseUint32HalfSatAdd";
    case WorldGenerationComputeKernel::DenseUint32HalfSatSub: return "DenseUint32HalfSatSub";
    case WorldGenerationComputeKernel::DenseUint32HalfMulHi: return "DenseUint32HalfMulHi";
    case WorldGenerationComputeKernel::DenseUint32HalfDiv: return "DenseUint32HalfDiv";
    case WorldGenerationComputeKernel::DenseUint32HalfMod: return "DenseUint32HalfMod";
    case WorldGenerationComputeKernel::DenseUint32HalfCeilDiv: return "DenseUint32HalfCeilDiv";
    case WorldGenerationComputeKernel::DenseUint32HalfIsDivisible: return "DenseUint32HalfIsDivisible";
    case WorldGenerationComputeKernel::DenseUint32HalfGcd: return "DenseUint32HalfGcd";
    case WorldGenerationComputeKernel::DenseUint32HalfSelect: return "DenseUint32HalfSelect";
    case WorldGenerationComputeKernel::DenseUint32HalfLcm: return "DenseUint32HalfLcm";
    case WorldGenerationComputeKernel::DenseUint32HalfCoprime: return "DenseUint32HalfCoprime";
    case WorldGenerationComputeKernel::DenseUint32HalfAlignUp: return "DenseUint32HalfAlignUp";
    case WorldGenerationComputeKernel::DenseUint32HalfAlignDown: return "DenseUint32HalfAlignDown";
    case WorldGenerationComputeKernel::DenseUint32HalfRotl: return "DenseUint32HalfRotl";
    case WorldGenerationComputeKernel::DenseUint32HalfRotr: return "DenseUint32HalfRotr";
    case WorldGenerationComputeKernel::DenseUint32HalfPopcount: return "DenseUint32HalfPopcount";
    case WorldGenerationComputeKernel::DenseUint32HalfClz: return "DenseUint32HalfClz";
    case WorldGenerationComputeKernel::DenseUint32HalfCtz: return "DenseUint32HalfCtz";
    case WorldGenerationComputeKernel::DenseUint32HalfParity: return "DenseUint32HalfParity";
    case WorldGenerationComputeKernel::DenseUint32HalfBitReverse: return "DenseUint32HalfBitReverse";
    case WorldGenerationComputeKernel::DenseUint32HalfBswap: return "DenseUint32HalfBswap";
    case WorldGenerationComputeKernel::DenseUint32HalfNot: return "DenseUint32HalfNot";
    case WorldGenerationComputeKernel::DenseUint32HalfNeg: return "DenseUint32HalfNeg";
    case WorldGenerationComputeKernel::DenseUint32HalfAbs: return "DenseUint32HalfAbs";
    case WorldGenerationComputeKernel::DenseUint32HalfSignMask: return "DenseUint32HalfSignMask";
    case WorldGenerationComputeKernel::DenseUint32HalfIsZero: return "DenseUint32HalfIsZero";
    case WorldGenerationComputeKernel::DenseUint32HalfIsNonzero: return "DenseUint32HalfIsNonzero";
    case WorldGenerationComputeKernel::DenseUint32HalfClo: return "DenseUint32HalfClo";
    case WorldGenerationComputeKernel::DenseUint32HalfCto: return "DenseUint32HalfCto";
    case WorldGenerationComputeKernel::DenseUint32HalfIsPow2: return "DenseUint32HalfIsPow2";
    case WorldGenerationComputeKernel::DenseUint32HalfIlog2: return "DenseUint32HalfIlog2";
    case WorldGenerationComputeKernel::DenseUint32HalfNextPow2: return "DenseUint32HalfNextPow2";
    case WorldGenerationComputeKernel::DenseUint32HalfMaskLow: return "DenseUint32HalfMaskLow";
    case WorldGenerationComputeKernel::DenseUint32HalfMaskHigh: return "DenseUint32HalfMaskHigh";
    case WorldGenerationComputeKernel::DenseUint32HalfBitTest: return "DenseUint32HalfBitTest";
    case WorldGenerationComputeKernel::DenseUint32HalfIsolateLsb: return "DenseUint32HalfIsolateLsb";
    case WorldGenerationComputeKernel::DenseUint32HalfIsolateMsb: return "DenseUint32HalfIsolateMsb";
    case WorldGenerationComputeKernel::DenseUint32HalfClearLsb: return "DenseUint32HalfClearLsb";
    case WorldGenerationComputeKernel::DenseUint32HalfLsbMask: return "DenseUint32HalfLsbMask";
    case WorldGenerationComputeKernel::DenseUint32HalfGrayEncode: return "DenseUint32HalfGrayEncode";
    case WorldGenerationComputeKernel::DenseUint32HalfGrayDecode: return "DenseUint32HalfGrayDecode";
    case WorldGenerationComputeKernel::DenseUint32HalfSmearLsb: return "DenseUint32HalfSmearLsb";
    case WorldGenerationComputeKernel::DenseUint32HalfClearMsb: return "DenseUint32HalfClearMsb";
    case WorldGenerationComputeKernel::DenseUint32HalfSmearMsb: return "DenseUint32HalfSmearMsb";
    case WorldGenerationComputeKernel::DenseUint32HalfKeepOddBits: return "DenseUint32HalfKeepOddBits";
    case WorldGenerationComputeKernel::DenseUint32HalfKeepEvenBits: return "DenseUint32HalfKeepEvenBits";
    case WorldGenerationComputeKernel::DenseUint32HalfSwapAdjacentBits: return "DenseUint32HalfSwapAdjacentBits";
    case WorldGenerationComputeKernel::DenseUint32HalfCompressOddBits: return "DenseUint32HalfCompressOddBits";
    case WorldGenerationComputeKernel::DenseUint32HalfCompressEvenBits: return "DenseUint32HalfCompressEvenBits";
    case WorldGenerationComputeKernel::DenseUint32HalfExpandLow8ToOdd: return "DenseUint32HalfExpandLow8ToOdd";
    case WorldGenerationComputeKernel::DenseUint32HalfExpandLow8ToEven: return "DenseUint32HalfExpandLow8ToEven";
    case WorldGenerationComputeKernel::DenseUint32HalfInterleaveBytes: return "DenseUint32HalfInterleaveBytes";
    case WorldGenerationComputeKernel::DenseUint32HalfDeinterleaveBytes: return "DenseUint32HalfDeinterleaveBytes";
    case WorldGenerationComputeKernel::DenseUint32HalfSwapBytes: return "DenseUint32HalfSwapBytes";
    case WorldGenerationComputeKernel::DenseUint32HalfNibbleXor: return "DenseUint32HalfNibbleXor";
    case WorldGenerationComputeKernel::DenseUint32HalfNibbleOr: return "DenseUint32HalfNibbleOr";
    case WorldGenerationComputeKernel::DenseUint32HalfNibbleAnd: return "DenseUint32HalfNibbleAnd";
    case WorldGenerationComputeKernel::DenseUint32HalfNibbleSum: return "DenseUint32HalfNibbleSum";
    case WorldGenerationComputeKernel::DenseUint32HalfNibbleMax: return "DenseUint32HalfNibbleMax";
    case WorldGenerationComputeKernel::DenseUint32HalfNibbleMin: return "DenseUint32HalfNibbleMin";
    case WorldGenerationComputeKernel::DenseUint32HalfSwapAdjacentNibbles: return "DenseUint32HalfSwapAdjacentNibbles";
    case WorldGenerationComputeKernel::DenseUint32HalfRolNibbles: return "DenseUint32HalfRolNibbles";
    case WorldGenerationComputeKernel::DenseUint32HalfRorNibbles: return "DenseUint32HalfRorNibbles";
    case WorldGenerationComputeKernel::DenseUint32HalfHasZeroNibble: return "DenseUint32HalfHasZeroNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfCountZeroNibbles: return "DenseUint32HalfCountZeroNibbles";
    case WorldGenerationComputeKernel::DenseUint32HalfBroadcastNibble: return "DenseUint32HalfBroadcastNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfSextNibble: return "DenseUint32HalfSextNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfExtractNibble: return "DenseUint32HalfExtractNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfInsertNibble: return "DenseUint32HalfInsertNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfAllNibblesEqual: return "DenseUint32HalfAllNibblesEqual";
    case WorldGenerationComputeKernel::DenseUint32HalfNibbleRange: return "DenseUint32HalfNibbleRange";
    case WorldGenerationComputeKernel::DenseUint32HalfMaskNibbles: return "DenseUint32HalfMaskNibbles";
    case WorldGenerationComputeKernel::DenseUint32HalfBroadcastNibbleAt: return "DenseUint32HalfBroadcastNibbleAt";
    case WorldGenerationComputeKernel::DenseUint32HalfReplaceNibble: return "DenseUint32HalfReplaceNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfReverseNibbles: return "DenseUint32HalfReverseNibbles";
    case WorldGenerationComputeKernel::DenseUint32HalfWidenNibbles: return "DenseUint32HalfWidenNibbles";
    case WorldGenerationComputeKernel::DenseUint32HalfDupNibbles: return "DenseUint32HalfDupNibbles";
    case WorldGenerationComputeKernel::DenseUint32HalfFoldAdjacentNibbles: return "DenseUint32HalfFoldAdjacentNibbles";
    case WorldGenerationComputeKernel::DenseUint32HalfCountNonzeroNibbles: return "DenseUint32HalfCountNonzeroNibbles";
    case WorldGenerationComputeKernel::DenseUint32HalfAddNibble: return "DenseUint32HalfAddNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfSatAddNibble: return "DenseUint32HalfSatAddNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfSubNibble: return "DenseUint32HalfSubNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfSatSubNibble: return "DenseUint32HalfSatSubNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfMulNibble: return "DenseUint32HalfMulNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfSatMulNibble: return "DenseUint32HalfSatMulNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfDivNibble: return "DenseUint32HalfDivNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfModNibble: return "DenseUint32HalfModNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfMinNibble: return "DenseUint32HalfMinNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfMaxNibble: return "DenseUint32HalfMaxNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfClampNibble: return "DenseUint32HalfClampNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfAndNibble: return "DenseUint32HalfAndNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfOrNibble: return "DenseUint32HalfOrNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfXorNibble: return "DenseUint32HalfXorNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfShlNibble: return "DenseUint32HalfShlNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfShrNibble: return "DenseUint32HalfShrNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfRolNibble: return "DenseUint32HalfRolNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfRorNibble: return "DenseUint32HalfRorNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfNotNibble: return "DenseUint32HalfNotNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfNegNibble: return "DenseUint32HalfNegNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfAbsNibble: return "DenseUint32HalfAbsNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfSignMaskNibble: return "DenseUint32HalfSignMaskNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfIsZeroNibble: return "DenseUint32HalfIsZeroNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfIsNonzeroNibble: return "DenseUint32HalfIsNonzeroNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfEqNibble: return "DenseUint32HalfEqNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfNeNibble: return "DenseUint32HalfNeNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfLtNibble: return "DenseUint32HalfLtNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfLeNibble: return "DenseUint32HalfLeNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfGtNibble: return "DenseUint32HalfGtNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfGeNibble: return "DenseUint32HalfGeNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfBetweenNibble: return "DenseUint32HalfBetweenNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfOutsideNibble: return "DenseUint32HalfOutsideNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfPopcountNibble: return "DenseUint32HalfPopcountNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfParityNibble: return "DenseUint32HalfParityNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfReverseBitsNibble: return "DenseUint32HalfReverseBitsNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfIsolateLsbNibble: return "DenseUint32HalfIsolateLsbNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfClearLsbNibble: return "DenseUint32HalfClearLsbNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfSmearLsbNibble: return "DenseUint32HalfSmearLsbNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfIsolateMsbNibble: return "DenseUint32HalfIsolateMsbNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfClearMsbNibble: return "DenseUint32HalfClearMsbNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfSmearMsbNibble: return "DenseUint32HalfSmearMsbNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfLsbMaskNibble: return "DenseUint32HalfLsbMaskNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfKeepOddBitsNibble: return "DenseUint32HalfKeepOddBitsNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfKeepEvenBitsNibble: return "DenseUint32HalfKeepEvenBitsNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfSwapAdjacentBitsNibble: return "DenseUint32HalfSwapAdjacentBitsNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfGrayEncodeNibble: return "DenseUint32HalfGrayEncodeNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfGrayDecodeNibble: return "DenseUint32HalfGrayDecodeNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfClzNibble: return "DenseUint32HalfClzNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfCtzNibble: return "DenseUint32HalfCtzNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfCloNibble: return "DenseUint32HalfCloNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfCtoNibble: return "DenseUint32HalfCtoNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfBitLengthNibble: return "DenseUint32HalfBitLengthNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfIsPowerOfTwoNibble: return "DenseUint32HalfIsPowerOfTwoNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfNextPow2Nibble: return "DenseUint32HalfNextPow2Nibble";
    case WorldGenerationComputeKernel::DenseUint32HalfPrevPow2Nibble: return "DenseUint32HalfPrevPow2Nibble";
    case WorldGenerationComputeKernel::DenseUint32HalfBroadcastLsbNibble: return "DenseUint32HalfBroadcastLsbNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfBroadcastMsbNibble: return "DenseUint32HalfBroadcastMsbNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfParityFillNibble: return "DenseUint32HalfParityFillNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfRotl1Nibble: return "DenseUint32HalfRotl1Nibble";
    case WorldGenerationComputeKernel::DenseUint32HalfRotr1Nibble: return "DenseUint32HalfRotr1Nibble";
    case WorldGenerationComputeKernel::DenseUint32HalfNegateNibble: return "DenseUint32HalfNegateNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfIncWrapNibble: return "DenseUint32HalfIncWrapNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfDecWrapNibble: return "DenseUint32HalfDecWrapNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfNandNibble: return "DenseUint32HalfNandNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfNorNibble: return "DenseUint32HalfNorNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfXnorNibble: return "DenseUint32HalfXnorNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfMajNibble: return "DenseUint32HalfMajNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfMuxNibble: return "DenseUint32HalfMuxNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfImpliesNibble: return "DenseUint32HalfImpliesNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfNeqFillNibble: return "DenseUint32HalfNeqFillNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfAbsDiffNibble: return "DenseUint32HalfAbsDiffNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfEqFillNibble: return "DenseUint32HalfEqFillNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfOddFillNibble: return "DenseUint32HalfOddFillNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfEvenFillNibble: return "DenseUint32HalfEvenFillNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfLtFillNibble: return "DenseUint32HalfLtFillNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfLeFillNibble: return "DenseUint32HalfLeFillNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfGtFillNibble: return "DenseUint32HalfGtFillNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfGeFillNibble: return "DenseUint32HalfGeFillNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfSatIncNibble: return "DenseUint32HalfSatIncNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfSatDecNibble: return "DenseUint32HalfSatDecNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfRotl2Nibble: return "DenseUint32HalfRotl2Nibble";
    case WorldGenerationComputeKernel::DenseUint32HalfRotl3Nibble: return "DenseUint32HalfRotl3Nibble";
    case WorldGenerationComputeKernel::DenseUint32HalfAvgNibble: return "DenseUint32HalfAvgNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfMidNibble: return "DenseUint32HalfMidNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfMin3Nibble: return "DenseUint32HalfMin3Nibble";
    case WorldGenerationComputeKernel::DenseUint32HalfMax3Nibble: return "DenseUint32HalfMax3Nibble";
    case WorldGenerationComputeKernel::DenseUint32HalfRange3Nibble: return "DenseUint32HalfRange3Nibble";
    case WorldGenerationComputeKernel::DenseUint32HalfSum3Nibble: return "DenseUint32HalfSum3Nibble";
    case WorldGenerationComputeKernel::DenseUint32HalfSatSum3Nibble: return "DenseUint32HalfSatSum3Nibble";
    case WorldGenerationComputeKernel::DenseUint32HalfAvg3Nibble: return "DenseUint32HalfAvg3Nibble";
    case WorldGenerationComputeKernel::DenseUint32HalfXor3Nibble: return "DenseUint32HalfXor3Nibble";
    case WorldGenerationComputeKernel::DenseUint32HalfMulAddNibble: return "DenseUint32HalfMulAddNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfSatMulAddNibble: return "DenseUint32HalfSatMulAddNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfMulSubNibble: return "DenseUint32HalfMulSubNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfEqual3FillNibble: return "DenseUint32HalfEqual3FillNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfNeq3FillNibble: return "DenseUint32HalfNeq3FillNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfAnyEqFillNibble: return "DenseUint32HalfAnyEqFillNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfAllDistinctFillNibble: return "DenseUint32HalfAllDistinctFillNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfBlendNibble: return "DenseUint32HalfBlendNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfWeightedAddNibble: return "DenseUint32HalfWeightedAddNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfBitMergeNibble: return "DenseUint32HalfBitMergeNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfSatWeightedAddNibble: return "DenseUint32HalfSatWeightedAddNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfMaskSelectNibble: return "DenseUint32HalfMaskSelectNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfIfEqPickNibble: return "DenseUint32HalfIfEqPickNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfIfNePickNibble: return "DenseUint32HalfIfNePickNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfThresholdPickNibble: return "DenseUint32HalfThresholdPickNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfModMulNibble: return "DenseUint32HalfModMulNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfModAddNibble: return "DenseUint32HalfModAddNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfModSubNibble: return "DenseUint32HalfModSubNibble";
    case WorldGenerationComputeKernel::DenseUint32HalfCeilDivNibble: return "DenseUint32HalfCeilDivNibble";
  }
  return nullptr;
}

Maybe<WorldGenerationComputeKernel> WorldGenerationComputeService::parseKernelName(
    char const* name) noexcept {
  if (!name)
    return {};
  for (auto kernel : supportedKernels()) {
    char const* label = kernelName(kernel);
    if (label && std::strcmp(label, name) == 0)
      return kernel;
  }
  return {};
}

char const* WorldGenerationComputeService::backendName(
    WorldGenerationComputeBackend backend) noexcept {
  switch (backend) {
    case WorldGenerationComputeBackend::CpuReference: return "CpuReference";
    case WorldGenerationComputeBackend::GpuOpenGlCompute: return "GpuOpenGlCompute";
  }
  return nullptr;
}

Maybe<WorldGenerationComputeBackend> WorldGenerationComputeService::parseBackendName(
    char const* name) noexcept {
  if (!name)
    return {};
  if (std::strcmp(name, "CpuReference") == 0)
    return WorldGenerationComputeBackend::CpuReference;
  if (std::strcmp(name, "GpuOpenGlCompute") == 0)
    return WorldGenerationComputeBackend::GpuOpenGlCompute;
  return {};
}

char const* WorldGenerationComputeService::pollStatusName(
    WorldGenerationComputePollStatus status) noexcept {
  switch (status) {
    case WorldGenerationComputePollStatus::Pending: return "Pending";
    case WorldGenerationComputePollStatus::Ready: return "Ready";
    case WorldGenerationComputePollStatus::Failed: return "Failed";
    case WorldGenerationComputePollStatus::Cancelled: return "Cancelled";
    case WorldGenerationComputePollStatus::UnknownTicket: return "UnknownTicket";
  }
  return nullptr;
}

char const* WorldGenerationComputeService::shadowVerdictName(
    WorldGenerationComputeShadowVerdict verdict) noexcept {
  switch (verdict) {
    case WorldGenerationComputeShadowVerdict::Equal: return "Equal";
    case WorldGenerationComputeShadowVerdict::Diverged: return "Diverged";
    case WorldGenerationComputeShadowVerdict::PrimaryFailed: return "PrimaryFailed";
    case WorldGenerationComputeShadowVerdict::ShadowUnavailable: return "ShadowUnavailable";
    case WorldGenerationComputeShadowVerdict::Cancelled: return "Cancelled";
  }
  return nullptr;
}

char const* WorldGenerationComputeService::commitVerdictName(
    WorldGenerationComputeCommitVerdict verdict) noexcept {
  switch (verdict) {
    case WorldGenerationComputeCommitVerdict::Accepted: return "Accepted";
    case WorldGenerationComputeCommitVerdict::StaleIdentity: return "StaleIdentity";
    case WorldGenerationComputeCommitVerdict::StaleInput: return "StaleInput";
    case WorldGenerationComputeCommitVerdict::NotReady: return "NotReady";
    case WorldGenerationComputeCommitVerdict::Cancelled: return "Cancelled";
    case WorldGenerationComputeCommitVerdict::Failed: return "Failed";
    case WorldGenerationComputeCommitVerdict::UnknownTicket: return "UnknownTicket";
  }
  return nullptr;
}

namespace {

uint32_t denseClz(uint32_t value) noexcept {
  if (value == 0u)
    return 32u;
  uint32_t n = 0u;
  if (value <= 0x0000ffffu) {
    n += 16u;
    value <<= 16u;
  }
  if (value <= 0x00ffffffu) {
    n += 8u;
    value <<= 8u;
  }
  if (value <= 0x0fffffffu) {
    n += 4u;
    value <<= 4u;
  }
  if (value <= 0x3fffffffu) {
    n += 2u;
    value <<= 2u;
  }
  if (value <= 0x7fffffffu)
    n += 1u;
  return n;
}

uint32_t denseCtz(uint32_t value) noexcept {
  if (value == 0u)
    return 32u;
  uint32_t n = 0u;
  if ((value & 0x0000ffffu) == 0u) {
    n += 16u;
    value >>= 16u;
  }
  if ((value & 0x000000ffu) == 0u) {
    n += 8u;
    value >>= 8u;
  }
  if ((value & 0x0000000fu) == 0u) {
    n += 4u;
    value >>= 4u;
  }
  if ((value & 0x00000003u) == 0u) {
    n += 2u;
    value >>= 2u;
  }
  if ((value & 0x00000001u) == 0u)
    n += 1u;
  return n;
}

uint32_t densePopcount(uint32_t value) noexcept {
  value = value - ((value >> 1u) & 0x55555555u);
  value = (value & 0x33333333u) + ((value >> 2u) & 0x33333333u);
  value = (value + (value >> 4u)) & 0x0f0f0f0fu;
  value = value + (value >> 8u);
  value = value + (value >> 16u);
  return value & 0x3fu;
}

uint32_t denseBitReverse(uint32_t value) noexcept {
  value = ((value >> 1u) & 0x55555555u) | ((value & 0x55555555u) << 1u);
  value = ((value >> 2u) & 0x33333333u) | ((value & 0x33333333u) << 2u);
  value = ((value >> 4u) & 0x0f0f0f0fu) | ((value & 0x0f0f0f0fu) << 4u);
  value = ((value >> 8u) & 0x00ff00ffu) | ((value & 0x00ff00ffu) << 8u);
  return (value >> 16u) | (value << 16u);
}

}

uint64_t WorldGenerationComputeService::bufferBytes(List<uint32_t> const& values) noexcept {
  return (uint64_t)values.size() * sizeof(uint32_t);
}

List<uint32_t> WorldGenerationComputeService::runKernel(WorldGenerationComputeKernel kernel,
    uint32_t kernelVersion, List<uint32_t> const& input, uint32_t parameter) {
  List<uint32_t> out;
  if (!supported(kernel, kernelVersion))
    return out;
  out.reserve(input.size());
  if (kernel == WorldGenerationComputeKernel::DenseUint32Xor) {
    for (uint32_t value : input)
      out.append(value ^ parameter);
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32Add) {
    for (uint32_t value : input)
      out.append(value + parameter);
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32And) {
    for (uint32_t value : input)
      out.append(value & parameter);
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32Or) {
    for (uint32_t value : input)
      out.append(value | parameter);
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32Mul) {
    for (uint32_t value : input)
      out.append(value * parameter);
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32Min) {
    for (uint32_t value : input)
      out.append(value < parameter ? value : parameter);
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32Max) {
    for (uint32_t value : input)
      out.append(value > parameter ? value : parameter);
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32Sub) {
    for (uint32_t value : input)
      out.append(value - parameter);
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32Shl) {
    uint32_t shift = parameter & 31u;
    for (uint32_t value : input)
      out.append(value << shift);
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32Shr) {
    uint32_t shift = parameter & 31u;
    for (uint32_t value : input)
      out.append(value >> shift);
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32Rotl) {
    uint32_t rot = parameter & 31u;
    for (uint32_t value : input)
      out.append((value << rot) | (value >> ((32u - rot) & 31u)));
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32Rotr) {
    uint32_t rot = parameter & 31u;
    for (uint32_t value : input)
      out.append((value >> rot) | (value << ((32u - rot) & 31u)));
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32Not) {
    for (uint32_t value : input)
      out.append(~value);
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32Clz) {
    for (uint32_t value : input)
      out.append(denseClz(value));
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32Ctz) {
    for (uint32_t value : input)
      out.append(denseCtz(value));
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32Popcount) {
    for (uint32_t value : input)
      out.append(densePopcount(value));
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32Eq) {
    for (uint32_t value : input)
      out.append(value == parameter ? 0xffffffffu : 0u);
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32Ne) {
    for (uint32_t value : input)
      out.append(value != parameter ? 0xffffffffu : 0u);
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32BitReverse) {
    for (uint32_t value : input)
      out.append(denseBitReverse(value));
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32Lt) {
    for (uint32_t value : input)
      out.append(value < parameter ? 0xffffffffu : 0u);
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32Leu) {
    for (uint32_t value : input)
      out.append(value <= parameter ? 0xffffffffu : 0u);
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32AbsDiff) {
    for (uint32_t value : input)
      out.append(value > parameter ? value - parameter : parameter - value);
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32Gt) {
    for (uint32_t value : input)
      out.append(value > parameter ? 0xffffffffu : 0u);
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32Geu) {
    for (uint32_t value : input)
      out.append(value >= parameter ? 0xffffffffu : 0u);
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32Xnor) {
    for (uint32_t value : input)
      out.append(~(value ^ parameter));
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32Nand) {
    for (uint32_t value : input)
      out.append(~(value & parameter));
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32Nor) {
    for (uint32_t value : input)
      out.append(~(value | parameter));
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32MulHi) {
    for (uint32_t value : input)
      out.append((uint32_t)(((uint64_t)value * (uint64_t)parameter) >> 32));
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32AndNot) {
    for (uint32_t value : input)
      out.append(value & ~parameter);
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32SatAdd) {
    for (uint32_t value : input) {
      uint64_t sum = (uint64_t)value + (uint64_t)parameter;
      out.append(sum > 0xffffffffull ? 0xffffffffu : (uint32_t)sum);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32SatSub) {
    for (uint32_t value : input)
      out.append(value > parameter ? value - parameter : 0u);
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32OrNot) {
    for (uint32_t value : input)
      out.append(value | ~parameter);
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32Avg) {
    for (uint32_t value : input)
      out.append((uint32_t)(((uint64_t)value + (uint64_t)parameter) >> 1));
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32IsPow2) {
    for (uint32_t value : input) {
      bool pow2 = value != 0u && (value & (value - 1u)) == 0u;
      out.append(pow2 ? 0xffffffffu : 0u);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32Bswap) {
    for (uint32_t value : input) {
      uint32_t swapped = ((value & 0x000000ffu) << 24) | ((value & 0x0000ff00u) << 8)
          | ((value & 0x00ff0000u) >> 8) | ((value & 0xff000000u) >> 24);
      out.append(swapped);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32Parity) {
    for (uint32_t value : input)
      out.append(densePopcount(value) & 1u);
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32BitTest) {
    uint32_t bit = parameter & 31u;
    for (uint32_t value : input)
      out.append(((value >> bit) & 1u) ? 0xffffffffu : 0u);
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HasZeroByte) {
    for (uint32_t value : input) {
      bool hasZero = ((value - 0x01010101u) & ~value & 0x80808080u) != 0u;
      out.append(hasZero ? 0xffffffffu : 0u);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32NextPow2) {
    for (uint32_t value : input) {
      if (value == 0u) {
        out.append(0u);
        continue;
      }
      uint32_t v = value - 1u;
      v |= v >> 1;
      v |= v >> 2;
      v |= v >> 4;
      v |= v >> 8;
      v |= v >> 16;
      out.append(v + 1u);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32MaskLow) {
    uint32_t bits = parameter > 32u ? 32u : parameter;
    uint32_t mask = bits == 32u ? 0xffffffffu : (bits == 0u ? 0u : ((1u << bits) - 1u));
    for (uint32_t value : input)
      out.append(value & mask);
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32Ilog2) {
    for (uint32_t value : input)
      out.append(value == 0u ? 0u : (31u - denseClz(value)));
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32BroadcastByte) {
    for (uint32_t value : input) {
      uint32_t b = value & 0xffu;
      out.append(b * 0x01010101u);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32ByteSum) {
    for (uint32_t value : input) {
      uint32_t sum = (value & 0xffu) + ((value >> 8) & 0xffu) + ((value >> 16) & 0xffu)
          + ((value >> 24) & 0xffu);
      out.append(sum);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32Sext8) {
    for (uint32_t value : input) {
      uint32_t low = value & 0xffu;
      out.append((low & 0x80u) ? (low | 0xffffff00u) : low);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32Sext16) {
    for (uint32_t value : input) {
      uint32_t low = value & 0xffffu;
      out.append((low & 0x8000u) ? (low | 0xffff0000u) : low);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32ExtractByte) {
    uint32_t shift = (parameter & 3u) * 8u;
    for (uint32_t value : input)
      out.append((value >> shift) & 0xffu);
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32BitSet) {
    uint32_t mask = 1u << (parameter & 31u);
    for (uint32_t value : input)
      out.append(value | mask);
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32BitClear) {
    uint32_t mask = 1u << (parameter & 31u);
    for (uint32_t value : input)
      out.append(value & ~mask);
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32BitToggle) {
    uint32_t mask = 1u << (parameter & 31u);
    for (uint32_t value : input)
      out.append(value ^ mask);
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32Neg) {
    for (uint32_t value : input)
      out.append((uint32_t)(0u - value));
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32Abs) {
    for (uint32_t value : input) {
      // Treat value as int32: abs(INT_MIN) stays 0x80000000.
      out.append((value & 0x80000000u) ? (uint32_t)(0u - value) : value);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32RolBytes) {
    uint32_t shift = (parameter & 3u) * 8u;
    for (uint32_t value : input) {
      if (shift == 0u)
        out.append(value);
      else
        out.append((value << shift) | (value >> (32u - shift)));
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32RorBytes) {
    uint32_t shift = (parameter & 3u) * 8u;
    for (uint32_t value : input) {
      if (shift == 0u)
        out.append(value);
      else
        out.append((value >> shift) | (value << (32u - shift)));
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32SignMask) {
    for (uint32_t value : input)
      out.append((value & 0x80000000u) ? 0xffffffffu : 0u);
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32IsZero) {
    for (uint32_t value : input)
      out.append(value == 0u ? 0xffffffffu : 0u);
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32IsNonzero) {
    for (uint32_t value : input)
      out.append(value != 0u ? 0xffffffffu : 0u);
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32MaskHigh) {
    uint32_t bits = parameter > 32u ? 32u : parameter;
    uint32_t mask = bits == 0u ? 0u
                               : (bits == 32u ? 0xffffffffu : (0xffffffffu << (32u - bits)));
    for (uint32_t value : input)
      out.append(value & mask);
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32SatMul) {
    for (uint32_t value : input) {
      uint64_t product = (uint64_t)value * (uint64_t)parameter;
      out.append(product > 0xffffffffull ? 0xffffffffu : (uint32_t)product);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32SwapHalves) {
    for (uint32_t value : input)
      out.append((value << 16) | (value >> 16));
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32Clo) {
    for (uint32_t value : input)
      out.append(denseClz(~value));
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32Cto) {
    for (uint32_t value : input)
      out.append(denseCtz(~value));
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32ReverseNibbles) {
    for (uint32_t value : input) {
      // Byte-swap then swap the two nibbles inside each byte => reverse nibble order.
      uint32_t v = ((value & 0x000000ffu) << 24) | ((value & 0x0000ff00u) << 8)
          | ((value & 0x00ff0000u) >> 8) | ((value & 0xff000000u) >> 24);
      v = ((v & 0x0f0f0f0fu) << 4) | ((v & 0xf0f0f0f0u) >> 4);
      out.append(v);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32ByteMax) {
    for (uint32_t value : input) {
      uint32_t b0 = value & 0xffu;
      uint32_t b1 = (value >> 8) & 0xffu;
      uint32_t b2 = (value >> 16) & 0xffu;
      uint32_t b3 = (value >> 24) & 0xffu;
      uint32_t m = b0 > b1 ? b0 : b1;
      m = m > b2 ? m : b2;
      m = m > b3 ? m : b3;
      out.append(m);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32ByteMin) {
    for (uint32_t value : input) {
      uint32_t b0 = value & 0xffu;
      uint32_t b1 = (value >> 8) & 0xffu;
      uint32_t b2 = (value >> 16) & 0xffu;
      uint32_t b3 = (value >> 24) & 0xffu;
      uint32_t m = b0 < b1 ? b0 : b1;
      m = m < b2 ? m : b2;
      m = m < b3 ? m : b3;
      out.append(m);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32DupLow16) {
    for (uint32_t value : input) {
      uint32_t low = value & 0xffffu;
      out.append(low | (low << 16));
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32DupHigh16) {
    for (uint32_t value : input) {
      uint32_t high = value & 0xffff0000u;
      out.append(high | (high >> 16));
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32ReverseBitsInBytes) {
    for (uint32_t value : input) {
      uint32_t v = value;
      v = ((v >> 1) & 0x55555555u) | ((v & 0x55555555u) << 1);
      v = ((v >> 2) & 0x33333333u) | ((v & 0x33333333u) << 2);
      v = ((v >> 4) & 0x0f0f0f0fu) | ((v & 0x0f0f0f0fu) << 4);
      out.append(v);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32IsolateLsb) {
    for (uint32_t value : input)
      out.append(value & (uint32_t)(0u - value));
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32IsolateMsb) {
    for (uint32_t value : input) {
      if (value == 0u)
        out.append(0u);
      else
        out.append(1u << (31u - denseClz(value)));
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32LsbMask) {
    for (uint32_t value : input)
      out.append(value ^ (value - 1u));
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32ClearLsb) {
    for (uint32_t value : input)
      out.append(value & (value - 1u));
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32GrayEncode) {
    for (uint32_t value : input)
      out.append(value ^ (value >> 1));
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32GrayDecode) {
    for (uint32_t value : input) {
      uint32_t v = value;
      v ^= v >> 1;
      v ^= v >> 2;
      v ^= v >> 4;
      v ^= v >> 8;
      v ^= v >> 16;
      out.append(v);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32SmearLsb) {
    for (uint32_t value : input)
      out.append(value | (value - 1u));
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32ClearMsb) {
    for (uint32_t value : input) {
      if (value == 0u)
        out.append(0u);
      else
        out.append(value ^ (1u << (31u - denseClz(value))));
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32ByteXor) {
    for (uint32_t value : input) {
      uint32_t x = value ^ (value >> 8) ^ (value >> 16) ^ (value >> 24);
      out.append(x & 0xffu);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32SmearMsb) {
    for (uint32_t value : input) {
      uint32_t v = value;
      v |= v >> 1;
      v |= v >> 2;
      v |= v >> 4;
      v |= v >> 8;
      v |= v >> 16;
      out.append(v);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32ByteOr) {
    for (uint32_t value : input) {
      uint32_t x = value | (value >> 8) | (value >> 16) | (value >> 24);
      out.append(x & 0xffu);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32ByteAnd) {
    for (uint32_t value : input) {
      uint32_t x = value & (value >> 8) & (value >> 16) & (value >> 24);
      out.append(x & 0xffu);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32KeepOddBits) {
    for (uint32_t value : input)
      out.append(value & 0x55555555u);
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32KeepEvenBits) {
    for (uint32_t value : input)
      out.append(value & 0xaaaaaaaau);
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32SwapAdjacentBits) {
    for (uint32_t value : input) {
      uint32_t odd = (value & 0x55555555u) << 1;
      uint32_t even = (value & 0xaaaaaaaau) >> 1;
      out.append(odd | even);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32CompressOddBits) {
    for (uint32_t value : input) {
      uint32_t x = value & 0x55555555u;
      x = (x | (x >> 1)) & 0x33333333u;
      x = (x | (x >> 2)) & 0x0f0f0f0fu;
      x = (x | (x >> 4)) & 0x00ff00ffu;
      x = (x | (x >> 8)) & 0x0000ffffu;
      out.append(x);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32CompressEvenBits) {
    for (uint32_t value : input) {
      uint32_t x = (value >> 1) & 0x55555555u;
      x = (x | (x >> 1)) & 0x33333333u;
      x = (x | (x >> 2)) & 0x0f0f0f0fu;
      x = (x | (x >> 4)) & 0x00ff00ffu;
      x = (x | (x >> 8)) & 0x0000ffffu;
      out.append(x);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32ExpandLow16ToOdd) {
    for (uint32_t value : input) {
      uint32_t x = value & 0xffffu;
      x = (x | (x << 8)) & 0x00ff00ffu;
      x = (x | (x << 4)) & 0x0f0f0f0fu;
      x = (x | (x << 2)) & 0x33333333u;
      x = (x | (x << 1)) & 0x55555555u;
      out.append(x);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32ExpandLow16ToEven) {
    for (uint32_t value : input) {
      uint32_t x = value & 0xffffu;
      x = (x | (x << 8)) & 0x00ff00ffu;
      x = (x | (x << 4)) & 0x0f0f0f0fu;
      x = (x | (x << 2)) & 0x33333333u;
      x = (x | (x << 1)) & 0x55555555u;
      out.append(x << 1);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32InterleaveHalves) {
    for (uint32_t value : input) {
      auto expandOdd = [](uint32_t x) {
        x &= 0xffffu;
        x = (x | (x << 8)) & 0x00ff00ffu;
        x = (x | (x << 4)) & 0x0f0f0f0fu;
        x = (x | (x << 2)) & 0x33333333u;
        x = (x | (x << 1)) & 0x55555555u;
        return x;
      };
      uint32_t lo = expandOdd(value);
      uint32_t hi = expandOdd(value >> 16);
      out.append((hi << 1) | lo);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32DeinterleaveHalves) {
    for (uint32_t value : input) {
      auto compressOdd = [](uint32_t x) {
        x &= 0x55555555u;
        x = (x | (x >> 1)) & 0x33333333u;
        x = (x | (x >> 2)) & 0x0f0f0f0fu;
        x = (x | (x >> 4)) & 0x00ff00ffu;
        x = (x | (x >> 8)) & 0x0000ffffu;
        return x;
      };
      uint32_t lo = compressOdd(value);
      uint32_t hi = compressOdd(value >> 1);
      out.append((hi << 16) | lo);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32NibbleXor) {
    for (uint32_t value : input) {
      uint32_t x = value;
      x ^= x >> 4;
      x ^= x >> 8;
      x ^= x >> 16;
      out.append(x & 0xfu);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32NibbleOr) {
    for (uint32_t value : input) {
      uint32_t x = value;
      x |= x >> 4;
      x |= x >> 8;
      x |= x >> 16;
      out.append(x & 0xfu);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32NibbleAnd) {
    for (uint32_t value : input) {
      uint32_t even = value & 0x0f0f0f0fu;
      uint32_t odd = (value >> 4) & 0x0f0f0f0fu;
      uint32_t m = even & odd;
      m &= m >> 8;
      m &= m >> 16;
      out.append(m & 0xfu);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32NibbleSum) {
    for (uint32_t value : input) {
      uint32_t x = (value & 0x0f0f0f0fu) + ((value >> 4) & 0x0f0f0f0fu);
      x = (x & 0xffu) + ((x >> 8) & 0xffu) + ((x >> 16) & 0xffu) + ((x >> 24) & 0xffu);
      out.append(x);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32NibbleMax) {
    for (uint32_t value : input) {
      uint32_t m = value & 0xfu;
      for (uint32_t shift = 4; shift < 32; shift += 4) {
        uint32_t n = (value >> shift) & 0xfu;
        if (n > m)
          m = n;
      }
      out.append(m);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32NibbleMin) {
    for (uint32_t value : input) {
      uint32_t m = value & 0xfu;
      for (uint32_t shift = 4; shift < 32; shift += 4) {
        uint32_t n = (value >> shift) & 0xfu;
        if (n < m)
          m = n;
      }
      out.append(m);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32ExtractNibble) {
    uint32_t shift = (parameter & 7u) * 4u;
    for (uint32_t value : input)
      out.append((value >> shift) & 0xfu);
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32InsertNibble) {
    uint32_t idx = parameter & 7u;
    uint32_t nib = (parameter >> 8) & 0xfu;
    uint32_t shift = idx * 4u;
    uint32_t mask = ~(0xfu << shift);
    for (uint32_t value : input)
      out.append((value & mask) | (nib << shift));
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32BroadcastNibble) {
    for (uint32_t value : input) {
      uint32_t n = value & 0xfu;
      out.append(n * 0x11111111u);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32SwapAdjacentNibbles) {
    for (uint32_t value : input) {
      uint32_t low = (value & 0x0f0f0f0fu) << 4;
      uint32_t high = (value & 0xf0f0f0f0u) >> 4;
      out.append(low | high);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32RolNibbles) {
    uint32_t rot = (parameter & 7u) * 4u;
    for (uint32_t value : input)
      out.append((value << rot) | (value >> ((32u - rot) & 31u)));
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32RorNibbles) {
    uint32_t rot = (parameter & 7u) * 4u;
    for (uint32_t value : input)
      out.append((value >> rot) | (value << ((32u - rot) & 31u)));
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HasZeroNibble) {
    for (uint32_t value : input) {
      bool hasZero = ((value - 0x11111111u) & ~value & 0x88888888u) != 0u;
      out.append(hasZero ? 0xffffffffu : 0u);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32SextNibble) {
    uint32_t shift = (parameter & 7u) * 4u;
    for (uint32_t value : input) {
      uint32_t n = (value >> shift) & 0xfu;
      out.append((n & 0x8u) ? (n | 0xfffffff0u) : n);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32CountZeroNibbles) {
    for (uint32_t value : input) {
      uint32_t count = 0;
      for (uint32_t shift = 0; shift < 32; shift += 4) {
        if (((value >> shift) & 0xfu) == 0u)
          ++count;
      }
      out.append(count);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32PackLowNibbles) {
    for (uint32_t value : input) {
      uint32_t packed = (value & 0xfu) | (((value >> 8) & 0xfu) << 4) | (((value >> 16) & 0xfu) << 8)
          | (((value >> 24) & 0xfu) << 12);
      out.append(packed);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32PackHighNibbles) {
    for (uint32_t value : input) {
      uint32_t packed = ((value >> 4) & 0xfu) | (((value >> 12) & 0xfu) << 4)
          | (((value >> 20) & 0xfu) << 8) | (((value >> 28) & 0xfu) << 12);
      out.append(packed);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32ExpandLowNibbles) {
    for (uint32_t value : input) {
      uint32_t p = value & 0xffffu;
      out.append((p & 0xfu) | (((p >> 4) & 0xfu) << 8) | (((p >> 8) & 0xfu) << 16)
          | (((p >> 12) & 0xfu) << 24));
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32ExpandHighNibbles) {
    for (uint32_t value : input) {
      uint32_t p = value & 0xffffu;
      out.append(((p & 0xfu) << 4) | (((p >> 4) & 0xfu) << 12) | (((p >> 8) & 0xfu) << 20)
          | (((p >> 12) & 0xfu) << 28));
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32ReplaceNibble) {
    uint32_t from = parameter & 0xfu;
    uint32_t to = (parameter >> 8) & 0xfu;
    for (uint32_t value : input) {
      uint32_t result = 0;
      for (uint32_t i = 0; i < 8; ++i) {
        uint32_t shift = i * 4u;
        uint32_t n = (value >> shift) & 0xfu;
        result |= (n == from ? to : n) << shift;
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32MaskNibbles) {
    uint32_t select = parameter & 0xffu;
    for (uint32_t value : input) {
      uint32_t result = 0;
      for (uint32_t i = 0; i < 8; ++i) {
        if (select & (1u << i)) {
          uint32_t shift = i * 4u;
          result |= value & (0xfu << shift);
        }
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32AllNibblesEqual) {
    for (uint32_t value : input) {
      uint32_t first = value & 0xfu;
      bool equal = true;
      for (uint32_t shift = 4; shift < 32; shift += 4) {
        if (((value >> shift) & 0xfu) != first) {
          equal = false;
          break;
        }
      }
      out.append(equal ? 0xffffffffu : 0u);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32NibbleRange) {
    for (uint32_t value : input) {
      uint32_t lo = value & 0xfu;
      uint32_t hi = lo;
      for (uint32_t shift = 4; shift < 32; shift += 4) {
        uint32_t n = (value >> shift) & 0xfu;
        if (n < lo)
          lo = n;
        if (n > hi)
          hi = n;
      }
      out.append(hi - lo);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32BroadcastNibbleAt) {
    uint32_t shift = (parameter & 7u) * 4u;
    for (uint32_t value : input) {
      uint32_t n = (value >> shift) & 0xfu;
      out.append(n * 0x11111111u);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfSum) {
    for (uint32_t value : input)
      out.append((value & 0xffffu) + (value >> 16));
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfMax) {
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t hi = value >> 16;
      out.append(lo > hi ? lo : hi);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfMin) {
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t hi = value >> 16;
      out.append(lo < hi ? lo : hi);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfXor) {
    for (uint32_t value : input)
      out.append((value & 0xffffu) ^ (value >> 16));
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfOr) {
    for (uint32_t value : input)
      out.append((value & 0xffffu) | (value >> 16));
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfAnd) {
    for (uint32_t value : input)
      out.append((value & 0xffffu) & (value >> 16));
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32BroadcastHalf) {
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      out.append(lo | (lo << 16));
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfEq) {
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t hi = value >> 16;
      out.append(lo == hi ? 0xffffffffu : 0u);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfRange) {
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t hi = value >> 16;
      out.append(lo > hi ? (lo - hi) : (hi - lo));
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfLt) {
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t hi = value >> 16;
      out.append(lo < hi ? 0xffffffffu : 0u);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfGt) {
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t hi = value >> 16;
      out.append(lo > hi ? 0xffffffffu : 0u);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfNe) {
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t hi = value >> 16;
      out.append(lo != hi ? 0xffffffffu : 0u);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfLeu) {
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t hi = value >> 16;
      out.append(lo <= hi ? 0xffffffffu : 0u);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfGeu) {
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t hi = value >> 16;
      out.append(lo >= hi ? 0xffffffffu : 0u);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfAvg) {
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t hi = value >> 16;
      out.append((lo + hi) >> 1);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32BroadcastHighHalf) {
    for (uint32_t value : input) {
      uint32_t hi = value >> 16;
      out.append(hi | (hi << 16));
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfSub) {
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t hi = value >> 16;
      out.append(lo - hi);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfMul) {
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t hi = value >> 16;
      out.append(lo * hi);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfSatAdd) {
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t hi = value >> 16;
      uint32_t sum = lo + hi;
      out.append(sum > 0xffffu ? 0xffffu : sum);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfSatSub) {
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t hi = value >> 16;
      out.append(lo > hi ? (lo - hi) : 0u);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfMulHi) {
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t hi = value >> 16;
      out.append((lo * hi) >> 16);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfDiv) {
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t hi = value >> 16;
      out.append(hi == 0u ? 0u : (lo / hi));
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfMod) {
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t hi = value >> 16;
      out.append(hi == 0u ? 0u : (lo % hi));
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfCeilDiv) {
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t hi = value >> 16;
      out.append(hi == 0u ? 0u : ((lo + hi - 1u) / hi));
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfIsDivisible) {
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t hi = value >> 16;
      out.append((hi != 0u && (lo % hi) == 0u) ? 0xffffffffu : 0u);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfGcd) {
    for (uint32_t value : input) {
      uint32_t a = value & 0xffffu;
      uint32_t b = value >> 16;
      while (b != 0u) {
        uint32_t t = a % b;
        a = b;
        b = t;
      }
      out.append(a);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfSelect) {
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t hi = value >> 16;
      out.append((parameter & 1u) ? hi : lo);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfLcm) {
    for (uint32_t value : input) {
      uint32_t a = value & 0xffffu;
      uint32_t b = value >> 16;
      if (a == 0u || b == 0u) {
        out.append(0u);
        continue;
      }
      uint32_t x = a;
      uint32_t y = b;
      while (y != 0u) {
        uint32_t t = x % y;
        x = y;
        y = t;
      }
      out.append((a / x) * b);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfCoprime) {
    for (uint32_t value : input) {
      uint32_t a = value & 0xffffu;
      uint32_t b = value >> 16;
      while (b != 0u) {
        uint32_t t = a % b;
        a = b;
        b = t;
      }
      out.append(a == 1u ? 0xffffffffu : 0u);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfAlignUp) {
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t hi = value >> 16;
      out.append(hi == 0u ? lo : ((lo + hi - 1u) / hi) * hi);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfAlignDown) {
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t hi = value >> 16;
      out.append(hi == 0u ? lo : (lo / hi) * hi);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfRotl) {
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t rot = (value >> 16) & 15u;
      out.append(((lo << rot) | (lo >> ((16u - rot) & 15u))) & 0xffffu);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfRotr) {
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t rot = (value >> 16) & 15u;
      out.append(((lo >> rot) | (lo << ((16u - rot) & 15u))) & 0xffffu);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfPopcount) {
    for (uint32_t value : input)
      out.append(densePopcount(value & 0xffffu));
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfClz) {
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      out.append(lo == 0u ? 16u : (denseClz(lo) - 16u));
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfCtz) {
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      out.append(lo == 0u ? 16u : denseCtz(lo));
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfParity) {
    for (uint32_t value : input)
      out.append(densePopcount(value & 0xffffu) & 1u);
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfBitReverse) {
    for (uint32_t value : input)
      out.append(denseBitReverse(value & 0xffffu) >> 16);
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfBswap) {
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      out.append(((lo & 0xffu) << 8) | ((lo >> 8) & 0xffu));
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfNot) {
    for (uint32_t value : input)
      out.append((~value) & 0xffffu);
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfNeg) {
    for (uint32_t value : input)
      out.append((0u - (value & 0xffffu)) & 0xffffu);
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfAbs) {
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      out.append((lo & 0x8000u) ? ((0u - lo) & 0xffffu) : lo);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfSignMask) {
    for (uint32_t value : input)
      out.append((value & 0x8000u) ? 0xffffffffu : 0u);
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfIsZero) {
    for (uint32_t value : input)
      out.append((value & 0xffffu) == 0u ? 0xffffffffu : 0u);
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfIsNonzero) {
    for (uint32_t value : input)
      out.append((value & 0xffffu) != 0u ? 0xffffffffu : 0u);
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfClo) {
    for (uint32_t value : input) {
      uint32_t ones = (~value) & 0xffffu;
      out.append(ones == 0u ? 16u : (denseClz(ones) - 16u));
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfCto) {
    for (uint32_t value : input) {
      uint32_t ones = (~value) & 0xffffu;
      out.append(ones == 0u ? 16u : denseCtz(ones));
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfIsPow2) {
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      out.append((lo != 0u && (lo & (lo - 1u)) == 0u) ? 0xffffffffu : 0u);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfIlog2) {
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      out.append(lo == 0u ? 0u : (31u - denseClz(lo)));
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfNextPow2) {
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      if (lo == 0u) {
        out.append(0u);
        continue;
      }
      uint32_t v = lo - 1u;
      v |= v >> 1;
      v |= v >> 2;
      v |= v >> 4;
      v |= v >> 8;
      out.append((v + 1u) & 0xffffu);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfMaskLow) {
    uint32_t bits = parameter > 16u ? 16u : parameter;
    uint32_t mask = bits == 16u ? 0xffffu : (bits == 0u ? 0u : ((1u << bits) - 1u));
    for (uint32_t value : input)
      out.append((value & 0xffffu) & mask);
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfMaskHigh) {
    uint32_t bits = parameter > 16u ? 16u : parameter;
    uint32_t mask = bits == 0u ? 0u
                               : (bits == 16u ? 0xffffu : ((0xffffu << (16u - bits)) & 0xffffu));
    for (uint32_t value : input)
      out.append((value & 0xffffu) & mask);
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfBitTest) {
    uint32_t bit = parameter & 15u;
    for (uint32_t value : input)
      out.append((((value & 0xffffu) >> bit) & 1u) ? 0xffffffffu : 0u);
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfIsolateLsb) {
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      out.append(lo & (uint32_t)(0u - lo));
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfIsolateMsb) {
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      if (lo == 0u)
        out.append(0u);
      else
        out.append(1u << (31u - denseClz(lo)));
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfClearLsb) {
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      out.append(lo & (lo - 1u));
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfLsbMask) {
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      out.append(((lo - 1u) & 0xffffu) ^ lo);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfGrayEncode) {
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      out.append(lo ^ (lo >> 1));
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfGrayDecode) {
    for (uint32_t value : input) {
      uint32_t v = value & 0xffffu;
      v ^= v >> 1;
      v ^= v >> 2;
      v ^= v >> 4;
      v ^= v >> 8;
      out.append(v & 0xffffu);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfSmearLsb) {
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      out.append(lo | ((lo - 1u) & 0xffffu));
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfClearMsb) {
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      if (lo == 0u)
        out.append(0u);
      else
        out.append(lo ^ (1u << (31u - denseClz(lo))));
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfSmearMsb) {
    for (uint32_t value : input) {
      uint32_t v = value & 0xffffu;
      v |= v >> 1;
      v |= v >> 2;
      v |= v >> 4;
      v |= v >> 8;
      out.append(v & 0xffffu);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfKeepOddBits) {
    for (uint32_t value : input)
      out.append((value & 0xffffu) & 0x5555u);
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfKeepEvenBits) {
    for (uint32_t value : input)
      out.append((value & 0xffffu) & 0xaaaau);
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfSwapAdjacentBits) {
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t odd = (lo & 0x5555u) << 1;
      uint32_t even = (lo & 0xaaaau) >> 1;
      out.append((odd | even) & 0xffffu);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfCompressOddBits) {
    for (uint32_t value : input) {
      uint32_t x = (value & 0xffffu) & 0x5555u;
      x = (x | (x >> 1)) & 0x3333u;
      x = (x | (x >> 2)) & 0x0f0fu;
      x = (x | (x >> 4)) & 0x00ffu;
      out.append(x);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfCompressEvenBits) {
    for (uint32_t value : input) {
      uint32_t x = ((value & 0xffffu) >> 1) & 0x5555u;
      x = (x | (x >> 1)) & 0x3333u;
      x = (x | (x >> 2)) & 0x0f0fu;
      x = (x | (x >> 4)) & 0x00ffu;
      out.append(x);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfExpandLow8ToOdd) {
    for (uint32_t value : input) {
      uint32_t x = value & 0xffu;
      x = (x | (x << 4)) & 0x0f0fu;
      x = (x | (x << 2)) & 0x3333u;
      x = (x | (x << 1)) & 0x5555u;
      out.append(x);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfExpandLow8ToEven) {
    for (uint32_t value : input) {
      uint32_t x = value & 0xffu;
      x = (x | (x << 4)) & 0x0f0fu;
      x = (x | (x << 2)) & 0x3333u;
      x = (x | (x << 1)) & 0x5555u;
      out.append((x << 1) & 0xffffu);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfInterleaveBytes) {
    for (uint32_t value : input) {
      auto expandOdd8 = [](uint32_t x) {
        x &= 0xffu;
        x = (x | (x << 4)) & 0x0f0fu;
        x = (x | (x << 2)) & 0x3333u;
        x = (x | (x << 1)) & 0x5555u;
        return x;
      };
      uint32_t lo = value & 0xffffu;
      uint32_t odd = expandOdd8(lo);
      uint32_t even = expandOdd8(lo >> 8);
      out.append(((even << 1) | odd) & 0xffffu);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfDeinterleaveBytes) {
    for (uint32_t value : input) {
      auto compressOdd16 = [](uint32_t x) {
        x &= 0x5555u;
        x = (x | (x >> 1)) & 0x3333u;
        x = (x | (x >> 2)) & 0x0f0fu;
        x = (x | (x >> 4)) & 0x00ffu;
        return x;
      };
      uint32_t lo = value & 0xffffu;
      uint32_t odd = compressOdd16(lo);
      uint32_t even = compressOdd16(lo >> 1);
      out.append(((even << 8) | odd) & 0xffffu);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfSwapBytes) {
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      out.append((((lo & 0xffu) << 8) | ((lo >> 8) & 0xffu)) & 0xffffu);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfNibbleXor) {
    for (uint32_t value : input) {
      uint32_t x = value & 0xffffu;
      x ^= x >> 4;
      x ^= x >> 8;
      out.append(x & 0xfu);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfNibbleOr) {
    for (uint32_t value : input) {
      uint32_t x = value & 0xffffu;
      x |= x >> 4;
      x |= x >> 8;
      out.append(x & 0xfu);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfNibbleAnd) {
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t even = lo & 0x0f0fu;
      uint32_t odd = (lo >> 4) & 0x0f0fu;
      uint32_t m = even & odd;
      m &= m >> 8;
      out.append(m & 0xfu);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfNibbleSum) {
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t x = (lo & 0x0f0fu) + ((lo >> 4) & 0x0f0fu);
      x = (x & 0xffu) + ((x >> 8) & 0xffu);
      out.append(x);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfNibbleMax) {
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t m = lo & 0xfu;
      for (uint32_t shift = 4; shift < 16; shift += 4) {
        uint32_t n = (lo >> shift) & 0xfu;
        if (n > m)
          m = n;
      }
      out.append(m);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfNibbleMin) {
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t m = lo & 0xfu;
      for (uint32_t shift = 4; shift < 16; shift += 4) {
        uint32_t n = (lo >> shift) & 0xfu;
        if (n < m)
          m = n;
      }
      out.append(m);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfSwapAdjacentNibbles) {
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t low = (lo & 0x0f0fu) << 4;
      uint32_t high = (lo & 0xf0f0u) >> 4;
      out.append((low | high) & 0xffffu);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfRolNibbles) {
    uint32_t n = parameter & 3u;
    uint32_t rot = n * 4u;
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      if (rot == 0u)
        out.append(lo);
      else
        out.append(((lo << rot) | (lo >> (16u - rot))) & 0xffffu);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfRorNibbles) {
    uint32_t n = parameter & 3u;
    uint32_t rot = n * 4u;
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      if (rot == 0u)
        out.append(lo);
      else
        out.append(((lo >> rot) | (lo << (16u - rot))) & 0xffffu);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfHasZeroNibble) {
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      bool hasZero = ((lo - 0x1111u) & ~lo & 0x8888u) != 0u;
      out.append(hasZero ? 0xffffffffu : 0u);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfCountZeroNibbles) {
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t count = 0;
      for (uint32_t shift = 0; shift < 16; shift += 4) {
        if (((lo >> shift) & 0xfu) == 0u)
          ++count;
      }
      out.append(count);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfBroadcastNibble) {
    for (uint32_t value : input) {
      uint32_t n = value & 0xfu;
      out.append((n * 0x1111u) & 0xffffu);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfSextNibble) {
    uint32_t shift = (parameter & 3u) * 4u;
    for (uint32_t value : input) {
      uint32_t n = ((value & 0xffffu) >> shift) & 0xfu;
      out.append((n & 0x8u) ? (n | 0xfffffff0u) : n);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfExtractNibble) {
    uint32_t shift = (parameter & 3u) * 4u;
    for (uint32_t value : input)
      out.append(((value & 0xffffu) >> shift) & 0xfu);
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfInsertNibble) {
    uint32_t idx = parameter & 3u;
    uint32_t nib = (parameter >> 8) & 0xfu;
    uint32_t shift = idx * 4u;
    uint32_t mask = ~(0xfu << shift) & 0xffffu;
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      out.append((lo & mask) | (nib << shift));
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfAllNibblesEqual) {
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t first = lo & 0xfu;
      bool equal = true;
      for (uint32_t shift = 4; shift < 16; shift += 4) {
        if (((lo >> shift) & 0xfu) != first) {
          equal = false;
          break;
        }
      }
      out.append(equal ? 0xffffffffu : 0u);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfNibbleRange) {
    for (uint32_t value : input) {
      uint32_t lo16 = value & 0xffffu;
      uint32_t lo = lo16 & 0xfu;
      uint32_t hi = lo;
      for (uint32_t shift = 4; shift < 16; shift += 4) {
        uint32_t n = (lo16 >> shift) & 0xfu;
        if (n < lo)
          lo = n;
        if (n > hi)
          hi = n;
      }
      out.append(hi - lo);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfMaskNibbles) {
    uint32_t select = parameter & 0xfu;
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        if (select & (1u << i)) {
          uint32_t shift = i * 4u;
          result |= lo & (0xfu << shift);
        }
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfBroadcastNibbleAt) {
    uint32_t shift = (parameter & 3u) * 4u;
    for (uint32_t value : input) {
      uint32_t n = ((value & 0xffffu) >> shift) & 0xfu;
      out.append((n * 0x1111u) & 0xffffu);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfReplaceNibble) {
    uint32_t from = parameter & 0xfu;
    uint32_t to = (parameter >> 8) & 0xfu;
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t sh = i * 4u;
        uint32_t n = (lo >> sh) & 0xfu;
        result |= (n == from ? to : n) << sh;
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfReverseNibbles) {
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i)
        result |= ((lo >> (i * 4u)) & 0xfu) << ((3u - i) * 4u);
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfWidenNibbles) {
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i)
        result |= ((lo >> (i * 4u)) & 0xfu) << (i * 8u);
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfDupNibbles) {
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t n = (lo >> (i * 4u)) & 0xfu;
        result |= n << (i * 8u);
        result |= n << (i * 8u + 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfFoldAdjacentNibbles) {
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t a = (lo & 0xfu) ^ ((lo >> 4) & 0xfu);
      uint32_t b = ((lo >> 8) & 0xfu) ^ ((lo >> 12) & 0xfu);
      out.append(a | (b << 4));
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfCountNonzeroNibbles) {
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t count = 0;
      for (uint32_t shift = 0; shift < 16; shift += 4) {
        if (((lo >> shift) & 0xfu) != 0u)
          ++count;
      }
      out.append(count);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfAddNibble) {
    uint32_t add = parameter & 0xfu;
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t n = (((lo >> (i * 4u)) & 0xfu) + add) & 0xfu;
        result |= n << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfSatAddNibble) {
    uint32_t add = parameter & 0xfu;
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t n = ((lo >> (i * 4u)) & 0xfu) + add;
        if (n > 0xfu)
          n = 0xfu;
        result |= n << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfSubNibble) {
    uint32_t sub = parameter & 0xfu;
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t n = (((lo >> (i * 4u)) & 0xfu) - sub) & 0xfu;
        result |= n << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfSatSubNibble) {
    uint32_t sub = parameter & 0xfu;
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t n = (lo >> (i * 4u)) & 0xfu;
        n = (n > sub) ? (n - sub) : 0u;
        result |= n << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfMulNibble) {
    uint32_t mul = parameter & 0xfu;
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t n = (((lo >> (i * 4u)) & 0xfu) * mul) & 0xfu;
        result |= n << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfSatMulNibble) {
    uint32_t mul = parameter & 0xfu;
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t n = ((lo >> (i * 4u)) & 0xfu) * mul;
        if (n > 0xfu)
          n = 0xfu;
        result |= n << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfDivNibble) {
    uint32_t div = parameter & 0xfu;
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t n = (lo >> (i * 4u)) & 0xfu;
        n = (div == 0u) ? 0u : (n / div);
        result |= n << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfModNibble) {
    uint32_t mod = parameter & 0xfu;
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t n = (lo >> (i * 4u)) & 0xfu;
        n = (mod == 0u) ? 0u : (n % mod);
        result |= n << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfMinNibble) {
    uint32_t bound = parameter & 0xfu;
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t n = (lo >> (i * 4u)) & 0xfu;
        if (n > bound)
          n = bound;
        result |= n << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfMaxNibble) {
    uint32_t bound = parameter & 0xfu;
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t n = (lo >> (i * 4u)) & 0xfu;
        if (n < bound)
          n = bound;
        result |= n << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfClampNibble) {
    uint32_t loBound = parameter & 0xfu;
    uint32_t hiBound = (parameter >> 8) & 0xfu;
    if (loBound > hiBound) {
      uint32_t tmp = loBound;
      loBound = hiBound;
      hiBound = tmp;
    }
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t n = (lo >> (i * 4u)) & 0xfu;
        if (n < loBound)
          n = loBound;
        if (n > hiBound)
          n = hiBound;
        result |= n << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfAndNibble) {
    uint32_t mask = parameter & 0xfu;
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t n = ((lo >> (i * 4u)) & 0xfu) & mask;
        result |= n << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfOrNibble) {
    uint32_t mask = parameter & 0xfu;
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t n = ((lo >> (i * 4u)) & 0xfu) | mask;
        result |= n << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfXorNibble) {
    uint32_t mask = parameter & 0xfu;
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t n = ((lo >> (i * 4u)) & 0xfu) ^ mask;
        result |= n << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfShlNibble) {
    uint32_t shift = parameter & 3u;
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t n = (((lo >> (i * 4u)) & 0xfu) << shift) & 0xfu;
        result |= n << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfShrNibble) {
    uint32_t shift = parameter & 3u;
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t n = ((lo >> (i * 4u)) & 0xfu) >> shift;
        result |= n << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfRolNibble) {
    uint32_t rot = parameter & 3u;
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t n = (lo >> (i * 4u)) & 0xfu;
        if (rot == 0u)
          ;
        else
          n = ((n << rot) | (n >> (4u - rot))) & 0xfu;
        result |= n << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfRorNibble) {
    uint32_t rot = parameter & 3u;
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t n = (lo >> (i * 4u)) & 0xfu;
        if (rot != 0u)
          n = ((n >> rot) | (n << (4u - rot))) & 0xfu;
        result |= n << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfNotNibble) {
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t n = (~((lo >> (i * 4u)) & 0xfu)) & 0xfu;
        result |= n << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfNegNibble) {
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t n = (0u - ((lo >> (i * 4u)) & 0xfu)) & 0xfu;
        result |= n << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfAbsNibble) {
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t n = (lo >> (i * 4u)) & 0xfu;
        if (n & 0x8u)
          n = (0u - n) & 0xfu;
        result |= n << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfSignMaskNibble) {
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t n = (lo >> (i * 4u)) & 0xfu;
        n = (n & 0x8u) ? 0xfu : 0u;
        result |= n << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfIsZeroNibble) {
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t n = (lo >> (i * 4u)) & 0xfu;
        n = (n == 0u) ? 0xfu : 0u;
        result |= n << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfIsNonzeroNibble) {
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t n = (lo >> (i * 4u)) & 0xfu;
        n = (n != 0u) ? 0xfu : 0u;
        result |= n << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfEqNibble) {
    uint32_t target = parameter & 0xfu;
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t n = (lo >> (i * 4u)) & 0xfu;
        n = (n == target) ? 0xfu : 0u;
        result |= n << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfNeNibble) {
    uint32_t target = parameter & 0xfu;
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t n = (lo >> (i * 4u)) & 0xfu;
        n = (n != target) ? 0xfu : 0u;
        result |= n << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfLtNibble) {
    uint32_t target = parameter & 0xfu;
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t n = (lo >> (i * 4u)) & 0xfu;
        n = (n < target) ? 0xfu : 0u;
        result |= n << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfLeNibble) {
    uint32_t target = parameter & 0xfu;
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t n = (lo >> (i * 4u)) & 0xfu;
        n = (n <= target) ? 0xfu : 0u;
        result |= n << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfGtNibble) {
    uint32_t target = parameter & 0xfu;
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t n = (lo >> (i * 4u)) & 0xfu;
        n = (n > target) ? 0xfu : 0u;
        result |= n << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfGeNibble) {
    uint32_t target = parameter & 0xfu;
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t n = (lo >> (i * 4u)) & 0xfu;
        n = (n >= target) ? 0xfu : 0u;
        result |= n << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfBetweenNibble) {
    uint32_t loBound = parameter & 0xfu;
    uint32_t hiBound = (parameter >> 8) & 0xfu;
    if (loBound > hiBound) {
      uint32_t tmp = loBound;
      loBound = hiBound;
      hiBound = tmp;
    }
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t n = (lo >> (i * 4u)) & 0xfu;
        n = (n >= loBound && n <= hiBound) ? 0xfu : 0u;
        result |= n << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfOutsideNibble) {
    uint32_t loBound = parameter & 0xfu;
    uint32_t hiBound = (parameter >> 8) & 0xfu;
    if (loBound > hiBound) {
      uint32_t tmp = loBound;
      loBound = hiBound;
      hiBound = tmp;
    }
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t n = (lo >> (i * 4u)) & 0xfu;
        n = (n < loBound || n > hiBound) ? 0xfu : 0u;
        result |= n << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfPopcountNibble) {
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t n = (lo >> (i * 4u)) & 0xfu;
        uint32_t pc = 0;
        while (n) {
          pc += n & 1u;
          n >>= 1;
        }
        result |= pc << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfParityNibble) {
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t n = (lo >> (i * 4u)) & 0xfu;
        uint32_t pc = 0;
        while (n) {
          pc ^= n & 1u;
          n >>= 1;
        }
        result |= pc << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfReverseBitsNibble) {
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t n = (lo >> (i * 4u)) & 0xfu;
        uint32_t rev = ((n & 1u) << 3) | ((n & 2u) << 1) | ((n & 4u) >> 1) | ((n & 8u) >> 3);
        result |= rev << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfIsolateLsbNibble) {
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t n = (lo >> (i * 4u)) & 0xfu;
        n = n & ((~n + 1u) & 0xfu);
        result |= n << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfClearLsbNibble) {
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t n = (lo >> (i * 4u)) & 0xfu;
        n = n & ((n - 1u) & 0xfu);
        result |= n << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfSmearLsbNibble) {
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t n = (lo >> (i * 4u)) & 0xfu;
        n = (n | ((n - 1u) & 0xfu)) & 0xfu;
        result |= n << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfIsolateMsbNibble) {
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t n = (lo >> (i * 4u)) & 0xfu;
        if (n != 0u) {
          uint32_t bit = 8u;
          while ((n & bit) == 0u)
            bit >>= 1;
          n = bit;
        }
        result |= n << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfClearMsbNibble) {
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t n = (lo >> (i * 4u)) & 0xfu;
        if (n != 0u) {
          uint32_t bit = 8u;
          while ((n & bit) == 0u)
            bit >>= 1;
          n ^= bit;
        }
        result |= n << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfSmearMsbNibble) {
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t n = (lo >> (i * 4u)) & 0xfu;
        n |= n >> 1;
        n |= n >> 2;
        n &= 0xfu;
        result |= n << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfLsbMaskNibble) {
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t n = (lo >> (i * 4u)) & 0xfu;
        n = (n ^ ((n - 1u) & 0xfu)) & 0xfu;
        result |= n << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfKeepOddBitsNibble) {
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t n = ((lo >> (i * 4u)) & 0xfu) & 0x5u;
        result |= n << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfKeepEvenBitsNibble) {
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t n = ((lo >> (i * 4u)) & 0xfu) & 0xau;
        result |= n << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfSwapAdjacentBitsNibble) {
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t n = (lo >> (i * 4u)) & 0xfu;
        n = (((n & 0x5u) << 1) | ((n & 0xau) >> 1)) & 0xfu;
        result |= n << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfGrayEncodeNibble) {
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t n = (lo >> (i * 4u)) & 0xfu;
        n = (n ^ (n >> 1)) & 0xfu;
        result |= n << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfGrayDecodeNibble) {
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t n = (lo >> (i * 4u)) & 0xfu;
        n ^= n >> 1;
        n ^= n >> 2;
        n &= 0xfu;
        result |= n << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfClzNibble) {
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t n = (lo >> (i * 4u)) & 0xfu;
        uint32_t z = 0;
        if (n == 0u)
          z = 4u;
        else {
          uint32_t bit = 8u;
          while ((n & bit) == 0u) {
            ++z;
            bit >>= 1;
          }
        }
        result |= z << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfCtzNibble) {
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t n = (lo >> (i * 4u)) & 0xfu;
        uint32_t z = 0;
        if (n == 0u)
          z = 4u;
        else {
          while ((n & 1u) == 0u) {
            ++z;
            n >>= 1;
          }
        }
        result |= z << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfCloNibble) {
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t n = (lo >> (i * 4u)) & 0xfu;
        uint32_t ones = 0;
        if (n == 0xfu)
          ones = 4u;
        else {
          uint32_t bit = 8u;
          while ((n & bit) != 0u) {
            ++ones;
            bit >>= 1;
          }
        }
        result |= ones << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfCtoNibble) {
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t n = (lo >> (i * 4u)) & 0xfu;
        uint32_t ones = 0;
        if (n == 0xfu)
          ones = 4u;
        else {
          while ((n & 1u) != 0u) {
            ++ones;
            n >>= 1;
          }
        }
        result |= ones << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfBitLengthNibble) {
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t n = (lo >> (i * 4u)) & 0xfu;
        uint32_t len = 0;
        if (n != 0u) {
          uint32_t bit = 8u;
          len = 4u;
          while ((n & bit) == 0u) {
            --len;
            bit >>= 1;
          }
        }
        result |= len << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfIsPowerOfTwoNibble) {
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t n = (lo >> (i * 4u)) & 0xfu;
        uint32_t flag = (n != 0u && (n & (n - 1u)) == 0u) ? 1u : 0u;
        result |= flag << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfNextPow2Nibble) {
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t n = (lo >> (i * 4u)) & 0xfu;
        uint32_t outN = 0;
        if (n != 0u) {
          if ((n & (n - 1u)) == 0u)
            outN = n;
          else {
            uint32_t v = n - 1u;
            v |= v >> 1;
            v |= v >> 2;
            outN = (v + 1u) & 0xfu;
            if (outN == 0u)
              outN = 0xfu;
          }
        }
        result |= outN << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfPrevPow2Nibble) {
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t n = (lo >> (i * 4u)) & 0xfu;
        uint32_t outN = 0;
        if (n != 0u) {
          uint32_t bit = 8u;
          while ((n & bit) == 0u)
            bit >>= 1;
          outN = bit;
        }
        result |= outN << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfBroadcastLsbNibble) {
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t n = (lo >> (i * 4u)) & 0xfu;
        uint32_t outN = (n & 1u) ? 0xfu : 0u;
        result |= outN << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfBroadcastMsbNibble) {
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t n = (lo >> (i * 4u)) & 0xfu;
        uint32_t outN = (n & 8u) ? 0xfu : 0u;
        result |= outN << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfParityFillNibble) {
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t n = (lo >> (i * 4u)) & 0xfu;
        uint32_t parity = ((n >> 0) ^ (n >> 1) ^ (n >> 2) ^ (n >> 3)) & 1u;
        uint32_t outN = parity ? 0xfu : 0u;
        result |= outN << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfRotl1Nibble) {
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t n = (lo >> (i * 4u)) & 0xfu;
        uint32_t outN = ((n << 1) | (n >> 3)) & 0xfu;
        result |= outN << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfRotr1Nibble) {
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t n = (lo >> (i * 4u)) & 0xfu;
        uint32_t outN = ((n >> 1) | (n << 3)) & 0xfu;
        result |= outN << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfNegateNibble) {
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t n = (lo >> (i * 4u)) & 0xfu;
        result |= ((~n) & 0xfu) << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfIncWrapNibble) {
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t n = (lo >> (i * 4u)) & 0xfu;
        result |= ((n + 1u) & 0xfu) << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfDecWrapNibble) {
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t n = (lo >> (i * 4u)) & 0xfu;
        result |= ((n - 1u) & 0xfu) << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfNandNibble) {
    uint32_t mask = parameter & 0xfu;
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t n = (lo >> (i * 4u)) & 0xfu;
        result |= ((~(n & mask)) & 0xfu) << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfNorNibble) {
    uint32_t mask = parameter & 0xfu;
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t n = (lo >> (i * 4u)) & 0xfu;
        result |= ((~(n | mask)) & 0xfu) << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfXnorNibble) {
    uint32_t mask = parameter & 0xfu;
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t n = (lo >> (i * 4u)) & 0xfu;
        result |= ((~(n ^ mask)) & 0xfu) << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfMajNibble) {
    uint32_t b = parameter & 0xfu;
    uint32_t cbits = (parameter >> 4) & 0xfu;
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t a = (lo >> (i * 4u)) & 0xfu;
        uint32_t maj = (a & b) | (a & cbits) | (b & cbits);
        result |= (maj & 0xfu) << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfMuxNibble) {
    uint32_t b = parameter & 0xfu;
    uint32_t sel = (parameter >> 4) & 0xfu;
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t a = (lo >> (i * 4u)) & 0xfu;
        uint32_t outN = (a & ~sel) | (b & sel);
        result |= (outN & 0xfu) << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfImpliesNibble) {
    uint32_t mask = parameter & 0xfu;
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t n = (lo >> (i * 4u)) & 0xfu;
        result |= (((~n) | mask) & 0xfu) << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfNeqFillNibble) {
    uint32_t target = parameter & 0xfu;
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t n = (lo >> (i * 4u)) & 0xfu;
        uint32_t outN = (n != target) ? 0xfu : 0u;
        result |= outN << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfAbsDiffNibble) {
    uint32_t p = parameter & 0xfu;
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t n = (lo >> (i * 4u)) & 0xfu;
        uint32_t diff = (n >= p) ? (n - p) : (p - n);
        result |= (diff & 0xfu) << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfEqFillNibble) {
    uint32_t target = parameter & 0xfu;
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t n = (lo >> (i * 4u)) & 0xfu;
        uint32_t outN = (n == target) ? 0xfu : 0u;
        result |= outN << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfOddFillNibble) {
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t n = (lo >> (i * 4u)) & 0xfu;
        uint32_t outN = (n & 1u) ? 0xfu : 0u;
        result |= outN << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfEvenFillNibble) {
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t n = (lo >> (i * 4u)) & 0xfu;
        uint32_t outN = (n & 1u) ? 0u : 0xfu;
        result |= outN << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfLtFillNibble) {
    uint32_t thr = parameter & 0xfu;
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t n = (lo >> (i * 4u)) & 0xfu;
        uint32_t outN = (n < thr) ? 0xfu : 0u;
        result |= outN << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfLeFillNibble) {
    uint32_t thr = parameter & 0xfu;
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t n = (lo >> (i * 4u)) & 0xfu;
        uint32_t outN = (n <= thr) ? 0xfu : 0u;
        result |= outN << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfGtFillNibble) {
    uint32_t thr = parameter & 0xfu;
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t n = (lo >> (i * 4u)) & 0xfu;
        uint32_t outN = (n > thr) ? 0xfu : 0u;
        result |= outN << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfGeFillNibble) {
    uint32_t thr = parameter & 0xfu;
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t n = (lo >> (i * 4u)) & 0xfu;
        uint32_t outN = (n >= thr) ? 0xfu : 0u;
        result |= outN << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfSatIncNibble) {
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t n = (lo >> (i * 4u)) & 0xfu;
        uint32_t outN = (n >= 0xfu) ? 0xfu : (n + 1u);
        result |= outN << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfSatDecNibble) {
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t n = (lo >> (i * 4u)) & 0xfu;
        uint32_t outN = (n == 0u) ? 0u : (n - 1u);
        result |= outN << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfRotl2Nibble) {
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t n = (lo >> (i * 4u)) & 0xfu;
        uint32_t outN = ((n << 2) | (n >> 2)) & 0xfu;
        result |= outN << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfRotl3Nibble) {
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t n = (lo >> (i * 4u)) & 0xfu;
        uint32_t outN = ((n << 3) | (n >> 1)) & 0xfu;
        result |= outN << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfAvgNibble) {
    uint32_t add = parameter & 0xfu;
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t n = (lo >> (i * 4u)) & 0xfu;
        uint32_t outN = (n + add) >> 1;
        result |= (outN & 0xfu) << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfMidNibble) {
    uint32_t b = parameter & 0xfu;
    uint32_t cbits = (parameter >> 4) & 0xfu;
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t a = (lo >> (i * 4u)) & 0xfu;
        uint32_t mn = a < b ? a : b;
        mn = mn < cbits ? mn : cbits;
        uint32_t mx = a > b ? a : b;
        mx = mx > cbits ? mx : cbits;
        uint32_t mid = a + b + cbits - mn - mx;
        result |= (mid & 0xfu) << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfMin3Nibble) {
    uint32_t b = parameter & 0xfu;
    uint32_t cbits = (parameter >> 4) & 0xfu;
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t a = (lo >> (i * 4u)) & 0xfu;
        uint32_t outN = a < b ? a : b;
        outN = outN < cbits ? outN : cbits;
        result |= outN << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfMax3Nibble) {
    uint32_t b = parameter & 0xfu;
    uint32_t cbits = (parameter >> 4) & 0xfu;
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t a = (lo >> (i * 4u)) & 0xfu;
        uint32_t outN = a > b ? a : b;
        outN = outN > cbits ? outN : cbits;
        result |= outN << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfRange3Nibble) {
    uint32_t b = parameter & 0xfu;
    uint32_t cbits = (parameter >> 4) & 0xfu;
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t a = (lo >> (i * 4u)) & 0xfu;
        uint32_t mn = a < b ? a : b;
        mn = mn < cbits ? mn : cbits;
        uint32_t mx = a > b ? a : b;
        mx = mx > cbits ? mx : cbits;
        result |= ((mx - mn) & 0xfu) << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfSum3Nibble) {
    uint32_t b = parameter & 0xfu;
    uint32_t cbits = (parameter >> 4) & 0xfu;
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t a = (lo >> (i * 4u)) & 0xfu;
        uint32_t outN = (a + b + cbits) & 0xfu;
        result |= outN << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfSatSum3Nibble) {
    uint32_t b = parameter & 0xfu;
    uint32_t cbits = (parameter >> 4) & 0xfu;
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t a = (lo >> (i * 4u)) & 0xfu;
        uint32_t sum = a + b + cbits;
        uint32_t outN = sum > 0xfu ? 0xfu : sum;
        result |= outN << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfAvg3Nibble) {
    uint32_t b = parameter & 0xfu;
    uint32_t cbits = (parameter >> 4) & 0xfu;
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t a = (lo >> (i * 4u)) & 0xfu;
        uint32_t outN = (a + b + cbits) / 3u;
        result |= (outN & 0xfu) << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfXor3Nibble) {
    uint32_t b = parameter & 0xfu;
    uint32_t cbits = (parameter >> 4) & 0xfu;
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t a = (lo >> (i * 4u)) & 0xfu;
        uint32_t outN = (a ^ b ^ cbits) & 0xfu;
        result |= outN << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfMulAddNibble) {
    uint32_t b = parameter & 0xfu;
    uint32_t cbits = (parameter >> 4) & 0xfu;
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t a = (lo >> (i * 4u)) & 0xfu;
        uint32_t outN = (a * b + cbits) & 0xfu;
        result |= outN << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfSatMulAddNibble) {
    uint32_t b = parameter & 0xfu;
    uint32_t cbits = (parameter >> 4) & 0xfu;
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t a = (lo >> (i * 4u)) & 0xfu;
        uint32_t sum = a * b + cbits;
        uint32_t outN = sum > 0xfu ? 0xfu : sum;
        result |= outN << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfMulSubNibble) {
    uint32_t b = parameter & 0xfu;
    uint32_t cbits = (parameter >> 4) & 0xfu;
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t a = (lo >> (i * 4u)) & 0xfu;
        uint32_t outN = (a * b - cbits) & 0xfu;
        result |= outN << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfEqual3FillNibble) {
    uint32_t b = parameter & 0xfu;
    uint32_t cbits = (parameter >> 4) & 0xfu;
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t a = (lo >> (i * 4u)) & 0xfu;
        uint32_t outN = (a == b && b == cbits) ? 0xfu : 0u;
        result |= outN << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfNeq3FillNibble) {
    uint32_t b = parameter & 0xfu;
    uint32_t cbits = (parameter >> 4) & 0xfu;
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t a = (lo >> (i * 4u)) & 0xfu;
        uint32_t outN = (a == b && b == cbits) ? 0u : 0xfu;
        result |= outN << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfAnyEqFillNibble) {
    uint32_t b = parameter & 0xfu;
    uint32_t cbits = (parameter >> 4) & 0xfu;
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t a = (lo >> (i * 4u)) & 0xfu;
        uint32_t outN = (a == b || a == cbits || b == cbits) ? 0xfu : 0u;
        result |= outN << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfAllDistinctFillNibble) {
    uint32_t b = parameter & 0xfu;
    uint32_t cbits = (parameter >> 4) & 0xfu;
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t a = (lo >> (i * 4u)) & 0xfu;
        uint32_t outN = (a != b && a != cbits && b != cbits) ? 0xfu : 0u;
        result |= outN << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfBlendNibble) {
    uint32_t w = parameter & 0xfu;
    uint32_t p = (parameter >> 4) & 0xfu;
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t a = (lo >> (i * 4u)) & 0xfu;
        uint32_t outN = (a * (15u - w) + p * w) / 15u;
        result |= (outN & 0xfu) << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfWeightedAddNibble) {
    uint32_t w = parameter & 0xfu;
    uint32_t p = (parameter >> 4) & 0xfu;
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t a = (lo >> (i * 4u)) & 0xfu;
        uint32_t outN = (a * w + p * (15u - w)) & 0xfu;
        result |= outN << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfBitMergeNibble) {
    uint32_t m = parameter & 0xfu;
    uint32_t p = (parameter >> 4) & 0xfu;
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t a = (lo >> (i * 4u)) & 0xfu;
        uint32_t outN = (a & (~m & 0xfu)) | (p & m);
        result |= outN << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfSatWeightedAddNibble) {
    uint32_t w = parameter & 0xfu;
    uint32_t p = (parameter >> 4) & 0xfu;
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t a = (lo >> (i * 4u)) & 0xfu;
        uint32_t sum = a * w + p * (15u - w);
        uint32_t outN = sum > 0xfu ? 0xfu : sum;
        result |= outN << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfMaskSelectNibble) {
    uint32_t m = parameter & 0xfu;
    uint32_t p = (parameter >> 4) & 0xfu;
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t a = (lo >> (i * 4u)) & 0xfu;
        uint32_t outN = (a & m) ? p : a;
        result |= outN << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfIfEqPickNibble) {
    uint32_t eq = parameter & 0xfu;
    uint32_t pick = (parameter >> 4) & 0xfu;
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t a = (lo >> (i * 4u)) & 0xfu;
        uint32_t outN = (a == eq) ? pick : a;
        result |= outN << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfIfNePickNibble) {
    uint32_t ne = parameter & 0xfu;
    uint32_t pick = (parameter >> 4) & 0xfu;
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t a = (lo >> (i * 4u)) & 0xfu;
        uint32_t outN = (a != ne) ? pick : a;
        result |= outN << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfThresholdPickNibble) {
    uint32_t thr = parameter & 0xfu;
    uint32_t pick = (parameter >> 4) & 0xfu;
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t a = (lo >> (i * 4u)) & 0xfu;
        uint32_t outN = (a >= thr) ? pick : a;
        result |= outN << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfModMulNibble) {
    uint32_t mul = parameter & 0xfu;
    uint32_t mod = (parameter >> 4) & 0xfu;
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t a = (lo >> (i * 4u)) & 0xfu;
        uint32_t outN = mod ? ((a * mul) % mod) : 0u;
        result |= outN << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfModAddNibble) {
    uint32_t add = parameter & 0xfu;
    uint32_t mod = (parameter >> 4) & 0xfu;
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t a = (lo >> (i * 4u)) & 0xfu;
        uint32_t outN = mod ? ((a + add) % mod) : 0u;
        result |= outN << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfModSubNibble) {
    uint32_t sub = parameter & 0xfu;
    uint32_t mod = (parameter >> 4) & 0xfu;
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t a = (lo >> (i * 4u)) & 0xfu;
        uint32_t outN = 0u;
        if (mod) {
          uint32_t s = sub % mod;
          outN = (a + mod - s) % mod;
        }
        result |= outN << (i * 4u);
      }
      out.append(result);
    }
  } else if (kernel == WorldGenerationComputeKernel::DenseUint32HalfCeilDivNibble) {
    uint32_t d = parameter & 0xfu;
    for (uint32_t value : input) {
      uint32_t lo = value & 0xffffu;
      uint32_t result = 0;
      for (uint32_t i = 0; i < 4; ++i) {
        uint32_t a = (lo >> (i * 4u)) & 0xfu;
        uint32_t outN = d ? ((a + d - 1u) / d) : 0u;
        result |= (outN & 0xfu) << (i * 4u);
      }
      out.append(result);
    }
  }
  return out;
}

List<uint32_t> WorldGenerationComputeService::acquireBuffer(size_t size) {
  for (size_t i = 0; i < m_bufferPool.size(); ++i) {
    if (m_bufferPool[i].size() >= size) {
      List<uint32_t> buffer = std::move(m_bufferPool[i]);
      m_bufferPool.eraseAt(i);
      buffer.resize(size);
      return buffer;
    }
  }
  return List<uint32_t>(size, 0u);
}

void WorldGenerationComputeService::releaseBuffer(List<uint32_t>&& buffer) {
  if (buffer.empty())
    return;
  if (m_bufferPool.size() < 16)
    m_bufferPool.append(std::move(buffer));
}

bool WorldGenerationComputeService::ownerAllowsLocked() const noexcept {
  return !m_hasOwnerThread || std::this_thread::get_id() == m_ownerThread;
}

void WorldGenerationComputeService::adoptOwnerThread() noexcept {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_ownerThread = std::this_thread::get_id();
  m_hasOwnerThread = true;
}

void WorldGenerationComputeService::clearOwnerThread() noexcept {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_hasOwnerThread = false;
  m_ownerThread = std::thread::id{};
}

bool WorldGenerationComputeService::hasOwnerThread() const noexcept {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_hasOwnerThread;
}

bool WorldGenerationComputeService::isOwnerThread() const noexcept {
  std::lock_guard<std::mutex> lock(m_mutex);
  return ownerAllowsLocked() && m_hasOwnerThread;
}

void WorldGenerationComputeService::setDeferredCpuCompletion(bool deferred) noexcept {
  std::lock_guard<std::mutex> lock(m_mutex);
  if (!ownerAllowsLocked())
    return;
  m_deferredCpu = deferred;
}

size_t WorldGenerationComputeService::openTickets() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_entries.size();
}

List<WorldGenerationComputeTicket> WorldGenerationComputeService::listOpenTickets() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  List<WorldGenerationComputeTicket> out;
  out.reserve(m_entries.size());
  for (auto const& pair : m_entries) {
    WorldGenerationComputeTicket ticket;
    ticket.id = pair.first;
    out.append(ticket);
  }
  return out;
}

List<WorldGenerationComputeTicket> WorldGenerationComputeService::listPendingOpenTickets() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  List<WorldGenerationComputeTicket> out;
  for (auto const& pair : m_entries) {
    if (pair.second.state != EntryState::Pending)
      continue;
    WorldGenerationComputeTicket ticket;
    ticket.id = pair.first;
    out.append(ticket);
  }
  return out;
}

List<WorldGenerationComputeTicket> WorldGenerationComputeService::listReadyOpenTickets() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  List<WorldGenerationComputeTicket> out;
  for (auto const& pair : m_entries) {
    if (pair.second.state != EntryState::Ready)
      continue;
    WorldGenerationComputeTicket ticket;
    ticket.id = pair.first;
    out.append(ticket);
  }
  return out;
}

List<WorldGenerationComputeTicket> WorldGenerationComputeService::listFailedOpenTickets() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  List<WorldGenerationComputeTicket> out;
  for (auto const& pair : m_entries) {
    if (pair.second.state != EntryState::Failed)
      continue;
    WorldGenerationComputeTicket ticket;
    ticket.id = pair.first;
    out.append(ticket);
  }
  return out;
}

List<WorldGenerationComputeTicket> WorldGenerationComputeService::listCancelledOpenTickets() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  List<WorldGenerationComputeTicket> out;
  for (auto const& pair : m_entries) {
    if (pair.second.state != EntryState::Cancelled)
      continue;
    WorldGenerationComputeTicket ticket;
    ticket.id = pair.first;
    out.append(ticket);
  }
  return out;
}

List<WorldGenerationComputeTicket> WorldGenerationComputeService::listTerminalOpenTickets() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  List<WorldGenerationComputeTicket> out;
  for (auto const& pair : m_entries) {
    if (pair.second.state == EntryState::Pending)
      continue;
    WorldGenerationComputeTicket ticket;
    ticket.id = pair.first;
    out.append(ticket);
  }
  return out;
}

List<WorldGenerationComputeKernel> WorldGenerationComputeService::listOpenKernels() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  List<WorldGenerationComputeKernel> out;
  std::unordered_set<uint16_t> seen;
  for (auto const& pair : m_entries) {
    auto kernel = pair.second.identity.kernel;
    uint16_t key = (uint16_t)kernel;
    if (!seen.insert(key).second)
      continue;
    out.append(kernel);
  }
  return out;
}

size_t WorldGenerationComputeService::countOpenKernels() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  std::unordered_set<uint16_t> seen;
  for (auto const& pair : m_entries)
    seen.insert((uint16_t)pair.second.identity.kernel);
  return seen.size();
}

bool WorldGenerationComputeService::anyOpenKernels() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return !m_entries.empty();
}

bool WorldGenerationComputeService::anyOpenTickets() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return !m_entries.empty();
}

size_t WorldGenerationComputeService::countOpenTickets() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_entries.size();
}

List<WorldGenerationComputeKernel> WorldGenerationComputeService::listReadyOpenKernels() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  List<WorldGenerationComputeKernel> out;
  std::unordered_set<uint16_t> seen;
  for (auto const& pair : m_entries) {
    if (pair.second.state != EntryState::Ready)
      continue;
    auto kernel = pair.second.identity.kernel;
    uint16_t key = (uint16_t)kernel;
    if (!seen.insert(key).second)
      continue;
    out.append(kernel);
  }
  return out;
}

size_t WorldGenerationComputeService::countReadyOpenKernels() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  std::unordered_set<uint16_t> seen;
  for (auto const& pair : m_entries) {
    if (pair.second.state != EntryState::Ready)
      continue;
    seen.insert((uint16_t)pair.second.identity.kernel);
  }
  return seen.size();
}

bool WorldGenerationComputeService::anyReadyOpenKernels() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  for (auto const& pair : m_entries) {
    if (pair.second.state == EntryState::Ready)
      return true;
  }
  return false;
}

List<WorldGenerationComputeKernel> WorldGenerationComputeService::listFailedOpenKernels() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  List<WorldGenerationComputeKernel> out;
  std::unordered_set<uint16_t> seen;
  for (auto const& pair : m_entries) {
    if (pair.second.state != EntryState::Failed)
      continue;
    auto kernel = pair.second.identity.kernel;
    uint16_t key = (uint16_t)kernel;
    if (!seen.insert(key).second)
      continue;
    out.append(kernel);
  }
  return out;
}

size_t WorldGenerationComputeService::countFailedOpenKernels() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  std::unordered_set<uint16_t> seen;
  for (auto const& pair : m_entries) {
    if (pair.second.state != EntryState::Failed)
      continue;
    seen.insert((uint16_t)pair.second.identity.kernel);
  }
  return seen.size();
}

bool WorldGenerationComputeService::anyFailedOpenKernels() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  for (auto const& pair : m_entries) {
    if (pair.second.state == EntryState::Failed)
      return true;
  }
  return false;
}

List<WorldGenerationComputeKernel> WorldGenerationComputeService::listCancelledOpenKernels() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  List<WorldGenerationComputeKernel> out;
  std::unordered_set<uint16_t> seen;
  for (auto const& pair : m_entries) {
    if (pair.second.state != EntryState::Cancelled)
      continue;
    auto kernel = pair.second.identity.kernel;
    uint16_t key = (uint16_t)kernel;
    if (!seen.insert(key).second)
      continue;
    out.append(kernel);
  }
  return out;
}

List<WorldGenerationComputeKernel> WorldGenerationComputeService::listTerminalOpenKernels() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  List<WorldGenerationComputeKernel> out;
  std::unordered_set<uint16_t> seen;
  for (auto const& pair : m_entries) {
    if (pair.second.state == EntryState::Pending)
      continue;
    auto kernel = pair.second.identity.kernel;
    uint16_t key = (uint16_t)kernel;
    if (!seen.insert(key).second)
      continue;
    out.append(kernel);
  }
  return out;
}

size_t WorldGenerationComputeService::countTerminalOpenKernels() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  std::unordered_set<uint16_t> seen;
  for (auto const& pair : m_entries) {
    if (pair.second.state == EntryState::Pending)
      continue;
    seen.insert((uint16_t)pair.second.identity.kernel);
  }
  return seen.size();
}

bool WorldGenerationComputeService::anyTerminalOpenKernels() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  for (auto const& pair : m_entries) {
    if (pair.second.state != EntryState::Pending)
      return true;
  }
  return false;
}


size_t WorldGenerationComputeService::countCancelledOpenKernels() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  std::unordered_set<uint16_t> seen;
  for (auto const& pair : m_entries) {
    if (pair.second.state != EntryState::Cancelled)
      continue;
    seen.insert((uint16_t)pair.second.identity.kernel);
  }
  return seen.size();
}

bool WorldGenerationComputeService::anyCancelledOpenKernels() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  for (auto const& pair : m_entries) {
    if (pair.second.state == EntryState::Cancelled)
      return true;
  }
  return false;
}

List<WorldGenerationComputeKernel> WorldGenerationComputeService::listPendingOpenKernels() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  List<WorldGenerationComputeKernel> out;
  std::unordered_set<uint16_t> seen;
  for (auto const& pair : m_entries) {
    if (pair.second.state != EntryState::Pending)
      continue;
    auto kernel = pair.second.identity.kernel;
    uint16_t key = (uint16_t)kernel;
    if (!seen.insert(key).second)
      continue;
    out.append(kernel);
  }
  return out;
}

size_t WorldGenerationComputeService::countPendingOpenKernels() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  std::unordered_set<uint16_t> seen;
  for (auto const& pair : m_entries) {
    if (pair.second.state != EntryState::Pending)
      continue;
    seen.insert((uint16_t)pair.second.identity.kernel);
  }
  return seen.size();
}

size_t WorldGenerationComputeService::countPendingOpenTickets() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  size_t count = 0;
  for (auto const& pair : m_entries) {
    if (pair.second.state == EntryState::Pending)
      ++count;
  }
  return count;
}

size_t WorldGenerationComputeService::countReadyOpenTickets() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  size_t count = 0;
  for (auto const& pair : m_entries) {
    if (pair.second.state == EntryState::Ready)
      ++count;
  }
  return count;
}

size_t WorldGenerationComputeService::countFailedOpenTickets() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  size_t count = 0;
  for (auto const& pair : m_entries) {
    if (pair.second.state == EntryState::Failed)
      ++count;
  }
  return count;
}

size_t WorldGenerationComputeService::countCancelledOpenTickets() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  size_t count = 0;
  for (auto const& pair : m_entries) {
    if (pair.second.state == EntryState::Cancelled)
      ++count;
  }
  return count;
}

size_t WorldGenerationComputeService::countTerminalOpenTickets() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  size_t count = 0;
  for (auto const& pair : m_entries) {
    if (pair.second.state != EntryState::Pending)
      ++count;
  }
  return count;
}

bool WorldGenerationComputeService::anyReadyOpenTickets() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  for (auto const& pair : m_entries) {
    if (pair.second.state == EntryState::Ready)
      return true;
  }
  return false;
}

bool WorldGenerationComputeService::anyFailedOpenTickets() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  for (auto const& pair : m_entries) {
    if (pair.second.state == EntryState::Failed)
      return true;
  }
  return false;
}

bool WorldGenerationComputeService::anyCancelledOpenTickets() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  for (auto const& pair : m_entries) {
    if (pair.second.state == EntryState::Cancelled)
      return true;
  }
  return false;
}

bool WorldGenerationComputeService::anyTerminalOpenTickets() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  for (auto const& pair : m_entries) {
    if (pair.second.state != EntryState::Pending)
      return true;
  }
  return false;
}

bool WorldGenerationComputeService::anyPendingOpenTickets() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  for (auto const& pair : m_entries) {
    if (pair.second.state == EntryState::Pending)
      return true;
  }
  return false;
}

bool WorldGenerationComputeService::anyPendingOpenKernels() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  for (auto const& pair : m_entries) {
    if (pair.second.state == EntryState::Pending)
      return true;
  }
  return false;
}

List<WorldGenerationComputeTicket> WorldGenerationComputeService::listOpenTicketsByKernel(
    WorldGenerationComputeKernel kernel) const {
  std::lock_guard<std::mutex> lock(m_mutex);
  List<WorldGenerationComputeTicket> out;
  for (auto const& pair : m_entries) {
    if (pair.second.identity.kernel != kernel)
      continue;
    WorldGenerationComputeTicket ticket;
    ticket.id = pair.first;
    out.append(ticket);
  }
  return out;
}

List<WorldGenerationComputeTicket> WorldGenerationComputeService::listReadyOpenTicketsByKernel(
    WorldGenerationComputeKernel kernel) const {
  std::lock_guard<std::mutex> lock(m_mutex);
  List<WorldGenerationComputeTicket> out;
  for (auto const& pair : m_entries) {
    if (pair.second.state != EntryState::Ready || pair.second.identity.kernel != kernel)
      continue;
    WorldGenerationComputeTicket ticket;
    ticket.id = pair.first;
    out.append(ticket);
  }
  return out;
}

List<WorldGenerationComputeTicket> WorldGenerationComputeService::listFailedOpenTicketsByKernel(
    WorldGenerationComputeKernel kernel) const {
  std::lock_guard<std::mutex> lock(m_mutex);
  List<WorldGenerationComputeTicket> out;
  for (auto const& pair : m_entries) {
    if (pair.second.state != EntryState::Failed || pair.second.identity.kernel != kernel)
      continue;
    WorldGenerationComputeTicket ticket;
    ticket.id = pair.first;
    out.append(ticket);
  }
  return out;
}

List<WorldGenerationComputeTicket> WorldGenerationComputeService::listCancelledOpenTicketsByKernel(
    WorldGenerationComputeKernel kernel) const {
  std::lock_guard<std::mutex> lock(m_mutex);
  List<WorldGenerationComputeTicket> out;
  for (auto const& pair : m_entries) {
    if (pair.second.state != EntryState::Cancelled || pair.second.identity.kernel != kernel)
      continue;
    WorldGenerationComputeTicket ticket;
    ticket.id = pair.first;
    out.append(ticket);
  }
  return out;
}


List<WorldGenerationComputeTicket> WorldGenerationComputeService::listTerminalOpenTicketsByKernel(
    WorldGenerationComputeKernel kernel) const {
  std::lock_guard<std::mutex> lock(m_mutex);
  List<WorldGenerationComputeTicket> out;
  for (auto const& pair : m_entries) {
    if (pair.second.state == EntryState::Pending || pair.second.identity.kernel != kernel)
      continue;
    WorldGenerationComputeTicket ticket;
    ticket.id = pair.first;
    out.append(ticket);
  }
  return out;
}

size_t WorldGenerationComputeService::countTerminalOpenTicketsByKernel(
    WorldGenerationComputeKernel kernel) const {
  std::lock_guard<std::mutex> lock(m_mutex);
  size_t count = 0;
  for (auto const& pair : m_entries) {
    if (pair.second.state == EntryState::Pending || pair.second.identity.kernel != kernel)
      continue;
    ++count;
  }
  return count;
}

bool WorldGenerationComputeService::anyTerminalOpenTicketsByKernel(
    WorldGenerationComputeKernel kernel) const {
  std::lock_guard<std::mutex> lock(m_mutex);
  for (auto const& pair : m_entries) {
    if (pair.second.state == EntryState::Pending || pair.second.identity.kernel != kernel)
      continue;
    return true;
  }
  return false;
}

List<WorldGenerationComputeTicket> WorldGenerationComputeService::listPendingOpenTicketsByKernel(
    WorldGenerationComputeKernel kernel) const {
  std::lock_guard<std::mutex> lock(m_mutex);
  List<WorldGenerationComputeTicket> out;
  for (auto const& pair : m_entries) {
    if (pair.second.state != EntryState::Pending || pair.second.identity.kernel != kernel)
      continue;
    WorldGenerationComputeTicket ticket;
    ticket.id = pair.first;
    out.append(ticket);
  }
  return out;
}

size_t WorldGenerationComputeService::countOpenTicketsByKernel(
    WorldGenerationComputeKernel kernel) const {
  std::lock_guard<std::mutex> lock(m_mutex);
  size_t count = 0;
  for (auto const& pair : m_entries) {
    if (pair.second.identity.kernel == kernel)
      ++count;
  }
  return count;
}

bool WorldGenerationComputeService::ticketIsOpen(WorldGenerationComputeTicket ticket) const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_entries.find(ticket.id) != m_entries.end();
}

size_t WorldGenerationComputeService::rejectedDuplicateCount() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_rejectedDuplicates.size();
}

bool WorldGenerationComputeService::anyRejectedDuplicates() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return !m_rejectedDuplicates.empty();
}

size_t WorldGenerationComputeService::countRejectedDuplicates() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_rejectedDuplicates.size();
}

List<WorldGenerationComputeTicket> WorldGenerationComputeService::listRejectedDuplicates() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  List<WorldGenerationComputeTicket> out;
  out.reserve(m_rejectedDuplicates.size());
  for (uint64_t id : m_rejectedDuplicates) {
    WorldGenerationComputeTicket ticket;
    ticket.id = id;
    out.append(ticket);
  }
  return out;
}

bool WorldGenerationComputeService::hasRejectedDuplicate(WorldGenerationComputeTicket ticket) const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_rejectedDuplicates.count(ticket.id) != 0;
}

bool WorldGenerationComputeService::removeRejectedDuplicate(WorldGenerationComputeTicket ticket) {
  std::lock_guard<std::mutex> lock(m_mutex);
  if (!ownerAllowsLocked())
    return false;
  return m_rejectedDuplicates.erase(ticket.id) != 0;
}

uint64_t WorldGenerationComputeService::fingerprintRejectedDuplicates() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  uint64_t fp = 0;
  for (uint64_t id : m_rejectedDuplicates)
    fp ^= id;
  return fp;
}

bool WorldGenerationComputeService::rejectedDuplicatesEmpty() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_rejectedDuplicates.empty();
}

uint64_t WorldGenerationComputeService::rejectedDuplicatesFingerprint() const {
  return fingerprintRejectedDuplicates();
}

bool WorldGenerationComputeService::hasRejectedDuplicates() const {
  return anyRejectedDuplicates();
}

size_t WorldGenerationComputeService::rejectedDuplicatesCount() const {
  return countRejectedDuplicates();
}

List<WorldGenerationComputeTicket> WorldGenerationComputeService::rejectedDuplicatesList() const {
  return listRejectedDuplicates();
}

bool WorldGenerationComputeService::rejectedDuplicatesAny() const {
  return anyRejectedDuplicates();
}

size_t WorldGenerationComputeService::rejectedDuplicatesClear() {
  return clearRejectedDuplicates();
}

size_t WorldGenerationComputeService::rejectedDuplicatesSize() const {
  return rejectedDuplicatesCount();
}

bool WorldGenerationComputeService::rejectedDuplicatesContains(
    WorldGenerationComputeTicket ticket) const {
  return hasRejectedDuplicate(ticket);
}

bool WorldGenerationComputeService::rejectedDuplicatesRemove(
    WorldGenerationComputeTicket ticket) {
  return removeRejectedDuplicate(ticket);
}

size_t WorldGenerationComputeService::rejectedDuplicatesReset() {
  return clearRejectedDuplicates();
}

bool WorldGenerationComputeService::rejectedDuplicatesIsEmpty() const {
  return rejectedDuplicatesEmpty();
}

size_t WorldGenerationComputeService::rejectedDuplicatesCardinality() const {
  return rejectedDuplicatesCount();
}

List<WorldGenerationComputeTicket> WorldGenerationComputeService::rejectedDuplicatesMembers() const {
  return listRejectedDuplicates();
}

uint64_t WorldGenerationComputeService::rejectedDuplicatesDigest() const {
  return fingerprintRejectedDuplicates();
}

size_t WorldGenerationComputeService::rejectedDuplicatesPurge() {
  return clearRejectedDuplicates();
}

size_t WorldGenerationComputeService::clearRejectedDuplicates() {
  std::lock_guard<std::mutex> lock(m_mutex);
  if (!ownerAllowsLocked())
    return 0;
  size_t cleared = m_rejectedDuplicates.size();
  m_rejectedDuplicates.clear();
  return cleared;
}

size_t WorldGenerationComputeService::drainOpenTickets() {
  std::lock_guard<std::mutex> lock(m_mutex);
  if (!ownerAllowsLocked())
    return 0;
  List<WorldGenerationComputeTicket> tickets;
  tickets.reserve(m_entries.size());
  for (auto const& pair : m_entries) {
    WorldGenerationComputeTicket ticket;
    ticket.id = pair.first;
    tickets.append(ticket);
  }
  size_t drained = 0;
  for (auto ticket : tickets) {
    auto it = m_entries.find(ticket.id);
    if (it != m_entries.end() && it->second.state == EntryState::Pending)
      completeEntry(it->second);
    pollLocked(ticket, nullptr);
    ++drained;
  }
  return drained;
}

size_t WorldGenerationComputeService::drainOpenTicketsByKernel(
    WorldGenerationComputeKernel kernel) {
  std::lock_guard<std::mutex> lock(m_mutex);
  if (!ownerAllowsLocked())
    return 0;
  List<WorldGenerationComputeTicket> tickets;
  for (auto const& pair : m_entries) {
    if (pair.second.identity.kernel != kernel)
      continue;
    WorldGenerationComputeTicket ticket;
    ticket.id = pair.first;
    tickets.append(ticket);
  }
  size_t drained = 0;
  for (auto ticket : tickets) {
    auto it = m_entries.find(ticket.id);
    if (it != m_entries.end() && it->second.state == EntryState::Pending)
      completeEntry(it->second);
    pollLocked(ticket, nullptr);
    ++drained;
  }
  return drained;
}

size_t WorldGenerationComputeService::discardReadyOpenTicketsByKernel(
    WorldGenerationComputeKernel kernel) {
  std::lock_guard<std::mutex> lock(m_mutex);
  if (!ownerAllowsLocked())
    return 0;
  List<WorldGenerationComputeTicket> tickets;
  for (auto const& pair : m_entries) {
    if (pair.second.state != EntryState::Ready || pair.second.identity.kernel != kernel)
      continue;
    WorldGenerationComputeTicket ticket;
    ticket.id = pair.first;
    tickets.append(ticket);
  }
  size_t discarded = 0;
  for (auto ticket : tickets) {
    pollLocked(ticket, nullptr);
    ++discarded;
  }
  return discarded;
}

size_t WorldGenerationComputeService::discardReadyOpenTickets() {
  std::lock_guard<std::mutex> lock(m_mutex);
  if (!ownerAllowsLocked())
    return 0;
  List<WorldGenerationComputeTicket> tickets;
  for (auto const& pair : m_entries) {
    if (pair.second.state != EntryState::Ready)
      continue;
    WorldGenerationComputeTicket ticket;
    ticket.id = pair.first;
    tickets.append(ticket);
  }
  size_t discarded = 0;
  for (auto ticket : tickets) {
    pollLocked(ticket, nullptr);
    ++discarded;
  }
  return discarded;
}

size_t WorldGenerationComputeService::discardFailedOpenTicketsByKernel(
    WorldGenerationComputeKernel kernel) {
  std::lock_guard<std::mutex> lock(m_mutex);
  if (!ownerAllowsLocked())
    return 0;
  List<WorldGenerationComputeTicket> tickets;
  for (auto const& pair : m_entries) {
    if (pair.second.state != EntryState::Failed || pair.second.identity.kernel != kernel)
      continue;
    WorldGenerationComputeTicket ticket;
    ticket.id = pair.first;
    tickets.append(ticket);
  }
  size_t discarded = 0;
  for (auto ticket : tickets) {
    pollLocked(ticket, nullptr);
    ++discarded;
  }
  return discarded;
}

size_t WorldGenerationComputeService::discardFailedOpenTickets() {
  std::lock_guard<std::mutex> lock(m_mutex);
  if (!ownerAllowsLocked())
    return 0;
  List<WorldGenerationComputeTicket> tickets;
  for (auto const& pair : m_entries) {
    if (pair.second.state != EntryState::Failed)
      continue;
    WorldGenerationComputeTicket ticket;
    ticket.id = pair.first;
    tickets.append(ticket);
  }
  size_t discarded = 0;
  for (auto ticket : tickets) {
    pollLocked(ticket, nullptr);
    ++discarded;
  }
  return discarded;
}

size_t WorldGenerationComputeService::discardCancelledOpenTicketsByKernel(
    WorldGenerationComputeKernel kernel) {
  std::lock_guard<std::mutex> lock(m_mutex);
  if (!ownerAllowsLocked())
    return 0;
  List<WorldGenerationComputeTicket> tickets;
  for (auto const& pair : m_entries) {
    if (pair.second.state != EntryState::Cancelled || pair.second.identity.kernel != kernel)
      continue;
    WorldGenerationComputeTicket ticket;
    ticket.id = pair.first;
    tickets.append(ticket);
  }
  size_t discarded = 0;
  for (auto ticket : tickets) {
    pollLocked(ticket, nullptr);
    ++discarded;
  }
  return discarded;
}

size_t WorldGenerationComputeService::discardCancelledOpenTickets() {
  std::lock_guard<std::mutex> lock(m_mutex);
  if (!ownerAllowsLocked())
    return 0;
  List<WorldGenerationComputeTicket> tickets;
  for (auto const& pair : m_entries) {
    if (pair.second.state != EntryState::Cancelled)
      continue;
    WorldGenerationComputeTicket ticket;
    ticket.id = pair.first;
    tickets.append(ticket);
  }
  size_t discarded = 0;
  for (auto ticket : tickets) {
    pollLocked(ticket, nullptr);
    ++discarded;
  }
  return discarded;
}

size_t WorldGenerationComputeService::discardPendingOpenTicketsByKernel(
    WorldGenerationComputeKernel kernel) {
  std::lock_guard<std::mutex> lock(m_mutex);
  if (!ownerAllowsLocked())
    return 0;
  List<WorldGenerationComputeTicket> tickets;
  for (auto& pair : m_entries) {
    Entry& entry = pair.second;
    if (entry.state != EntryState::Pending || entry.identity.kernel != kernel)
      continue;
    entry.publicationRevoked = true;
    entry.state = EntryState::Cancelled;
    entry.output.clear();
    WorldGenerationComputeTicket ticket;
    ticket.id = pair.first;
    tickets.append(ticket);
  }
  size_t discarded = 0;
  for (auto ticket : tickets) {
    pollLocked(ticket, nullptr);
    ++discarded;
  }
  return discarded;
}

size_t WorldGenerationComputeService::discardPendingOpenTickets() {
  std::lock_guard<std::mutex> lock(m_mutex);
  if (!ownerAllowsLocked())
    return 0;
  List<WorldGenerationComputeTicket> tickets;
  for (auto& pair : m_entries) {
    Entry& entry = pair.second;
    if (entry.state != EntryState::Pending)
      continue;
    entry.publicationRevoked = true;
    entry.state = EntryState::Cancelled;
    entry.output.clear();
    WorldGenerationComputeTicket ticket;
    ticket.id = pair.first;
    tickets.append(ticket);
  }
  size_t discarded = 0;
  for (auto ticket : tickets) {
    pollLocked(ticket, nullptr);
    ++discarded;
  }
  return discarded;
}

size_t WorldGenerationComputeService::discardTerminalOpenTicketsByKernel(
    WorldGenerationComputeKernel kernel) {
  std::lock_guard<std::mutex> lock(m_mutex);
  if (!ownerAllowsLocked())
    return 0;
  List<WorldGenerationComputeTicket> tickets;
  for (auto const& pair : m_entries) {
    if (pair.second.identity.kernel != kernel)
      continue;
    if (pair.second.state != EntryState::Ready && pair.second.state != EntryState::Failed
        && pair.second.state != EntryState::Cancelled)
      continue;
    WorldGenerationComputeTicket ticket;
    ticket.id = pair.first;
    tickets.append(ticket);
  }
  size_t discarded = 0;
  for (auto ticket : tickets) {
    pollLocked(ticket, nullptr);
    ++discarded;
  }
  return discarded;
}

size_t WorldGenerationComputeService::discardTerminalOpenTickets() {
  std::lock_guard<std::mutex> lock(m_mutex);
  if (!ownerAllowsLocked())
    return 0;
  List<WorldGenerationComputeTicket> tickets;
  for (auto const& pair : m_entries) {
    if (pair.second.state != EntryState::Ready && pair.second.state != EntryState::Failed
        && pair.second.state != EntryState::Cancelled)
      continue;
    WorldGenerationComputeTicket ticket;
    ticket.id = pair.first;
    tickets.append(ticket);
  }
  size_t discarded = 0;
  for (auto ticket : tickets) {
    pollLocked(ticket, nullptr);
    ++discarded;
  }
  return discarded;
}


size_t WorldGenerationComputeService::inFlight() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  size_t count = 0;
  for (auto const& entry : m_entries) {
    if (entry.second.state == EntryState::Pending)
      ++count;
  }
  return count;
}

size_t WorldGenerationComputeService::inFlightByKernel(WorldGenerationComputeKernel kernel) const {
  std::lock_guard<std::mutex> lock(m_mutex);
  size_t count = 0;
  for (auto const& entry : m_entries) {
    if (entry.second.state == EntryState::Pending && entry.second.identity.kernel == kernel)
      ++count;
  }
  return count;
}

List<WorldGenerationComputeTicket> WorldGenerationComputeService::listInFlightByKernel(
    WorldGenerationComputeKernel kernel) const {
  std::lock_guard<std::mutex> lock(m_mutex);
  List<WorldGenerationComputeTicket> out;
  for (auto const& pair : m_entries) {
    if (pair.second.state != EntryState::Pending || pair.second.identity.kernel != kernel)
      continue;
    WorldGenerationComputeTicket ticket;
    ticket.id = pair.first;
    out.append(ticket);
  }
  return out;
}

bool WorldGenerationComputeService::anyInFlightByKernel(
    WorldGenerationComputeKernel kernel) const {
  std::lock_guard<std::mutex> lock(m_mutex);
  for (auto const& entry : m_entries) {
    if (entry.second.state == EntryState::Pending && entry.second.identity.kernel == kernel)
      return true;
  }
  return false;
}

size_t WorldGenerationComputeService::pendingOpenTicketsByKernel(
    WorldGenerationComputeKernel kernel) const {
  std::lock_guard<std::mutex> lock(m_mutex);
  size_t count = 0;
  for (auto const& entry : m_entries) {
    if (entry.second.state == EntryState::Pending && entry.second.identity.kernel == kernel)
      ++count;
  }
  return count;
}

bool WorldGenerationComputeService::anyPendingOpenTicketsByKernel(
    WorldGenerationComputeKernel kernel) const {
  std::lock_guard<std::mutex> lock(m_mutex);
  for (auto const& entry : m_entries) {
    if (entry.second.state == EntryState::Pending && entry.second.identity.kernel == kernel)
      return true;
  }
  return false;
}

size_t WorldGenerationComputeService::readyOpenTicketsByKernel(
    WorldGenerationComputeKernel kernel) const {
  std::lock_guard<std::mutex> lock(m_mutex);
  size_t count = 0;
  for (auto const& entry : m_entries) {
    if (entry.second.state == EntryState::Ready && entry.second.identity.kernel == kernel)
      ++count;
  }
  return count;
}

bool WorldGenerationComputeService::anyReadyOpenTicketsByKernel(
    WorldGenerationComputeKernel kernel) const {
  std::lock_guard<std::mutex> lock(m_mutex);
  for (auto const& entry : m_entries) {
    if (entry.second.state == EntryState::Ready && entry.second.identity.kernel == kernel)
      return true;
  }
  return false;
}

size_t WorldGenerationComputeService::cancelledOpenTicketsByKernel(
    WorldGenerationComputeKernel kernel) const {
  std::lock_guard<std::mutex> lock(m_mutex);
  size_t count = 0;
  for (auto const& entry : m_entries) {
    if (entry.second.state == EntryState::Cancelled && entry.second.identity.kernel == kernel)
      ++count;
  }
  return count;
}

bool WorldGenerationComputeService::anyCancelledOpenTicketsByKernel(
    WorldGenerationComputeKernel kernel) const {
  std::lock_guard<std::mutex> lock(m_mutex);
  for (auto const& entry : m_entries) {
    if (entry.second.state == EntryState::Cancelled && entry.second.identity.kernel == kernel)
      return true;
  }
  return false;
}

bool WorldGenerationComputeService::anyOpenTicketsByKernel(
    WorldGenerationComputeKernel kernel) const {
  std::lock_guard<std::mutex> lock(m_mutex);
  for (auto const& entry : m_entries) {
    if (entry.second.identity.kernel == kernel)
      return true;
  }
  return false;
}

size_t WorldGenerationComputeService::failedOpenTicketsByKernel(
    WorldGenerationComputeKernel kernel) const {
  std::lock_guard<std::mutex> lock(m_mutex);
  size_t count = 0;
  for (auto const& entry : m_entries) {
    if (entry.second.state == EntryState::Failed && entry.second.identity.kernel == kernel)
      ++count;
  }
  return count;
}

bool WorldGenerationComputeService::anyFailedOpenTicketsByKernel(
    WorldGenerationComputeKernel kernel) const {
  std::lock_guard<std::mutex> lock(m_mutex);
  for (auto const& entry : m_entries) {
    if (entry.second.state == EntryState::Failed && entry.second.identity.kernel == kernel)
      return true;
  }
  return false;
}

size_t WorldGenerationComputeService::stagingBytes() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return (size_t)m_stagingBytes;
}

size_t WorldGenerationComputeService::pooledBuffers() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_bufferPool.size();
}

WorldGenerationComputeStagingSnapshot WorldGenerationComputeService::stagingSnapshot() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  WorldGenerationComputeStagingSnapshot snap;
  snap.stagingBytes = m_stagingBytes;
  snap.peakStagingBytes = m_peakStagingBytes;
  snap.maxStagingBytes = m_capabilities.maxStagingBytes;
  snap.openTickets = m_entries.size();
  snap.peakOpenTickets = m_peakOpenTickets;
  snap.gpuCapable = m_capabilities.gpuOpenGlCompute;
  snap.pooledBufferCount = m_bufferPool.size();
  for (auto const& buffer : m_bufferPool)
    snap.pooledBufferBytes += bufferBytes(buffer);
  for (auto const& pair : m_entries) {
    Entry const& entry = pair.second;
    if (entry.state == EntryState::Pending)
      ++snap.inFlight;
    if (entry.backend == WorldGenerationComputeBackend::GpuOpenGlCompute
        && entry.state == EntryState::Pending) {
      ++snap.gpuInFlight;
      snap.estimatedVramBytes += entry.stagingBytes;
    }
  }
  return snap;
}

void WorldGenerationComputeService::resetPeakStagingBytes() noexcept {
  std::lock_guard<std::mutex> lock(m_mutex);
  if (!ownerAllowsLocked())
    return;
  m_peakStagingBytes = m_stagingBytes;
}

void WorldGenerationComputeService::resetPeakOpenTickets() noexcept {
  std::lock_guard<std::mutex> lock(m_mutex);
  if (!ownerAllowsLocked())
    return;
  m_peakOpenTickets = m_entries.size();
}

bool WorldGenerationComputeService::runBackendKernel(WorldGenerationComputeBackend backend,
    WorldGenerationComputeKernel kernel, uint32_t kernelVersion, List<uint32_t> const& input,
    uint32_t parameter, List<uint32_t>& out) const {
  if (!supported(kernel, kernelVersion))
    return false;
  if (backend == WorldGenerationComputeBackend::GpuOpenGlCompute) {
    if (!m_capabilities.gpuOpenGlCompute)
      return false;
    return WorldGenerationComputeGlDevice::dispatch(kernel, kernelVersion, input, parameter, out);
  }
  if (backend != WorldGenerationComputeBackend::CpuReference || !m_capabilities.cpuReference)
    return false;
  out = runKernel(kernel, kernelVersion, input, parameter);
  return true;
}

void WorldGenerationComputeService::completeEntry(Entry& entry) {
  if (entry.publicationRevoked || entry.contextFailed) {
    entry.state = EntryState::Cancelled;
    entry.output.clear();
    return;
  }
  if (!runBackendKernel(entry.backend, entry.identity.kernel, entry.identity.kernelVersion,
          entry.input, entry.parameter, entry.output)) {
    entry.state = EntryState::Failed;
    entry.output.clear();
    return;
  }
  entry.state = EntryState::Ready;
}

Maybe<WorldGenerationComputeTicket> WorldGenerationComputeService::submitLocked(
    WorldGenerationComputeInputIdentity identity, List<uint32_t> input, uint32_t parameter,
    WorldGenerationComputeBackend backend) {
  if (!supported(identity.kernel, identity.kernelVersion))
    return {};
  if (backend == WorldGenerationComputeBackend::GpuOpenGlCompute) {
    if (!m_capabilities.gpuOpenGlCompute)
      return {};
  } else if (backend != WorldGenerationComputeBackend::CpuReference
      || !m_capabilities.cpuReference) {
    return {};
  }

  uint64_t bytes = bufferBytes(input);
  if (bytes > m_capabilities.maxImmutableBufferBytes)
    return {};
  if (m_stagingBytes + bytes > m_capabilities.maxStagingBytes)
    return {};

  size_t pending = 0;
  for (auto const& entry : m_entries) {
    if (entry.second.state == EntryState::Pending)
      ++pending;
  }
  if (pending >= m_capabilities.maxInFlight)
    return {};

  WorldGenerationComputeTicket ticket{m_nextTicket++};
  Entry entry;
  entry.identity = identity;
  entry.backend = backend;
  entry.parameter = parameter;
  entry.input = acquireBuffer(input.size());
  for (size_t i = 0; i < input.size(); ++i)
    entry.input[i] = input[i];
  entry.inputFingerprint = fingerprintInput(entry.input);
  entry.stagingBytes = bytes;
  m_stagingBytes += bytes;
  if (m_stagingBytes > m_peakStagingBytes)
    m_peakStagingBytes = m_stagingBytes;

  // GPU completions use the same sync/deferred switch as CPU for the first
  // harness: deferred leaves Pending until pump()/complete(); sync runs now.
  if (!m_deferredCpu)
    completeEntry(entry);
  else
    entry.state = EntryState::Pending;

  m_entries.emplace(ticket.id, std::move(entry));
  if (m_entries.size() > m_peakOpenTickets)
    m_peakOpenTickets = m_entries.size();
  return ticket;
}

Maybe<WorldGenerationComputeTicket> WorldGenerationComputeService::submit(
    WorldGenerationComputeInputIdentity identity, List<uint32_t> input, uint32_t parameter,
    WorldGenerationComputeBackend backend) {
  std::lock_guard<std::mutex> lock(m_mutex);
  if (!ownerAllowsLocked())
    return {};
  return submitLocked(std::move(identity), std::move(input), parameter, backend);
}

Maybe<WorldGenerationComputeTicket> WorldGenerationComputeService::submitNamed(char const* kernelName,
    WorldGenerationComputeInputIdentity identity, List<uint32_t> input, uint32_t parameter,
    WorldGenerationComputeBackend backend) {
  auto kernel = parseKernelName(kernelName);
  if (!kernel)
    return {};
  identity.kernel = *kernel;
  return submit(std::move(identity), std::move(input), parameter, backend);
}

Maybe<List<WorldGenerationComputeTicket>> WorldGenerationComputeService::submitBatch(
    List<WorldGenerationComputeBatchItem> items, WorldGenerationComputeBackend backend) {
  std::lock_guard<std::mutex> lock(m_mutex);
  if (!ownerAllowsLocked())
    return {};
  if (items.empty())
    return List<WorldGenerationComputeTicket>{};

  size_t pending = 0;
  for (auto const& entry : m_entries) {
    if (entry.second.state == EntryState::Pending)
      ++pending;
  }
  if (pending + items.size() > m_capabilities.maxInFlight)
    return {};

  uint64_t extra = 0;
  for (auto const& item : items) {
    if (!supported(item.identity.kernel, item.identity.kernelVersion))
      return {};
    uint64_t bytes = bufferBytes(item.input);
    if (bytes > m_capabilities.maxImmutableBufferBytes)
      return {};
    extra += bytes;
  }
  if (m_stagingBytes + extra > m_capabilities.maxStagingBytes)
    return {};

  List<WorldGenerationComputeTicket> tickets;
  tickets.reserve(items.size());
  for (auto& item : items) {
    auto ticket = submitLocked(item.identity, std::move(item.input), item.parameter, backend);
    if (!ticket) {
      // Roll back any tickets admitted in this batch.
      for (auto const& issued : tickets) {
        auto it = m_entries.find(issued.id);
        if (it != m_entries.end()) {
          m_stagingBytes -= it->second.stagingBytes;
          releaseBuffer(std::move(it->second.input));
          m_entries.erase(it);
        }
      }
      return {};
    }
    tickets.append(*ticket);
  }
  return tickets;
}

size_t WorldGenerationComputeService::pump(size_t maxCompletions) {
  std::lock_guard<std::mutex> lock(m_mutex);
  if (!ownerAllowsLocked())
    return 0;
  size_t completed = 0;
  for (auto& pair : m_entries) {
    if (maxCompletions && completed >= maxCompletions)
      break;
    Entry& entry = pair.second;
    if (entry.state != EntryState::Pending)
      continue;
    completeEntry(entry);
    ++completed;
  }
  return completed;
}

bool WorldGenerationComputeService::complete(WorldGenerationComputeTicket ticket) {
  std::lock_guard<std::mutex> lock(m_mutex);
  if (!ownerAllowsLocked())
    return false;
  auto it = m_entries.find(ticket.id);
  if (it == m_entries.end())
    return false;
  if (it->second.state != EntryState::Pending)
    return false;
  completeEntry(it->second);
  return it->second.state == EntryState::Ready || it->second.state == EntryState::Cancelled
      || it->second.state == EntryState::Failed;
}

List<bool> WorldGenerationComputeService::completeBatch(List<WorldGenerationComputeTicket> tickets) {
  List<bool> out;
  out.reserve(tickets.size());
  std::lock_guard<std::mutex> lock(m_mutex);
  if (!ownerAllowsLocked()) {
    for (size_t i = 0; i < tickets.size(); ++i)
      out.append(false);
    return out;
  }
  for (auto ticket : tickets) {
    auto it = m_entries.find(ticket.id);
    if (it == m_entries.end() || it->second.state != EntryState::Pending) {
      out.append(false);
      continue;
    }
    completeEntry(it->second);
    out.append(it->second.state == EntryState::Ready || it->second.state == EntryState::Cancelled
        || it->second.state == EntryState::Failed);
  }
  return out;
}

size_t WorldGenerationComputeService::completeOpenTicketsByKernel(
    WorldGenerationComputeKernel kernel) {
  std::lock_guard<std::mutex> lock(m_mutex);
  if (!ownerAllowsLocked())
    return 0;
  size_t completed = 0;
  for (auto& pair : m_entries) {
    Entry& entry = pair.second;
    if (entry.identity.kernel != kernel || entry.state != EntryState::Pending)
      continue;
    completeEntry(entry);
    if (entry.state == EntryState::Ready || entry.state == EntryState::Cancelled
        || entry.state == EntryState::Failed)
      ++completed;
  }
  return completed;
}

WorldGenerationComputePollStatus WorldGenerationComputeService::pollLocked(
    WorldGenerationComputeTicket ticket, WorldGenerationComputeResult* out) {
  if (m_rejectedDuplicates.count(ticket.id))
    return WorldGenerationComputePollStatus::UnknownTicket;

  auto it = m_entries.find(ticket.id);
  if (it == m_entries.end())
    return WorldGenerationComputePollStatus::UnknownTicket;

  Entry& entry = it->second;
  if (entry.publicationRevoked && entry.state != EntryState::Cancelled) {
    entry.state = EntryState::Cancelled;
    entry.output.clear();
  }

  WorldGenerationComputePollStatus status = WorldGenerationComputePollStatus::Pending;
  switch (entry.state) {
    case EntryState::Pending: status = WorldGenerationComputePollStatus::Pending; break;
    case EntryState::Ready: status = WorldGenerationComputePollStatus::Ready; break;
    case EntryState::Failed: status = WorldGenerationComputePollStatus::Failed; break;
    case EntryState::Cancelled: status = WorldGenerationComputePollStatus::Cancelled; break;
  }

  if (status == WorldGenerationComputePollStatus::Ready && out && !entry.publicationRevoked) {
    out->identity = entry.identity;
    out->backend = entry.backend;
    out->values = entry.output;
  } else if (out) {
    *out = WorldGenerationComputeResult{};
  }

  if (status != WorldGenerationComputePollStatus::Pending) {
    m_stagingBytes -= entry.stagingBytes;
    releaseBuffer(std::move(entry.input));
    if (entry.contextFailed)
      m_rejectedDuplicates.insert(ticket.id);
    m_entries.erase(it);
  }
  return status;
}

WorldGenerationComputePollStatus WorldGenerationComputeService::poll(
    WorldGenerationComputeTicket ticket, WorldGenerationComputeResult* out) {
  std::lock_guard<std::mutex> lock(m_mutex);
  if (!ownerAllowsLocked())
    return WorldGenerationComputePollStatus::UnknownTicket;
  return pollLocked(ticket, out);
}

bool WorldGenerationComputeService::cancel(WorldGenerationComputeTicket ticket) {
  std::lock_guard<std::mutex> lock(m_mutex);
  if (!ownerAllowsLocked())
    return false;
  auto it = m_entries.find(ticket.id);
  if (it == m_entries.end())
    return false;
  it->second.publicationRevoked = true;
  if (it->second.state == EntryState::Pending || it->second.state == EntryState::Ready) {
    it->second.state = EntryState::Cancelled;
    it->second.output.clear();
  }
  return true;
}

List<bool> WorldGenerationComputeService::cancelBatch(List<WorldGenerationComputeTicket> tickets) {
  List<bool> out;
  out.reserve(tickets.size());
  std::lock_guard<std::mutex> lock(m_mutex);
  if (!ownerAllowsLocked()) {
    for (size_t i = 0; i < tickets.size(); ++i)
      out.append(false);
    return out;
  }
  for (auto ticket : tickets) {
    auto it = m_entries.find(ticket.id);
    if (it == m_entries.end()) {
      out.append(false);
      continue;
    }
    it->second.publicationRevoked = true;
    if (it->second.state == EntryState::Pending || it->second.state == EntryState::Ready) {
      it->second.state = EntryState::Cancelled;
      it->second.output.clear();
    }
    out.append(true);
  }
  return out;
}

size_t WorldGenerationComputeService::cancelAll() {
  std::lock_guard<std::mutex> lock(m_mutex);
  if (!ownerAllowsLocked())
    return 0;
  size_t cancelled = 0;
  for (auto& pair : m_entries) {
    Entry& entry = pair.second;
    if (entry.publicationRevoked && entry.state == EntryState::Cancelled)
      continue;
    entry.publicationRevoked = true;
    if (entry.state == EntryState::Pending || entry.state == EntryState::Ready) {
      entry.state = EntryState::Cancelled;
      entry.output.clear();
    }
    ++cancelled;
  }
  return cancelled;
}

size_t WorldGenerationComputeService::cancelOpenTicketsByKernel(
    WorldGenerationComputeKernel kernel) {
  std::lock_guard<std::mutex> lock(m_mutex);
  if (!ownerAllowsLocked())
    return 0;
  size_t cancelled = 0;
  for (auto& pair : m_entries) {
    Entry& entry = pair.second;
    if (entry.identity.kernel != kernel)
      continue;
    if (entry.publicationRevoked && entry.state == EntryState::Cancelled)
      continue;
    entry.publicationRevoked = true;
    if (entry.state == EntryState::Pending || entry.state == EntryState::Ready) {
      entry.state = EntryState::Cancelled;
      entry.output.clear();
    }
    ++cancelled;
  }
  return cancelled;
}

List<WorldGenerationComputePollStatus> WorldGenerationComputeService::pollBatch(
    List<WorldGenerationComputeTicket> tickets, List<WorldGenerationComputeResult>* outs) {
  List<WorldGenerationComputePollStatus> statuses;
  statuses.reserve(tickets.size());
  std::lock_guard<std::mutex> lock(m_mutex);
  if (outs)
    outs->resize(tickets.size());
  if (!ownerAllowsLocked()) {
    for (size_t i = 0; i < tickets.size(); ++i) {
      statuses.append(WorldGenerationComputePollStatus::UnknownTicket);
      if (outs)
        (*outs)[i] = WorldGenerationComputeResult{};
    }
    return statuses;
  }
  for (size_t i = 0; i < tickets.size(); ++i) {
    WorldGenerationComputeResult* slot = outs ? &(*outs)[i] : nullptr;
    statuses.append(pollLocked(tickets[i], slot));
  }
  return statuses;
}

size_t WorldGenerationComputeService::failContext() {
  std::lock_guard<std::mutex> lock(m_mutex);
  if (!ownerAllowsLocked())
    return 0;
  size_t affected = 0;
  for (auto& pair : m_entries) {
    Entry& entry = pair.second;
    entry.contextFailed = true;
    // GPU-02: recover by recomputing on the CPU reference; one poll may publish
    // the recovered result, then the ticket id is rejected as a duplicate.
    entry.publicationRevoked = false;
    entry.backend = WorldGenerationComputeBackend::CpuReference;
    if (supported(entry.identity.kernel, entry.identity.kernelVersion)) {
      entry.output = runKernel(entry.identity.kernel, entry.identity.kernelVersion, entry.input,
          entry.parameter);
      entry.state = EntryState::Ready;
    } else {
      entry.output.clear();
      entry.state = EntryState::Failed;
    }
    ++affected;
  }
  return affected;
}

List<uint32_t> WorldGenerationComputeService::shadowEdgeCorpus() {
  // Negative sector-ish coords as uint32 two's complement, wrap edges, and
  // near-limit patterns — no world/assets required.
  return List<uint32_t>{0u, 1u, 2u, 0xffffffffu /* -1 */, 0xffffffe0u /* -32 */,
      0x80000000u, 0x7fffffffu, 0xfffffffeu, 0x80000001u, 0x55aa55aau, 0xa5a5a5a5u,
      0x0000ffffu, 0xffff0000u};
}

uint64_t WorldGenerationComputeService::fingerprintInput(List<uint32_t> const& input) noexcept {
  // FNV-1a 64 with position mix — deterministic, no assets/crypto dependency.
  uint64_t hash = 14695981039346656037ull;
  for (size_t i = 0; i < input.size(); ++i) {
    hash ^= (uint64_t)input[i];
    hash *= 1099511628211ull;
    hash ^= (uint64_t)(i + 1u) * 0x9e3779b97f4a7c15ull;
  }
  hash ^= ((uint64_t)input.size() << 32) | (uint64_t)input.size();
  return hash;
}

uint64_t WorldGenerationComputeService::fingerprintIdentity(
    WorldGenerationComputeInputIdentity const& identity) noexcept {
  uint64_t hash = 14695981039346656037ull;
  auto mix = [&](uint64_t lane) {
    hash ^= lane;
    hash *= 1099511628211ull;
  };
  mix(identity.worldEpoch);
  mix(identity.sequence);
  mix(identity.kernelVersion);
  mix((uint64_t)identity.kernel);
  return hash;
}

uint64_t WorldGenerationComputeService::fingerprintCapabilities(
    WorldGenerationComputeCapabilities const& capabilities) noexcept {
  uint64_t hash = 14695981039346656037ull;
  auto mix = [&](uint64_t lane) {
    hash ^= lane;
    hash *= 1099511628211ull;
  };
  mix(capabilities.cpuReference ? 1ull : 0ull);
  mix(capabilities.gpuOpenGlCompute ? 1ull : 0ull);
  mix(capabilities.maxWorkGroupInvocations);
  mix(capabilities.maxInFlight);
  mix(capabilities.maxImmutableBufferBytes);
  mix(capabilities.maxStagingBytes);
  return hash;
}

uint64_t WorldGenerationComputeService::fingerprintStagingSnapshot(
    WorldGenerationComputeStagingSnapshot const& snapshot) noexcept {
  uint64_t hash = 14695981039346656037ull;
  auto mix = [&](uint64_t lane) {
    hash ^= lane;
    hash *= 1099511628211ull;
  };
  mix(snapshot.stagingBytes);
  mix(snapshot.peakStagingBytes);
  mix(snapshot.maxStagingBytes);
  mix(snapshot.pooledBufferBytes);
  mix(snapshot.pooledBufferCount);
  mix(snapshot.openTickets);
  mix(snapshot.peakOpenTickets);
  mix(snapshot.inFlight);
  mix(snapshot.gpuInFlight);
  mix(snapshot.estimatedVramBytes);
  mix(snapshot.gpuCapable ? 1ull : 0ull);
  return hash;
}

uint64_t WorldGenerationComputeService::fingerprintShadowSummary(
    WorldGenerationComputeShadowSummary const& summary) noexcept {
  uint64_t hash = 14695981039346656037ull;
  auto mix = [&](uint64_t lane) {
    hash ^= lane;
    hash *= 1099511628211ull;
  };
  mix(summary.total);
  mix(summary.equal);
  mix(summary.diverged);
  mix(summary.primaryFailed);
  mix(summary.shadowUnavailable);
  mix(summary.cancelled);
  mix(summary.totalDivergenceCount);
  return hash;
}

uint32_t WorldGenerationComputeService::drawRng(WorldGenerationComputeRngStream& stream) noexcept {
  // SplitMix64-style step over (seed, streamId, counter); exact uint32 lane.
  uint64_t z = stream.seed + 0x9e3779b97f4a7c15ull
      + ((uint64_t)stream.streamId << 1) + stream.counter;
  z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
  z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
  z = z ^ (z >> 31);
  ++stream.counter;
  return (uint32_t)z;
}

List<uint32_t> WorldGenerationComputeService::fillRng(
    WorldGenerationComputeRngStream& stream, size_t count) {
  List<uint32_t> out;
  out.reserve(count);
  for (size_t i = 0; i < count; ++i)
    out.append(drawRng(stream));
  return out;
}

WorldGenerationComputeCommitReport WorldGenerationComputeService::commit(
    WorldGenerationComputeTicket ticket, WorldGenerationComputeInputIdentity const& liveIdentity,
    uint64_t liveInputFingerprint, WorldGenerationComputeResult* out) {
  std::lock_guard<std::mutex> lock(m_mutex);
  WorldGenerationComputeCommitReport report;
  report.ticket = ticket;
  report.liveIdentity = liveIdentity;
  report.liveInputFingerprint = liveInputFingerprint;

  if (!ownerAllowsLocked()) {
    report.verdict = WorldGenerationComputeCommitVerdict::UnknownTicket;
    if (out)
      *out = WorldGenerationComputeResult{};
    return report;
  }

  if (m_rejectedDuplicates.count(ticket.id)) {
    report.verdict = WorldGenerationComputeCommitVerdict::UnknownTicket;
    if (out)
      *out = WorldGenerationComputeResult{};
    return report;
  }

  auto it = m_entries.find(ticket.id);
  if (it == m_entries.end()) {
    report.verdict = WorldGenerationComputeCommitVerdict::UnknownTicket;
    if (out)
      *out = WorldGenerationComputeResult{};
    return report;
  }

  Entry& entry = it->second;
  report.submittedIdentity = entry.identity;
  report.submittedInputFingerprint = entry.inputFingerprint;
  report.backend = entry.backend;

  if (entry.publicationRevoked || entry.state == EntryState::Cancelled) {
    report.verdict = WorldGenerationComputeCommitVerdict::Cancelled;
    entry.state = EntryState::Cancelled;
    entry.output.clear();
  } else if (entry.state == EntryState::Pending) {
    report.verdict = WorldGenerationComputeCommitVerdict::NotReady;
    if (out)
      *out = WorldGenerationComputeResult{};
    return report;
  } else if (entry.state == EntryState::Failed) {
    report.verdict = WorldGenerationComputeCommitVerdict::Failed;
  } else if (entry.identity != liveIdentity) {
    report.verdict = WorldGenerationComputeCommitVerdict::StaleIdentity;
    entry.publicationRevoked = true;
    entry.state = EntryState::Cancelled;
    entry.output.clear();
  } else if (entry.inputFingerprint != liveInputFingerprint) {
    report.verdict = WorldGenerationComputeCommitVerdict::StaleInput;
    entry.publicationRevoked = true;
    entry.state = EntryState::Cancelled;
    entry.output.clear();
  } else if (entry.state == EntryState::Ready) {
    report.verdict = WorldGenerationComputeCommitVerdict::Accepted;
    if (out) {
      out->identity = entry.identity;
      out->backend = entry.backend;
      out->values = entry.output;
    }
  } else {
    report.verdict = WorldGenerationComputeCommitVerdict::Failed;
  }

  // Consume ticket (same lifetime rules as poll for terminal states).
  if (report.verdict != WorldGenerationComputeCommitVerdict::NotReady) {
    m_stagingBytes -= entry.stagingBytes;
    releaseBuffer(std::move(entry.input));
    if (entry.contextFailed
        || report.verdict == WorldGenerationComputeCommitVerdict::StaleIdentity
        || report.verdict == WorldGenerationComputeCommitVerdict::StaleInput
        || report.verdict == WorldGenerationComputeCommitVerdict::Cancelled)
      m_rejectedDuplicates.insert(ticket.id);
    m_entries.erase(it);
  }
  if (report.verdict != WorldGenerationComputeCommitVerdict::Accepted && out)
    *out = WorldGenerationComputeResult{};
  return report;
}

List<WorldGenerationComputeCommitReport> WorldGenerationComputeService::commitBatch(
    List<CommitBatchItem> items, List<WorldGenerationComputeResult>* outs) {
  List<WorldGenerationComputeCommitReport> reports;
  reports.reserve(items.size());
  if (outs) {
    outs->clear();
    outs->reserve(items.size());
  }
  for (auto const& item : items) {
    WorldGenerationComputeResult result;
    auto report = commit(item.ticket, item.liveIdentity, item.liveInputFingerprint, &result);
    reports.append(report);
    if (outs)
      outs->append(std::move(result));
  }
  return reports;
}

List<WorldGenerationComputeShadowReport> WorldGenerationComputeService::compareShadowBatch(
    List<uint32_t> const& input, List<ShadowBatchItem> items,
    WorldGenerationComputeBackend primary, WorldGenerationComputeBackend shadow) {
  List<WorldGenerationComputeShadowReport> reports;
  reports.reserve(items.size());
  for (auto const& item : items)
    reports.append(compareShadow(item.identity, input, item.parameter, primary, shadow));
  return reports;
}

WorldGenerationComputeShadowSummary WorldGenerationComputeService::summarizeShadowReports(
    List<WorldGenerationComputeShadowReport> const& reports) noexcept {
  WorldGenerationComputeShadowSummary summary;
  summary.total = reports.size();
  for (auto const& report : reports) {
    summary.totalDivergenceCount += report.divergenceCount;
    switch (report.verdict) {
      case WorldGenerationComputeShadowVerdict::Equal: ++summary.equal; break;
      case WorldGenerationComputeShadowVerdict::Diverged: ++summary.diverged; break;
      case WorldGenerationComputeShadowVerdict::PrimaryFailed: ++summary.primaryFailed; break;
      case WorldGenerationComputeShadowVerdict::ShadowUnavailable:
        ++summary.shadowUnavailable;
        break;
      case WorldGenerationComputeShadowVerdict::Cancelled: ++summary.cancelled; break;
    }
  }
  return summary;
}

WorldGenerationComputeShadowReport WorldGenerationComputeService::compareShadowBuffers(
    WorldGenerationComputeInputIdentity identity, List<uint32_t> const& input, uint32_t parameter,
    List<uint32_t> const& primaryValues, List<uint32_t> const& shadowValues,
    WorldGenerationComputeBackend primary, WorldGenerationComputeBackend shadow) {
  WorldGenerationComputeShadowReport report;
  report.identity = identity;
  report.parameter = parameter;
  report.primaryBackend = primary;
  report.shadowBackend = shadow;

  size_t n = std::min(primaryValues.size(), shadowValues.size());
  report.comparedValues = n;
  for (size_t i = 0; i < n; ++i) {
    if (primaryValues[i] != shadowValues[i]) {
      if (!report.divergenceCount) {
        report.firstDivergenceIndex = i;
        report.primarySample = primaryValues[i];
        report.shadowSample = shadowValues[i];
        report.reproducer.valid = true;
        report.reproducer.worldEpoch = identity.worldEpoch;
        report.reproducer.sequence = identity.sequence;
        report.reproducer.kernelVersion = identity.kernelVersion;
        report.reproducer.kernel = identity.kernel;
        report.reproducer.parameter = parameter;
        report.reproducer.index = i;
        report.reproducer.inputSample = i < input.size() ? input[i] : 0u;
        report.reproducer.primarySample = primaryValues[i];
        report.reproducer.shadowSample = shadowValues[i];
      }
      ++report.divergenceCount;
    }
  }
  if (primaryValues.size() != shadowValues.size())
    ++report.divergenceCount;
  report.verdict = report.divergenceCount
      ? WorldGenerationComputeShadowVerdict::Diverged
      : WorldGenerationComputeShadowVerdict::Equal;
  return report;
}

WorldGenerationComputeShadowReport WorldGenerationComputeService::compareShadow(
    WorldGenerationComputeInputIdentity identity, List<uint32_t> const& input, uint32_t parameter,
    WorldGenerationComputeBackend primary, WorldGenerationComputeBackend shadow) {
  WorldGenerationComputeShadowReport report;
  report.identity = identity;
  report.parameter = parameter;
  report.primaryBackend = primary;
  report.shadowBackend = shadow;
  report.comparedValues = input.size();

  {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!ownerAllowsLocked()) {
      report.verdict = WorldGenerationComputeShadowVerdict::ShadowUnavailable;
      return report;
    }
  }

  List<uint32_t> primaryValues;
  List<uint32_t> shadowValues;
  if (!runBackendKernel(primary, identity.kernel, identity.kernelVersion, input, parameter,
          primaryValues)) {
    report.verdict = WorldGenerationComputeShadowVerdict::PrimaryFailed;
    return report;
  }
  if (!runBackendKernel(shadow, identity.kernel, identity.kernelVersion, input, parameter,
          shadowValues)) {
    report.verdict = WorldGenerationComputeShadowVerdict::ShadowUnavailable;
    return report;
  }
  return compareShadowBuffers(identity, input, parameter, primaryValues, shadowValues, primary,
      shadow);
}

WorldGenerationComputeShadowReport WorldGenerationComputeService::compareShadowNamed(
    char const* kernelName, WorldGenerationComputeInputIdentity identity,
    List<uint32_t> const& input, uint32_t parameter, WorldGenerationComputeBackend primary,
    WorldGenerationComputeBackend shadow) {
  auto kernel = parseKernelName(kernelName);
  if (!kernel) {
    WorldGenerationComputeShadowReport report;
    report.identity = identity;
    report.parameter = parameter;
    report.primaryBackend = primary;
    report.shadowBackend = shadow;
    report.comparedValues = input.size();
    report.verdict = WorldGenerationComputeShadowVerdict::PrimaryFailed;
    return report;
  }
  identity.kernel = *kernel;
  return compareShadow(std::move(identity), input, parameter, primary, shadow);
}

}
