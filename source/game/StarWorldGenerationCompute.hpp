#pragma once

#include "StarList.hpp"
#include "StarMaybe.hpp"

#include <cstdint>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>

namespace Star {

// Optional worldgen compute path. Independent of WorldServer, Root, Lua and the
// renderer. CPU reference is always available. OpenGL compute is never claimed
// without a real 4.3+/extension probe. The probe/dispatch live behind
// STAR_ENABLE_WORLDGEN_GL_COMPUTE (diagnostic / Windows harness); normal builds
// keep gpuOpenGlCompute false.

enum class WorldGenerationComputeBackend : uint8_t {
  CpuReference = 0,
  GpuOpenGlCompute = 1
};

enum class WorldGenerationComputeKernel : uint16_t {
  // Exact-integer dense transforms (GPU-03 first candidate family; asset-free).
  DenseUint32Xor = 1,
  DenseUint32Add = 2,
  DenseUint32And = 3,
  DenseUint32Or = 4,
  DenseUint32Mul = 5, // wrapping uint32 multiply
  DenseUint32Min = 6,
  DenseUint32Max = 7,
  DenseUint32Sub = 8, // wrapping uint32 subtract (value - parameter)
  DenseUint32Shl = 9, // value << (parameter & 31)
  DenseUint32Shr = 10, // logical value >> (parameter & 31)
  DenseUint32Rotl = 11, // rotate left by (parameter & 31)
  DenseUint32Rotr = 12, // rotate right by (parameter & 31)
  DenseUint32Not = 13, // bitwise complement (~value); parameter ignored
  DenseUint32Clz = 14, // count leading zeros; 32 for zero; parameter ignored
  DenseUint32Ctz = 15, // count trailing zeros; 32 for zero; parameter ignored
  DenseUint32Popcount = 16, // population count; parameter ignored
  DenseUint32Eq = 17, // value == parameter ? 0xffffffff : 0
  DenseUint32Ne = 18, // value != parameter ? 0xffffffff : 0
  DenseUint32BitReverse = 19, // bitfield reverse; parameter ignored
  DenseUint32Lt = 20, // unsigned value < parameter ? 0xffffffff : 0
  DenseUint32Leu = 21, // unsigned value <= parameter ? 0xffffffff : 0
  DenseUint32AbsDiff = 22, // unsigned |value - parameter|
  DenseUint32Gt = 23, // unsigned value > parameter ? 0xffffffff : 0
  DenseUint32Geu = 24, // unsigned value >= parameter ? 0xffffffff : 0
  DenseUint32Xnor = 25, // ~(value ^ parameter)
  DenseUint32Nand = 26, // ~(value & parameter)
  DenseUint32Nor = 27, // ~(value | parameter)
  DenseUint32MulHi = 28, // high 32 bits of (uint64)value * parameter
  DenseUint32AndNot = 29, // value & ~parameter
  DenseUint32SatAdd = 30, // saturating add (clamp to 0xffffffff)
  DenseUint32SatSub = 31, // saturating subtract (clamp to 0)
  DenseUint32OrNot = 32, // value | ~parameter
  DenseUint32Avg = 33, // ((uint64)value + parameter) >> 1
  DenseUint32IsPow2 = 34, // nonzero power-of-two mask; parameter ignored
  DenseUint32Bswap = 35, // byte-swap; parameter ignored
  DenseUint32Parity = 36, // popcount(value) & 1; parameter ignored
  DenseUint32BitTest = 37, // ((value >> (parameter & 31)) & 1) ? 0xffffffff : 0
  DenseUint32HasZeroByte = 38, // any zero byte in value ? 0xffffffff : 0; parameter ignored
  DenseUint32NextPow2 = 39, // round up to power of two (0 stays 0; overflow wraps to 0)
  DenseUint32MaskLow = 40, // value & low-bit mask; parameter = bit count clamped to [0,32]
  DenseUint32Ilog2 = 41, // floor(log2(value)); 0 when value==0; parameter ignored
  DenseUint32BroadcastByte = 42, // replicate low 8 bits across word; parameter ignored
  DenseUint32ByteSum = 43, // sum of four bytes as uint32; parameter ignored
  DenseUint32Sext8 = 44, // sign-extend low 8 bits; parameter ignored
  DenseUint32Sext16 = 45, // sign-extend low 16 bits; parameter ignored
  DenseUint32ExtractByte = 46, // (value >> ((parameter & 3) * 8)) & 0xff
  DenseUint32BitSet = 47, // value | (1u << (parameter & 31))
  DenseUint32BitClear = 48, // value & ~(1u << (parameter & 31))
  DenseUint32BitToggle = 49, // value ^ (1u << (parameter & 31))
  DenseUint32Neg = 50, // 0 - value (two's complement); parameter ignored
  DenseUint32Abs = 51, // signed absolute value as uint32 bits; parameter ignored
  DenseUint32RolBytes = 52, // rotate left by 8 * (parameter & 3) bits
  DenseUint32RorBytes = 53, // rotate right by 8 * (parameter & 3) bits
  DenseUint32SignMask = 54, // (int32)value < 0 ? 0xffffffff : 0; parameter ignored
  DenseUint32IsZero = 55, // value == 0 ? 0xffffffff : 0; parameter ignored
  DenseUint32IsNonzero = 56, // value != 0 ? 0xffffffff : 0; parameter ignored
  DenseUint32MaskHigh = 57, // value & high-bit mask; parameter = bit count clamped to [0,32]
  DenseUint32SatMul = 58, // saturating multiply (clamp product to 0xffffffff)
  DenseUint32SwapHalves = 59, // swap high/low 16-bit halves; parameter ignored
  DenseUint32Clo = 60, // count leading ones; parameter ignored
  DenseUint32Cto = 61, // count trailing ones; parameter ignored
  DenseUint32ReverseNibbles = 62, // reverse 4-bit nibbles; parameter ignored
  DenseUint32ByteMax = 63, // max of four bytes as uint32; parameter ignored
  DenseUint32ByteMin = 64, // min of four bytes as uint32; parameter ignored
  DenseUint32DupLow16 = 65, // duplicate low 16 bits into both halves; parameter ignored
  DenseUint32DupHigh16 = 66, // duplicate high 16 bits into both halves; parameter ignored
  DenseUint32ReverseBitsInBytes = 67, // reverse bits inside each byte; parameter ignored
  DenseUint32IsolateLsb = 68, // value & -value; parameter ignored
  DenseUint32IsolateMsb = 69, // highest set bit only; 0 when value==0; parameter ignored
  DenseUint32LsbMask = 70, // value ^ (value - 1); bits through lowest set bit
  DenseUint32ClearLsb = 71, // value & (value - 1); parameter ignored
  DenseUint32GrayEncode = 72, // value ^ (value >> 1); parameter ignored
  DenseUint32GrayDecode = 73, // binary from reflected Gray code; parameter ignored
  DenseUint32SmearLsb = 74, // value | (value - 1); ones through lowest set bit
  DenseUint32ClearMsb = 75, // clear highest set bit; 0 stays 0; parameter ignored
  DenseUint32ByteXor = 76, // XOR of the four bytes as uint32; parameter ignored
  DenseUint32SmearMsb = 77, // fill ones downward from highest set bit; 0 stays 0
  DenseUint32ByteOr = 78, // OR of the four bytes as uint32; parameter ignored
  DenseUint32ByteAnd = 79, // AND of the four bytes as uint32; parameter ignored
  DenseUint32KeepOddBits = 80, // value & 0x55555555; parameter ignored
  DenseUint32KeepEvenBits = 81, // value & 0xaaaaaaaa; parameter ignored
  DenseUint32SwapAdjacentBits = 82, // swap bit pairs 0\leftrightarrow1, 2\leftrightarrow3, …; parameter ignored
  DenseUint32CompressOddBits = 83, // pack bits 0,2,4,… into low 16 bits; parameter ignored
  DenseUint32CompressEvenBits = 84, // pack bits 1,3,5,… into low 16 bits; parameter ignored
  DenseUint32ExpandLow16ToOdd = 85, // place low 16 bits into positions 0,2,4,…; parameter ignored
  DenseUint32ExpandLow16ToEven = 86, // place low 16 bits into positions 1,3,5,…; parameter ignored
  DenseUint32InterleaveHalves = 87, // Morton-merge high/low 16-bit halves; parameter ignored
  DenseUint32DeinterleaveHalves = 88, // split odd/even bits into low/high 16-bit halves; parameter ignored
  DenseUint32NibbleXor = 89, // XOR of eight nibbles as uint32; parameter ignored
  DenseUint32NibbleOr = 90, // OR of eight nibbles as uint32; parameter ignored
  DenseUint32NibbleAnd = 91, // AND of eight nibbles as uint32; parameter ignored
  DenseUint32NibbleSum = 92, // sum of eight nibbles as uint32; parameter ignored
  DenseUint32NibbleMax = 93, // max of eight nibbles as uint32; parameter ignored
  DenseUint32NibbleMin = 94, // min of eight nibbles as uint32; parameter ignored
  DenseUint32ExtractNibble = 95, // (value >> ((parameter & 7) * 4)) & 0xf
  DenseUint32InsertNibble = 96, // replace nibble (parameter & 7) with (parameter >> 8) & 0xf
  DenseUint32BroadcastNibble = 97, // replicate low 4 bits across all nibbles; parameter ignored
  DenseUint32SwapAdjacentNibbles = 98, // swap nibble pairs 0↔1, 2↔3, …; parameter ignored
  DenseUint32RolNibbles = 99, // rotate left by (parameter & 7) nibbles
  DenseUint32RorNibbles = 100, // rotate right by (parameter & 7) nibbles
  DenseUint32HasZeroNibble = 101, // 0xffffffff if any nibble is 0, else 0; parameter ignored
  DenseUint32SextNibble = 102, // sign-extend nibble (parameter & 7) to 32 bits
  DenseUint32CountZeroNibbles = 103, // count of zero nibbles (0..8); parameter ignored
  DenseUint32PackLowNibbles = 104, // pack low nibble of each byte into low 16 bits
  DenseUint32PackHighNibbles = 105, // pack high nibble of each byte into low 16 bits
  DenseUint32ExpandLowNibbles = 106, // expand low 16 packed nibbles into low half of each byte
  DenseUint32ExpandHighNibbles = 107, // expand low 16 packed nibbles into high half of each byte
  DenseUint32ReplaceNibble = 108, // replace nibbles==(parameter&0xf) with (parameter>>8)&0xf
  DenseUint32MaskNibbles = 109, // keep nibbles whose bit in (parameter&0xff) is set; clear others
  DenseUint32AllNibblesEqual = 110, // 0xffffffff if all eight nibbles equal, else 0
  DenseUint32NibbleRange = 111, // max(nibbles) - min(nibbles); parameter ignored
  DenseUint32BroadcastNibbleAt = 112, // broadcast nibble (parameter & 7) across all nibbles
  DenseUint32HalfSum = 113, // (value & 0xffff) + (value >> 16); parameter ignored
  DenseUint32HalfMax = 114, // max(low16, high16); parameter ignored
  DenseUint32HalfMin = 115, // min(low16, high16); parameter ignored
  DenseUint32HalfXor = 116, // (value & 0xffff) ^ (value >> 16); parameter ignored
  DenseUint32HalfOr = 117, // (value & 0xffff) | (value >> 16); parameter ignored
  DenseUint32HalfAnd = 118, // (value & 0xffff) & (value >> 16); parameter ignored
  DenseUint32BroadcastHalf = 119, // replicate low 16 bits into both halves; parameter ignored
  DenseUint32HalfEq = 120, // 0xffffffff if low16==high16, else 0; parameter ignored
  DenseUint32HalfRange = 121, // abs(low16 - high16); parameter ignored
  DenseUint32HalfLt = 122, // 0xffffffff if low16 < high16, else 0; parameter ignored
  DenseUint32HalfGt = 123, // 0xffffffff if low16 > high16, else 0; parameter ignored
  DenseUint32HalfNe = 124, // 0xffffffff if low16 != high16, else 0; parameter ignored
  DenseUint32HalfLeu = 125, // 0xffffffff if low16 <= high16, else 0; parameter ignored
  DenseUint32HalfGeu = 126, // 0xffffffff if low16 >= high16, else 0; parameter ignored
  DenseUint32HalfAvg = 127, // (low16 + high16) >> 1; parameter ignored
  DenseUint32BroadcastHighHalf = 128, // replicate high 16 bits into both halves; parameter ignored
  DenseUint32HalfSub = 129, // low16 - high16 (uint32 wrap); parameter ignored
  DenseUint32HalfMul = 130, // low16 * high16 (uint32 product); parameter ignored
  DenseUint32HalfSatAdd = 131, // min(low16 + high16, 0xffff); parameter ignored
  DenseUint32HalfSatSub = 132, // low16 > high16 ? low16 - high16 : 0; parameter ignored
  DenseUint32HalfMulHi = 133, // (low16 * high16) >> 16; parameter ignored
  DenseUint32HalfDiv = 134, // high16==0 ? 0 : low16 / high16; parameter ignored
  DenseUint32HalfMod = 135, // high16==0 ? 0 : low16 % high16; parameter ignored
  DenseUint32HalfCeilDiv = 136, // high16==0 ? 0 : (low16 + high16 - 1) / high16; parameter ignored
  DenseUint32HalfIsDivisible = 137, // 0xffffffff if high16!=0 && low16%high16==0, else 0; parameter ignored
  DenseUint32HalfGcd = 138, // gcd(low16, high16); 0 if both 0; parameter ignored
  DenseUint32HalfSelect = 139, // (parameter & 1) ? high16 : low16; parameter selects half
  DenseUint32HalfLcm = 140, // lcm(low16, high16); 0 if either 0; parameter ignored
  DenseUint32HalfCoprime = 141, // 0xffffffff if gcd(low16, high16)==1, else 0; parameter ignored
  DenseUint32HalfAlignUp = 142, // high16==0 ? low16 : ((low16 + high16 - 1) / high16) * high16; parameter ignored
  DenseUint32HalfAlignDown = 143, // high16==0 ? low16 : (low16 / high16) * high16; parameter ignored
  DenseUint32HalfRotl = 144, // rotate low16 left by (high16 & 15); parameter ignored
  DenseUint32HalfRotr = 145, // rotate low16 right by (high16 & 15); parameter ignored
  DenseUint32HalfPopcount = 146, // popcount(low16); parameter ignored
  DenseUint32HalfClz = 147, // leading zero count of low16 in 16-bit width; 16 if low16==0; parameter ignored
  DenseUint32HalfCtz = 148, // trailing zero count of low16; 16 if low16==0; parameter ignored
  DenseUint32HalfParity = 149, // popcount(low16) & 1; parameter ignored
  DenseUint32HalfBitReverse = 150, // reverse bits of low16; parameter ignored
  DenseUint32HalfBswap = 151, // swap bytes of low16; parameter ignored
  DenseUint32HalfNot = 152, // (~low16) & 0xffff; parameter ignored
  DenseUint32HalfNeg = 153, // (0 - low16) & 0xffff; parameter ignored
  DenseUint32HalfAbs = 154, // signed abs of low16 as uint16; 0x8000 stays 0x8000; parameter ignored
  DenseUint32HalfSignMask = 155, // (int16)low16 < 0 ? 0xffffffff : 0; parameter ignored
  DenseUint32HalfIsZero = 156, // low16==0 ? 0xffffffff : 0; parameter ignored
  DenseUint32HalfIsNonzero = 157, // low16!=0 ? 0xffffffff : 0; parameter ignored
  DenseUint32HalfClo = 158, // count leading ones of low16 in 16-bit width; 16 if low16==0xffff; parameter ignored
  DenseUint32HalfCto = 159, // count trailing ones of low16; 16 if low16==0xffff; parameter ignored
  DenseUint32HalfIsPow2 = 160, // low16 nonzero power-of-two mask; parameter ignored
  DenseUint32HalfIlog2 = 161, // floor(log2(low16)); 0 when low16==0; parameter ignored
  DenseUint32HalfNextPow2 = 162, // round low16 up to power of two (0 stays 0; overflow wraps to 0)
  DenseUint32HalfMaskLow = 163, // low16 & low-bit mask; parameter = bit count clamped to [0,16]
  DenseUint32HalfMaskHigh = 164, // low16 & high-bit mask; parameter = bit count clamped to [0,16]
  DenseUint32HalfBitTest = 165, // ((low16 >> (parameter & 15)) & 1) ? 0xffffffff : 0
  DenseUint32HalfIsolateLsb = 166, // low16 & -low16; parameter ignored
  DenseUint32HalfIsolateMsb = 167, // highest set bit of low16; 0 when low16==0; parameter ignored
  DenseUint32HalfClearLsb = 168, // low16 & (low16 - 1); parameter ignored
  DenseUint32HalfLsbMask = 169, // low16 ^ (low16 - 1) in 16-bit width; parameter ignored
  DenseUint32HalfGrayEncode = 170, // low16 ^ (low16 >> 1); parameter ignored
  DenseUint32HalfGrayDecode = 171, // binary from reflected Gray code of low16; parameter ignored
  DenseUint32HalfSmearLsb = 172, // low16 | (low16 - 1) in 16-bit width; parameter ignored
  DenseUint32HalfClearMsb = 173, // clear highest set bit of low16; 0 stays 0; parameter ignored
  DenseUint32HalfSmearMsb = 174, // fill ones downward from highest set bit of low16; 0 stays 0
  DenseUint32HalfKeepOddBits = 175, // low16 & 0x5555; parameter ignored
  DenseUint32HalfKeepEvenBits = 176, // low16 & 0xaaaa; parameter ignored
  DenseUint32HalfSwapAdjacentBits = 177, // swap bit pairs in low16; parameter ignored
  DenseUint32HalfCompressOddBits = 178, // pack odd bits of low16 into low 8 bits; parameter ignored
  DenseUint32HalfCompressEvenBits = 179, // pack even bits of low16 into low 8 bits; parameter ignored
  DenseUint32HalfExpandLow8ToOdd = 180, // expand low 8 bits into odd positions of low16
  DenseUint32HalfExpandLow8ToEven = 181, // expand low 8 bits into even positions of low16
  DenseUint32HalfInterleaveBytes = 182, // Morton-merge high/low bytes of low16; parameter ignored
  DenseUint32HalfDeinterleaveBytes = 183, // split odd/even of low16 into low/high bytes
  DenseUint32HalfSwapBytes = 184, // swap bytes of low16; parameter ignored
  DenseUint32HalfNibbleXor = 185, // XOR of four nibbles of low16; parameter ignored
  DenseUint32HalfNibbleOr = 186, // OR of four nibbles of low16; parameter ignored
  DenseUint32HalfNibbleAnd = 187, // AND of four nibbles of low16; parameter ignored
  DenseUint32HalfNibbleSum = 188, // sum of four nibbles of low16; parameter ignored
  DenseUint32HalfNibbleMax = 189, // max of four nibbles of low16; parameter ignored
  DenseUint32HalfNibbleMin = 190, // min of four nibbles of low16; parameter ignored
  DenseUint32HalfSwapAdjacentNibbles = 191, // swap nibble pairs in low16; parameter ignored
  DenseUint32HalfRolNibbles = 192, // rotate low16 left by (parameter & 3) nibbles
  DenseUint32HalfRorNibbles = 193, // rotate low16 right by (parameter & 3) nibbles
  DenseUint32HalfHasZeroNibble = 194, // 0xffffffff if any low16 nibble is 0, else 0
  DenseUint32HalfCountZeroNibbles = 195, // count of zero nibbles in low16 (0..4)
  DenseUint32HalfBroadcastNibble = 196, // replicate low 4 bits across low16; parameter ignored
  DenseUint32HalfSextNibble = 197, // sign-extend nibble (parameter & 3) of low16 to 32 bits
  DenseUint32HalfExtractNibble = 198, // (low16 >> ((parameter & 3) * 4)) & 0xf
  DenseUint32HalfInsertNibble = 199, // replace nibble (parameter & 3) of low16 with (parameter >> 8) & 0xf
  DenseUint32HalfAllNibblesEqual = 200, // 0xffffffff if four low16 nibbles equal, else 0
  DenseUint32HalfNibbleRange = 201, // max(nibbles)-min(nibbles) of low16; parameter ignored
  DenseUint32HalfMaskNibbles = 202, // keep low16 nibbles whose bit in (parameter&0xf) is set
  DenseUint32HalfBroadcastNibbleAt = 203, // broadcast nibble (parameter & 3) across low16
  DenseUint32HalfReplaceNibble = 204, // replace low16 nibbles==(parameter&0xf) with (parameter>>8)&0xf
  DenseUint32HalfReverseNibbles = 205, // reverse four nibbles of low16; parameter ignored
  DenseUint32HalfWidenNibbles = 206, // each low16 nibble becomes a byte (little-endian); parameter ignored
  DenseUint32HalfDupNibbles = 207, // duplicate each low16 nibble into a byte; parameter ignored
  DenseUint32HalfFoldAdjacentNibbles = 208, // XOR nibble pairs 0^1 and 2^3 into low byte
  DenseUint32HalfCountNonzeroNibbles = 209, // count nonzero nibbles in low16 (0..4)
  DenseUint32HalfAddNibble = 210, // add (parameter & 0xf) to each low16 nibble (wrap 4-bit)
  DenseUint32HalfSatAddNibble = 211, // add (parameter & 0xf) to each low16 nibble (saturate to 0xf)
  DenseUint32HalfSubNibble = 212, // subtract (parameter & 0xf) from each low16 nibble (wrap 4-bit)
  DenseUint32HalfSatSubNibble = 213, // subtract (parameter & 0xf) from each low16 nibble (floor 0)
  DenseUint32HalfMulNibble = 214, // multiply each low16 nibble by (parameter & 0xf) (wrap 4-bit)
  DenseUint32HalfSatMulNibble = 215, // multiply each low16 nibble by (parameter & 0xf) (saturate to 0xf)
  DenseUint32HalfDivNibble = 216, // divide each low16 nibble by (parameter & 0xf); 0 if divisor==0
  DenseUint32HalfModNibble = 217, // each low16 nibble % (parameter & 0xf); 0 if divisor==0
  DenseUint32HalfMinNibble = 218, // min(each low16 nibble, parameter & 0xf)
  DenseUint32HalfMaxNibble = 219, // max(each low16 nibble, parameter & 0xf)
  DenseUint32HalfClampNibble = 220, // clamp each low16 nibble to [(parameter&0xf), ((parameter>>8)&0xf)]
  DenseUint32HalfAndNibble = 221, // AND each low16 nibble with (parameter & 0xf)
  DenseUint32HalfOrNibble = 222, // OR each low16 nibble with (parameter & 0xf)
  DenseUint32HalfXorNibble = 223, // XOR each low16 nibble with (parameter & 0xf)
  DenseUint32HalfShlNibble = 224, // shift each low16 nibble left by (parameter & 3) (wrap 4-bit)
  DenseUint32HalfShrNibble = 225, // shift each low16 nibble right by (parameter & 3)
  DenseUint32HalfRolNibble = 226, // rotate each low16 nibble left by (parameter & 3)
  DenseUint32HalfRorNibble = 227, // rotate each low16 nibble right by (parameter & 3)
  DenseUint32HalfNotNibble = 228, // (~each low16 nibble) & 0xf; parameter ignored
  DenseUint32HalfNegNibble = 229, // (0 - each low16 nibble) & 0xf; parameter ignored
  DenseUint32HalfAbsNibble = 230, // signed abs of each low16 nibble; 0x8 stays 0x8
  DenseUint32HalfSignMaskNibble = 231, // 0xf if nibble high bit set else 0; parameter ignored
  DenseUint32HalfIsZeroNibble = 232, // 0xf if nibble==0 else 0; parameter ignored
  DenseUint32HalfIsNonzeroNibble = 233, // 0xf if nibble!=0 else 0; parameter ignored
  DenseUint32HalfEqNibble = 234, // 0xf if nibble==(parameter&0xf) else 0
  DenseUint32HalfNeNibble = 235, // 0xf if nibble!=(parameter&0xf) else 0
  DenseUint32HalfLtNibble = 236, // 0xf if nibble < (parameter&0xf) else 0
  DenseUint32HalfLeNibble = 237, // 0xf if nibble <= (parameter&0xf) else 0
  DenseUint32HalfGtNibble = 238, // 0xf if nibble > (parameter&0xf) else 0
  DenseUint32HalfGeNibble = 239, // 0xf if nibble >= (parameter&0xf) else 0
  DenseUint32HalfBetweenNibble = 240, // 0xf if nibble in [(parameter&0xf),((parameter>>8)&0xf)]
  DenseUint32HalfOutsideNibble = 241, // 0xf if nibble outside [(parameter&0xf),((parameter>>8)&0xf)]
  DenseUint32HalfPopcountNibble = 242, // popcount of each low16 nibble (0..4); parameter ignored
  DenseUint32HalfParityNibble = 243, // popcount(nibble)&1 for each low16 nibble; parameter ignored
  DenseUint32HalfReverseBitsNibble = 244, // reverse 4 bits of each low16 nibble; parameter ignored
  DenseUint32HalfIsolateLsbNibble = 245, // nibble & -nibble for each low16 nibble; parameter ignored
  DenseUint32HalfClearLsbNibble = 246, // nibble & (nibble-1) for each low16 nibble; parameter ignored
  DenseUint32HalfSmearLsbNibble = 247, // nibble | (nibble-1) for each low16 nibble; parameter ignored
  DenseUint32HalfIsolateMsbNibble = 248, // highest set bit of each low16 nibble; 0 stays 0
  DenseUint32HalfClearMsbNibble = 249, // clear highest set bit of each low16 nibble; 0 stays 0
  DenseUint32HalfSmearMsbNibble = 250, // fill ones downward from MSB of each low16 nibble; 0 stays 0
  DenseUint32HalfLsbMaskNibble = 251, // nibble ^ (nibble-1) for each low16 nibble; parameter ignored
  DenseUint32HalfKeepOddBitsNibble = 252, // each low16 nibble & 0x5; parameter ignored
  DenseUint32HalfKeepEvenBitsNibble = 253, // each low16 nibble & 0xa; parameter ignored
  DenseUint32HalfSwapAdjacentBitsNibble = 254, // swap bit pairs in each low16 nibble; parameter ignored
  DenseUint32HalfGrayEncodeNibble = 255, // nibble ^ (nibble >> 1) for each low16 nibble; parameter ignored
  DenseUint32HalfGrayDecodeNibble = 256, // binary from Gray code of each low16 nibble; parameter ignored
  DenseUint32HalfClzNibble = 257, // leading zero count of each low16 nibble in 4-bit width; 4 if 0
  DenseUint32HalfCtzNibble = 258, // trailing zero count of each low16 nibble; 4 if 0
  DenseUint32HalfCloNibble = 259, // leading one count of each low16 nibble in 4-bit width; 4 if 0xf
  DenseUint32HalfCtoNibble = 260, // trailing one count of each low16 nibble; 4 if 0xf
  DenseUint32HalfBitLengthNibble = 261, // floor(log2(n))+1 per low16 nibble; 0 if n==0
  DenseUint32HalfIsPowerOfTwoNibble = 262, // 1 in nibble if power-of-two (incl. 1); else 0; parameter ignored
  DenseUint32HalfNextPow2Nibble = 263, // next power-of-two per low16 nibble (0->0; >8 clamps to 0xf)
  DenseUint32HalfPrevPow2Nibble = 264, // largest power-of-two <= nibble (0->0)
  DenseUint32HalfBroadcastLsbNibble = 265, // fill each low16 nibble with its LSB (0 or 0xf)
  DenseUint32HalfBroadcastMsbNibble = 266, // fill each low16 nibble with its MSB (0 or 0xf)
  DenseUint32HalfParityFillNibble = 267, // 0xf if nibble popcount odd, else 0; parameter ignored
  DenseUint32HalfRotl1Nibble = 268, // rotate-left by 1 within each low16 nibble; parameter ignored
  DenseUint32HalfRotr1Nibble = 269, // rotate-right by 1 within each low16 nibble; parameter ignored
  DenseUint32HalfNegateNibble = 270, // bitwise not of each low16 nibble (&0xf); parameter ignored
  DenseUint32HalfIncWrapNibble = 271, // (n+1)&0xf per low16 nibble; parameter ignored
  DenseUint32HalfDecWrapNibble = 272, // (n-1)&0xf per low16 nibble; parameter ignored
  DenseUint32HalfNandNibble = 273, // ~(n & (parameter&0xf)) & 0xf per low16 nibble
  DenseUint32HalfNorNibble = 274, // ~(n | (parameter&0xf)) & 0xf per low16 nibble
  DenseUint32HalfXnorNibble = 275, // ~(n ^ (parameter&0xf)) & 0xf per low16 nibble
  DenseUint32HalfMajNibble = 276, // bitwise majority(n, parameter&0xf, (parameter>>4)&0xf) per nibble
  DenseUint32HalfMuxNibble = 277, // (sel ? b : a) per bit; a=n, b=parameter&0xf, sel=(parameter>>4)&0xf
  DenseUint32HalfImpliesNibble = 278, // (~n | (parameter&0xf)) & 0xf per low16 nibble
  DenseUint32HalfNeqFillNibble = 279, // 0xf if nibble != (parameter&0xf), else 0
  DenseUint32HalfAbsDiffNibble = 280, // abs(n - (parameter&0xf)) per low16 nibble
  DenseUint32HalfEqFillNibble = 281, // 0xf if nibble == (parameter&0xf), else 0
  DenseUint32HalfOddFillNibble = 282, // 0xf if nibble odd, else 0; parameter ignored
  DenseUint32HalfEvenFillNibble = 283, // 0xf if nibble even, else 0; parameter ignored
  DenseUint32HalfLtFillNibble = 284, // 0xf if nibble < (parameter&0xf), else 0
  DenseUint32HalfLeFillNibble = 285, // 0xf if nibble <= (parameter&0xf), else 0
  DenseUint32HalfGtFillNibble = 286, // 0xf if nibble > (parameter&0xf), else 0
  DenseUint32HalfGeFillNibble = 287, // 0xf if nibble >= (parameter&0xf), else 0
  DenseUint32HalfSatIncNibble = 288, // min(n+1, 0xf) per low16 nibble; parameter ignored
  DenseUint32HalfSatDecNibble = 289, // max(n-1, 0) per low16 nibble; parameter ignored
  DenseUint32HalfRotl2Nibble = 290, // rotate-left by 2 within each low16 nibble; parameter ignored
  DenseUint32HalfRotl3Nibble = 291, // rotate-left by 3 within each low16 nibble; parameter ignored
  DenseUint32HalfAvgNibble = 292, // ((n + (parameter&0xf)) >> 1) per low16 nibble
  DenseUint32HalfMidNibble = 293, // median(n, parameter&0xf, (parameter>>4)&0xf) per nibble
  DenseUint32HalfMin3Nibble = 294, // min(n, parameter&0xf, (parameter>>4)&0xf) per nibble
  DenseUint32HalfMax3Nibble = 295, // max(n, parameter&0xf, (parameter>>4)&0xf) per nibble
  DenseUint32HalfRange3Nibble = 296, // max3-min3 of (n, parameter&0xf, (parameter>>4)&0xf) per nibble
  DenseUint32HalfSum3Nibble = 297, // (n + (parameter&0xf) + ((parameter>>4)&0xf)) & 0xf per nibble
  DenseUint32HalfSatSum3Nibble = 298, // min(0xf, n + (parameter&0xf) + ((parameter>>4)&0xf)) per nibble
  DenseUint32HalfAvg3Nibble = 299, // (n + (parameter&0xf) + ((parameter>>4)&0xf)) / 3 per nibble
  DenseUint32HalfXor3Nibble = 300, // n ^ (parameter&0xf) ^ ((parameter>>4)&0xf) per nibble
  DenseUint32HalfMulAddNibble = 301, // (n * (parameter&0xf) + ((parameter>>4)&0xf)) & 0xf per nibble
  DenseUint32HalfSatMulAddNibble = 302, // min(0xf, n*(parameter&0xf)+((parameter>>4)&0xf)) per nibble
  DenseUint32HalfMulSubNibble = 303, // (n*(parameter&0xf) - ((parameter>>4)&0xf)) & 0xf per nibble
  DenseUint32HalfEqual3FillNibble = 304, // 0xf if n==(parameter&0xf)==((parameter>>4)&0xf), else 0
  DenseUint32HalfNeq3FillNibble = 305, // 0xf unless n==(parameter&0xf)==((parameter>>4)&0xf)
  DenseUint32HalfAnyEqFillNibble = 306, // 0xf if any equality among n and two param nibbles
  DenseUint32HalfAllDistinctFillNibble = 307, // 0xf if n and two param nibbles are pairwise distinct
  DenseUint32HalfBlendNibble = 308, // ((n*(15-w)+p*w)/15); w=parameter&0xf, p=(parameter>>4)&0xf
  DenseUint32HalfWeightedAddNibble = 309, // (n*w + p*(15-w)) & 0xf; w/p as Blend
  DenseUint32HalfBitMergeNibble = 310, // (n & ~m) | (p & m); m=parameter&0xf, p=(parameter>>4)&0xf
  DenseUint32HalfSatWeightedAddNibble = 311, // min(0xf, n*w + p*(15-w)); w/p as WeightedAdd
  DenseUint32HalfMaskSelectNibble = 312, // (n & m) ? p : n; m=parameter&0xf, p=(parameter>>4)&0xf
  DenseUint32HalfIfEqPickNibble = 313, // n==(parameter&0xf) ? ((parameter>>4)&0xf) : n
  DenseUint32HalfIfNePickNibble = 314, // n!=(parameter&0xf) ? ((parameter>>4)&0xf) : n
  DenseUint32HalfThresholdPickNibble = 315, // n>=(parameter&0xf) ? ((parameter>>4)&0xf) : n
  DenseUint32HalfModMulNibble = 316, // (n*(parameter&0xf)) % m; m=(parameter>>4)&0xf, m==0 -> 0
  DenseUint32HalfModAddNibble = 317, // (n+(parameter&0xf)) % m; m=(parameter>>4)&0xf, m==0 -> 0
  DenseUint32HalfModSubNibble = 318, // (n+m-(parameter&0xf)%m)%m; m=(parameter>>4)&0xf, m==0 -> 0
  DenseUint32HalfCeilDivNibble = 319, // (n+d-1)/d; d=parameter&0xf, d==0 -> 0; high nibble ignored
  DenseUint32HalfFloorDivNibble = 320, // n/d; d=parameter&0xf, d==0 -> 0; high nibble ignored
  DenseUint32HalfRoundDivNibble = 321, // (n+d/2)/d; d=parameter&0xf, d==0 -> 0; high nibble ignored
  DenseUint32HalfRemNibble = 322, // n%d; d=parameter&0xf, d==0 -> 0; high nibble ignored
  DenseUint32HalfShlWrapNibble = 323, // (n<<(parameter&3))&0xf; high nibble ignored
  DenseUint32HalfShrWrapNibble = 324, // n>>(parameter&3); high nibble ignored
  DenseUint32HalfFunnelShiftNibble = 325, // ((n<<k)|(((parameter>>4)&0xf)>>(4-k)))&0xf; k=parameter&3
  DenseUint32HalfRotlWrapNibble = 326, // rotate-left by (parameter&3) within nibble; high ignored
  DenseUint32HalfRotrWrapNibble = 327, // rotate-right by (parameter&3) within nibble; high ignored
  DenseUint32HalfBitPairSwapNibble = 328, // swap bit-pairs (1:0)<->(3:2); parameter ignored
  DenseUint32HalfNibbleMirror = 329, // reverse 4 bits in each low16 nibble; parameter ignored
  DenseUint32HalfXorShiftNibble = 330, // (n ^ (n<<(parameter&3))) & 0xf; high ignored
  DenseUint32HalfAddCarryFillNibble = 331, // 0xf if n+(parameter&0xf) > 15 else 0; high ignored
  DenseUint32HalfSubBorrowFillNibble = 332, // 0xf if n < (parameter&0xf) else 0; high ignored
  DenseUint32HalfMulOverflowFillNibble = 333, // 0xf if n*(parameter&0xf) > 15 else 0; high ignored
  DenseUint32HalfInterleaveLowNibble = 334, // interleave low2 of n with low2 of (parameter&0xf); high ignored
  DenseUint32HalfDeinterleaveEvenNibble = 335, // pack even bits of n into low2; high ignored
  DenseUint32HalfDeinterleaveOddNibble = 336, // pack odd bits of n into low2; high ignored
  DenseUint32HalfBitDepositNibble = 337, // PDEP-like: deposit low bits of n into mask (parameter&0xf); high ignored
  DenseUint32HalfBitExtractNibble = 338, // PEXT-like: extract bits of n under mask (parameter&0xf) to low; high ignored
  DenseUint32HalfReversePairsNibble = 339, // swap adjacent bit pairs within nibble; high ignored
  DenseUint32HalfSatShlNibble = 340, // saturate (n << (parameter&3)) to 0xf per nibble; high ignored
  DenseUint32HalfRoundShrNibble = 341, // round-half-up (n >> (parameter&3)) for k>0; high ignored
  DenseUint32HalfMinParamNibble = 342, // min(n, parameter&0xf) per nibble; high ignored
  DenseUint32HalfAvgTruncNibble = 343, // (n + (parameter&0xf)) >> 1 per nibble; high ignored
  DenseUint32HalfMaxParamNibble = 344, // max(n, parameter&0xf) per nibble; high ignored
  DenseUint32HalfAbsDiffParamNibble = 345, // |n - (parameter&0xf)| per nibble; high ignored
  DenseUint32HalfMulHiNibble = 346, // high nibble of n*(parameter&0xf); high ignored
  DenseUint32HalfSquareNibble = 347, // (n*n)&0xf per nibble; high ignored
  DenseUint32HalfIsOneFillNibble = 348, // 0xf if n==1 else 0; high ignored
  DenseUint32HalfRevBitsNibble = 349, // reverse 4 bits of each nibble; high ignored
  DenseUint32HalfCubeNibble = 350, // (n*n*n)&0xf per nibble; high ignored
  DenseUint32HalfSatSquareNibble = 351, // min(n*n, 0xf) per nibble; high ignored
  DenseUint32HalfOnesCompNibble = 352, // n ^ 0xf per nibble; high ignored
  DenseUint32HalfTwosCompNibble = 353, // (-n)&0xf per nibble; high ignored
  DenseUint32HalfIncNibble = 354, // (n+1)&0xf per nibble; high ignored
  DenseUint32HalfToggleBitNibble = 355, // n ^ (1<<(parameter&3)) per nibble; high ignored
  DenseUint32HalfDecNibble = 356, // (n-1)&0xf per nibble; high ignored
  DenseUint32HalfSetBitNibble = 357, // n | (1<<(parameter&3)) per nibble; high ignored
  DenseUint32HalfClearBitNibble = 358, // n & ~(1<<(parameter&3)) per nibble; high ignored
  DenseUint32HalfTestBitNibble = 359, // 0xf if bit (parameter&3) set else 0; high ignored
  DenseUint32HalfExtractBitNibble = 360, // (n>>(parameter&3))&1 to low bit; high ignored
  DenseUint32HalfKeepLowBitsNibble = 361, // n & ((1<<((parameter&3)+1))-1); high ignored
  DenseUint32HalfAndNotNibble = 362, // n & ~(parameter&0xf) per nibble; high ignored
  DenseUint32HalfOrNotNibble = 363, // (n | ~(parameter&0xf)) & 0xf; high ignored
  DenseUint32HalfXorNotNibble = 364, // n ^ (~(parameter&0xf) & 0xf); high ignored
  DenseUint32HalfKeepHighBitsNibble = 365, // keep high ((parameter&3)+1) bits; high ignored
  DenseUint32HalfScatterLsbNibble = 366, // (n&1)<<(parameter&3); high ignored
  DenseUint32HalfInsertBitNibble = 367, // clear bit (parameter&3), insert ((parameter>>2)&1); high ignored
  DenseUint32HalfAvgRoundNibble = 368, // (n+(parameter&0xf)+1)>>1 per nibble; high ignored
  DenseUint32HalfMulLoNibble = 369, // (n*(parameter&0xf))&0xf per nibble; high ignored
  DenseUint32HalfAddSatParamNibble = 370, // min(n+(parameter&0xf), 0xf); high ignored
  DenseUint32HalfSubSatParamNibble = 371, // max(n-(parameter&0xf), 0); high ignored
  DenseUint32HalfNextMultipleNibble = 372, // align-up to multiple of (parameter&0xf), sat 0xf; high ignored
  DenseUint32HalfPrevMultipleNibble = 373, // align-down to multiple of (parameter&0xf); high ignored
  DenseUint32HalfMulSatNibble = 374, // min(n*(parameter&0xf), 0xf); high ignored
  DenseUint32HalfStepNibble = 375, // 0xf if n>=(parameter&0xf) else 0; high ignored
  DenseUint32HalfGateNibble = 376, // n if n>=(parameter&0xf) else 0; high ignored
  DenseUint32HalfPassLtNibble = 377, // n if n<(parameter&0xf) else 0; high ignored
  DenseUint32HalfPassGtNibble = 378, // n if n>(parameter&0xf) else 0; high ignored
  DenseUint32HalfStepLtNibble = 379, // 0xf if n<(parameter&0xf) else 0; high ignored
  DenseUint32HalfPassLeNibble = 380, // n if n<=(parameter&0xf) else 0; high ignored
  DenseUint32HalfStepGtNibble = 381, // 0xf if n>(parameter&0xf) else 0; high ignored
  DenseUint32HalfStepLeNibble = 382, // 0xf if n<=(parameter&0xf) else 0; high ignored
  DenseUint32HalfPassEqNibble = 383, // n if n==(parameter&0xf) else 0; high ignored
  DenseUint32HalfPassNeNibble = 384, // n if n!=(parameter&0xf) else 0; high ignored
  DenseUint32HalfStepNeNibble = 385, // 0xf if n!=(parameter&0xf) else 0; high ignored
  DenseUint32HalfStepEqNibble = 386, // 0xf if n==(parameter&0xf) else 0; high ignored
  DenseUint32HalfMirrorNibble = 387, // (p - n) & 0xf; p=parameter&0xf; high ignored
  DenseUint32HalfSatMirrorNibble = 388, // p-n if n<=p else 0; p=parameter&0xf; high ignored
  DenseUint32HalfScale15Nibble = 389, // (n * p) / 15; p=parameter&0xf; high ignored
  DenseUint32HalfInvScale15Nibble = 390, // min((n*15)/p,0xf); p==0 -> 0; high ignored
  DenseUint32HalfSquareDiffNibble = 391, // (|n-p|^2) & 0xf; p=parameter&0xf; high ignored
  DenseUint32HalfSameSignNibble = 392, // 0xf if (n&8)==(p&8) else 0; high ignored
  DenseUint32HalfDiffSignNibble = 393, // 0xf if (n&8)!=(p&8) else 0; high ignored
  DenseUint32HalfParityMatchNibble = 394, // 0xf if (popcount(n)&1)==(p&1) else 0; high ignored
  DenseUint32HalfSignCopyNibble = 395, // (n & 7) | (p & 8); high ignored
  DenseUint32HalfMagnitudeNibble = 396, // n & 7; parameter ignored
  DenseUint32HalfNegIfSignNibble = 397, // (-n)&0xf if (p&8) else n; high ignored
  DenseUint32HalfIsqrtNibble = 398, // floor(sqrt(n)); parameter ignored
  DenseUint32HalfPow2FillNibble = 399, // 1 << (n & 3); parameter ignored
  DenseUint32HalfLog2FloorNibble = 400 // floor(log2(n)), 0 if n==0; parameter ignored
};

enum class WorldGenerationComputePollStatus : uint8_t {
  Pending = 0,
  Ready = 1,
  Failed = 2,
  Cancelled = 3,
  UnknownTicket = 4
};

enum class WorldGenerationComputeShadowVerdict : uint8_t {
  Equal = 0,
  Diverged = 1,
  PrimaryFailed = 2,
  ShadowUnavailable = 3,
  Cancelled = 4
};

// GPU-04 commit gate: kernel equivalence alone does not authorize publication.
enum class WorldGenerationComputeCommitVerdict : uint8_t {
  Accepted = 0,
  StaleIdentity = 1,
  StaleInput = 2,
  NotReady = 3,
  Cancelled = 4,
  Failed = 5,
  UnknownTicket = 6
};

struct WorldGenerationComputeCapabilities {
  bool cpuReference = true;
  bool gpuOpenGlCompute = false;
  uint32_t maxWorkGroupInvocations = 64;
  uint32_t maxInFlight = 8;
  uint64_t maxImmutableBufferBytes = 16ull * 1024ull * 1024ull;
  uint64_t maxStagingBytes = 32ull * 1024ull * 1024ull;
};

// GPU-04: explicit approved RNG stream (no float noise / no assets). Callers
// must not invent ad-hoc streams; consume via drawRng to preserve order.
struct WorldGenerationComputeRngStream {
  uint64_t seed = 0;
  uint64_t counter = 0;
  uint32_t streamId = 0; // approved stream class / channel
};

struct WorldGenerationComputeInputIdentity {
  uint64_t worldEpoch = 0;
  uint64_t sequence = 0;
  uint32_t kernelVersion = 1;
  WorldGenerationComputeKernel kernel = WorldGenerationComputeKernel::DenseUint32Xor;

