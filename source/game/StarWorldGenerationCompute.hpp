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

enum class WorldGenerationComputeKernel : uint8_t {
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
  DenseUint32ExpandLow16ToOdd = 85 // place low 16 bits into positions 0,2,4,…; parameter ignored
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
  // Ready open tickets whose identity.kernel matches (asset-free observation).
  size_t readyOpenTicketsByKernel(WorldGenerationComputeKernel kernel) const;
  // Cancelled open tickets whose identity.kernel matches (asset-free observation).
  size_t cancelledOpenTicketsByKernel(WorldGenerationComputeKernel kernel) const;
  // True while any open ticket matches identity.kernel (asset-free observation).
  bool anyOpenTicketsByKernel(WorldGenerationComputeKernel kernel) const;
  // Failed open tickets whose identity.kernel matches (asset-free observation).
  size_t failedOpenTicketsByKernel(WorldGenerationComputeKernel kernel) const;
  size_t stagingBytes() const;
  size_t pooledBuffers() const;

  // GPU-02 observation (any thread under mutex): open ticket ids snapshot.
  List<WorldGenerationComputeTicket> listOpenTickets() const;
  // Open tickets whose identity.kernel matches (asset-free observation).
  List<WorldGenerationComputeTicket> listOpenTicketsByKernel(
      WorldGenerationComputeKernel kernel) const;
  // Ready open tickets whose identity.kernel matches (asset-free observation).
  List<WorldGenerationComputeTicket> listReadyOpenTicketsByKernel(
      WorldGenerationComputeKernel kernel) const;
  // Failed open tickets whose identity.kernel matches (asset-free observation).
  List<WorldGenerationComputeTicket> listFailedOpenTicketsByKernel(
      WorldGenerationComputeKernel kernel) const;
  size_t countOpenTicketsByKernel(WorldGenerationComputeKernel kernel) const;
  // True while the ticket id is still present in the open-entry table.
  bool ticketIsOpen(WorldGenerationComputeTicket ticket) const;
  // Count of ticket ids rejected as late duplicates after failContext/commit revoke.
  size_t rejectedDuplicateCount() const;
  // Clears the rejected-duplicate set (asset-free harness reset; owner-thread gated).
  size_t clearRejectedDuplicates();

  // GPU-02: complete any Pending tickets, then poll+discard every open ticket
  // (owner-thread). Returns how many tickets were drained.
  size_t drainOpenTickets();

  // GPU-02: drain only open tickets whose identity.kernel matches (owner-thread).
  size_t drainOpenTicketsByKernel(WorldGenerationComputeKernel kernel);

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