  bool operator==(WorldGenerationComputeInputIdentity const& rhs) const;
  bool operator!=(WorldGenerationComputeInputIdentity const& rhs) const;
};

struct WorldGenerationComputeTicket {
  uint64_t id = 0;

  bool operator==(WorldGenerationComputeTicket const& rhs) const { return id == rhs.id; }
  bool operator!=(WorldGenerationComputeTicket const& rhs) const { return id != rhs.id; }
  explicit operator bool() const { return id != 0; }
};

struct WorldGenerationComputeCommitReport {
  WorldGenerationComputeCommitVerdict verdict = WorldGenerationComputeCommitVerdict::UnknownTicket;
  WorldGenerationComputeTicket ticket;
  WorldGenerationComputeInputIdentity submittedIdentity;
  WorldGenerationComputeInputIdentity liveIdentity;
  uint64_t submittedInputFingerprint = 0;
  uint64_t liveInputFingerprint = 0;
  WorldGenerationComputeBackend backend = WorldGenerationComputeBackend::CpuReference;
};

struct WorldGenerationComputeResult {
  WorldGenerationComputeInputIdentity identity;
  WorldGenerationComputeBackend backend = WorldGenerationComputeBackend::CpuReference;
  List<uint32_t> values;
};

struct WorldGenerationComputeBatchItem {
  WorldGenerationComputeInputIdentity identity;
  List<uint32_t> input;
  uint32_t parameter = 0;
};

// GPU-04 minimal numeric reproducer (no mod payload): enough to replay the
// first divergence with the same kernel/version/parameter/input sample.
struct WorldGenerationComputeShadowReproducer {
  bool valid = false;
  uint64_t worldEpoch = 0;
  uint64_t sequence = 0;
  uint32_t kernelVersion = 0;
  WorldGenerationComputeKernel kernel = WorldGenerationComputeKernel::DenseUint32Xor;
  uint32_t parameter = 0;
  size_t index = 0;
  uint32_t inputSample = 0;
  uint32_t primarySample = 0;
  uint32_t shadowSample = 0;
};

struct WorldGenerationComputeShadowReport {
  WorldGenerationComputeShadowVerdict verdict = WorldGenerationComputeShadowVerdict::Equal;
  WorldGenerationComputeInputIdentity identity;
  size_t comparedValues = 0;
  size_t divergenceCount = 0;
  size_t firstDivergenceIndex = 0;
  uint32_t primarySample = 0;
  uint32_t shadowSample = 0;
  uint32_t parameter = 0;
  WorldGenerationComputeBackend primaryBackend = WorldGenerationComputeBackend::CpuReference;
  WorldGenerationComputeBackend shadowBackend = WorldGenerationComputeBackend::CpuReference;
  WorldGenerationComputeShadowReproducer reproducer;
};

// GPU-04: aggregate counts over many shadow reports (asset-free harness).
struct WorldGenerationComputeShadowSummary {
  size_t total = 0;
  size_t equal = 0;
  size_t diverged = 0;
  size_t primaryFailed = 0;
  size_t shadowUnavailable = 0;
  size_t cancelled = 0;
  size_t totalDivergenceCount = 0;
};

// GPU-02 → RAM-01: staging / pool / estimated VRAM observation (no assets).
struct WorldGenerationComputeStagingSnapshot {
  uint64_t stagingBytes = 0;
  uint64_t peakStagingBytes = 0;
  uint64_t maxStagingBytes = 0;
  uint64_t pooledBufferBytes = 0;
  size_t pooledBufferCount = 0;
  size_t openTickets = 0;
  size_t peakOpenTickets = 0;
  size_t inFlight = 0;
  size_t gpuInFlight = 0;
  uint64_t estimatedVramBytes = 0; // in-flight GPU staging estimate (SSBO-sized)
  bool gpuCapable = false;
};

// Owner-thread service: explicit queue, reusable staging pool, in-flight limit,
// fence pump, cancel/fail recovery. Results never write the live world.
class WorldGenerationComputeService {
public:
  static WorldGenerationComputeCapabilities probeCapabilities() noexcept;

  explicit WorldGenerationComputeService(WorldGenerationComputeCapabilities capabilities =
      probeCapabilities());
  ~WorldGenerationComputeService();

  WorldGenerationComputeService(WorldGenerationComputeService const&) = delete;
  WorldGenerationComputeService& operator=(WorldGenerationComputeService const&) = delete;

  WorldGenerationComputeCapabilities const& capabilities() const noexcept { return m_capabilities; }
  size_t openTickets() const;
  size_t inFlight() const;
  // Pending tickets whose identity.kernel matches (asset-free observation).
  size_t inFlightByKernel(WorldGenerationComputeKernel kernel) const;
  // Pending open tickets whose identity.kernel matches (asset-free observation).
  List<WorldGenerationComputeTicket> listInFlightByKernel(
      WorldGenerationComputeKernel kernel) const;
  // True while any Pending ticket matches identity.kernel (asset-free observation).
  bool anyInFlightByKernel(WorldGenerationComputeKernel kernel) const;
  // Pending open tickets whose identity.kernel matches (asset-free observation).
  size_t pendingOpenTicketsByKernel(WorldGenerationComputeKernel kernel) const;
  // True while any Pending ticket matches identity.kernel (asset-free observation).
  bool anyPendingOpenTicketsByKernel(WorldGenerationComputeKernel kernel) const;
  // Ready open tickets whose identity.kernel matches (asset-free observation).
  size_t readyOpenTicketsByKernel(WorldGenerationComputeKernel kernel) const;
  // True while any Ready ticket matches identity.kernel (asset-free observation).
  bool anyReadyOpenTicketsByKernel(WorldGenerationComputeKernel kernel) const;
  // Cancelled open tickets whose identity.kernel matches (asset-free observation).
  size_t cancelledOpenTicketsByKernel(WorldGenerationComputeKernel kernel) const;
  // True while any Cancelled ticket matches identity.kernel (asset-free observation).
  bool anyCancelledOpenTicketsByKernel(WorldGenerationComputeKernel kernel) const;
  // True while any open ticket matches identity.kernel (asset-free observation).
  bool anyOpenTicketsByKernel(WorldGenerationComputeKernel kernel) const;
  // Failed open tickets whose identity.kernel matches (asset-free observation).
  size_t failedOpenTicketsByKernel(WorldGenerationComputeKernel kernel) const;
  // True while any Failed ticket matches identity.kernel (asset-free observation).
  bool anyFailedOpenTicketsByKernel(WorldGenerationComputeKernel kernel) const;
  size_t stagingBytes() const;
  size_t pooledBuffers() const;

  // GPU-02 observation (any thread under mutex): open ticket ids snapshot.
  List<WorldGenerationComputeTicket> listOpenTickets() const;
  // Pending open ticket ids snapshot (asset-free observation).
  List<WorldGenerationComputeTicket> listPendingOpenTickets() const;
  // Ready open ticket ids snapshot (asset-free observation).
  List<WorldGenerationComputeTicket> listReadyOpenTickets() const;
  // Failed open ticket ids snapshot (asset-free observation).
  List<WorldGenerationComputeTicket> listFailedOpenTickets() const;
  // Cancelled open ticket ids snapshot (asset-free observation).
  List<WorldGenerationComputeTicket> listCancelledOpenTickets() const;
  // Terminal (Ready/Failed/Cancelled) open ticket ids snapshot (asset-free).
  List<WorldGenerationComputeTicket> listTerminalOpenTickets() const;
  // Distinct identity.kernel values among currently open tickets (asset-free).
  List<WorldGenerationComputeKernel> listOpenKernels() const;
  // Count of distinct identity.kernel values among open tickets (asset-free).
  size_t countOpenKernels() const;
  // True while any open ticket exists (asset-free observation).
  bool anyOpenKernels() const;
  // True while any open ticket exists (ticket-named alias; asset-free).
  bool anyOpenTickets() const;
  // Count of open tickets (ticket-named alias of openTickets; asset-free).
  size_t countOpenTickets() const;
  // Distinct identity.kernel values among Ready open tickets (asset-free).
  List<WorldGenerationComputeKernel> listReadyOpenKernels() const;
  // Count of distinct identity.kernel values among Ready open tickets (asset-free).
  size_t countReadyOpenKernels() const;
  // True while any Ready open ticket exists (asset-free observation).
  bool anyReadyOpenKernels() const;
  // Distinct identity.kernel values among Failed open tickets (asset-free).
  List<WorldGenerationComputeKernel> listFailedOpenKernels() const;
  // Count of distinct identity.kernel values among Failed open tickets (asset-free).
  size_t countFailedOpenKernels() const;
  // True while any Failed open ticket exists (asset-free observation).
  bool anyFailedOpenKernels() const;
  // Distinct identity.kernel values among Cancelled open tickets (asset-free).
  List<WorldGenerationComputeKernel> listCancelledOpenKernels() const;
  // Count of distinct identity.kernel values among Cancelled open tickets (asset-free).
  size_t countCancelledOpenKernels() const;
  // True while any Cancelled open ticket exists (asset-free observation).
  bool anyCancelledOpenKernels() const;
  // Distinct identity.kernel values among terminal open tickets (asset-free).
  List<WorldGenerationComputeKernel> listTerminalOpenKernels() const;
  // Count of distinct identity.kernel values among terminal open tickets.
  size_t countTerminalOpenKernels() const;
  // True while any terminal open ticket exists (distinct-kernel presence).
  bool anyTerminalOpenKernels() const;
  // Distinct identity.kernel values among Pending open tickets (asset-free).
  List<WorldGenerationComputeKernel> listPendingOpenKernels() const;
  // Count of distinct identity.kernel values among Pending open tickets (asset-free).
  size_t countPendingOpenKernels() const;
  // Count of Pending open tickets (asset-free observation; not distinct kernels).
  size_t countPendingOpenTickets() const;
  // Count of Ready open tickets (asset-free observation; not distinct kernels).
  size_t countReadyOpenTickets() const;
  // Count of Failed open tickets (asset-free observation; not distinct kernels).
  size_t countFailedOpenTickets() const;
  // Count of Cancelled open tickets (asset-free observation; not distinct kernels).
  size_t countCancelledOpenTickets() const;
  // Count of terminal (Ready/Failed/Cancelled) open tickets (asset-free).
  size_t countTerminalOpenTickets() const;
  // True while any Ready open ticket exists (asset-free observation).
  bool anyReadyOpenTickets() const;
  // True while any Failed open ticket exists (asset-free observation).
  bool anyFailedOpenTickets() const;
  // True while any Cancelled open ticket exists (asset-free observation).
  bool anyCancelledOpenTickets() const;
  // True while any terminal (Ready/Failed/Cancelled) open ticket exists.
  bool anyTerminalOpenTickets() const;
  // True while any Pending open ticket exists (asset-free observation).
  bool anyPendingOpenTickets() const;
  // True while any Pending open ticket exists (asset-free observation).
  bool anyPendingOpenKernels() const;
  // Open tickets whose identity.kernel matches (asset-free observation).
  List<WorldGenerationComputeTicket> listOpenTicketsByKernel(
      WorldGenerationComputeKernel kernel) const;
  // Ready open tickets whose identity.kernel matches (asset-free observation).
  List<WorldGenerationComputeTicket> listReadyOpenTicketsByKernel(
      WorldGenerationComputeKernel kernel) const;
  // Failed open tickets whose identity.kernel matches (asset-free observation).
  List<WorldGenerationComputeTicket> listFailedOpenTicketsByKernel(
      WorldGenerationComputeKernel kernel) const;
  // Cancelled open tickets whose identity.kernel matches (asset-free observation).
  List<WorldGenerationComputeTicket> listCancelledOpenTicketsByKernel(
      WorldGenerationComputeKernel kernel) const;
  // Terminal open tickets whose identity.kernel matches (asset-free observation).
  List<WorldGenerationComputeTicket> listTerminalOpenTicketsByKernel(
      WorldGenerationComputeKernel kernel) const;
  // Count of terminal open tickets whose identity.kernel matches (asset-free).
  size_t countTerminalOpenTicketsByKernel(WorldGenerationComputeKernel kernel) const;
  // True while any terminal open ticket matches identity.kernel (asset-free).
  bool anyTerminalOpenTicketsByKernel(WorldGenerationComputeKernel kernel) const;
  // Pending open tickets whose identity.kernel matches (asset-free observation).
  List<WorldGenerationComputeTicket> listPendingOpenTicketsByKernel(
      WorldGenerationComputeKernel kernel) const;
  size_t countOpenTicketsByKernel(WorldGenerationComputeKernel kernel) const;
  // True while the ticket id is still present in the open-entry table.
  bool ticketIsOpen(WorldGenerationComputeTicket ticket) const;
  // Count of ticket ids rejected as late duplicates after failContext/commit revoke.
  size_t rejectedDuplicateCount() const;
  // True while any rejected-duplicate ticket id is tracked (asset-free).
  bool anyRejectedDuplicates() const;
  // Count alias of rejectedDuplicateCount (asset-free observation naming).
  size_t countRejectedDuplicates() const;
  // Snapshot of ticket ids currently tracked as rejected duplicates (asset-free).
  List<WorldGenerationComputeTicket> listRejectedDuplicates() const;
  // True if ticket.id is currently tracked as a rejected duplicate (asset-free).
  bool hasRejectedDuplicate(WorldGenerationComputeTicket ticket) const;
  // Removes one rejected-duplicate ticket id if present (asset-free; owner-thread gated).
  bool removeRejectedDuplicate(WorldGenerationComputeTicket ticket);
  // Order-independent XOR fingerprint of rejected-duplicate ticket ids (asset-free).
  uint64_t fingerprintRejectedDuplicates() const;
  // True when the rejected-duplicate set is empty (asset-free observation naming).
  bool rejectedDuplicatesEmpty() const;
  // Alias of fingerprintRejectedDuplicates (asset-free observation naming).
  uint64_t rejectedDuplicatesFingerprint() const;
  // Alias of anyRejectedDuplicates (asset-free observation naming).
  bool hasRejectedDuplicates() const;
  // Alias of countRejectedDuplicates / rejectedDuplicateCount (asset-free naming).
  size_t rejectedDuplicatesCount() const;
  // Alias of listRejectedDuplicates (asset-free observation naming).
  List<WorldGenerationComputeTicket> rejectedDuplicatesList() const;
  // Alias of anyRejectedDuplicates / hasRejectedDuplicates (asset-free naming).
  bool rejectedDuplicatesAny() const;
  // Alias of clearRejectedDuplicates (asset-free observation naming).
  size_t rejectedDuplicatesClear();
  // Alias of rejectedDuplicatesCount (asset-free observation naming).
  size_t rejectedDuplicatesSize() const;
  // Alias of hasRejectedDuplicate (asset-free observation naming).
  bool rejectedDuplicatesContains(WorldGenerationComputeTicket ticket) const;
  // Alias of removeRejectedDuplicate (asset-free observation naming).
  bool rejectedDuplicatesRemove(WorldGenerationComputeTicket ticket);
  // Alias of clearRejectedDuplicates (asset-free observation naming).
  size_t rejectedDuplicatesReset();
  // Alias of rejectedDuplicatesEmpty (asset-free observation naming).
  bool rejectedDuplicatesIsEmpty() const;
  // Alias of rejectedDuplicatesCount (asset-free observation naming).
  size_t rejectedDuplicatesCardinality() const;
  // Alias of listRejectedDuplicates (asset-free observation naming).
  List<WorldGenerationComputeTicket> rejectedDuplicatesMembers() const;
  // Alias of fingerprintRejectedDuplicates (asset-free observation naming).
  uint64_t rejectedDuplicatesDigest() const;
  // Alias of clearRejectedDuplicates (asset-free observation naming).
  size_t rejectedDuplicatesPurge();
  // Alias of fingerprintRejectedDuplicates (asset-free observation naming).
  uint64_t rejectedDuplicatesHash() const;
  // Alias of rejectedDuplicatesEmpty (asset-free observation naming).
  bool rejectedDuplicatesVacant() const;
  // Alias of rejectedDuplicatesCount (asset-free observation naming).
  size_t rejectedDuplicatesPopulation() const;
  // Alias of listRejectedDuplicates (asset-free observation naming).
  List<WorldGenerationComputeTicket> rejectedDuplicatesSnapshot() const;
  // Alias of hasRejectedDuplicate (asset-free observation naming).
  bool rejectedDuplicatesIncludes(WorldGenerationComputeTicket ticket) const;
  // Alias of rejectedDuplicatesCount (asset-free observation naming).
  size_t rejectedDuplicatesLength() const;
  // Alias of listRejectedDuplicates (asset-free observation naming).
  List<WorldGenerationComputeTicket> rejectedDuplicatesCollection() const;
  // Alias of clearRejectedDuplicates (asset-free observation naming).
  size_t rejectedDuplicatesFlush();
  // Alias of fingerprintRejectedDuplicates (asset-free observation naming).
  uint64_t rejectedDuplicatesChecksum() const;
  // Alias of anyRejectedDuplicates (asset-free observation naming).
  bool rejectedDuplicatesPresent() const;
  // Alias of removeRejectedDuplicate (asset-free observation naming).
  bool rejectedDuplicatesErase(WorldGenerationComputeTicket ticket);
  // Alias of rejectedDuplicatesCount (asset-free observation naming).
  size_t rejectedDuplicatesExtent() const;
  // Alias of listRejectedDuplicates (asset-free observation naming).
  List<WorldGenerationComputeTicket> rejectedDuplicatesInventory() const;
  // Alias of listRejectedDuplicates (asset-free observation naming).
  List<WorldGenerationComputeTicket> rejectedDuplicatesRoster() const;
  // Alias of hasRejectedDuplicate (asset-free observation naming).
  bool rejectedDuplicatesHolds(WorldGenerationComputeTicket ticket) const;
  // Alias of clearRejectedDuplicates (asset-free observation naming).
  size_t rejectedDuplicatesDrain();
  // Alias of fingerprintRejectedDuplicates (asset-free observation naming).
  uint64_t rejectedDuplicatesSeal() const;
  // Alias of rejectedDuplicatesCount (asset-free observation naming).
  size_t rejectedDuplicatesSpan() const;
  // Alias of rejectedDuplicatesEmpty (asset-free observation naming).
  bool rejectedDuplicatesAbsent() const;
  // Alias of rejectedDuplicatesCount (asset-free observation naming).
  size_t rejectedDuplicatesVolume() const;
  // Alias of rejectedDuplicatesList (asset-free observation naming).
  List<WorldGenerationComputeTicket> rejectedDuplicatesLedger() const;
  // Alias of fingerprintRejectedDuplicates (asset-free observation naming).
  uint64_t rejectedDuplicatesMark() const;
  // Alias of rejectedDuplicatesEmpty (asset-free observation naming).
  bool rejectedDuplicatesVoid() const;
  // Alias of rejectedDuplicatesList (asset-free observation naming).
  List<WorldGenerationComputeTicket> rejectedDuplicatesCatalog() const;
  // Alias of rejectedDuplicatesCount (asset-free observation naming).
  size_t rejectedDuplicatesMagnitude() const;
  // Alias of rejectedDuplicatesContains (asset-free observation naming).
  bool rejectedDuplicatesOwns(WorldGenerationComputeTicket ticket) const;
  // Clears the rejected-duplicate set (asset-free harness reset; owner-thread gated).
  size_t clearRejectedDuplicates();

  // GPU-02: complete any Pending tickets, then poll+discard every open ticket
  // (owner-thread). Returns how many tickets were drained.
  size_t drainOpenTickets();

  // GPU-02: drain only open tickets whose identity.kernel matches (owner-thread).
  size_t drainOpenTicketsByKernel(WorldGenerationComputeKernel kernel);

  // GPU-02: poll+discard Ready tickets whose identity.kernel matches (owner-thread).
  // Does not complete Pending; leaves Failed/Cancelled/Pending untouched.
  size_t discardReadyOpenTicketsByKernel(WorldGenerationComputeKernel kernel);

  // GPU-02: poll+discard every Ready open ticket (owner-thread).
  // Does not complete Pending; leaves Failed/Cancelled/Pending untouched.
  size_t discardReadyOpenTickets();

  // GPU-02: poll+discard Failed tickets whose identity.kernel matches (owner-thread).
  // Does not complete Pending; leaves Ready/Cancelled/Pending untouched.
  size_t discardFailedOpenTicketsByKernel(WorldGenerationComputeKernel kernel);

  // GPU-02: poll+discard every Failed open ticket (owner-thread).
  // Does not complete Pending; leaves Ready/Cancelled/Pending untouched.
  size_t discardFailedOpenTickets();

  // GPU-02: poll+discard Cancelled tickets whose identity.kernel matches (owner-thread).
  // Does not complete Pending; leaves Ready/Failed/Pending untouched.
  size_t discardCancelledOpenTicketsByKernel(WorldGenerationComputeKernel kernel);

  // GPU-02: poll+discard every Cancelled open ticket (owner-thread).
  // Does not complete Pending; leaves Ready/Failed/Pending untouched.
  size_t discardCancelledOpenTickets();

  // GPU-02: cancel+poll Pending tickets whose identity.kernel matches (owner-thread).
  // Leaves Ready/Failed/Cancelled untouched.
  size_t discardPendingOpenTicketsByKernel(WorldGenerationComputeKernel kernel);

  // GPU-02: cancel+poll every Pending open ticket (owner-thread).
  // Leaves Ready/Failed/Cancelled untouched.
  size_t discardPendingOpenTickets();

  // GPU-02: poll+discard Ready/Failed/Cancelled tickets for one kernel (owner-thread).
  // Does not complete Pending.
  size_t discardTerminalOpenTicketsByKernel(WorldGenerationComputeKernel kernel);

  // GPU-02: poll+discard every Ready/Failed/Cancelled open ticket (owner-thread).
  // Does not complete Pending.
  size_t discardTerminalOpenTickets();

  // GPU-02 accounting for RAM-01 consumers (asset-free observation only).
  WorldGenerationComputeStagingSnapshot stagingSnapshot() const;
  void resetPeakStagingBytes() noexcept;
  void resetPeakOpenTickets() noexcept;

  // GPU-02 owner-thread affinity (asset-free; no WorldThread yet). When adopted,
  // mutating ops and compareShadow/dispatch refuse foreign threads. Observation
  // getters remain usable under the mutex from any thread.
  void adoptOwnerThread() noexcept;
  void clearOwnerThread() noexcept;
  bool hasOwnerThread() const noexcept;
  bool isOwnerThread() const noexcept;

  // Deferred CPU completion leaves Pending until pump()/complete(). Synchronous
  // mode (default) finishes CPU work before submit returns.
  void setDeferredCpuCompletion(bool deferred) noexcept;

  Maybe<WorldGenerationComputeTicket> submit(WorldGenerationComputeInputIdentity identity,
      List<uint32_t> input, uint32_t parameter,
      WorldGenerationComputeBackend backend = WorldGenerationComputeBackend::CpuReference);

  // GPU-01: submit using a stable ASCII kernel name; fills identity.kernel.
  // Empty when the name is unknown or submit refuses the work.
  Maybe<WorldGenerationComputeTicket> submitNamed(char const* kernelName,
      WorldGenerationComputeInputIdentity identity, List<uint32_t> input, uint32_t parameter,
      WorldGenerationComputeBackend backend = WorldGenerationComputeBackend::CpuReference);

  // One ticket per item; fails entirely (no partial admit) if capacity is short.
  Maybe<List<WorldGenerationComputeTicket>> submitBatch(List<WorldGenerationComputeBatchItem> items,
      WorldGenerationComputeBackend backend = WorldGenerationComputeBackend::CpuReference);

  // Advances deferred fences without blocking the caller on GPU/GL.
  size_t pump(size_t maxCompletions = 0);

  bool complete(WorldGenerationComputeTicket ticket);

  // GPU-02: complete many deferred Pending tickets; each bool is independent.
  List<bool> completeBatch(List<WorldGenerationComputeTicket> tickets);

  // GPU-02: complete Pending tickets whose identity.kernel matches (owner-thread).
  // Leaves Ready/Failed/Cancelled entries open until poll; returns how many completed.
  size_t completeOpenTicketsByKernel(WorldGenerationComputeKernel kernel);

  WorldGenerationComputePollStatus poll(WorldGenerationComputeTicket ticket,
      WorldGenerationComputeResult* out = nullptr);

  bool cancel(WorldGenerationComputeTicket ticket);

  // GPU-02: cancel many tickets; each result is independent (true if that id
  // was open and revoked on the owner thread).
  List<bool> cancelBatch(List<WorldGenerationComputeTicket> tickets);

  // GPU-02: revoke every currently open ticket; returns how many were cancelled.
  size_t cancelAll();

  // GPU-02: revoke open tickets whose identity.kernel matches; owner-thread gated.
  size_t cancelOpenTicketsByKernel(WorldGenerationComputeKernel kernel);

  // GPU-02: poll many tickets; each status/result is independent. When outs is
  // non-null it is resized to tickets.size(); Ready slots fill values.
  List<WorldGenerationComputePollStatus> pollBatch(List<WorldGenerationComputeTicket> tickets,
      List<WorldGenerationComputeResult>* outs = nullptr);

  // Context/driver failure (GPU-02): recalculate open tickets on the CPU
  // reference path, then reject later duplicate polls for those ids (late GPU
  // results must not publish). Returns the number of open tickets affected.
  size_t failContext();

  // GPU-04: run primary and shadow on identical inputs. Without GPU capability
  // or failed GL dispatch, shadowBackend GpuOpenGlCompute yields
  // ShadowUnavailable. With a successful probe, GPU path uses compute shaders
  // (DenseUint32*) rather than the CPU helper. Diverged reports fill
  // `reproducer` with a minimal numeric replay record.
  WorldGenerationComputeShadowReport compareShadow(WorldGenerationComputeInputIdentity identity,
      List<uint32_t> const& input, uint32_t parameter,
      WorldGenerationComputeBackend primary = WorldGenerationComputeBackend::CpuReference,
      WorldGenerationComputeBackend shadow = WorldGenerationComputeBackend::GpuOpenGlCompute);

  // GPU-04: compareShadow with identity.kernel filled from a stable ASCII name.
  // Unknown names yield PrimaryFailed without dispatch.
  WorldGenerationComputeShadowReport compareShadowNamed(char const* kernelName,
      WorldGenerationComputeInputIdentity identity, List<uint32_t> const& input, uint32_t parameter,
      WorldGenerationComputeBackend primary = WorldGenerationComputeBackend::CpuReference,
      WorldGenerationComputeBackend shadow = WorldGenerationComputeBackend::CpuReference);

  // Compare already-materialized buffers (asset-free harness / unit path).
  static WorldGenerationComputeShadowReport compareShadowBuffers(
      WorldGenerationComputeInputIdentity identity, List<uint32_t> const& input,
      uint32_t parameter, List<uint32_t> const& primaryValues,
      List<uint32_t> const& shadowValues,
      WorldGenerationComputeBackend primary = WorldGenerationComputeBackend::CpuReference,
      WorldGenerationComputeBackend shadow = WorldGenerationComputeBackend::CpuReference);

  // Edge corpus for wrap / negative-as-uint32 / near-limit patterns (no assets).
  static List<uint32_t> shadowEdgeCorpus();

  // Stable fingerprint of an immutable input buffer (asset-free; no mod payload).
  static uint64_t fingerprintInput(List<uint32_t> const& input) noexcept;

  // GPU-04: stable fingerprint of an input identity (asset-free; no mod payload).
  static uint64_t fingerprintIdentity(WorldGenerationComputeInputIdentity const& identity) noexcept;

  // GPU-01: stable fingerprint of probed/configured capabilities (asset-free).
  static uint64_t fingerprintCapabilities(
      WorldGenerationComputeCapabilities const& capabilities) noexcept;

  // GPU-02: stable fingerprint of a staging/pool observation snapshot (asset-free).
  static uint64_t fingerprintStagingSnapshot(
      WorldGenerationComputeStagingSnapshot const& snapshot) noexcept;

  // GPU-04: stable fingerprint of a shadow-batch summary (asset-free harness).
  static uint64_t fingerprintShadowSummary(
      WorldGenerationComputeShadowSummary const& summary) noexcept;

  // GPU-04 approved RNG: deterministic uint32 draws; advances counter by 1.
  static uint32_t drawRng(WorldGenerationComputeRngStream& stream) noexcept;
  static List<uint32_t> fillRng(WorldGenerationComputeRngStream& stream, size_t count);

  // GPU-04: publish only after live identity + input fingerprint still match the
  // submitted ticket. Consumes the ticket like poll. Stale/mismatch revokes
  // publication and rejects later duplicate polls for that id.
  WorldGenerationComputeCommitReport commit(WorldGenerationComputeTicket ticket,
      WorldGenerationComputeInputIdentity const& liveIdentity, uint64_t liveInputFingerprint,
      WorldGenerationComputeResult* out = nullptr);

  struct CommitBatchItem {
    WorldGenerationComputeTicket ticket;
    WorldGenerationComputeInputIdentity liveIdentity;
    uint64_t liveInputFingerprint = 0;
  };

  // Commit many ready tickets; each item is independent (no partial rollback).
  List<WorldGenerationComputeCommitReport> commitBatch(List<CommitBatchItem> items,
      List<WorldGenerationComputeResult>* outs = nullptr);

  static List<uint32_t> runKernel(WorldGenerationComputeKernel kernel, uint32_t kernelVersion,
      List<uint32_t> const& input, uint32_t parameter);

  // GPU-01: run by stable ASCII name; empty when the name/version is unsupported.
  static List<uint32_t> runNamedKernel(char const* name, uint32_t kernelVersion,
      List<uint32_t> const& input, uint32_t parameter);

  // GPU-01 capability catalog (asset-free): kernels accepted at a version.
  static bool isKernelSupported(WorldGenerationComputeKernel kernel,
      uint32_t kernelVersion = 1u) noexcept;
  static List<WorldGenerationComputeKernel> supportedKernels(uint32_t kernelVersion = 1u);
  static size_t supportedKernelCount(uint32_t kernelVersion = 1u) noexcept;
  static bool isBackendSupported(WorldGenerationComputeBackend backend,
      WorldGenerationComputeCapabilities const& capabilities) noexcept;
  // Stable ASCII name for catalogs/logs; nullptr if the enum is unknown.
  static char const* kernelName(WorldGenerationComputeKernel kernel) noexcept;
  // Inverse of kernelName; empty when the token is unknown (asset-free).
  static Maybe<WorldGenerationComputeKernel> parseKernelName(char const* name) noexcept;
  static char const* backendName(WorldGenerationComputeBackend backend) noexcept;
  static Maybe<WorldGenerationComputeBackend> parseBackendName(char const* name) noexcept;
  static char const* pollStatusName(WorldGenerationComputePollStatus status) noexcept;
  static char const* shadowVerdictName(WorldGenerationComputeShadowVerdict verdict) noexcept;
  static char const* commitVerdictName(WorldGenerationComputeCommitVerdict verdict) noexcept;

  struct ShadowBatchItem {
    WorldGenerationComputeInputIdentity identity;
    uint32_t parameter = 0;
  };

  // GPU-04: shadow many kernels against the same immutable input corpus.
  List<WorldGenerationComputeShadowReport> compareShadowBatch(List<uint32_t> const& input,
      List<ShadowBatchItem> items,
      WorldGenerationComputeBackend primary = WorldGenerationComputeBackend::CpuReference,
      WorldGenerationComputeBackend shadow = WorldGenerationComputeBackend::CpuReference);

  // GPU-04: tally verdicts / divergence counts across a batch of reports.
  static WorldGenerationComputeShadowSummary summarizeShadowReports(
      List<WorldGenerationComputeShadowReport> const& reports) noexcept;

private:
  enum class EntryState : uint8_t { Pending, Ready, Failed, Cancelled };

  struct Entry {
    WorldGenerationComputeInputIdentity identity;
    WorldGenerationComputeBackend backend = WorldGenerationComputeBackend::CpuReference;
    EntryState state = EntryState::Pending;
    uint32_t parameter = 0;
    List<uint32_t> input;
    List<uint32_t> output;
    uint64_t inputFingerprint = 0;
    uint64_t stagingBytes = 0;
    bool publicationRevoked = false;
    bool contextFailed = false;
  };

  static bool supported(WorldGenerationComputeKernel kernel, uint32_t version) noexcept;
  static uint64_t bufferBytes(List<uint32_t> const& values) noexcept;
  List<uint32_t> acquireBuffer(size_t size);
  void releaseBuffer(List<uint32_t>&& buffer);
  void completeEntry(Entry& entry);
  bool runBackendKernel(WorldGenerationComputeBackend backend,
      WorldGenerationComputeKernel kernel, uint32_t kernelVersion, List<uint32_t> const& input,
      uint32_t parameter, List<uint32_t>& out) const;
  Maybe<WorldGenerationComputeTicket> submitLocked(WorldGenerationComputeInputIdentity identity,
      List<uint32_t> input, uint32_t parameter, WorldGenerationComputeBackend backend);
  WorldGenerationComputePollStatus pollLocked(WorldGenerationComputeTicket ticket,
      WorldGenerationComputeResult* out);

  bool ownerAllowsLocked() const noexcept;

  WorldGenerationComputeCapabilities m_capabilities;
  bool m_deferredCpu = false;
  bool m_hasOwnerThread = false;
  std::thread::id m_ownerThread;
  mutable std::mutex m_mutex;
  uint64_t m_nextTicket = 1;
  uint64_t m_stagingBytes = 0;
  uint64_t m_peakStagingBytes = 0;
  size_t m_peakOpenTickets = 0;
  std::unordered_map<uint64_t, Entry> m_entries;
  std::unordered_set<uint64_t> m_rejectedDuplicates;
  List<List<uint32_t>> m_bufferPool;
};

}
