#include "StarWorldGenerationCompute.hpp"

#include "gtest/gtest.h"

#include <atomic>
#include <thread>

using namespace Star;

namespace {

WorldGenerationComputeInputIdentity makeIdentity(uint64_t sequence = 1,
    WorldGenerationComputeKernel kernel = WorldGenerationComputeKernel::DenseUint32Xor) {
  WorldGenerationComputeInputIdentity id;
  id.worldEpoch = 7;
  id.sequence = sequence;
  id.kernelVersion = 1;
  id.kernel = kernel;
  return id;
}

List<uint32_t> sampleInput() {
  return List<uint32_t>{1u, 2u, 3u, 0xffffffffu};
}

List<uint32_t> edgeCorpus() {
  return WorldGenerationComputeService::shadowEdgeCorpus();
}

}

TEST(WorldGenerationComputeTest, ProbeKeepsCpuAndDoesNotClaimGpuWithoutDetection) {
  auto caps = WorldGenerationComputeService::probeCapabilities();
  EXPECT_TRUE(caps.cpuReference);
#ifndef STAR_ENABLE_WORLDGEN_GL_COMPUTE
  // Normal builds must never claim GL compute without the diagnostic probe.
  EXPECT_FALSE(caps.gpuOpenGlCompute);
#endif
  EXPECT_GE(caps.maxImmutableBufferBytes, sizeof(uint32_t));
  EXPECT_GE(caps.maxInFlight, 1u);
}

TEST(WorldGenerationComputeTest, ForcedGpuCapabilityWithoutLiveDispatchIsUnavailable) {
  // Capability bit alone is not enough: dispatch must succeed for shadow Equal.
  WorldGenerationComputeCapabilities caps = WorldGenerationComputeService::probeCapabilities();
  caps.gpuOpenGlCompute = true;
#ifndef STAR_ENABLE_WORLDGEN_GL_COMPUTE
  WorldGenerationComputeService service(caps);
  auto report = service.compareShadow(makeIdentity(63), edgeCorpus(), 1u,
      WorldGenerationComputeBackend::CpuReference,
      WorldGenerationComputeBackend::GpuOpenGlCompute);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::ShadowUnavailable, report.verdict);
#else
  (void)caps;
  SUCCEED();
#endif
}

TEST(WorldGenerationComputeTest, CpuReferenceSubmitPollProducesExactXor) {
  WorldGenerationComputeService service;
  auto ticket = service.submit(makeIdentity(), sampleInput(), 0x55aa55aau);
  ASSERT_TRUE(ticket);
  EXPECT_EQ(1u, service.openTickets());

  WorldGenerationComputeResult result;
  EXPECT_EQ(WorldGenerationComputePollStatus::Ready, service.poll(*ticket, &result));
  EXPECT_EQ(0u, service.openTickets());
  EXPECT_EQ(makeIdentity(), result.identity);
  EXPECT_EQ(WorldGenerationComputeBackend::CpuReference, result.backend);
  ASSERT_EQ(4u, result.values.size());
  EXPECT_EQ(1u ^ 0x55aa55aau, result.values[0]);
  EXPECT_EQ(2u ^ 0x55aa55aau, result.values[1]);
  EXPECT_EQ(3u ^ 0x55aa55aau, result.values[2]);
  EXPECT_EQ(0xffffffffu ^ 0x55aa55aau, result.values[3]);
  EXPECT_EQ(WorldGenerationComputePollStatus::UnknownTicket, service.poll(*ticket, &result));
}

TEST(WorldGenerationComputeTest, CancelRevokesPublicationBeforePoll) {
  WorldGenerationComputeService service;
  auto ticket = service.submit(makeIdentity(2), sampleInput(), 7u);
  ASSERT_TRUE(ticket);
  EXPECT_TRUE(service.cancel(*ticket));
  WorldGenerationComputeResult result;
  EXPECT_EQ(WorldGenerationComputePollStatus::Cancelled, service.poll(*ticket, &result));
  EXPECT_TRUE(result.values.empty());
  EXPECT_EQ(0u, service.openTickets());
  EXPECT_FALSE(service.cancel(*ticket));
}

TEST(WorldGenerationComputeTest, GpuSubmitRefusedWithoutCapability) {
  // Contract: GPU submit is refused when the capability bit is false, even if
  // this process could probe GL in an opt-in build.
  WorldGenerationComputeCapabilities caps = WorldGenerationComputeService::probeCapabilities();
  caps.gpuOpenGlCompute = false;
  WorldGenerationComputeService service(caps);
  auto ticket = service.submit(makeIdentity(3), sampleInput(), 1u,
      WorldGenerationComputeBackend::GpuOpenGlCompute);
  EXPECT_FALSE(ticket);
  EXPECT_EQ(0u, service.openTickets());

#ifdef STAR_ENABLE_WORLDGEN_GL_COMPUTE
  auto probed = WorldGenerationComputeService::probeCapabilities();
  if (probed.gpuOpenGlCompute) {
    WorldGenerationComputeService gpuService(probed);
    auto gpuTicket = gpuService.submit(makeIdentity(31), sampleInput(), 1u,
        WorldGenerationComputeBackend::GpuOpenGlCompute);
    ASSERT_TRUE(gpuTicket);
    WorldGenerationComputeResult result;
    EXPECT_EQ(WorldGenerationComputePollStatus::Ready, gpuService.poll(*gpuTicket, &result));
    EXPECT_EQ(WorldGenerationComputeBackend::GpuOpenGlCompute, result.backend);
  }
#endif
}

TEST(WorldGenerationComputeTest, OversizedBufferAndUnknownKernelAreRefused) {
  WorldGenerationComputeCapabilities caps = WorldGenerationComputeService::probeCapabilities();
  caps.maxImmutableBufferBytes = sizeof(uint32_t);
  WorldGenerationComputeService service(caps);
  EXPECT_FALSE(service.submit(makeIdentity(4), sampleInput(), 1u));

  auto bad = makeIdentity(5);
  bad.kernelVersion = 99;
  EXPECT_FALSE(service.submit(bad, List<uint32_t>{1u}, 1u));
  EXPECT_EQ(0u, service.openTickets());
}

TEST(WorldGenerationComputeTest, SubmitCopiesInputBuffer) {
  WorldGenerationComputeService service;
  List<uint32_t> input = sampleInput();
  auto ticket = service.submit(makeIdentity(6), input, 0u);
  ASSERT_TRUE(ticket);
  input[0] = 99u;
  WorldGenerationComputeResult result;
  EXPECT_EQ(WorldGenerationComputePollStatus::Ready, service.poll(*ticket, &result));
  EXPECT_EQ(1u, result.values[0]);
}

TEST(WorldGenerationComputeTest, DeferredQueuePumpAndInFlightLimit) {
  WorldGenerationComputeCapabilities caps = WorldGenerationComputeService::probeCapabilities();
  caps.maxInFlight = 2;
  WorldGenerationComputeService service(caps);
  service.setDeferredCpuCompletion(true);

  auto a = service.submit(makeIdentity(10), sampleInput(), 1u);
  auto b = service.submit(makeIdentity(11), sampleInput(), 2u);
  auto c = service.submit(makeIdentity(12), sampleInput(), 3u);
  ASSERT_TRUE(a);
  ASSERT_TRUE(b);
  EXPECT_FALSE(c);
  EXPECT_EQ(2u, service.inFlight());
  EXPECT_EQ(2u, service.openTickets());
  EXPECT_GT(service.stagingBytes(), 0u);

  EXPECT_TRUE(service.complete(*a));
  EXPECT_EQ(1u, service.inFlight());
  WorldGenerationComputeResult result;
  EXPECT_EQ(WorldGenerationComputePollStatus::Ready, service.poll(*a, &result));
  EXPECT_EQ(1u ^ 1u, result.values[0]);

  auto d = service.submit(makeIdentity(13), sampleInput(), 4u);
  ASSERT_TRUE(d);
  EXPECT_EQ(2u, service.pump());
  EXPECT_EQ(0u, service.inFlight());
  EXPECT_EQ(WorldGenerationComputePollStatus::Ready, service.poll(*b, &result));
  EXPECT_EQ(WorldGenerationComputePollStatus::Ready, service.poll(*d, &result));
  EXPECT_EQ(0u, service.openTickets());
  EXPECT_EQ(0u, service.stagingBytes());
}

TEST(WorldGenerationComputeTest, CancelWhilePendingThenPump) {
  WorldGenerationComputeService service;
  service.setDeferredCpuCompletion(true);
  auto ticket = service.submit(makeIdentity(20), sampleInput(), 9u);
  ASSERT_TRUE(ticket);
  EXPECT_TRUE(service.cancel(*ticket));
  // Cancel already terminalizes the entry; pump has nothing Pending left.
  EXPECT_EQ(0u, service.pump());
  WorldGenerationComputeResult result;
  EXPECT_EQ(WorldGenerationComputePollStatus::Cancelled, service.poll(*ticket, &result));
  EXPECT_TRUE(result.values.empty());
}

TEST(WorldGenerationComputeTest, ContextFailureRecalculatesOnCpuAndRejectsDuplicates) {
  WorldGenerationComputeService service;
  service.setDeferredCpuCompletion(true);
  auto input = sampleInput();
  uint32_t parameter = 0x55aa55aau;
  auto ticket = service.submit(makeIdentity(21), input, parameter);
  ASSERT_TRUE(ticket);
  EXPECT_EQ(1u, service.failContext());

  WorldGenerationComputeResult result;
  EXPECT_EQ(WorldGenerationComputePollStatus::Ready, service.poll(*ticket, &result));
  EXPECT_EQ(WorldGenerationComputeBackend::CpuReference, result.backend);
  ASSERT_EQ(input.size(), result.values.size());
  for (size_t i = 0; i < input.size(); ++i)
    EXPECT_EQ(input[i] ^ parameter, result.values[i]) << i;
  // Late duplicate (e.g. GPU finishing after context loss) must not publish.
  EXPECT_EQ(WorldGenerationComputePollStatus::UnknownTicket, service.poll(*ticket, &result));
}

TEST(WorldGenerationComputeTest, BatchSubmitExactAddKernelAndPoolReuse) {
  WorldGenerationComputeService service;
  List<WorldGenerationComputeBatchItem> items;
  for (uint64_t i = 0; i < 3; ++i) {
    WorldGenerationComputeBatchItem item;
    item.identity = makeIdentity(30 + i, WorldGenerationComputeKernel::DenseUint32Add);
    item.input = sampleInput();
    item.parameter = (uint32_t)i + 1u;
    items.append(std::move(item));
  }
  auto tickets = service.submitBatch(std::move(items));
  ASSERT_TRUE(tickets);
  ASSERT_EQ(3u, tickets->size());

  for (size_t i = 0; i < tickets->size(); ++i) {
    WorldGenerationComputeResult result;
    EXPECT_EQ(WorldGenerationComputePollStatus::Ready, service.poll((*tickets)[i], &result));
    EXPECT_EQ(WorldGenerationComputeKernel::DenseUint32Add, result.identity.kernel);
    EXPECT_EQ(sampleInput()[0] + (uint32_t)i + 1u, result.values[0]);
  }
  EXPECT_EQ(0u, service.openTickets());
  EXPECT_GT(service.pooledBuffers(), 0u);

  auto again = service.submit(makeIdentity(40, WorldGenerationComputeKernel::DenseUint32Add),
      sampleInput(), 5u);
  ASSERT_TRUE(again);
  WorldGenerationComputeResult result;
  EXPECT_EQ(WorldGenerationComputePollStatus::Ready, service.poll(*again, &result));
  EXPECT_EQ(sampleInput()[0] + 5u, result.values[0]);
}

TEST(WorldGenerationComputeTest, BatchRollsBackWhenInFlightExceeded) {
  WorldGenerationComputeCapabilities caps = WorldGenerationComputeService::probeCapabilities();
  caps.maxInFlight = 2;
  WorldGenerationComputeService service(caps);
  service.setDeferredCpuCompletion(true);
  ASSERT_TRUE(service.submit(makeIdentity(50), sampleInput(), 1u));

  List<WorldGenerationComputeBatchItem> items;
  for (uint64_t i = 0; i < 2; ++i) {
    WorldGenerationComputeBatchItem item;
    item.identity = makeIdentity(51 + i);
    item.input = sampleInput();
    item.parameter = 1u;
    items.append(std::move(item));
  }
  EXPECT_FALSE(service.submitBatch(std::move(items)));
  EXPECT_EQ(1u, service.openTickets());
}

TEST(WorldGenerationComputeTest, ShadowCompareCpuEqualsCpuOnEdgeCorpus) {
  WorldGenerationComputeService service;
  auto report = service.compareShadow(makeIdentity(60), edgeCorpus(), 0x11u,
      WorldGenerationComputeBackend::CpuReference,
      WorldGenerationComputeBackend::CpuReference);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal, report.verdict);
  EXPECT_EQ(0u, report.divergenceCount);
  EXPECT_EQ(edgeCorpus().size(), report.comparedValues);

  auto addId = makeIdentity(61, WorldGenerationComputeKernel::DenseUint32Add);
  auto addReport = service.compareShadow(addId, edgeCorpus(), 3u,
      WorldGenerationComputeBackend::CpuReference,
      WorldGenerationComputeBackend::CpuReference);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal, addReport.verdict);
}

TEST(WorldGenerationComputeTest, ShadowCompareGpuUnavailableWithoutProbe) {
  // Without the capability bit, shadow GPU must stay ShadowUnavailable.
  WorldGenerationComputeCapabilities caps = WorldGenerationComputeService::probeCapabilities();
  caps.gpuOpenGlCompute = false;
  WorldGenerationComputeService service(caps);
  auto report = service.compareShadow(makeIdentity(62), edgeCorpus(), 1u,
      WorldGenerationComputeBackend::CpuReference,
      WorldGenerationComputeBackend::GpuOpenGlCompute);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::ShadowUnavailable, report.verdict);
  EXPECT_EQ(0u, report.divergenceCount);

#ifdef STAR_ENABLE_WORLDGEN_GL_COMPUTE
  auto probed = WorldGenerationComputeService::probeCapabilities();
  if (probed.gpuOpenGlCompute) {
    WorldGenerationComputeService gpuService(probed);
    auto equalReport = gpuService.compareShadow(makeIdentity(64), edgeCorpus(), 0x11u,
        WorldGenerationComputeBackend::CpuReference,
        WorldGenerationComputeBackend::GpuOpenGlCompute);
    EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal, equalReport.verdict);
    EXPECT_EQ(0u, equalReport.divergenceCount);
  }
#endif
}

TEST(WorldGenerationComputeTest, KernelHelperMatchesServiceOutput) {
  auto input = edgeCorpus();
  auto direct = WorldGenerationComputeService::runKernel(
      WorldGenerationComputeKernel::DenseUint32Xor, 1u, input, 0xabu);
  WorldGenerationComputeService service;
  auto ticket = service.submit(makeIdentity(70), input, 0xabu);
  ASSERT_TRUE(ticket);
  WorldGenerationComputeResult result;
  EXPECT_EQ(WorldGenerationComputePollStatus::Ready, service.poll(*ticket, &result));
  ASSERT_EQ(direct.size(), result.values.size());
  for (size_t i = 0; i < direct.size(); ++i)
    EXPECT_EQ(direct[i], result.values[i]) << i;
}

TEST(WorldGenerationComputeTest, ShadowEdgeCorpusAndMinimalReproducer) {
  auto corpus = WorldGenerationComputeService::shadowEdgeCorpus();
  ASSERT_GE(corpus.size(), 8u);
  // Negative-as-uint32 encodings present.
  bool sawNeg1 = false;
  bool sawNeg32 = false;
  for (uint32_t v : corpus) {
    if (v == 0xffffffffu)
      sawNeg1 = true;
    if (v == 0xffffffe0u)
      sawNeg32 = true;
  }
  EXPECT_TRUE(sawNeg1);
  EXPECT_TRUE(sawNeg32);

  WorldGenerationComputeService service;
  auto equal = service.compareShadow(makeIdentity(100), corpus, 0x11u,
      WorldGenerationComputeBackend::CpuReference,
      WorldGenerationComputeBackend::CpuReference);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal, equal.verdict);
  EXPECT_FALSE(equal.reproducer.valid);

  auto primary = WorldGenerationComputeService::runKernel(
      WorldGenerationComputeKernel::DenseUint32Xor, 1u, corpus, 0x11u);
  auto shadow = primary;
  ASSERT_GE(shadow.size(), 3u);
  shadow[2] ^= 0xffffffffu; // force a single divergence
  auto diverged = WorldGenerationComputeService::compareShadowBuffers(
      makeIdentity(101), corpus, 0x11u, primary, shadow);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Diverged, diverged.verdict);
  EXPECT_EQ(1u, diverged.divergenceCount);
  EXPECT_EQ(2u, diverged.firstDivergenceIndex);
  ASSERT_TRUE(diverged.reproducer.valid);
  EXPECT_EQ(101u, diverged.reproducer.sequence);
  EXPECT_EQ(0x11u, diverged.reproducer.parameter);
  EXPECT_EQ(2u, diverged.reproducer.index);
  EXPECT_EQ(corpus[2], diverged.reproducer.inputSample);
  EXPECT_EQ(primary[2], diverged.reproducer.primarySample);
  EXPECT_EQ(shadow[2], diverged.reproducer.shadowSample);
}

TEST(WorldGenerationComputeTest, DenseUint32AndOrMulExactCpuAndShadow) {
  // GPU-03 next asset-free dense kernels: And / Or / Mul (wrapping).
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  uint32_t const mask = 0x0f0f0f0fu;
  uint32_t const factor = 3u;

  auto andOut = WorldGenerationComputeService::runKernel(
      WorldGenerationComputeKernel::DenseUint32And, 1u, input, mask);
  ASSERT_EQ(input.size(), andOut.size());
  for (size_t i = 0; i < input.size(); ++i)
    EXPECT_EQ(input[i] & mask, andOut[i]) << i;

  auto orOut = WorldGenerationComputeService::runKernel(
      WorldGenerationComputeKernel::DenseUint32Or, 1u, input, mask);
  for (size_t i = 0; i < input.size(); ++i)
    EXPECT_EQ(input[i] | mask, orOut[i]) << i;

  auto mulOut = WorldGenerationComputeService::runKernel(
      WorldGenerationComputeKernel::DenseUint32Mul, 1u, input, factor);
  for (size_t i = 0; i < input.size(); ++i)
    EXPECT_EQ(input[i] * factor, mulOut[i]) << i;

  auto andShadow = service.compareShadow(
      makeIdentity(80, WorldGenerationComputeKernel::DenseUint32And), input, mask,
      WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal, andShadow.verdict);

  auto orShadow = service.compareShadow(
      makeIdentity(81, WorldGenerationComputeKernel::DenseUint32Or), input, mask,
      WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal, orShadow.verdict);

  auto mulShadow = service.compareShadow(
      makeIdentity(82, WorldGenerationComputeKernel::DenseUint32Mul), input, factor,
      WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal, mulShadow.verdict);

  auto ticket = service.submit(makeIdentity(83, WorldGenerationComputeKernel::DenseUint32And),
      input, mask);
  ASSERT_TRUE(ticket);
  WorldGenerationComputeResult result;
  EXPECT_EQ(WorldGenerationComputePollStatus::Ready, service.poll(*ticket, &result));
  ASSERT_EQ(andOut.size(), result.values.size());
  for (size_t i = 0; i < andOut.size(); ++i)
    EXPECT_EQ(andOut[i], result.values[i]) << i;

#ifdef STAR_ENABLE_WORLDGEN_GL_COMPUTE
  auto probed = WorldGenerationComputeService::probeCapabilities();
  if (probed.gpuOpenGlCompute) {
    WorldGenerationComputeService gpuService(probed);
    for (auto kernel : {WorldGenerationComputeKernel::DenseUint32And,
             WorldGenerationComputeKernel::DenseUint32Or,
             WorldGenerationComputeKernel::DenseUint32Mul}) {
      uint32_t parameter = kernel == WorldGenerationComputeKernel::DenseUint32Mul ? factor : mask;
      auto report = gpuService.compareShadow(makeIdentity(90 + (uint64_t)kernel, kernel), input,
          parameter, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::GpuOpenGlCompute);
      EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal, report.verdict) << (int)kernel;
    }
  }
#endif
}

TEST(WorldGenerationComputeTest, CommitBatchAcceptsIndependentTickets) {
  WorldGenerationComputeService service;
  auto inputA = sampleInput();
  auto inputB = edgeCorpus();
  uint64_t fpA = WorldGenerationComputeService::fingerprintInput(inputA);
  uint64_t fpB = WorldGenerationComputeService::fingerprintInput(inputB);
  auto ticketA = service.submit(makeIdentity(160), inputA, 1u);
  auto ticketB = service.submit(makeIdentity(161), inputB, 2u);
  ASSERT_TRUE(ticketA);
  ASSERT_TRUE(ticketB);

  List<WorldGenerationComputeService::CommitBatchItem> items;
  WorldGenerationComputeService::CommitBatchItem a;
  a.ticket = *ticketA;
  a.liveIdentity = makeIdentity(160);
  a.liveInputFingerprint = fpA;
  items.append(a);
  WorldGenerationComputeService::CommitBatchItem b;
  b.ticket = *ticketB;
  b.liveIdentity = makeIdentity(161);
  b.liveInputFingerprint = fpB ^ 1u; // stale on purpose
  items.append(b);

  List<WorldGenerationComputeResult> outs;
  auto reports = service.commitBatch(std::move(items), &outs);
  ASSERT_EQ(2u, reports.size());
  ASSERT_EQ(2u, outs.size());
  EXPECT_EQ(WorldGenerationComputeCommitVerdict::Accepted, reports[0].verdict);
  EXPECT_EQ(inputA[0] ^ 1u, outs[0].values[0]);
  EXPECT_EQ(WorldGenerationComputeCommitVerdict::StaleInput, reports[1].verdict);
  EXPECT_TRUE(outs[1].values.empty());
}

TEST(WorldGenerationComputeTest, ApprovedRngStreamPreservesOrder) {
  // GPU-04: explicit streams only; same seed/streamId/counter → same sequence.
  WorldGenerationComputeRngStream a;
  a.seed = 0xcafef00dull;
  a.streamId = 7;
  WorldGenerationComputeRngStream b = a;
  auto seqA = WorldGenerationComputeService::fillRng(a, 8);
  auto seqB = WorldGenerationComputeService::fillRng(b, 8);
  ASSERT_EQ(8u, seqA.size());
  for (size_t i = 0; i < seqA.size(); ++i)
    EXPECT_EQ(seqA[i], seqB[i]) << i;
  EXPECT_EQ(8u, a.counter);
  EXPECT_EQ(8u, b.counter);

  WorldGenerationComputeRngStream c;
  c.seed = a.seed;
  c.streamId = 8; // different approved channel
  auto seqC = WorldGenerationComputeService::fillRng(c, 8);
  bool differed = false;
  for (size_t i = 0; i < seqA.size(); ++i) {
    if (seqA[i] != seqC[i])
      differed = true;
  }
  EXPECT_TRUE(differed);

  // Feed RNG into dense kernel + commit fingerprint path without assets.
  WorldGenerationComputeService service;
  WorldGenerationComputeRngStream stream;
  stream.seed = 1;
  stream.streamId = 1;
  auto input = WorldGenerationComputeService::fillRng(stream, 4);
  uint64_t fp = WorldGenerationComputeService::fingerprintInput(input);
  auto ticket = service.submit(makeIdentity(150), input, 0x11u);
  ASSERT_TRUE(ticket);
  WorldGenerationComputeResult result;
  EXPECT_EQ(WorldGenerationComputeCommitVerdict::Accepted,
      service.commit(*ticket, makeIdentity(150), fp, &result).verdict);
  EXPECT_EQ(input[0] ^ 0x11u, result.values[0]);
}

TEST(WorldGenerationComputeTest, SupportedKernelsCatalogListsDenseFamily) {
  EXPECT_TRUE(WorldGenerationComputeService::isKernelSupported(
      WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_FALSE(WorldGenerationComputeService::isKernelSupported(
      WorldGenerationComputeKernel::DenseUint32Xor, 99u));
  auto kernels = WorldGenerationComputeService::supportedKernels();
  ASSERT_EQ(328u, kernels.size());
  EXPECT_EQ(WorldGenerationComputeKernel::DenseUint32Xor, kernels[0]);
  EXPECT_EQ(WorldGenerationComputeKernel::DenseUint32HalfBitPairSwapNibble, kernels[kernels.size() - 1]);
  EXPECT_TRUE(WorldGenerationComputeService::supportedKernels(2u).empty());
}

TEST(WorldGenerationComputeTest, CompareShadowBatchRunsIndependentKernels) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  List<WorldGenerationComputeService::ShadowBatchItem> items;
  for (auto kernel : WorldGenerationComputeService::supportedKernels()) {
    WorldGenerationComputeService::ShadowBatchItem item;
    item.identity = makeIdentity(180 + (uint64_t)kernel, kernel);
    item.parameter = (kernel == WorldGenerationComputeKernel::DenseUint32Shl
            || kernel == WorldGenerationComputeKernel::DenseUint32Shr
            || kernel == WorldGenerationComputeKernel::DenseUint32Rotl
            || kernel == WorldGenerationComputeKernel::DenseUint32Rotr
            || kernel == WorldGenerationComputeKernel::DenseUint32RolNibbles
            || kernel == WorldGenerationComputeKernel::DenseUint32RorNibbles)
        ? 3u
        : (kernel == WorldGenerationComputeKernel::DenseUint32MaskNibbles ? 0xffu : 0x11u);
    items.append(item);
  }
  auto reports = service.compareShadowBatch(input, std::move(items));
  ASSERT_EQ(328u, reports.size());
  for (auto const& report : reports) {
    EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal, report.verdict)
        << (int)report.identity.kernel;
    EXPECT_FALSE(report.reproducer.valid);
  }
}

TEST(WorldGenerationComputeTest, DenseUint32ExtractInsertBroadcastNibbleExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  EXPECT_EQ(0x8u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32ExtractNibble, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);
  EXPECT_EQ(0x1u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32ExtractNibble, 1u, List<uint32_t>{0x12345678u},
          7u)[0]);
  EXPECT_EQ(0x1234567au,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32InsertNibble, 1u, List<uint32_t>{0x12345678u},
          0x0a00u)[0]);
  EXPECT_EQ(0x55555555u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32BroadcastNibble, 1u, List<uint32_t>{0x5u},
          0u)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(710, WorldGenerationComputeKernel::DenseUint32ExtractNibble), input, 1u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(711, WorldGenerationComputeKernel::DenseUint32InsertNibble), input, 0x0a01u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(712, WorldGenerationComputeKernel::DenseUint32BroadcastNibble), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
}

TEST(WorldGenerationComputeTest, DenseUint32SwapRolRorNibblesExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  // 0x12345678 → nibble pairs swap → 0x21436587
  EXPECT_EQ(0x21436587u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32SwapAdjacentNibbles, 1u,
          List<uint32_t>{0x12345678u}, 0u)[0]);
  // rotate left 1 nibble: 0x12345678 → 0x23456781
  EXPECT_EQ(0x23456781u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32RolNibbles, 1u, List<uint32_t>{0x12345678u},
          1u)[0]);
  // rotate right 1 nibble: 0x12345678 → 0x81234567
  EXPECT_EQ(0x81234567u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32RorNibbles, 1u, List<uint32_t>{0x12345678u},
          1u)[0]);
  EXPECT_EQ(0x12345678u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32RolNibbles, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(730, WorldGenerationComputeKernel::DenseUint32SwapAdjacentNibbles), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(731, WorldGenerationComputeKernel::DenseUint32RolNibbles), input, 3u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(732, WorldGenerationComputeKernel::DenseUint32RorNibbles), input, 3u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
}

TEST(WorldGenerationComputeTest, DenseUint32HasZeroSextCountZeroNibblesExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  EXPECT_EQ(0xffffffffu,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HasZeroNibble, 1u, List<uint32_t>{0x12305678u},
          0u)[0]);
  EXPECT_EQ(0u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HasZeroNibble, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);
  // nibble 0 of 0x12345678 is 8 → sign-extended
  EXPECT_EQ(0xfffffff8u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32SextNibble, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);
  EXPECT_EQ(0x7u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32SextNibble, 1u, List<uint32_t>{0x12345678u},
          1u)[0]);
  EXPECT_EQ(1u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32CountZeroNibbles, 1u,
          List<uint32_t>{0x12305678u}, 0u)[0]);
  EXPECT_EQ(0u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32CountZeroNibbles, 1u,
          List<uint32_t>{0x12345678u}, 0u)[0]);
  EXPECT_EQ(8u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32CountZeroNibbles, 1u, List<uint32_t>{0u}, 0u)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(750, WorldGenerationComputeKernel::DenseUint32HasZeroNibble), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(751, WorldGenerationComputeKernel::DenseUint32SextNibble), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(752, WorldGenerationComputeKernel::DenseUint32CountZeroNibbles), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
}

TEST(WorldGenerationComputeTest, DiscardReadyOpenTicketsByKernelIsSelective) {
  WorldGenerationComputeService service;
  service.setDeferredCpuCompletion(true);
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(760, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{1u}, 0u));
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(761, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{2u}, 0u));
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(762, WorldGenerationComputeKernel::DenseUint32Add), List<uint32_t>{3u}, 0u));

  EXPECT_EQ(0u,
      service.discardReadyOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_EQ(2u, service.completeOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_EQ(2u, service.readyOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_EQ(2u,
      service.discardReadyOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_EQ(0u, service.readyOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_EQ(0u, service.countOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_EQ(1u, service.inFlightByKernel(WorldGenerationComputeKernel::DenseUint32Add));
  EXPECT_EQ(0u,
      service.discardReadyOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Add));
}

TEST(WorldGenerationComputeTest, DenseUint32PackExpandLowHighNibblesExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  // 0x12345678 → low nibbles 8,6,4,2 → 0x2468
  EXPECT_EQ(0x2468u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32PackLowNibbles, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);
  // high nibbles 7,5,3,1 → 0x1357
  EXPECT_EQ(0x1357u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32PackHighNibbles, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);
  EXPECT_EQ(0x02040608u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32ExpandLowNibbles, 1u, List<uint32_t>{0x2468u},
          0u)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(770, WorldGenerationComputeKernel::DenseUint32PackLowNibbles), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(771, WorldGenerationComputeKernel::DenseUint32PackHighNibbles), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(772, WorldGenerationComputeKernel::DenseUint32ExpandLowNibbles), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
}

TEST(WorldGenerationComputeTest, DiscardCancelledOpenTicketsByKernelIsSelective) {
  WorldGenerationComputeService service;
  service.setDeferredCpuCompletion(true);
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(800, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{1u}, 0u));
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(801, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{2u}, 0u));
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(802, WorldGenerationComputeKernel::DenseUint32Add), List<uint32_t>{3u}, 0u));

  EXPECT_EQ(0u,
      service.discardCancelledOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_EQ(2u, service.cancelOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_EQ(2u, service.cancelledOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_EQ(2u,
      service.discardCancelledOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_EQ(0u, service.cancelledOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_EQ(0u, service.countOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_EQ(1u, service.inFlightByKernel(WorldGenerationComputeKernel::DenseUint32Add));
  EXPECT_EQ(0u,
      service.discardCancelledOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Add));
}

TEST(WorldGenerationComputeTest, DiscardTerminalOpenTicketsByKernelIsSelective) {
  WorldGenerationComputeService service;
  service.setDeferredCpuCompletion(true);
  auto xorReady = service.submit(
      makeIdentity(820, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{1u}, 0u);
  auto xorCancel = service.submit(
      makeIdentity(821, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{2u}, 0u);
  auto xorPending = service.submit(
      makeIdentity(822, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{3u}, 0u);
  ASSERT_TRUE((bool)xorReady);
  ASSERT_TRUE((bool)xorCancel);
  ASSERT_TRUE((bool)xorPending);
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(823, WorldGenerationComputeKernel::DenseUint32Add), List<uint32_t>{4u}, 0u));

  EXPECT_TRUE(service.complete(*xorReady));
  EXPECT_TRUE(service.cancel(*xorCancel));
  EXPECT_EQ(1u, service.readyOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_EQ(1u, service.cancelledOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_EQ(1u, service.inFlightByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_EQ(2u,
      service.discardTerminalOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_EQ(0u, service.readyOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_EQ(0u, service.cancelledOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_EQ(1u, service.inFlightByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_EQ(1u, service.inFlightByKernel(WorldGenerationComputeKernel::DenseUint32Add));
  EXPECT_EQ(0u,
      service.discardTerminalOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Add));
}

TEST(WorldGenerationComputeTest, ListOpenKernelsReturnsDistinctKernels) {
  WorldGenerationComputeService service;
  service.setDeferredCpuCompletion(true);
  EXPECT_TRUE(service.listOpenKernels().empty());
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(840, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{1u}, 0u));
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(841, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{2u}, 0u));
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(842, WorldGenerationComputeKernel::DenseUint32Add), List<uint32_t>{3u}, 0u));

  auto kernels = service.listOpenKernels();
  ASSERT_EQ(2u, kernels.size());
  EXPECT_TRUE(kernels[0] == WorldGenerationComputeKernel::DenseUint32Xor
      || kernels[0] == WorldGenerationComputeKernel::DenseUint32Add);
  EXPECT_TRUE(kernels[1] == WorldGenerationComputeKernel::DenseUint32Xor
      || kernels[1] == WorldGenerationComputeKernel::DenseUint32Add);
  EXPECT_NE(kernels[0], kernels[1]);

  EXPECT_EQ(2u, service.drainOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  kernels = service.listOpenKernels();
  ASSERT_EQ(1u, kernels.size());
  EXPECT_EQ(WorldGenerationComputeKernel::DenseUint32Add, kernels[0]);
  EXPECT_EQ(1u, service.drainOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Add));
  EXPECT_TRUE(service.listOpenKernels().empty());
}

TEST(WorldGenerationComputeTest, CountOpenKernelsTracksDistinctKernels) {
  WorldGenerationComputeService service;
  service.setDeferredCpuCompletion(true);
  EXPECT_EQ(0u, service.countOpenKernels());
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(860, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{1u}, 0u));
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(861, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{2u}, 0u));
  EXPECT_EQ(1u, service.countOpenKernels());
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(862, WorldGenerationComputeKernel::DenseUint32Add), List<uint32_t>{3u}, 0u));
  EXPECT_EQ(2u, service.countOpenKernels());
  EXPECT_EQ(service.listOpenKernels().size(), service.countOpenKernels());
  EXPECT_EQ(2u, service.drainOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_EQ(1u, service.countOpenKernels());
  EXPECT_EQ(1u, service.drainOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Add));
  EXPECT_EQ(0u, service.countOpenKernels());
}

TEST(WorldGenerationComputeTest, AnyOpenKernelsTracksPresence) {
  WorldGenerationComputeService service;
  service.setDeferredCpuCompletion(true);
  EXPECT_FALSE(service.anyOpenKernels());
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(880, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{1u}, 0u));
  EXPECT_TRUE(service.anyOpenKernels());
  EXPECT_EQ(1u, service.countOpenKernels());
  EXPECT_EQ(1u, service.drainOpenTickets());
  EXPECT_FALSE(service.anyOpenKernels());
  EXPECT_EQ(0u, service.countOpenKernels());
}

TEST(WorldGenerationComputeTest, ListReadyOpenKernelsReturnsReadyDistinctKernels) {
  WorldGenerationComputeService service;
  service.setDeferredCpuCompletion(true);
  EXPECT_TRUE(service.listReadyOpenKernels().empty());
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(900, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{1u}, 0u));
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(901, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{2u}, 0u));
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(902, WorldGenerationComputeKernel::DenseUint32Add), List<uint32_t>{3u}, 0u));
  EXPECT_TRUE(service.listReadyOpenKernels().empty());
  EXPECT_EQ(2u, service.countOpenKernels());
  EXPECT_EQ(2u, service.completeOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  auto ready = service.listReadyOpenKernels();
  ASSERT_EQ(1u, ready.size());
  EXPECT_EQ(WorldGenerationComputeKernel::DenseUint32Xor, ready[0]);
  EXPECT_EQ(1u, service.completeOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Add));
  ready = service.listReadyOpenKernels();
  ASSERT_EQ(2u, ready.size());
  EXPECT_TRUE(ready[0] == WorldGenerationComputeKernel::DenseUint32Xor
      || ready[0] == WorldGenerationComputeKernel::DenseUint32Add);
  EXPECT_TRUE(ready[1] == WorldGenerationComputeKernel::DenseUint32Xor
      || ready[1] == WorldGenerationComputeKernel::DenseUint32Add);
  EXPECT_NE(ready[0], ready[1]);
  EXPECT_EQ(3u, service.drainOpenTickets());
  EXPECT_TRUE(service.listReadyOpenKernels().empty());
}

TEST(WorldGenerationComputeTest, CountReadyOpenKernelsTracksReadyDistinctKernels) {
  WorldGenerationComputeService service;
  service.setDeferredCpuCompletion(true);
  EXPECT_EQ(0u, service.countReadyOpenKernels());
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(920, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{1u}, 0u));
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(921, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{2u}, 0u));
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(922, WorldGenerationComputeKernel::DenseUint32Add), List<uint32_t>{3u}, 0u));
  EXPECT_EQ(0u, service.countReadyOpenKernels());
  EXPECT_EQ(2u, service.countOpenKernels());
  EXPECT_EQ(2u, service.completeOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_EQ(1u, service.countReadyOpenKernels());
  EXPECT_EQ(service.listReadyOpenKernels().size(), service.countReadyOpenKernels());
  EXPECT_EQ(1u, service.completeOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Add));
  EXPECT_EQ(2u, service.countReadyOpenKernels());
  EXPECT_EQ(service.listReadyOpenKernels().size(), service.countReadyOpenKernels());
  EXPECT_EQ(3u, service.drainOpenTickets());
  EXPECT_EQ(0u, service.countReadyOpenKernels());
}

TEST(WorldGenerationComputeTest, AnyReadyOpenKernelsTracksReadyPresence) {
  WorldGenerationComputeService service;
  service.setDeferredCpuCompletion(true);
  EXPECT_FALSE(service.anyReadyOpenKernels());
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(940, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{1u}, 0u));
  EXPECT_FALSE(service.anyReadyOpenKernels());
  EXPECT_TRUE(service.anyOpenKernels());
  EXPECT_EQ(1u, service.completeOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_TRUE(service.anyReadyOpenKernels());
  EXPECT_EQ(1u, service.countReadyOpenKernels());
  EXPECT_EQ(1u, service.drainOpenTickets());
  EXPECT_FALSE(service.anyReadyOpenKernels());
  EXPECT_EQ(0u, service.countReadyOpenKernels());
}

TEST(WorldGenerationComputeTest, ListFailedOpenKernelsReturnsFailedDistinctKernels) {
  WorldGenerationComputeCapabilities caps = WorldGenerationComputeService::probeCapabilities();
  caps.gpuOpenGlCompute = true;
  WorldGenerationComputeService service(caps);
  service.setDeferredCpuCompletion(true);
  EXPECT_TRUE(service.listFailedOpenKernels().empty());
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(960, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{1u}, 0u,
      WorldGenerationComputeBackend::GpuOpenGlCompute));
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(961, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{2u}, 0u,
      WorldGenerationComputeBackend::GpuOpenGlCompute));
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(962, WorldGenerationComputeKernel::DenseUint32Add), List<uint32_t>{3u}, 0u,
      WorldGenerationComputeBackend::GpuOpenGlCompute));
  EXPECT_TRUE(service.listFailedOpenKernels().empty());
  EXPECT_EQ(2u, service.completeOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
#ifndef STAR_ENABLE_WORLDGEN_GL_COMPUTE
  auto failed = service.listFailedOpenKernels();
  ASSERT_EQ(1u, failed.size());
  EXPECT_EQ(WorldGenerationComputeKernel::DenseUint32Xor, failed[0]);
  EXPECT_EQ(1u, service.completeOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Add));
  failed = service.listFailedOpenKernels();
  ASSERT_EQ(2u, failed.size());
  EXPECT_TRUE(failed[0] == WorldGenerationComputeKernel::DenseUint32Xor
      || failed[0] == WorldGenerationComputeKernel::DenseUint32Add);
  EXPECT_TRUE(failed[1] == WorldGenerationComputeKernel::DenseUint32Xor
      || failed[1] == WorldGenerationComputeKernel::DenseUint32Add);
  EXPECT_NE(failed[0], failed[1]);
  EXPECT_EQ(3u, service.drainOpenTickets());
  EXPECT_TRUE(service.listFailedOpenKernels().empty());
#else
  // With GL builds, dispatch may succeed; just ensure the API stays consistent.
  size_t failedCount = service.listFailedOpenKernels().size();
  size_t readyCount = service.listReadyOpenKernels().size();
  EXPECT_EQ(1u, failedCount + readyCount);
  EXPECT_EQ(1u, service.completeOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Add));
  EXPECT_EQ(3u, service.drainOpenTickets());
  EXPECT_TRUE(service.listFailedOpenKernels().empty());
#endif
}

TEST(WorldGenerationComputeTest, CountFailedOpenKernelsTracksFailedDistinctKernels) {
  WorldGenerationComputeCapabilities caps = WorldGenerationComputeService::probeCapabilities();
  caps.gpuOpenGlCompute = true;
  WorldGenerationComputeService service(caps);
  service.setDeferredCpuCompletion(true);
  EXPECT_EQ(0u, service.countFailedOpenKernels());
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(980, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{1u}, 0u,
      WorldGenerationComputeBackend::GpuOpenGlCompute));
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(981, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{2u}, 0u,
      WorldGenerationComputeBackend::GpuOpenGlCompute));
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(982, WorldGenerationComputeKernel::DenseUint32Add), List<uint32_t>{3u}, 0u,
      WorldGenerationComputeBackend::GpuOpenGlCompute));
  EXPECT_EQ(0u, service.countFailedOpenKernels());
  EXPECT_EQ(2u, service.completeOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
#ifndef STAR_ENABLE_WORLDGEN_GL_COMPUTE
  EXPECT_EQ(1u, service.countFailedOpenKernels());
  EXPECT_EQ(service.listFailedOpenKernels().size(), service.countFailedOpenKernels());
  EXPECT_EQ(1u, service.completeOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Add));
  EXPECT_EQ(2u, service.countFailedOpenKernels());
  EXPECT_EQ(service.listFailedOpenKernels().size(), service.countFailedOpenKernels());
  EXPECT_EQ(3u, service.drainOpenTickets());
  EXPECT_EQ(0u, service.countFailedOpenKernels());
#else
  EXPECT_EQ(service.listFailedOpenKernels().size(), service.countFailedOpenKernels());
  EXPECT_EQ(1u, service.completeOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Add));
  EXPECT_EQ(service.listFailedOpenKernels().size(), service.countFailedOpenKernels());
  EXPECT_EQ(3u, service.drainOpenTickets());
  EXPECT_EQ(0u, service.countFailedOpenKernels());
#endif
}

TEST(WorldGenerationComputeTest, AnyFailedOpenKernelsTracksFailedPresence) {
  WorldGenerationComputeCapabilities caps = WorldGenerationComputeService::probeCapabilities();
  caps.gpuOpenGlCompute = true;
  WorldGenerationComputeService service(caps);
  service.setDeferredCpuCompletion(true);
  EXPECT_FALSE(service.anyFailedOpenKernels());
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(1000, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{1u}, 0u,
      WorldGenerationComputeBackend::GpuOpenGlCompute));
  EXPECT_FALSE(service.anyFailedOpenKernels());
  EXPECT_EQ(1u, service.completeOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
#ifndef STAR_ENABLE_WORLDGEN_GL_COMPUTE
  EXPECT_TRUE(service.anyFailedOpenKernels());
  EXPECT_EQ(1u, service.countFailedOpenKernels());
  EXPECT_EQ(1u, service.drainOpenTickets());
  EXPECT_FALSE(service.anyFailedOpenKernels());
  EXPECT_EQ(0u, service.countFailedOpenKernels());
#else
  bool failed = service.anyFailedOpenKernels();
  bool ready = service.anyReadyOpenKernels();
  EXPECT_TRUE(failed || ready);
  EXPECT_EQ(1u, service.drainOpenTickets());
  EXPECT_FALSE(service.anyFailedOpenKernels());
#endif
}

TEST(WorldGenerationComputeTest, ListCancelledOpenKernelsReturnsCancelledDistinctKernels) {
  WorldGenerationComputeService service;
  service.setDeferredCpuCompletion(true);
  EXPECT_TRUE(service.listCancelledOpenKernels().empty());
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(1020, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{1u}, 0u));
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(1021, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{2u}, 0u));
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(1022, WorldGenerationComputeKernel::DenseUint32Add), List<uint32_t>{3u}, 0u));
  EXPECT_TRUE(service.listCancelledOpenKernels().empty());
  EXPECT_EQ(2u, service.cancelOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  auto cancelled = service.listCancelledOpenKernels();
  ASSERT_EQ(1u, cancelled.size());
  EXPECT_EQ(WorldGenerationComputeKernel::DenseUint32Xor, cancelled[0]);
  EXPECT_EQ(1u, service.cancelOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Add));
  cancelled = service.listCancelledOpenKernels();
  ASSERT_EQ(2u, cancelled.size());
  EXPECT_TRUE(cancelled[0] == WorldGenerationComputeKernel::DenseUint32Xor
      || cancelled[0] == WorldGenerationComputeKernel::DenseUint32Add);
  EXPECT_TRUE(cancelled[1] == WorldGenerationComputeKernel::DenseUint32Xor
      || cancelled[1] == WorldGenerationComputeKernel::DenseUint32Add);
  EXPECT_NE(cancelled[0], cancelled[1]);
  EXPECT_EQ(3u, service.drainOpenTickets());
  EXPECT_TRUE(service.listCancelledOpenKernels().empty());
}

TEST(WorldGenerationComputeTest, CountCancelledOpenKernelsTracksCancelledDistinctKernels) {
  WorldGenerationComputeService service;
  service.setDeferredCpuCompletion(true);
  EXPECT_EQ(0u, service.countCancelledOpenKernels());
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(1040, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{1u}, 0u));
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(1041, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{2u}, 0u));
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(1042, WorldGenerationComputeKernel::DenseUint32Add), List<uint32_t>{3u}, 0u));
  EXPECT_EQ(0u, service.countCancelledOpenKernels());
  EXPECT_EQ(2u, service.cancelOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_EQ(1u, service.countCancelledOpenKernels());
  EXPECT_EQ(service.listCancelledOpenKernels().size(), service.countCancelledOpenKernels());
  EXPECT_EQ(1u, service.cancelOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Add));
  EXPECT_EQ(2u, service.countCancelledOpenKernels());
  EXPECT_EQ(service.listCancelledOpenKernels().size(), service.countCancelledOpenKernels());
  EXPECT_EQ(3u, service.drainOpenTickets());
  EXPECT_EQ(0u, service.countCancelledOpenKernels());
}

TEST(WorldGenerationComputeTest, AnyCancelledOpenKernelsTracksCancelledPresence) {
  WorldGenerationComputeService service;
  service.setDeferredCpuCompletion(true);
  EXPECT_FALSE(service.anyCancelledOpenKernels());
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(1060, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{1u}, 0u));
  EXPECT_FALSE(service.anyCancelledOpenKernels());
  EXPECT_TRUE(service.anyOpenKernels());
  EXPECT_EQ(1u, service.cancelOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_TRUE(service.anyCancelledOpenKernels());
  EXPECT_EQ(1u, service.countCancelledOpenKernels());
  EXPECT_EQ(1u, service.drainOpenTickets());
  EXPECT_FALSE(service.anyCancelledOpenKernels());
  EXPECT_EQ(0u, service.countCancelledOpenKernels());
}

TEST(WorldGenerationComputeTest, ListPendingOpenKernelsReturnsPendingDistinctKernels) {
  WorldGenerationComputeService service;
  service.setDeferredCpuCompletion(true);
  EXPECT_TRUE(service.listPendingOpenKernels().empty());
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(1080, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{1u}, 0u));
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(1081, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{2u}, 0u));
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(1082, WorldGenerationComputeKernel::DenseUint32Add), List<uint32_t>{3u}, 0u));
  auto pending = service.listPendingOpenKernels();
  ASSERT_EQ(2u, pending.size());
  EXPECT_TRUE(pending[0] == WorldGenerationComputeKernel::DenseUint32Xor
      || pending[0] == WorldGenerationComputeKernel::DenseUint32Add);
  EXPECT_TRUE(pending[1] == WorldGenerationComputeKernel::DenseUint32Xor
      || pending[1] == WorldGenerationComputeKernel::DenseUint32Add);
  EXPECT_NE(pending[0], pending[1]);
  EXPECT_EQ(2u, service.completeOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  pending = service.listPendingOpenKernels();
  ASSERT_EQ(1u, pending.size());
  EXPECT_EQ(WorldGenerationComputeKernel::DenseUint32Add, pending[0]);
  EXPECT_EQ(1u, service.completeOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Add));
  EXPECT_TRUE(service.listPendingOpenKernels().empty());
  EXPECT_EQ(3u, service.drainOpenTickets());
  EXPECT_TRUE(service.listPendingOpenKernels().empty());
}

TEST(WorldGenerationComputeTest, CountPendingOpenKernelsTracksPendingDistinctKernels) {
  WorldGenerationComputeService service;
  service.setDeferredCpuCompletion(true);
  EXPECT_EQ(0u, service.countPendingOpenKernels());
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(1100, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{1u}, 0u));
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(1101, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{2u}, 0u));
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(1102, WorldGenerationComputeKernel::DenseUint32Add), List<uint32_t>{3u}, 0u));
  EXPECT_EQ(2u, service.countPendingOpenKernels());
  EXPECT_EQ(service.listPendingOpenKernels().size(), service.countPendingOpenKernels());
  EXPECT_EQ(2u, service.completeOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_EQ(1u, service.countPendingOpenKernels());
  EXPECT_EQ(service.listPendingOpenKernels().size(), service.countPendingOpenKernels());
  EXPECT_EQ(1u, service.completeOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Add));
  EXPECT_EQ(0u, service.countPendingOpenKernels());
  EXPECT_EQ(3u, service.drainOpenTickets());
  EXPECT_EQ(0u, service.countPendingOpenKernels());
}


TEST(WorldGenerationComputeTest, AnyPendingOpenKernelsTracksPendingPresence) {
  WorldGenerationComputeService service;
  service.setDeferredCpuCompletion(true);
  EXPECT_FALSE(service.anyPendingOpenKernels());
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(1120, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{1u}, 0u));
  EXPECT_TRUE(service.anyPendingOpenKernels());
  EXPECT_EQ(1u, service.countPendingOpenKernels());
  EXPECT_EQ(1u, service.completeOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_FALSE(service.anyPendingOpenKernels());
  EXPECT_EQ(0u, service.countPendingOpenKernels());
  EXPECT_EQ(1u, service.drainOpenTickets());
  EXPECT_FALSE(service.anyPendingOpenKernels());
}


TEST(WorldGenerationComputeTest, ListPendingOpenTicketsByKernelReturnsPendingIdsOnly) {
  WorldGenerationComputeService service;
  service.setDeferredCpuCompletion(true);
  auto xorA = service.submit(
      makeIdentity(1140, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{1u}, 0u);
  auto xorB = service.submit(
      makeIdentity(1141, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{2u}, 0u);
  auto add = service.submit(
      makeIdentity(1142, WorldGenerationComputeKernel::DenseUint32Add), List<uint32_t>{3u}, 0u);
  ASSERT_TRUE((bool)xorA);
  ASSERT_TRUE((bool)xorB);
  ASSERT_TRUE((bool)add);

  auto pendingXor =
      service.listPendingOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor);
  ASSERT_EQ(2u, pendingXor.size());
  EXPECT_TRUE(pendingXor[0] == *xorA || pendingXor[0] == *xorB);
  EXPECT_TRUE(pendingXor[1] == *xorA || pendingXor[1] == *xorB);
  EXPECT_NE(pendingXor[0].id, pendingXor[1].id);
  auto pendingAdd =
      service.listPendingOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Add);
  ASSERT_EQ(1u, pendingAdd.size());
  EXPECT_EQ(*add, pendingAdd[0]);
  EXPECT_EQ(2u, service.completeOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_TRUE(service.listPendingOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor)
          .empty());
  EXPECT_EQ(1u, service.inFlightByKernel(WorldGenerationComputeKernel::DenseUint32Add));
}


TEST(WorldGenerationComputeTest, PendingOpenTicketsByKernelCountsPendingMatches) {
  WorldGenerationComputeService service;
  service.setDeferredCpuCompletion(true);
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(1160, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{1u}, 0u));
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(1161, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{2u}, 0u));
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(1162, WorldGenerationComputeKernel::DenseUint32Add), List<uint32_t>{3u}, 0u));
  EXPECT_EQ(2u, service.pendingOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_EQ(1u, service.pendingOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Add));
  EXPECT_EQ(2u, service.completeOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_EQ(0u, service.pendingOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_EQ(2u, service.readyOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_EQ(1u, service.pendingOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Add));
  EXPECT_EQ(1u, service.inFlightByKernel(WorldGenerationComputeKernel::DenseUint32Add));
}


TEST(WorldGenerationComputeTest, AnyPendingOpenTicketsByKernelTracksPendingPresence) {
  WorldGenerationComputeService service;
  service.setDeferredCpuCompletion(true);
  EXPECT_FALSE(service.anyPendingOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(1180, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{1u}, 0u));
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(1181, WorldGenerationComputeKernel::DenseUint32Add), List<uint32_t>{2u}, 0u));
  EXPECT_TRUE(service.anyPendingOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_TRUE(service.anyPendingOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Add));
  EXPECT_EQ(1u, service.completeOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_FALSE(service.anyPendingOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_TRUE(service.anyReadyOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_TRUE(service.anyPendingOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Add));
}


TEST(WorldGenerationComputeTest, DiscardPendingOpenTicketsByKernelIsSelective) {
  WorldGenerationComputeService service;
  service.setDeferredCpuCompletion(true);
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(1200, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{1u}, 0u));
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(1201, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{2u}, 0u));
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(1202, WorldGenerationComputeKernel::DenseUint32Add), List<uint32_t>{3u}, 0u));
  EXPECT_EQ(2u, service.pendingOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_EQ(2u,
      service.discardPendingOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_EQ(0u, service.pendingOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_EQ(0u, service.countOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_EQ(1u, service.inFlightByKernel(WorldGenerationComputeKernel::DenseUint32Add));
  EXPECT_EQ(0u,
      service.discardPendingOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_EQ(1u,
      service.discardPendingOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Add));
  EXPECT_EQ(0u, service.countOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Add));
}


TEST(WorldGenerationComputeTest, CountPendingOpenTicketsTracksPendingTicketCount) {
  WorldGenerationComputeService service;
  service.setDeferredCpuCompletion(true);
  EXPECT_EQ(0u, service.countPendingOpenTickets());
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(1220, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{1u}, 0u));
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(1221, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{2u}, 0u));
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(1222, WorldGenerationComputeKernel::DenseUint32Add), List<uint32_t>{3u}, 0u));
  EXPECT_EQ(3u, service.countPendingOpenTickets());
  EXPECT_EQ(2u, service.countPendingOpenKernels());
  EXPECT_EQ(2u, service.completeOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_EQ(1u, service.countPendingOpenTickets());
  EXPECT_EQ(1u, service.countPendingOpenKernels());
  EXPECT_EQ(1u,
      service.discardPendingOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Add));
  EXPECT_EQ(0u, service.countPendingOpenTickets());
  EXPECT_EQ(2u, service.drainOpenTickets());
  EXPECT_EQ(0u, service.countPendingOpenTickets());
}


TEST(WorldGenerationComputeTest, ListPendingOpenTicketsReturnsPendingIdsOnly) {
  WorldGenerationComputeService service;
  service.setDeferredCpuCompletion(true);
  EXPECT_TRUE(service.listPendingOpenTickets().empty());
  auto xorA = service.submit(
      makeIdentity(1240, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{1u}, 0u);
  auto xorB = service.submit(
      makeIdentity(1241, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{2u}, 0u);
  auto add = service.submit(
      makeIdentity(1242, WorldGenerationComputeKernel::DenseUint32Add), List<uint32_t>{3u}, 0u);
  ASSERT_TRUE((bool)xorA);
  ASSERT_TRUE((bool)xorB);
  ASSERT_TRUE((bool)add);
  auto pending = service.listPendingOpenTickets();
  ASSERT_EQ(3u, pending.size());
  EXPECT_EQ(3u, service.countPendingOpenTickets());
  EXPECT_EQ(2u, service.completeOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  pending = service.listPendingOpenTickets();
  ASSERT_EQ(1u, pending.size());
  EXPECT_EQ(*add, pending[0]);
  EXPECT_EQ(3u, service.listOpenTickets().size());
  EXPECT_EQ(1u,
      service.discardPendingOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Add));
  EXPECT_TRUE(service.listPendingOpenTickets().empty());
  EXPECT_EQ(2u, service.drainOpenTickets());
  EXPECT_TRUE(service.listOpenTickets().empty());
}


TEST(WorldGenerationComputeTest, ListReadyOpenTicketsReturnsReadyIdsOnly) {
  WorldGenerationComputeService service;
  service.setDeferredCpuCompletion(true);
  EXPECT_TRUE(service.listReadyOpenTickets().empty());
  auto xorA = service.submit(
      makeIdentity(1260, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{1u}, 0u);
  auto xorB = service.submit(
      makeIdentity(1261, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{2u}, 0u);
  auto add = service.submit(
      makeIdentity(1262, WorldGenerationComputeKernel::DenseUint32Add), List<uint32_t>{3u}, 0u);
  ASSERT_TRUE((bool)xorA);
  ASSERT_TRUE((bool)xorB);
  ASSERT_TRUE((bool)add);
  EXPECT_TRUE(service.listReadyOpenTickets().empty());
  EXPECT_EQ(2u, service.completeOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  auto ready = service.listReadyOpenTickets();
  ASSERT_EQ(2u, ready.size());
  EXPECT_TRUE(ready[0] == *xorA || ready[0] == *xorB);
  EXPECT_TRUE(ready[1] == *xorA || ready[1] == *xorB);
  EXPECT_NE(ready[0].id, ready[1].id);
  EXPECT_EQ(1u, service.listPendingOpenTickets().size());
  EXPECT_EQ(2u,
      service.discardReadyOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_TRUE(service.listReadyOpenTickets().empty());
  EXPECT_EQ(1u, service.inFlightByKernel(WorldGenerationComputeKernel::DenseUint32Add));
}


TEST(WorldGenerationComputeTest, CountReadyOpenTicketsTracksReadyTicketCount) {
  WorldGenerationComputeService service;
  service.setDeferredCpuCompletion(true);
  EXPECT_EQ(0u, service.countReadyOpenTickets());
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(1280, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{1u}, 0u));
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(1281, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{2u}, 0u));
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(1282, WorldGenerationComputeKernel::DenseUint32Add), List<uint32_t>{3u}, 0u));
  EXPECT_EQ(0u, service.countReadyOpenTickets());
  EXPECT_EQ(3u, service.countPendingOpenTickets());
  EXPECT_EQ(2u, service.completeOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_EQ(2u, service.countReadyOpenTickets());
  EXPECT_EQ(1u, service.countPendingOpenTickets());
  EXPECT_EQ(2u,
      service.discardReadyOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_EQ(0u, service.countReadyOpenTickets());
  EXPECT_EQ(1u, service.discardPendingOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Add));
  EXPECT_EQ(0u, service.countReadyOpenTickets());
  EXPECT_EQ(0u, service.countPendingOpenTickets());
}


TEST(WorldGenerationComputeTest, AnyReadyOpenTicketsTracksReadyPresence) {
  WorldGenerationComputeService service;
  service.setDeferredCpuCompletion(true);
  EXPECT_FALSE(service.anyReadyOpenTickets());
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(1300, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{1u}, 0u));
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(1301, WorldGenerationComputeKernel::DenseUint32Add), List<uint32_t>{2u}, 0u));
  EXPECT_FALSE(service.anyReadyOpenTickets());
  EXPECT_EQ(1u, service.completeOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_TRUE(service.anyReadyOpenTickets());
  EXPECT_EQ(1u, service.countReadyOpenTickets());
  EXPECT_EQ(1u,
      service.discardReadyOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_FALSE(service.anyReadyOpenTickets());
  EXPECT_EQ(1u, service.inFlightByKernel(WorldGenerationComputeKernel::DenseUint32Add));
}


TEST(WorldGenerationComputeTest, DiscardReadyOpenTicketsDrainsAllReady) {
  WorldGenerationComputeService service;
  service.setDeferredCpuCompletion(true);
  EXPECT_EQ(0u, service.discardReadyOpenTickets());
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(1320, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{1u}, 0u));
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(1321, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{2u}, 0u));
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(1322, WorldGenerationComputeKernel::DenseUint32Add), List<uint32_t>{3u}, 0u));
  EXPECT_EQ(0u, service.discardReadyOpenTickets());
  EXPECT_EQ(2u, service.completeOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_EQ(1u, service.completeOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Add));
  EXPECT_EQ(3u, service.countReadyOpenTickets());
  EXPECT_EQ(3u, service.discardReadyOpenTickets());
  EXPECT_EQ(0u, service.countReadyOpenTickets());
  EXPECT_EQ(0u, service.openTickets());
  EXPECT_EQ(0u, service.discardReadyOpenTickets());
}


TEST(WorldGenerationComputeTest, DiscardFailedOpenTicketsDrainsAllFailed) {
  WorldGenerationComputeCapabilities caps = WorldGenerationComputeService::probeCapabilities();
  caps.gpuOpenGlCompute = true;
  WorldGenerationComputeService service(caps);
  service.setDeferredCpuCompletion(true);
  EXPECT_EQ(0u, service.discardFailedOpenTickets());
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(1340, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{1u}, 0u,
      WorldGenerationComputeBackend::GpuOpenGlCompute));
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(1341, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{2u}, 0u,
      WorldGenerationComputeBackend::GpuOpenGlCompute));
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(1342, WorldGenerationComputeKernel::DenseUint32Add), List<uint32_t>{3u}, 0u,
      WorldGenerationComputeBackend::GpuOpenGlCompute));
  EXPECT_EQ(0u, service.discardFailedOpenTickets());
  EXPECT_EQ(2u, service.completeOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_EQ(1u, service.completeOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Add));
#ifndef STAR_ENABLE_WORLDGEN_GL_COMPUTE
  EXPECT_EQ(3u, service.discardFailedOpenTickets());
  EXPECT_EQ(0u, service.openTickets());
  EXPECT_EQ(0u, service.discardFailedOpenTickets());
#else
  size_t discardedFailed = service.discardFailedOpenTickets();
  size_t discardedReady = service.discardReadyOpenTickets();
  EXPECT_EQ(3u, discardedFailed + discardedReady);
  EXPECT_EQ(0u, service.openTickets());
#endif
}


TEST(WorldGenerationComputeTest, DiscardCancelledOpenTicketsDrainsAllCancelled) {
  WorldGenerationComputeService service;
  service.setDeferredCpuCompletion(true);
  EXPECT_EQ(0u, service.discardCancelledOpenTickets());
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(1360, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{1u}, 0u));
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(1361, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{2u}, 0u));
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(1362, WorldGenerationComputeKernel::DenseUint32Add), List<uint32_t>{3u}, 0u));
  EXPECT_EQ(0u, service.discardCancelledOpenTickets());
  EXPECT_EQ(2u, service.cancelOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_EQ(1u, service.cancelOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Add));
  EXPECT_EQ(3u, service.discardCancelledOpenTickets());
  EXPECT_EQ(0u, service.openTickets());
  EXPECT_EQ(0u, service.discardCancelledOpenTickets());
}


TEST(WorldGenerationComputeTest, DiscardTerminalOpenTicketsDrainsAllTerminal) {
  WorldGenerationComputeService service;
  service.setDeferredCpuCompletion(true);
  EXPECT_EQ(0u, service.discardTerminalOpenTickets());
  auto ready = service.submit(
      makeIdentity(1380, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{1u}, 0u);
  auto cancel = service.submit(
      makeIdentity(1381, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{2u}, 0u);
  auto pending = service.submit(
      makeIdentity(1382, WorldGenerationComputeKernel::DenseUint32Add), List<uint32_t>{3u}, 0u);
  ASSERT_TRUE((bool)ready);
  ASSERT_TRUE((bool)cancel);
  ASSERT_TRUE((bool)pending);
  EXPECT_TRUE(service.complete(*ready));
  EXPECT_TRUE(service.cancel(*cancel));
  EXPECT_EQ(2u, service.discardTerminalOpenTickets());
  EXPECT_EQ(1u, service.inFlightByKernel(WorldGenerationComputeKernel::DenseUint32Add));
  EXPECT_EQ(0u, service.countReadyOpenTickets());
  EXPECT_EQ(0u, service.discardCancelledOpenTickets());
  EXPECT_EQ(1u, service.discardPendingOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Add));
  EXPECT_EQ(0u, service.openTickets());
}


TEST(WorldGenerationComputeTest, DiscardPendingOpenTicketsDrainsAllPending) {
  WorldGenerationComputeService service;
  service.setDeferredCpuCompletion(true);
  EXPECT_EQ(0u, service.discardPendingOpenTickets());
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(1400, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{1u}, 0u));
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(1401, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{2u}, 0u));
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(1402, WorldGenerationComputeKernel::DenseUint32Add), List<uint32_t>{3u}, 0u));
  EXPECT_EQ(3u, service.countPendingOpenTickets());
  EXPECT_EQ(3u, service.discardPendingOpenTickets());
  EXPECT_EQ(0u, service.countPendingOpenTickets());
  EXPECT_EQ(0u, service.openTickets());
  EXPECT_EQ(0u, service.discardPendingOpenTickets());
}


TEST(WorldGenerationComputeTest, AnyPendingOpenTicketsTracksPendingPresence) {
  WorldGenerationComputeService service;
  service.setDeferredCpuCompletion(true);
  EXPECT_FALSE(service.anyPendingOpenTickets());
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(1420, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{1u}, 0u));
  EXPECT_TRUE(service.anyPendingOpenTickets());
  EXPECT_EQ(1u, service.completeOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_FALSE(service.anyPendingOpenTickets());
  EXPECT_TRUE(service.anyReadyOpenTickets());
  EXPECT_EQ(1u, service.discardReadyOpenTickets());
  EXPECT_FALSE(service.anyPendingOpenTickets());
}


TEST(WorldGenerationComputeTest, ListFailedOpenTicketsReturnsFailedIdsOnly) {
  WorldGenerationComputeCapabilities caps = WorldGenerationComputeService::probeCapabilities();
  caps.gpuOpenGlCompute = true;
  WorldGenerationComputeService service(caps);
  service.setDeferredCpuCompletion(true);
  EXPECT_TRUE(service.listFailedOpenTickets().empty());
  auto a = service.submit(
      makeIdentity(1440, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{1u}, 0u,
      WorldGenerationComputeBackend::GpuOpenGlCompute);
  auto b = service.submit(
      makeIdentity(1441, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{2u}, 0u,
      WorldGenerationComputeBackend::GpuOpenGlCompute);
  auto c = service.submit(
      makeIdentity(1442, WorldGenerationComputeKernel::DenseUint32Add), List<uint32_t>{3u}, 0u,
      WorldGenerationComputeBackend::GpuOpenGlCompute);
  ASSERT_TRUE((bool)a);
  ASSERT_TRUE((bool)b);
  ASSERT_TRUE((bool)c);
  EXPECT_EQ(2u, service.completeOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
#ifndef STAR_ENABLE_WORLDGEN_GL_COMPUTE
  auto failed = service.listFailedOpenTickets();
  ASSERT_EQ(2u, failed.size());
  EXPECT_TRUE(failed[0] == *a || failed[0] == *b);
  EXPECT_TRUE(failed[1] == *a || failed[1] == *b);
  EXPECT_NE(failed[0].id, failed[1].id);
  EXPECT_EQ(1u, service.inFlightByKernel(WorldGenerationComputeKernel::DenseUint32Add));
  EXPECT_EQ(2u, service.discardFailedOpenTickets());
  EXPECT_TRUE(service.listFailedOpenTickets().empty());
#else
  size_t failedCount = service.listFailedOpenTickets().size();
  size_t readyCount = service.listReadyOpenTickets().size();
  EXPECT_EQ(2u, failedCount + readyCount);
  EXPECT_EQ(failedCount, service.discardFailedOpenTickets());
  EXPECT_EQ(readyCount, service.discardReadyOpenTickets());
#endif
}


TEST(WorldGenerationComputeTest, CountFailedOpenTicketsTracksFailedTicketCount) {
  WorldGenerationComputeCapabilities caps = WorldGenerationComputeService::probeCapabilities();
  caps.gpuOpenGlCompute = true;
  WorldGenerationComputeService service(caps);
  service.setDeferredCpuCompletion(true);
  EXPECT_EQ(0u, service.countFailedOpenTickets());
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(1460, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{1u}, 0u,
      WorldGenerationComputeBackend::GpuOpenGlCompute));
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(1461, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{2u}, 0u,
      WorldGenerationComputeBackend::GpuOpenGlCompute));
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(1462, WorldGenerationComputeKernel::DenseUint32Add), List<uint32_t>{3u}, 0u,
      WorldGenerationComputeBackend::GpuOpenGlCompute));
  EXPECT_EQ(2u, service.completeOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
#ifndef STAR_ENABLE_WORLDGEN_GL_COMPUTE
  EXPECT_EQ(2u, service.countFailedOpenTickets());
  EXPECT_EQ(2u, service.discardFailedOpenTickets());
  EXPECT_EQ(0u, service.countFailedOpenTickets());
#else
  size_t failed = service.countFailedOpenTickets();
  size_t ready = service.countReadyOpenTickets();
  EXPECT_EQ(2u, failed + ready);
  EXPECT_EQ(failed, service.discardFailedOpenTickets());
  EXPECT_EQ(ready, service.discardReadyOpenTickets());
#endif
}


TEST(WorldGenerationComputeTest, AnyFailedOpenTicketsTracksFailedPresence) {
  WorldGenerationComputeCapabilities caps = WorldGenerationComputeService::probeCapabilities();
  caps.gpuOpenGlCompute = true;
  WorldGenerationComputeService service(caps);
  service.setDeferredCpuCompletion(true);
  EXPECT_FALSE(service.anyFailedOpenTickets());
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(1480, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{1u}, 0u,
      WorldGenerationComputeBackend::GpuOpenGlCompute));
  EXPECT_EQ(1u, service.completeOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
#ifndef STAR_ENABLE_WORLDGEN_GL_COMPUTE
  EXPECT_TRUE(service.anyFailedOpenTickets());
  EXPECT_EQ(1u, service.discardFailedOpenTickets());
  EXPECT_FALSE(service.anyFailedOpenTickets());
#else
  EXPECT_TRUE(service.anyFailedOpenTickets() || service.anyReadyOpenTickets());
  service.discardFailedOpenTickets();
  service.discardReadyOpenTickets();
  EXPECT_FALSE(service.anyFailedOpenTickets());
#endif
}































































TEST(WorldGenerationComputeTest, RejectedDuplicatesVacantAliasesRejectedDuplicatesEmpty) {
  WorldGenerationComputeService service;
  service.setDeferredCpuCompletion(true);
  EXPECT_TRUE(service.rejectedDuplicatesVacant());
  auto ticket = service.submit(makeIdentity(2230), List<uint32_t>{4u}, 1u);
  ASSERT_TRUE((bool)ticket);
  EXPECT_EQ(1u, service.failContext());
  WorldGenerationComputeResult result;
  EXPECT_EQ(WorldGenerationComputePollStatus::Ready, service.poll(*ticket, &result));
  EXPECT_FALSE(service.rejectedDuplicatesVacant());
  EXPECT_EQ(service.rejectedDuplicatesEmpty(), service.rejectedDuplicatesVacant());
  EXPECT_EQ(service.rejectedDuplicatesIsEmpty(), service.rejectedDuplicatesVacant());
  EXPECT_EQ(1u, service.clearRejectedDuplicates());
  EXPECT_TRUE(service.rejectedDuplicatesVacant());
}

TEST(WorldGenerationComputeTest, RejectedDuplicatesHashAliasesFingerprintRejectedDuplicates) {
  WorldGenerationComputeService service;
  service.setDeferredCpuCompletion(true);
  EXPECT_EQ(service.fingerprintRejectedDuplicates(), service.rejectedDuplicatesHash());
  auto ticket = service.submit(makeIdentity(2210), List<uint32_t>{4u}, 1u);
  ASSERT_TRUE((bool)ticket);
  EXPECT_EQ(1u, service.failContext());
  WorldGenerationComputeResult result;
  EXPECT_EQ(WorldGenerationComputePollStatus::Ready, service.poll(*ticket, &result));
  EXPECT_EQ(service.fingerprintRejectedDuplicates(), service.rejectedDuplicatesHash());
  EXPECT_EQ(service.rejectedDuplicatesDigest(), service.rejectedDuplicatesHash());
  EXPECT_EQ(1u, service.clearRejectedDuplicates());
  EXPECT_EQ(service.fingerprintRejectedDuplicates(), service.rejectedDuplicatesHash());
}

TEST(WorldGenerationComputeTest, RejectedDuplicatesPurgeAliasesClearRejectedDuplicates) {
  WorldGenerationComputeService service;
  service.setDeferredCpuCompletion(true);
  auto ticket = service.submit(makeIdentity(2190), List<uint32_t>{4u}, 1u);
  ASSERT_TRUE((bool)ticket);
  EXPECT_EQ(1u, service.failContext());
  WorldGenerationComputeResult result;
  EXPECT_EQ(WorldGenerationComputePollStatus::Ready, service.poll(*ticket, &result));
  EXPECT_TRUE(service.anyRejectedDuplicates());
  EXPECT_EQ(1u, service.rejectedDuplicatesPurge());
  EXPECT_FALSE(service.anyRejectedDuplicates());
  EXPECT_EQ(0u, service.rejectedDuplicatesPurge());
}

TEST(WorldGenerationComputeTest, RejectedDuplicatesDigestAliasesFingerprintRejectedDuplicates) {
  WorldGenerationComputeService service;
  service.setDeferredCpuCompletion(true);
  EXPECT_EQ(service.fingerprintRejectedDuplicates(), service.rejectedDuplicatesDigest());
  auto ticket = service.submit(makeIdentity(2170), List<uint32_t>{4u}, 1u);
  ASSERT_TRUE((bool)ticket);
  EXPECT_EQ(1u, service.failContext());
  WorldGenerationComputeResult result;
  EXPECT_EQ(WorldGenerationComputePollStatus::Ready, service.poll(*ticket, &result));
  EXPECT_EQ(service.fingerprintRejectedDuplicates(), service.rejectedDuplicatesDigest());
  EXPECT_NE(0u, service.rejectedDuplicatesDigest());
  EXPECT_EQ(1u, service.clearRejectedDuplicates());
  EXPECT_EQ(service.fingerprintRejectedDuplicates(), service.rejectedDuplicatesDigest());
}

TEST(WorldGenerationComputeTest, RejectedDuplicatesMembersAliasesListRejectedDuplicates) {
  WorldGenerationComputeService service;
  service.setDeferredCpuCompletion(true);
  EXPECT_TRUE(service.rejectedDuplicatesMembers().empty());
  auto ticket = service.submit(makeIdentity(2150), List<uint32_t>{4u}, 1u);
  ASSERT_TRUE((bool)ticket);
  EXPECT_EQ(1u, service.failContext());
  WorldGenerationComputeResult result;
  EXPECT_EQ(WorldGenerationComputePollStatus::Ready, service.poll(*ticket, &result));
  auto members = service.rejectedDuplicatesMembers();
  ASSERT_EQ(1u, members.size());
  EXPECT_EQ(*ticket, members[0]);
  EXPECT_EQ(service.listRejectedDuplicates().size(), members.size());
  EXPECT_EQ(1u, service.clearRejectedDuplicates());
  EXPECT_TRUE(service.rejectedDuplicatesMembers().empty());
}

TEST(WorldGenerationComputeTest, RejectedDuplicatesCardinalityAliasesRejectedDuplicatesCount) {
  WorldGenerationComputeService service;
  service.setDeferredCpuCompletion(true);
  EXPECT_EQ(0u, service.rejectedDuplicatesCardinality());
  auto ticket = service.submit(makeIdentity(2130), List<uint32_t>{4u}, 1u);
  ASSERT_TRUE((bool)ticket);
  EXPECT_EQ(1u, service.failContext());
  WorldGenerationComputeResult result;
  EXPECT_EQ(WorldGenerationComputePollStatus::Ready, service.poll(*ticket, &result));
  EXPECT_EQ(1u, service.rejectedDuplicatesCardinality());
  EXPECT_EQ(service.rejectedDuplicatesCount(), service.rejectedDuplicatesCardinality());
  EXPECT_EQ(1u, service.clearRejectedDuplicates());
  EXPECT_EQ(0u, service.rejectedDuplicatesCardinality());
}

TEST(WorldGenerationComputeTest, RejectedDuplicatesIsEmptyAliasesRejectedDuplicatesEmpty) {
  WorldGenerationComputeService service;
  service.setDeferredCpuCompletion(true);
  EXPECT_TRUE(service.rejectedDuplicatesIsEmpty());
  auto ticket = service.submit(makeIdentity(2110), List<uint32_t>{4u}, 1u);
  ASSERT_TRUE((bool)ticket);
  EXPECT_EQ(1u, service.failContext());
  WorldGenerationComputeResult result;
  EXPECT_EQ(WorldGenerationComputePollStatus::Ready, service.poll(*ticket, &result));
  EXPECT_FALSE(service.rejectedDuplicatesIsEmpty());
  EXPECT_EQ(service.rejectedDuplicatesEmpty(), service.rejectedDuplicatesIsEmpty());
  EXPECT_EQ(1u, service.clearRejectedDuplicates());
  EXPECT_TRUE(service.rejectedDuplicatesIsEmpty());
}

TEST(WorldGenerationComputeTest, RejectedDuplicatesResetAliasesClearRejectedDuplicates) {
  WorldGenerationComputeService service;
  service.setDeferredCpuCompletion(true);
  auto ticket = service.submit(makeIdentity(2090), List<uint32_t>{4u}, 1u);
  ASSERT_TRUE((bool)ticket);
  EXPECT_EQ(1u, service.failContext());
  WorldGenerationComputeResult result;
  EXPECT_EQ(WorldGenerationComputePollStatus::Ready, service.poll(*ticket, &result));
  EXPECT_TRUE(service.anyRejectedDuplicates());
  EXPECT_EQ(1u, service.rejectedDuplicatesReset());
  EXPECT_FALSE(service.anyRejectedDuplicates());
  EXPECT_EQ(0u, service.rejectedDuplicatesReset());
}

TEST(WorldGenerationComputeTest, RejectedDuplicatesRemoveAliasesRemoveRejectedDuplicate) {
  WorldGenerationComputeService service;
  service.setDeferredCpuCompletion(true);
  auto ticket = service.submit(makeIdentity(2070), List<uint32_t>{4u}, 1u);
  ASSERT_TRUE((bool)ticket);
  EXPECT_EQ(1u, service.failContext());
  WorldGenerationComputeResult result;
  EXPECT_EQ(WorldGenerationComputePollStatus::Ready, service.poll(*ticket, &result));
  EXPECT_TRUE(service.hasRejectedDuplicate(*ticket));
  EXPECT_TRUE(service.rejectedDuplicatesRemove(*ticket));
  EXPECT_FALSE(service.hasRejectedDuplicate(*ticket));
  EXPECT_FALSE(service.rejectedDuplicatesRemove(*ticket));
}

TEST(WorldGenerationComputeTest, RejectedDuplicatesContainsAliasesHasRejectedDuplicate) {
  WorldGenerationComputeService service;
  service.setDeferredCpuCompletion(true);
  auto ticket = service.submit(makeIdentity(2050), List<uint32_t>{4u}, 1u);
  ASSERT_TRUE((bool)ticket);
  EXPECT_EQ(1u, service.failContext());
  WorldGenerationComputeResult result;
  EXPECT_EQ(WorldGenerationComputePollStatus::Ready, service.poll(*ticket, &result));
  EXPECT_TRUE(service.rejectedDuplicatesContains(*ticket));
  EXPECT_EQ(service.hasRejectedDuplicate(*ticket), service.rejectedDuplicatesContains(*ticket));
  EXPECT_EQ(1u, service.clearRejectedDuplicates());
  EXPECT_FALSE(service.rejectedDuplicatesContains(*ticket));
}

TEST(WorldGenerationComputeTest, RejectedDuplicatesSizeAliasesRejectedDuplicatesCount) {
  WorldGenerationComputeService service;
  service.setDeferredCpuCompletion(true);
  EXPECT_EQ(0u, service.rejectedDuplicatesSize());
  auto ticket = service.submit(makeIdentity(2030), List<uint32_t>{4u}, 1u);
  ASSERT_TRUE((bool)ticket);
  EXPECT_EQ(1u, service.failContext());
  WorldGenerationComputeResult result;
  EXPECT_EQ(WorldGenerationComputePollStatus::Ready, service.poll(*ticket, &result));
  EXPECT_EQ(1u, service.rejectedDuplicatesSize());
  EXPECT_EQ(service.rejectedDuplicatesCount(), service.rejectedDuplicatesSize());
  EXPECT_EQ(1u, service.clearRejectedDuplicates());
  EXPECT_EQ(0u, service.rejectedDuplicatesSize());
}

TEST(WorldGenerationComputeTest, RejectedDuplicatesClearAliasesClearRejectedDuplicates) {
  WorldGenerationComputeService service;
  service.setDeferredCpuCompletion(true);
  auto ticket = service.submit(makeIdentity(2010), List<uint32_t>{1u}, 1u);
  ASSERT_TRUE((bool)ticket);
  EXPECT_EQ(1u, service.failContext());
  WorldGenerationComputeResult result;
  EXPECT_EQ(WorldGenerationComputePollStatus::Ready, service.poll(*ticket, &result));
  EXPECT_TRUE(service.anyRejectedDuplicates());
  EXPECT_EQ(1u, service.rejectedDuplicatesClear());
  EXPECT_FALSE(service.anyRejectedDuplicates());
  EXPECT_EQ(0u, service.rejectedDuplicatesClear());
}

TEST(WorldGenerationComputeTest, RejectedDuplicatesAnyAliasesAnyRejectedDuplicates) {
  WorldGenerationComputeService service;
  service.setDeferredCpuCompletion(true);
  EXPECT_FALSE(service.rejectedDuplicatesAny());
  auto ticket = service.submit(makeIdentity(1990), List<uint32_t>{6u}, 1u);
  ASSERT_TRUE((bool)ticket);
  EXPECT_EQ(1u, service.failContext());
  WorldGenerationComputeResult result;
  EXPECT_EQ(WorldGenerationComputePollStatus::Ready, service.poll(*ticket, &result));
  EXPECT_TRUE(service.rejectedDuplicatesAny());
  EXPECT_EQ(service.anyRejectedDuplicates(), service.rejectedDuplicatesAny());
  EXPECT_EQ(service.hasRejectedDuplicates(), service.rejectedDuplicatesAny());
  EXPECT_EQ(1u, service.clearRejectedDuplicates());
  EXPECT_FALSE(service.rejectedDuplicatesAny());
}

TEST(WorldGenerationComputeTest, RejectedDuplicatesListAliasesListRejectedDuplicates) {
  WorldGenerationComputeService service;
  service.setDeferredCpuCompletion(true);
  EXPECT_EQ(0u, service.rejectedDuplicatesList().size());
  auto ticket = service.submit(makeIdentity(1970), List<uint32_t>{3u}, 1u);
  ASSERT_TRUE((bool)ticket);
  EXPECT_EQ(1u, service.failContext());
  WorldGenerationComputeResult result;
  EXPECT_EQ(WorldGenerationComputePollStatus::Ready, service.poll(*ticket, &result));
  auto listed = service.rejectedDuplicatesList();
  auto listed2 = service.listRejectedDuplicates();
  ASSERT_EQ(1u, listed.size());
  ASSERT_EQ(listed2.size(), listed.size());
  EXPECT_EQ(listed2[0].id, listed[0].id);
  EXPECT_EQ(ticket->id, listed[0].id);
  EXPECT_EQ(1u, service.clearRejectedDuplicates());
  EXPECT_EQ(0u, service.rejectedDuplicatesList().size());
}

TEST(WorldGenerationComputeTest, RejectedDuplicatesCountAliasesCountRejectedDuplicates) {
  WorldGenerationComputeService service;
  service.setDeferredCpuCompletion(true);
  EXPECT_EQ(0u, service.rejectedDuplicatesCount());
  auto ticket = service.submit(makeIdentity(1950), List<uint32_t>{2u}, 1u);
  ASSERT_TRUE((bool)ticket);
  EXPECT_EQ(1u, service.failContext());
  WorldGenerationComputeResult result;
  EXPECT_EQ(WorldGenerationComputePollStatus::Ready, service.poll(*ticket, &result));
  EXPECT_EQ(1u, service.rejectedDuplicatesCount());
  EXPECT_EQ(service.countRejectedDuplicates(), service.rejectedDuplicatesCount());
  EXPECT_EQ(service.rejectedDuplicateCount(), service.rejectedDuplicatesCount());
  EXPECT_EQ(1u, service.clearRejectedDuplicates());
  EXPECT_EQ(0u, service.rejectedDuplicatesCount());
}

TEST(WorldGenerationComputeTest, HasRejectedDuplicatesAliasesAnyRejectedDuplicates) {
  WorldGenerationComputeService service;
  service.setDeferredCpuCompletion(true);
  EXPECT_FALSE(service.hasRejectedDuplicates());
  auto ticket = service.submit(makeIdentity(1930), List<uint32_t>{8u}, 1u);
  ASSERT_TRUE((bool)ticket);
  EXPECT_EQ(1u, service.failContext());
  WorldGenerationComputeResult result;
  EXPECT_EQ(WorldGenerationComputePollStatus::Ready, service.poll(*ticket, &result));
  EXPECT_TRUE(service.hasRejectedDuplicates());
  EXPECT_EQ(service.anyRejectedDuplicates(), service.hasRejectedDuplicates());
  EXPECT_EQ(1u, service.clearRejectedDuplicates());
  EXPECT_FALSE(service.hasRejectedDuplicates());
}

TEST(WorldGenerationComputeTest, RejectedDuplicatesFingerprintAliasesFingerprintRejectedDuplicates) {
  WorldGenerationComputeService service;
  service.setDeferredCpuCompletion(true);
  EXPECT_EQ(0u, service.rejectedDuplicatesFingerprint());
  auto t1 = service.submit(makeIdentity(1910), List<uint32_t>{1u}, 1u);
  auto t2 = service.submit(makeIdentity(1911), List<uint32_t>{2u}, 1u);
  ASSERT_TRUE((bool)t1);
  ASSERT_TRUE((bool)t2);
  EXPECT_EQ(2u, service.failContext());
  WorldGenerationComputeResult result;
  EXPECT_EQ(WorldGenerationComputePollStatus::Ready, service.poll(*t1, &result));
  EXPECT_EQ(WorldGenerationComputePollStatus::Ready, service.poll(*t2, &result));
  EXPECT_EQ(service.fingerprintRejectedDuplicates(), service.rejectedDuplicatesFingerprint());
  EXPECT_EQ(t1->id ^ t2->id, service.rejectedDuplicatesFingerprint());
  EXPECT_EQ(2u, service.clearRejectedDuplicates());
  EXPECT_EQ(0u, service.rejectedDuplicatesFingerprint());
}

TEST(WorldGenerationComputeTest, RejectedDuplicatesEmptyTracksRejectSet) {
  WorldGenerationComputeService service;
  service.setDeferredCpuCompletion(true);
  EXPECT_TRUE(service.rejectedDuplicatesEmpty());
  auto ticket = service.submit(makeIdentity(1890), List<uint32_t>{5u}, 1u);
  ASSERT_TRUE((bool)ticket);
  EXPECT_EQ(1u, service.failContext());
  WorldGenerationComputeResult result;
  EXPECT_EQ(WorldGenerationComputePollStatus::Ready, service.poll(*ticket, &result));
  EXPECT_FALSE(service.rejectedDuplicatesEmpty());
  EXPECT_EQ(!service.anyRejectedDuplicates(), service.rejectedDuplicatesEmpty());
  EXPECT_EQ(1u, service.clearRejectedDuplicates());
  EXPECT_TRUE(service.rejectedDuplicatesEmpty());
}

TEST(WorldGenerationComputeTest, FingerprintRejectedDuplicatesXorsTicketIds) {
  WorldGenerationComputeService service;
  service.setDeferredCpuCompletion(true);
  EXPECT_EQ(0u, service.fingerprintRejectedDuplicates());
  auto t1 = service.submit(makeIdentity(1870), List<uint32_t>{1u}, 1u);
  auto t2 = service.submit(makeIdentity(1871), List<uint32_t>{2u}, 1u);
  ASSERT_TRUE((bool)t1);
  ASSERT_TRUE((bool)t2);
  EXPECT_EQ(2u, service.failContext());
  WorldGenerationComputeResult result;
  EXPECT_EQ(WorldGenerationComputePollStatus::Ready, service.poll(*t1, &result));
  EXPECT_EQ(WorldGenerationComputePollStatus::Ready, service.poll(*t2, &result));
  EXPECT_EQ(t1->id ^ t2->id, service.fingerprintRejectedDuplicates());
  EXPECT_EQ(2u, service.clearRejectedDuplicates());
  EXPECT_EQ(0u, service.fingerprintRejectedDuplicates());
}

TEST(WorldGenerationComputeTest, RemoveRejectedDuplicateClearsSingleTicket) {
  WorldGenerationComputeService service;
  service.setDeferredCpuCompletion(true);
  auto ticket = service.submit(makeIdentity(1850), List<uint32_t>{4u}, 1u);
  ASSERT_TRUE((bool)ticket);
  EXPECT_EQ(1u, service.failContext());
  WorldGenerationComputeResult result;
  EXPECT_EQ(WorldGenerationComputePollStatus::Ready, service.poll(*ticket, &result));
  EXPECT_TRUE(service.hasRejectedDuplicate(*ticket));
  EXPECT_TRUE(service.removeRejectedDuplicate(*ticket));
  EXPECT_FALSE(service.hasRejectedDuplicate(*ticket));
  EXPECT_FALSE(service.removeRejectedDuplicate(*ticket));
  EXPECT_EQ(0u, service.countRejectedDuplicates());
}

TEST(WorldGenerationComputeTest, HasRejectedDuplicateTracksTicketMembership) {
  WorldGenerationComputeService service;
  service.setDeferredCpuCompletion(true);
  auto ticket = service.submit(makeIdentity(1830), List<uint32_t>{3u}, 1u);
  ASSERT_TRUE((bool)ticket);
  EXPECT_FALSE(service.hasRejectedDuplicate(*ticket));
  EXPECT_EQ(1u, service.failContext());
  WorldGenerationComputeResult result;
  EXPECT_EQ(WorldGenerationComputePollStatus::Ready, service.poll(*ticket, &result));
  EXPECT_TRUE(service.hasRejectedDuplicate(*ticket));
  WorldGenerationComputeTicket missing;
  missing.id = ticket->id + 999u;
  EXPECT_FALSE(service.hasRejectedDuplicate(missing));
  EXPECT_EQ(1u, service.clearRejectedDuplicates());
  EXPECT_FALSE(service.hasRejectedDuplicate(*ticket));
}

TEST(WorldGenerationComputeTest, ListRejectedDuplicatesSnapshotsRejectIds) {
  WorldGenerationComputeService service;
  service.setDeferredCpuCompletion(true);
  EXPECT_EQ(0u, service.listRejectedDuplicates().size());
  auto ticket = service.submit(makeIdentity(1810), List<uint32_t>{9u}, 1u);
  ASSERT_TRUE((bool)ticket);
  EXPECT_EQ(1u, service.failContext());
  WorldGenerationComputeResult result;
  EXPECT_EQ(WorldGenerationComputePollStatus::Ready, service.poll(*ticket, &result));
  auto listed = service.listRejectedDuplicates();
  ASSERT_EQ(1u, listed.size());
  EXPECT_EQ(ticket->id, listed[0].id);
  EXPECT_EQ(1u, service.clearRejectedDuplicates());
  EXPECT_EQ(0u, service.listRejectedDuplicates().size());
}

TEST(WorldGenerationComputeTest, CountRejectedDuplicatesMatchesRejectedDuplicateCount) {
  WorldGenerationComputeService service;
  service.setDeferredCpuCompletion(true);
  EXPECT_EQ(0u, service.countRejectedDuplicates());
  auto ticket = service.submit(makeIdentity(1790), List<uint32_t>{7u}, 1u);
  ASSERT_TRUE((bool)ticket);
  EXPECT_EQ(1u, service.failContext());
  WorldGenerationComputeResult result;
  EXPECT_EQ(WorldGenerationComputePollStatus::Ready, service.poll(*ticket, &result));
  EXPECT_EQ(1u, service.countRejectedDuplicates());
  EXPECT_EQ(service.rejectedDuplicateCount(), service.countRejectedDuplicates());
  EXPECT_EQ(1u, service.clearRejectedDuplicates());
  EXPECT_EQ(0u, service.countRejectedDuplicates());
}

TEST(WorldGenerationComputeTest, AnyRejectedDuplicatesTracksRejectPresence) {
  WorldGenerationComputeService service;
  service.setDeferredCpuCompletion(true);
  EXPECT_FALSE(service.anyRejectedDuplicates());
  auto ticket = service.submit(makeIdentity(1770), List<uint32_t>{7u}, 1u);
  ASSERT_TRUE((bool)ticket);
  EXPECT_EQ(1u, service.failContext());
  WorldGenerationComputeResult result;
  EXPECT_EQ(WorldGenerationComputePollStatus::Ready, service.poll(*ticket, &result));
  EXPECT_TRUE(service.anyRejectedDuplicates());
  EXPECT_EQ(1u, service.clearRejectedDuplicates());
  EXPECT_FALSE(service.anyRejectedDuplicates());
}

TEST(WorldGenerationComputeTest, CountOpenTicketsTracksOpenTicketCount) {
  WorldGenerationComputeService service;
  service.setDeferredCpuCompletion(true);
  EXPECT_EQ(0u, service.countOpenTickets());
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(1750, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{1u}, 0u));
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(1751, WorldGenerationComputeKernel::DenseUint32Add), List<uint32_t>{2u}, 0u));
  EXPECT_EQ(2u, service.countOpenTickets());
  EXPECT_EQ(2u, service.openTickets());
  EXPECT_EQ(2u, service.discardPendingOpenTickets());
  EXPECT_EQ(0u, service.countOpenTickets());
}

TEST(WorldGenerationComputeTest, AnyOpenTicketsTracksOpenPresence) {
  WorldGenerationComputeService service;
  service.setDeferredCpuCompletion(true);
  EXPECT_FALSE(service.anyOpenTickets());
  auto a = service.submit(
      makeIdentity(1730, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{1u}, 0u);
  ASSERT_TRUE((bool)a);
  EXPECT_TRUE(service.anyOpenTickets());
  EXPECT_EQ(1u, service.discardPendingOpenTickets());
  EXPECT_FALSE(service.anyOpenTickets());
}

TEST(WorldGenerationComputeTest, AnyTerminalOpenTicketsByKernelTracksNonPendingPresence) {
  WorldGenerationComputeService service;
  service.setDeferredCpuCompletion(true);
  EXPECT_FALSE(
      service.anyTerminalOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  auto a = service.submit(
      makeIdentity(1710, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{1u}, 0u);
  auto b = service.submit(
      makeIdentity(1711, WorldGenerationComputeKernel::DenseUint32Add), List<uint32_t>{2u}, 0u);
  ASSERT_TRUE((bool)a);
  ASSERT_TRUE((bool)b);
  EXPECT_FALSE(
      service.anyTerminalOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_TRUE(service.complete(*a));
  EXPECT_TRUE(service.anyTerminalOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_FALSE(
      service.anyTerminalOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Add));
  EXPECT_EQ(1u, service.discardTerminalOpenTickets());
  EXPECT_FALSE(
      service.anyTerminalOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
}

TEST(WorldGenerationComputeTest, CountTerminalOpenTicketsByKernelTracksNonPendingMatches) {
  WorldGenerationComputeService service;
  service.setDeferredCpuCompletion(true);
  auto xorA = service.submit(
      makeIdentity(1690, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{1u}, 0u);
  auto xorB = service.submit(
      makeIdentity(1691, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{2u}, 0u);
  auto add = service.submit(
      makeIdentity(1692, WorldGenerationComputeKernel::DenseUint32Add), List<uint32_t>{3u}, 0u);
  ASSERT_TRUE((bool)xorA);
  ASSERT_TRUE((bool)xorB);
  ASSERT_TRUE((bool)add);
  EXPECT_EQ(0u,
      service.countTerminalOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_TRUE(service.complete(*xorA));
  EXPECT_TRUE(service.cancel(*xorB));
  EXPECT_EQ(2u,
      service.countTerminalOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_EQ(0u,
      service.countTerminalOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Add));
  EXPECT_EQ(1u, service.inFlightByKernel(WorldGenerationComputeKernel::DenseUint32Add));
}

TEST(WorldGenerationComputeTest, ListTerminalOpenTicketsByKernelReturnsNonPendingMatches) {
  WorldGenerationComputeService service;
  service.setDeferredCpuCompletion(true);
  auto xorA = service.submit(
      makeIdentity(1670, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{1u}, 0u);
  auto xorB = service.submit(
      makeIdentity(1671, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{2u}, 0u);
  auto add = service.submit(
      makeIdentity(1672, WorldGenerationComputeKernel::DenseUint32Add), List<uint32_t>{3u}, 0u);
  ASSERT_TRUE((bool)xorA);
  ASSERT_TRUE((bool)xorB);
  ASSERT_TRUE((bool)add);
  EXPECT_TRUE(service.complete(*xorA));
  EXPECT_TRUE(service.cancel(*xorB));
  auto terminalXor =
      service.listTerminalOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor);
  ASSERT_EQ(2u, terminalXor.size());
  EXPECT_TRUE(terminalXor[0] == *xorA || terminalXor[0] == *xorB);
  EXPECT_TRUE(terminalXor[1] == *xorA || terminalXor[1] == *xorB);
  EXPECT_NE(terminalXor[0].id, terminalXor[1].id);
  EXPECT_TRUE(
      service.listTerminalOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Add)
          .empty());
  EXPECT_EQ(1u, service.inFlightByKernel(WorldGenerationComputeKernel::DenseUint32Add));
}

TEST(WorldGenerationComputeTest, AnyTerminalOpenKernelsTracksTerminalKernelPresence) {
  WorldGenerationComputeService service;
  service.setDeferredCpuCompletion(true);
  EXPECT_FALSE(service.anyTerminalOpenKernels());
  auto a = service.submit(
      makeIdentity(1650, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{1u}, 0u);
  auto b = service.submit(
      makeIdentity(1651, WorldGenerationComputeKernel::DenseUint32Add), List<uint32_t>{2u}, 0u);
  ASSERT_TRUE((bool)a);
  ASSERT_TRUE((bool)b);
  EXPECT_FALSE(service.anyTerminalOpenKernels());
  EXPECT_TRUE(service.complete(*a));
  EXPECT_TRUE(service.anyTerminalOpenKernels());
  EXPECT_EQ(1u, service.discardTerminalOpenTickets());
  EXPECT_FALSE(service.anyTerminalOpenKernels());
}

TEST(WorldGenerationComputeTest, CountTerminalOpenKernelsTracksDistinctTerminalKernels) {
  WorldGenerationComputeService service;
  service.setDeferredCpuCompletion(true);
  EXPECT_EQ(0u, service.countTerminalOpenKernels());
  auto a = service.submit(
      makeIdentity(1630, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{1u}, 0u);
  auto b = service.submit(
      makeIdentity(1631, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{2u}, 0u);
  auto c = service.submit(
      makeIdentity(1632, WorldGenerationComputeKernel::DenseUint32Add), List<uint32_t>{3u}, 0u);
  ASSERT_TRUE((bool)a);
  ASSERT_TRUE((bool)b);
  ASSERT_TRUE((bool)c);
  EXPECT_TRUE(service.complete(*a));
  EXPECT_TRUE(service.cancel(*b));
  EXPECT_EQ(1u, service.countTerminalOpenKernels());
  EXPECT_TRUE(service.complete(*c));
  EXPECT_EQ(2u, service.countTerminalOpenKernels());
  EXPECT_EQ(3u, service.discardTerminalOpenTickets());
  EXPECT_EQ(0u, service.countTerminalOpenKernels());
}

TEST(WorldGenerationComputeTest, ListTerminalOpenKernelsReturnsDistinctTerminalKernels) {
  WorldGenerationComputeService service;
  service.setDeferredCpuCompletion(true);
  EXPECT_TRUE(service.listTerminalOpenKernels().empty());
  auto a = service.submit(
      makeIdentity(1610, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{1u}, 0u);
  auto b = service.submit(
      makeIdentity(1611, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{2u}, 0u);
  auto c = service.submit(
      makeIdentity(1612, WorldGenerationComputeKernel::DenseUint32Add), List<uint32_t>{3u}, 0u);
  ASSERT_TRUE((bool)a);
  ASSERT_TRUE((bool)b);
  ASSERT_TRUE((bool)c);
  EXPECT_TRUE(service.complete(*a));
  EXPECT_TRUE(service.cancel(*b));
  auto kernels = service.listTerminalOpenKernels();
  ASSERT_EQ(1u, kernels.size());
  EXPECT_EQ(WorldGenerationComputeKernel::DenseUint32Xor, kernels[0]);
  EXPECT_EQ(1u, service.inFlightByKernel(WorldGenerationComputeKernel::DenseUint32Add));
  EXPECT_EQ(2u, service.discardTerminalOpenTickets());
  EXPECT_TRUE(service.listTerminalOpenKernels().empty());
}

TEST(WorldGenerationComputeTest, AnyTerminalOpenTicketsTracksNonPendingPresence) {
  WorldGenerationComputeService service;
  service.setDeferredCpuCompletion(true);
  EXPECT_FALSE(service.anyTerminalOpenTickets());
  auto ready = service.submit(
      makeIdentity(1590, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{1u}, 0u);
  auto pending = service.submit(
      makeIdentity(1591, WorldGenerationComputeKernel::DenseUint32Add), List<uint32_t>{2u}, 0u);
  ASSERT_TRUE((bool)ready);
  ASSERT_TRUE((bool)pending);
  EXPECT_FALSE(service.anyTerminalOpenTickets());
  EXPECT_TRUE(service.complete(*ready));
  EXPECT_TRUE(service.anyTerminalOpenTickets());
  EXPECT_EQ(1u, service.discardTerminalOpenTickets());
  EXPECT_FALSE(service.anyTerminalOpenTickets());
}

TEST(WorldGenerationComputeTest, CountTerminalOpenTicketsTracksNonPendingCount) {
  WorldGenerationComputeService service;
  service.setDeferredCpuCompletion(true);
  EXPECT_EQ(0u, service.countTerminalOpenTickets());
  auto ready = service.submit(
      makeIdentity(1570, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{1u}, 0u);
  auto cancel = service.submit(
      makeIdentity(1571, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{2u}, 0u);
  auto pending = service.submit(
      makeIdentity(1572, WorldGenerationComputeKernel::DenseUint32Add), List<uint32_t>{3u}, 0u);
  ASSERT_TRUE((bool)ready);
  ASSERT_TRUE((bool)cancel);
  ASSERT_TRUE((bool)pending);
  EXPECT_TRUE(service.complete(*ready));
  EXPECT_TRUE(service.cancel(*cancel));
  EXPECT_EQ(2u, service.countTerminalOpenTickets());
  EXPECT_EQ(1u, service.inFlightByKernel(WorldGenerationComputeKernel::DenseUint32Add));
  EXPECT_EQ(2u, service.discardTerminalOpenTickets());
  EXPECT_EQ(0u, service.countTerminalOpenTickets());
}

TEST(WorldGenerationComputeTest, ListTerminalOpenTicketsReturnsNonPendingIds) {
  WorldGenerationComputeService service;
  service.setDeferredCpuCompletion(true);
  EXPECT_TRUE(service.listTerminalOpenTickets().empty());
  auto ready = service.submit(
      makeIdentity(1550, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{1u}, 0u);
  auto cancel = service.submit(
      makeIdentity(1551, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{2u}, 0u);
  auto pending = service.submit(
      makeIdentity(1552, WorldGenerationComputeKernel::DenseUint32Add), List<uint32_t>{3u}, 0u);
  ASSERT_TRUE((bool)ready);
  ASSERT_TRUE((bool)cancel);
  ASSERT_TRUE((bool)pending);
  EXPECT_TRUE(service.complete(*ready));
  EXPECT_TRUE(service.cancel(*cancel));
  auto terminal = service.listTerminalOpenTickets();
  ASSERT_EQ(2u, terminal.size());
  EXPECT_TRUE(terminal[0] == *ready || terminal[0] == *cancel);
  EXPECT_TRUE(terminal[1] == *ready || terminal[1] == *cancel);
  EXPECT_NE(terminal[0].id, terminal[1].id);
  EXPECT_EQ(1u, service.inFlightByKernel(WorldGenerationComputeKernel::DenseUint32Add));
  EXPECT_EQ(2u, service.discardTerminalOpenTickets());
  EXPECT_TRUE(service.listTerminalOpenTickets().empty());
}

TEST(WorldGenerationComputeTest, AnyCancelledOpenTicketsTracksCancelledPresence) {
  WorldGenerationComputeService service;
  service.setDeferredCpuCompletion(true);
  EXPECT_FALSE(service.anyCancelledOpenTickets());
  auto a = service.submit(
      makeIdentity(1530, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{1u}, 0u);
  ASSERT_TRUE((bool)a);
  EXPECT_EQ(1u, service.cancelOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_TRUE(service.anyCancelledOpenTickets());
  EXPECT_EQ(1u, service.discardCancelledOpenTickets());
  EXPECT_FALSE(service.anyCancelledOpenTickets());
}

TEST(WorldGenerationComputeTest, CountCancelledOpenTicketsTracksCancelledTicketCount) {
  WorldGenerationComputeService service;
  service.setDeferredCpuCompletion(true);
  EXPECT_EQ(0u, service.countCancelledOpenTickets());
  auto a = service.submit(
      makeIdentity(1520, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{1u}, 0u);
  auto b = service.submit(
      makeIdentity(1521, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{2u}, 0u);
  auto c = service.submit(
      makeIdentity(1522, WorldGenerationComputeKernel::DenseUint32Add), List<uint32_t>{3u}, 0u);
  ASSERT_TRUE((bool)a);
  ASSERT_TRUE((bool)b);
  ASSERT_TRUE((bool)c);
  EXPECT_EQ(2u, service.cancelOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_EQ(2u, service.countCancelledOpenTickets());
  EXPECT_EQ(1u, service.inFlightByKernel(WorldGenerationComputeKernel::DenseUint32Add));
  EXPECT_EQ(2u, service.discardCancelledOpenTickets());
  EXPECT_EQ(0u, service.countCancelledOpenTickets());
}

TEST(WorldGenerationComputeTest, ListCancelledOpenTicketsReturnsCancelledIdsOnly) {
  WorldGenerationComputeService service;
  service.setDeferredCpuCompletion(true);
  EXPECT_TRUE(service.listCancelledOpenTickets().empty());
  auto a = service.submit(
      makeIdentity(1500, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{1u}, 0u);
  auto b = service.submit(
      makeIdentity(1501, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{2u}, 0u);
  auto c = service.submit(
      makeIdentity(1502, WorldGenerationComputeKernel::DenseUint32Add), List<uint32_t>{3u}, 0u);
  ASSERT_TRUE((bool)a);
  ASSERT_TRUE((bool)b);
  ASSERT_TRUE((bool)c);
  EXPECT_EQ(2u, service.cancelOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  auto cancelled = service.listCancelledOpenTickets();
  ASSERT_EQ(2u, cancelled.size());
  EXPECT_TRUE(cancelled[0] == *a || cancelled[0] == *b);
  EXPECT_TRUE(cancelled[1] == *a || cancelled[1] == *b);
  EXPECT_NE(cancelled[0].id, cancelled[1].id);
  EXPECT_EQ(1u, service.inFlightByKernel(WorldGenerationComputeKernel::DenseUint32Add));
  EXPECT_EQ(2u, service.discardCancelledOpenTickets());
  EXPECT_TRUE(service.listCancelledOpenTickets().empty());
}

TEST(WorldGenerationComputeTest, DenseUint32HalfSumMaxMinExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  // 0x12345678 → low 0x5678 + high 0x1234 = 0x68ac
  EXPECT_EQ(0x68acu,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfSum, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);
  EXPECT_EQ(0x5678u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfMax, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);
  EXPECT_EQ(0x1234u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfMin, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(830, WorldGenerationComputeKernel::DenseUint32HalfSum), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(831, WorldGenerationComputeKernel::DenseUint32HalfMax), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(832, WorldGenerationComputeKernel::DenseUint32HalfMin), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
}

TEST(WorldGenerationComputeTest, DenseUint32HalfXorOrAndExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  // 0x12345678 → 0x5678 ^ 0x1234 = 0x444c
  EXPECT_EQ(0x444cu,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfXor, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);
  EXPECT_EQ(0x567cu,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfOr, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);
  EXPECT_EQ(0x1230u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfAnd, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(850, WorldGenerationComputeKernel::DenseUint32HalfXor), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(851, WorldGenerationComputeKernel::DenseUint32HalfOr), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(852, WorldGenerationComputeKernel::DenseUint32HalfAnd), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
}

TEST(WorldGenerationComputeTest, DenseUint32BroadcastHalfEqRangeExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  EXPECT_EQ(0x56785678u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32BroadcastHalf, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);
  EXPECT_EQ(0xffffffffu,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfEq, 1u, List<uint32_t>{0x12341234u},
          0u)[0]);
  EXPECT_EQ(0u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfEq, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);
  EXPECT_EQ(0x4444u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfRange, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(870, WorldGenerationComputeKernel::DenseUint32BroadcastHalf), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(871, WorldGenerationComputeKernel::DenseUint32HalfEq), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(872, WorldGenerationComputeKernel::DenseUint32HalfRange), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
}

TEST(WorldGenerationComputeTest, DenseUint32HalfLtGtNeExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  // 0x12345678 → lo 0x5678 > hi 0x1234
  EXPECT_EQ(0u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfLt, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);
  EXPECT_EQ(0xffffffffu,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfLt, 1u, List<uint32_t>{0x00020001u},
          0u)[0]);
  EXPECT_EQ(0xffffffffu,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfGt, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);
  EXPECT_EQ(0u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfGt, 1u, List<uint32_t>{0x00020001u},
          0u)[0]);
  EXPECT_EQ(0xffffffffu,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfNe, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);
  EXPECT_EQ(0u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfNe, 1u, List<uint32_t>{0x12341234u},
          0u)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(890, WorldGenerationComputeKernel::DenseUint32HalfLt), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(891, WorldGenerationComputeKernel::DenseUint32HalfGt), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(892, WorldGenerationComputeKernel::DenseUint32HalfNe), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
}

TEST(WorldGenerationComputeTest, DenseUint32HalfLeuGeuAvgExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  // 0x12345678 → lo 0x5678 > hi 0x1234; avg (0x5678+0x1234)>>1 = 0x3456
  EXPECT_EQ(0u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfLeu, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);
  EXPECT_EQ(0xffffffffu,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfLeu, 1u, List<uint32_t>{0x00020001u},
          0u)[0]);
  EXPECT_EQ(0xffffffffu,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfLeu, 1u, List<uint32_t>{0x12341234u},
          0u)[0]);
  EXPECT_EQ(0xffffffffu,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfGeu, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);
  EXPECT_EQ(0u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfGeu, 1u, List<uint32_t>{0x00020001u},
          0u)[0]);
  EXPECT_EQ(0x3456u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfAvg, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(910, WorldGenerationComputeKernel::DenseUint32HalfLeu), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(911, WorldGenerationComputeKernel::DenseUint32HalfGeu), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(912, WorldGenerationComputeKernel::DenseUint32HalfAvg), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
}

TEST(WorldGenerationComputeTest, DenseUint32BroadcastHighHalfSubMulExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  EXPECT_EQ(0x12341234u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32BroadcastHighHalf, 1u,
          List<uint32_t>{0x12345678u}, 0u)[0]);
  EXPECT_EQ(0x4444u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfSub, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);
  EXPECT_EQ(0x6260060u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfMul, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(930, WorldGenerationComputeKernel::DenseUint32BroadcastHighHalf), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(931, WorldGenerationComputeKernel::DenseUint32HalfSub), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(932, WorldGenerationComputeKernel::DenseUint32HalfMul), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
}

TEST(WorldGenerationComputeTest, DenseUint32HalfSatAddSatSubMulHiExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  EXPECT_EQ(0x68acu,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfSatAdd, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);
  EXPECT_EQ(0xffffu,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfSatAdd, 1u, List<uint32_t>{0xffff0001u},
          0u)[0]);
  EXPECT_EQ(0x4444u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfSatSub, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);
  EXPECT_EQ(0u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfSatSub, 1u, List<uint32_t>{0x00020001u},
          0u)[0]);
  EXPECT_EQ(0x626u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfMulHi, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(950, WorldGenerationComputeKernel::DenseUint32HalfSatAdd), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(951, WorldGenerationComputeKernel::DenseUint32HalfSatSub), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(952, WorldGenerationComputeKernel::DenseUint32HalfMulHi), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
}

TEST(WorldGenerationComputeTest, DenseUint32HalfDivModCeilDivExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  // 0x12345678 → lo 0x5678 / hi 0x1234 = 4 rem 0xda8; ceil = 5
  EXPECT_EQ(4u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfDiv, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);
  EXPECT_EQ(0u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfDiv, 1u, List<uint32_t>{0x00005678u},
          0u)[0]);
  EXPECT_EQ(0xda8u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfMod, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);
  EXPECT_EQ(0u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfMod, 1u, List<uint32_t>{0x00005678u},
          0u)[0]);
  EXPECT_EQ(5u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfCeilDiv, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);
  EXPECT_EQ(0u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfCeilDiv, 1u, List<uint32_t>{0x00005678u},
          0u)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(970, WorldGenerationComputeKernel::DenseUint32HalfDiv), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(971, WorldGenerationComputeKernel::DenseUint32HalfMod), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(972, WorldGenerationComputeKernel::DenseUint32HalfCeilDiv), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
}

TEST(WorldGenerationComputeTest, DenseUint32HalfIsDivisibleGcdSelectExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  EXPECT_EQ(0u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfIsDivisible, 1u,
          List<uint32_t>{0x12345678u}, 0u)[0]);
  EXPECT_EQ(0xffffffffu,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfIsDivisible, 1u,
          List<uint32_t>{0x00040008u}, 0u)[0]);
  EXPECT_EQ(4u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfGcd, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);
  EXPECT_EQ(0x5678u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfSelect, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);
  EXPECT_EQ(0x1234u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfSelect, 1u, List<uint32_t>{0x12345678u},
          1u)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(990, WorldGenerationComputeKernel::DenseUint32HalfIsDivisible), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(991, WorldGenerationComputeKernel::DenseUint32HalfGcd), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(992, WorldGenerationComputeKernel::DenseUint32HalfSelect), input, 1u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
}

TEST(WorldGenerationComputeTest, DenseUint32HalfLcmCoprimeAlignUpExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  EXPECT_EQ(0x1898018u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfLcm, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);
  EXPECT_EQ(0u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfLcm, 1u, List<uint32_t>{0x00005678u},
          0u)[0]);
  EXPECT_EQ(0u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfCoprime, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);
  EXPECT_EQ(0xffffffffu,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfCoprime, 1u, List<uint32_t>{0x00030004u},
          0u)[0]);
  EXPECT_EQ(0x5b04u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfAlignUp, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);
  EXPECT_EQ(0x5678u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfAlignUp, 1u, List<uint32_t>{0x00005678u},
          0u)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1010, WorldGenerationComputeKernel::DenseUint32HalfLcm), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1011, WorldGenerationComputeKernel::DenseUint32HalfCoprime), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1012, WorldGenerationComputeKernel::DenseUint32HalfAlignUp), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
}

TEST(WorldGenerationComputeTest, DenseUint32HalfAlignDownRotlRotrExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  // hi&15 = 4 for 0x12345678
  EXPECT_EQ(0x48d0u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfAlignDown, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);
  EXPECT_EQ(0x5678u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfAlignDown, 1u, List<uint32_t>{0x00005678u},
          0u)[0]);
  EXPECT_EQ(0x6785u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfRotl, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);
  EXPECT_EQ(0x8567u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfRotr, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1030, WorldGenerationComputeKernel::DenseUint32HalfAlignDown), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1031, WorldGenerationComputeKernel::DenseUint32HalfRotl), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1032, WorldGenerationComputeKernel::DenseUint32HalfRotr), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
}

TEST(WorldGenerationComputeTest, DenseUint32HalfPopcountClzCtzExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  // 0x5678 = 0101 0110 0111 1000 → pop=8, clz=1, ctz=3
  EXPECT_EQ(8u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfPopcount, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);
  EXPECT_EQ(1u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfClz, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);
  EXPECT_EQ(16u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfClz, 1u, List<uint32_t>{0x12340000u},
          0u)[0]);
  EXPECT_EQ(3u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfCtz, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);
  EXPECT_EQ(16u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfCtz, 1u, List<uint32_t>{0x12340000u},
          0u)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1050, WorldGenerationComputeKernel::DenseUint32HalfPopcount), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1051, WorldGenerationComputeKernel::DenseUint32HalfClz), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1052, WorldGenerationComputeKernel::DenseUint32HalfCtz), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
}

TEST(WorldGenerationComputeTest, DenseUint32HalfParityBitReverseBswapExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  EXPECT_EQ(0u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfParity, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);
  EXPECT_EQ(1u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfParity, 1u, List<uint32_t>{0x12345679u},
          0u)[0]);
  EXPECT_EQ(0x1e6au,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfBitReverse, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);
  EXPECT_EQ(0x7856u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfBswap, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1070, WorldGenerationComputeKernel::DenseUint32HalfParity), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1071, WorldGenerationComputeKernel::DenseUint32HalfBitReverse), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1072, WorldGenerationComputeKernel::DenseUint32HalfBswap), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
}

TEST(WorldGenerationComputeTest, DenseUint32HalfNotNegAbsExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  // 0x5678 → not=0xa987, neg=0xa988, abs=0x5678
  EXPECT_EQ(0xa987u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfNot, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);
  EXPECT_EQ(0xa988u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfNeg, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);
  EXPECT_EQ(0x5678u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfAbs, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);
  EXPECT_EQ(0x8000u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfAbs, 1u, List<uint32_t>{0x12348000u},
          0u)[0]);
  EXPECT_EQ(1u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfAbs, 1u, List<uint32_t>{0x1234ffffu},
          0u)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1090, WorldGenerationComputeKernel::DenseUint32HalfNot), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1091, WorldGenerationComputeKernel::DenseUint32HalfNeg), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1092, WorldGenerationComputeKernel::DenseUint32HalfAbs), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
}


TEST(WorldGenerationComputeTest, DenseUint32HalfSignMaskIsZeroIsNonzeroExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  EXPECT_EQ(0u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfSignMask, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);
  EXPECT_EQ(0xffffffffu,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfSignMask, 1u, List<uint32_t>{0x12348000u},
          0u)[0]);
  EXPECT_EQ(0u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfIsZero, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);
  EXPECT_EQ(0xffffffffu,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfIsZero, 1u, List<uint32_t>{0x12340000u},
          0u)[0]);
  EXPECT_EQ(0xffffffffu,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfIsNonzero, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);
  EXPECT_EQ(0u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfIsNonzero, 1u, List<uint32_t>{0x12340000u},
          0u)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1110, WorldGenerationComputeKernel::DenseUint32HalfSignMask), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1111, WorldGenerationComputeKernel::DenseUint32HalfIsZero), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1112, WorldGenerationComputeKernel::DenseUint32HalfIsNonzero), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
}


TEST(WorldGenerationComputeTest, DenseUint32HalfCloCtoIsPow2ExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  // 0x5678 leading/trailing ones = 0; not pow2
  EXPECT_EQ(0u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfClo, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);
  EXPECT_EQ(0u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfCto, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);
  EXPECT_EQ(0u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfIsPow2, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);
  EXPECT_EQ(16u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfClo, 1u, List<uint32_t>{0x1234ffffu},
          0u)[0]);
  EXPECT_EQ(16u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfCto, 1u, List<uint32_t>{0x1234ffffu},
          0u)[0]);
  EXPECT_EQ(1u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfClo, 1u, List<uint32_t>{0x12348000u},
          0u)[0]);
  EXPECT_EQ(0xffffffffu,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfIsPow2, 1u, List<uint32_t>{0x12348000u},
          0u)[0]);
  EXPECT_EQ(1u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfCto, 1u, List<uint32_t>{0x12340001u},
          0u)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1130, WorldGenerationComputeKernel::DenseUint32HalfClo), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1131, WorldGenerationComputeKernel::DenseUint32HalfCto), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1132, WorldGenerationComputeKernel::DenseUint32HalfIsPow2), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
}


TEST(WorldGenerationComputeTest, DenseUint32HalfIlog2NextPow2MaskLowExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  EXPECT_EQ(14u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfIlog2, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);
  EXPECT_EQ(0u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfIlog2, 1u, List<uint32_t>{0x12340000u},
          0u)[0]);
  EXPECT_EQ(0x8000u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfNextPow2, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);
  EXPECT_EQ(0u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfNextPow2, 1u, List<uint32_t>{0x12348001u},
          0u)[0]);
  EXPECT_EQ(0x78u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfMaskLow, 1u, List<uint32_t>{0x12345678u},
          8u)[0]);
  EXPECT_EQ(0x5678u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfMaskLow, 1u, List<uint32_t>{0x12345678u},
          16u)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1150, WorldGenerationComputeKernel::DenseUint32HalfIlog2), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1151, WorldGenerationComputeKernel::DenseUint32HalfNextPow2), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1152, WorldGenerationComputeKernel::DenseUint32HalfMaskLow), input, 8u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
}


TEST(WorldGenerationComputeTest, DenseUint32HalfMaskHighBitTestIsolateLsbExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  EXPECT_EQ(0x5600u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfMaskHigh, 1u, List<uint32_t>{0x12345678u},
          8u)[0]);
  EXPECT_EQ(0x5678u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfMaskHigh, 1u, List<uint32_t>{0x12345678u},
          16u)[0]);
  EXPECT_EQ(0xffffffffu,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfBitTest, 1u, List<uint32_t>{0x12345678u},
          3u)[0]);
  EXPECT_EQ(0u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfBitTest, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);
  EXPECT_EQ(0x8u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfIsolateLsb, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1170, WorldGenerationComputeKernel::DenseUint32HalfMaskHigh), input, 8u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1171, WorldGenerationComputeKernel::DenseUint32HalfBitTest), input, 3u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1172, WorldGenerationComputeKernel::DenseUint32HalfIsolateLsb), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
}


TEST(WorldGenerationComputeTest, DenseUint32HalfIsolateMsbClearLsbLsbMaskExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  EXPECT_EQ(0x4000u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfIsolateMsb, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);
  EXPECT_EQ(0u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfIsolateMsb, 1u, List<uint32_t>{0x12340000u},
          0u)[0]);
  EXPECT_EQ(0x5670u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfClearLsb, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);
  EXPECT_EQ(0xfu,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfLsbMask, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);
  EXPECT_EQ(0xffffu,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfLsbMask, 1u, List<uint32_t>{0x12340000u},
          0u)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1190, WorldGenerationComputeKernel::DenseUint32HalfIsolateMsb), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1191, WorldGenerationComputeKernel::DenseUint32HalfClearLsb), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1192, WorldGenerationComputeKernel::DenseUint32HalfLsbMask), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
}


TEST(WorldGenerationComputeTest, DenseUint32HalfGrayEncodeDecodeSmearLsbExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  EXPECT_EQ(0x7d44u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfGrayEncode, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);
  EXPECT_EQ(0x5678u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfGrayDecode, 1u,
          WorldGenerationComputeService::runKernel(
              WorldGenerationComputeKernel::DenseUint32HalfGrayEncode, 1u,
              List<uint32_t>{0x12345678u}, 0u),
          0u)[0]);
  EXPECT_EQ(0x567fu,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfSmearLsb, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);
  EXPECT_EQ(0xffffu,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfSmearLsb, 1u, List<uint32_t>{0x12340000u},
          0u)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1210, WorldGenerationComputeKernel::DenseUint32HalfGrayEncode), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1211, WorldGenerationComputeKernel::DenseUint32HalfGrayDecode), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1212, WorldGenerationComputeKernel::DenseUint32HalfSmearLsb), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
}


TEST(WorldGenerationComputeTest, DenseUint32HalfClearMsbSmearMsbKeepOddBitsExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  EXPECT_EQ(0x1678u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfClearMsb, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);
  EXPECT_EQ(0u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfClearMsb, 1u, List<uint32_t>{0x12340000u},
          0u)[0]);
  EXPECT_EQ(0x7fffu,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfSmearMsb, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);
  EXPECT_EQ(0x5450u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfKeepOddBits, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1230, WorldGenerationComputeKernel::DenseUint32HalfClearMsb), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1231, WorldGenerationComputeKernel::DenseUint32HalfSmearMsb), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1232, WorldGenerationComputeKernel::DenseUint32HalfKeepOddBits), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
}


TEST(WorldGenerationComputeTest, DenseUint32HalfKeepEvenSwapCompressOddExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  EXPECT_EQ(0x228u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfKeepEvenBits, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);
  EXPECT_EQ(0xa9b4u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfSwapAdjacentBits, 1u,
          List<uint32_t>{0x12345678u}, 0u)[0]);
  EXPECT_EQ(0xecu,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfCompressOddBits, 1u,
          List<uint32_t>{0x12345678u}, 0u)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1250, WorldGenerationComputeKernel::DenseUint32HalfKeepEvenBits), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1251, WorldGenerationComputeKernel::DenseUint32HalfSwapAdjacentBits), input,
          0u, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1252, WorldGenerationComputeKernel::DenseUint32HalfCompressOddBits), input,
          0u, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
}


TEST(WorldGenerationComputeTest, DenseUint32HalfCompressEvenExpandLow8ExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  EXPECT_EQ(0x16u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfCompressEvenBits, 1u,
          List<uint32_t>{0x12345678u}, 0u)[0]);
  EXPECT_EQ(0x1540u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfExpandLow8ToOdd, 1u,
          List<uint32_t>{0x12345678u}, 0u)[0]);
  EXPECT_EQ(0x2a80u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfExpandLow8ToEven, 1u,
          List<uint32_t>{0x12345678u}, 0u)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1270, WorldGenerationComputeKernel::DenseUint32HalfCompressEvenBits), input,
          0u, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1271, WorldGenerationComputeKernel::DenseUint32HalfExpandLow8ToOdd), input,
          0u, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1272, WorldGenerationComputeKernel::DenseUint32HalfExpandLow8ToEven), input,
          0u, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
}


TEST(WorldGenerationComputeTest, DenseUint32HalfInterleaveDeinterleaveSwapBytesExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  EXPECT_EQ(0x3768u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfInterleaveBytes, 1u,
          List<uint32_t>{0x12345678u}, 0u)[0]);
  EXPECT_EQ(0x16ecu,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfDeinterleaveBytes, 1u,
          List<uint32_t>{0x12345678u}, 0u)[0]);
  EXPECT_EQ(0x7856u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfSwapBytes, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1290, WorldGenerationComputeKernel::DenseUint32HalfInterleaveBytes), input,
          0u, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1291, WorldGenerationComputeKernel::DenseUint32HalfDeinterleaveBytes), input,
          0u, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1292, WorldGenerationComputeKernel::DenseUint32HalfSwapBytes), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
}


TEST(WorldGenerationComputeTest, DenseUint32HalfNibbleXorOrAndExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  EXPECT_EQ(0xcu,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfNibbleXor, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);
  EXPECT_EQ(0xfu,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfNibbleOr, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);
  EXPECT_EQ(0x0u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfNibbleAnd, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1310, WorldGenerationComputeKernel::DenseUint32HalfNibbleXor), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1311, WorldGenerationComputeKernel::DenseUint32HalfNibbleOr), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1312, WorldGenerationComputeKernel::DenseUint32HalfNibbleAnd), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
}


TEST(WorldGenerationComputeTest, DenseUint32HalfNibbleSumMaxMinExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  EXPECT_EQ(0x1au,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfNibbleSum, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);
  EXPECT_EQ(0x8u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfNibbleMax, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);
  EXPECT_EQ(0x5u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfNibbleMin, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1330, WorldGenerationComputeKernel::DenseUint32HalfNibbleSum), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1331, WorldGenerationComputeKernel::DenseUint32HalfNibbleMax), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1332, WorldGenerationComputeKernel::DenseUint32HalfNibbleMin), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
}


TEST(WorldGenerationComputeTest, DenseUint32HalfSwapRolRorNibblesExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  EXPECT_EQ(0x6587u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfSwapAdjacentNibbles, 1u,
          List<uint32_t>{0x12345678u}, 0u)[0]);
  EXPECT_EQ(0x6785u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfRolNibbles, 1u, List<uint32_t>{0x12345678u},
          1u)[0]);
  EXPECT_EQ(0x8567u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfRorNibbles, 1u, List<uint32_t>{0x12345678u},
          1u)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1350, WorldGenerationComputeKernel::DenseUint32HalfSwapAdjacentNibbles),
          input, 0u, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1351, WorldGenerationComputeKernel::DenseUint32HalfRolNibbles), input, 1u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1352, WorldGenerationComputeKernel::DenseUint32HalfRorNibbles), input, 1u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
}


TEST(WorldGenerationComputeTest, DenseUint32HalfHasZeroCountBroadcastNibbleExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  EXPECT_EQ(0u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfHasZeroNibble, 1u,
          List<uint32_t>{0x12345678u}, 0u)[0]);
  EXPECT_EQ(0xffffffffu,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfHasZeroNibble, 1u,
          List<uint32_t>{0x12345670u}, 0u)[0]);
  EXPECT_EQ(0u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfCountZeroNibbles, 1u,
          List<uint32_t>{0x12345678u}, 0u)[0]);
  EXPECT_EQ(1u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfCountZeroNibbles, 1u,
          List<uint32_t>{0x12345670u}, 0u)[0]);
  EXPECT_EQ(0x8888u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfBroadcastNibble, 1u,
          List<uint32_t>{0x12345678u}, 0u)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1370, WorldGenerationComputeKernel::DenseUint32HalfHasZeroNibble), input,
          0u, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1371, WorldGenerationComputeKernel::DenseUint32HalfCountZeroNibbles), input,
          0u, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1372, WorldGenerationComputeKernel::DenseUint32HalfBroadcastNibble), input,
          0u, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
}


TEST(WorldGenerationComputeTest, DenseUint32HalfSextExtractInsertNibbleExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  EXPECT_EQ(0xfffffff8u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfSextNibble, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);
  EXPECT_EQ(0x5u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfSextNibble, 1u, List<uint32_t>{0x12345678u},
          3u)[0]);
  EXPECT_EQ(0x7u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfExtractNibble, 1u,
          List<uint32_t>{0x12345678u}, 1u)[0]);
  EXPECT_EQ(0x56a8u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfInsertNibble, 1u,
          List<uint32_t>{0x12345678u}, 0x0a01u)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1390, WorldGenerationComputeKernel::DenseUint32HalfSextNibble), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1391, WorldGenerationComputeKernel::DenseUint32HalfExtractNibble), input, 1u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1392, WorldGenerationComputeKernel::DenseUint32HalfInsertNibble), input,
          0x0a01u, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
}


TEST(WorldGenerationComputeTest, DenseUint32HalfAllEqualRangeMaskNibblesExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  EXPECT_EQ(0u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfAllNibblesEqual, 1u,
          List<uint32_t>{0x12345678u}, 0u)[0]);
  EXPECT_EQ(0xffffffffu,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfAllNibblesEqual, 1u,
          List<uint32_t>{0x11111111u}, 0u)[0]);
  EXPECT_EQ(0x3u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfNibbleRange, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);
  EXPECT_EQ(0x608u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfMaskNibbles, 1u, List<uint32_t>{0x12345678u},
          0x5u)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1410, WorldGenerationComputeKernel::DenseUint32HalfAllNibblesEqual), input,
          0u, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1411, WorldGenerationComputeKernel::DenseUint32HalfNibbleRange), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1412, WorldGenerationComputeKernel::DenseUint32HalfMaskNibbles), input, 0x5u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
}


TEST(WorldGenerationComputeTest, DenseUint32HalfBroadcastReplaceReverseNibblesExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  EXPECT_EQ(0x7777u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfBroadcastNibbleAt, 1u,
          List<uint32_t>{0x12345678u}, 1u)[0]);
  EXPECT_EQ(0x5a78u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfReplaceNibble, 1u,
          List<uint32_t>{0x12345678u}, 0x0a06u)[0]);
  EXPECT_EQ(0x8765u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfReverseNibbles, 1u,
          List<uint32_t>{0x12345678u}, 0u)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1430, WorldGenerationComputeKernel::DenseUint32HalfBroadcastNibbleAt), input,
          1u, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1431, WorldGenerationComputeKernel::DenseUint32HalfReplaceNibble), input,
          0x0a06u, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1432, WorldGenerationComputeKernel::DenseUint32HalfReverseNibbles), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
}


TEST(WorldGenerationComputeTest, DenseUint32HalfWidenDupFoldNibblesExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  EXPECT_EQ(0x05060708u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfWidenNibbles, 1u,
          List<uint32_t>{0x12345678u}, 0u)[0]);
  EXPECT_EQ(0x55667788u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfDupNibbles, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);
  EXPECT_EQ(0x3fu,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfFoldAdjacentNibbles, 1u,
          List<uint32_t>{0x12345678u}, 0u)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1450, WorldGenerationComputeKernel::DenseUint32HalfWidenNibbles), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1451, WorldGenerationComputeKernel::DenseUint32HalfDupNibbles), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1452, WorldGenerationComputeKernel::DenseUint32HalfFoldAdjacentNibbles),
          input, 0u, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
}


TEST(WorldGenerationComputeTest, DenseUint32HalfCountNonzeroAddSatNibbleExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  EXPECT_EQ(4u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfCountNonzeroNibbles, 1u,
          List<uint32_t>{0x12345678u}, 0u)[0]);
  EXPECT_EQ(0x6789u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfAddNibble, 1u, List<uint32_t>{0x12345678u},
          1u)[0]);
  EXPECT_EQ(0xefffu,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfSatAddNibble, 1u,
          List<uint32_t>{0x12345678u}, 9u)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1470, WorldGenerationComputeKernel::DenseUint32HalfCountNonzeroNibbles),
          input, 0u, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1471, WorldGenerationComputeKernel::DenseUint32HalfAddNibble), input, 1u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1472, WorldGenerationComputeKernel::DenseUint32HalfSatAddNibble), input, 9u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
}


TEST(WorldGenerationComputeTest, DenseUint32HalfSubSatSubMulNibbleExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  EXPECT_EQ(0x4567u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfSubNibble, 1u, List<uint32_t>{0x12345678u},
          1u)[0]);
  EXPECT_EQ(0u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfSatSubNibble, 1u,
          List<uint32_t>{0x12345678u}, 9u)[0]);
  EXPECT_EQ(0xace0u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfMulNibble, 1u, List<uint32_t>{0x12345678u},
          2u)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1490, WorldGenerationComputeKernel::DenseUint32HalfSubNibble), input, 1u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1491, WorldGenerationComputeKernel::DenseUint32HalfSatSubNibble), input, 9u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1492, WorldGenerationComputeKernel::DenseUint32HalfMulNibble), input, 2u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
}


TEST(WorldGenerationComputeTest, DenseUint32HalfSatMulDivModNibbleExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  EXPECT_EQ(0xacefu,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfSatMulNibble, 1u,
          List<uint32_t>{0x12345678u}, 2u)[0]);
  EXPECT_EQ(0x2334u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfDivNibble, 1u, List<uint32_t>{0x12345678u},
          2u)[0]);
  EXPECT_EQ(0x2012u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfModNibble, 1u, List<uint32_t>{0x12345678u},
          3u)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1510, WorldGenerationComputeKernel::DenseUint32HalfSatMulNibble), input, 2u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1511, WorldGenerationComputeKernel::DenseUint32HalfDivNibble), input, 2u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1512, WorldGenerationComputeKernel::DenseUint32HalfModNibble), input, 3u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
}


TEST(WorldGenerationComputeTest, DenseUint32HalfMinMaxClampNibbleExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  EXPECT_EQ(0x5666u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfMinNibble, 1u, List<uint32_t>{0x12345678u},
          6u)[0]);
  EXPECT_EQ(0x6678u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfMaxNibble, 1u, List<uint32_t>{0x12345678u},
          6u)[0]);
  EXPECT_EQ(0x5666u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfClampNibble, 1u,
          List<uint32_t>{0x12345678u}, 0x0603u)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1540, WorldGenerationComputeKernel::DenseUint32HalfMinNibble), input, 6u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1541, WorldGenerationComputeKernel::DenseUint32HalfMaxNibble), input, 6u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1542, WorldGenerationComputeKernel::DenseUint32HalfClampNibble), input,
          0x0603u, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
}


TEST(WorldGenerationComputeTest, DenseUint32HalfAndOrXorNibbleExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  EXPECT_EQ(0x5450u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfAndNibble, 1u, List<uint32_t>{0x12345678u},
          5u)[0]);
  EXPECT_EQ(0x577du,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfOrNibble, 1u, List<uint32_t>{0x12345678u},
          5u)[0]);
  EXPECT_EQ(0x032du,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfXorNibble, 1u, List<uint32_t>{0x12345678u},
          5u)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1560, WorldGenerationComputeKernel::DenseUint32HalfAndNibble), input, 5u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1561, WorldGenerationComputeKernel::DenseUint32HalfOrNibble), input, 5u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1562, WorldGenerationComputeKernel::DenseUint32HalfXorNibble), input, 5u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
}


TEST(WorldGenerationComputeTest, DenseUint32HalfShlShrRolNibbleExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  EXPECT_EQ(0xace0u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfShlNibble, 1u, List<uint32_t>{0x12345678u},
          1u)[0]);
  EXPECT_EQ(0x2334u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfShrNibble, 1u, List<uint32_t>{0x12345678u},
          1u)[0]);
  EXPECT_EQ(0xace1u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfRolNibble, 1u, List<uint32_t>{0x12345678u},
          1u)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1580, WorldGenerationComputeKernel::DenseUint32HalfShlNibble), input, 1u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1581, WorldGenerationComputeKernel::DenseUint32HalfShrNibble), input, 1u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1582, WorldGenerationComputeKernel::DenseUint32HalfRolNibble), input, 1u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
}


TEST(WorldGenerationComputeTest, DenseUint32HalfRorNotNegNibbleExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  EXPECT_EQ(0xa3b4u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfRorNibble, 1u, List<uint32_t>{0x12345678u},
          1u)[0]);
  EXPECT_EQ(0xa987u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfNotNibble, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);
  EXPECT_EQ(0xba98u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfNegNibble, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1600, WorldGenerationComputeKernel::DenseUint32HalfRorNibble), input, 1u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1601, WorldGenerationComputeKernel::DenseUint32HalfNotNibble), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1602, WorldGenerationComputeKernel::DenseUint32HalfNegNibble), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
}


TEST(WorldGenerationComputeTest, DenseUint32HalfAbsSignMaskIsZeroNibbleExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  EXPECT_EQ(0x7654u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfAbsNibble, 1u, List<uint32_t>{0x12349abcu},
          0u)[0]);
  EXPECT_EQ(0x000fu,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfSignMaskNibble, 1u,
          List<uint32_t>{0x12345678u}, 0u)[0]);
  EXPECT_EQ(0x0f00u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfIsZeroNibble, 1u, List<uint32_t>{0x12345078u},
          0u)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1620, WorldGenerationComputeKernel::DenseUint32HalfAbsNibble), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1621, WorldGenerationComputeKernel::DenseUint32HalfSignMaskNibble), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1622, WorldGenerationComputeKernel::DenseUint32HalfIsZeroNibble), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
}


TEST(WorldGenerationComputeTest, DenseUint32HalfIsNonzeroEqNeNibbleExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  EXPECT_EQ(0xffffu,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfIsNonzeroNibble, 1u,
          List<uint32_t>{0x12345678u}, 0u)[0]);
  EXPECT_EQ(0x0f00u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfEqNibble, 1u, List<uint32_t>{0x12345678u},
          6u)[0]);
  EXPECT_EQ(0xf0ffu,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfNeNibble, 1u, List<uint32_t>{0x12345678u},
          6u)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1640, WorldGenerationComputeKernel::DenseUint32HalfIsNonzeroNibble), input,
          0u, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1641, WorldGenerationComputeKernel::DenseUint32HalfEqNibble), input, 6u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1642, WorldGenerationComputeKernel::DenseUint32HalfNeNibble), input, 6u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
}


TEST(WorldGenerationComputeTest, DenseUint32HalfLtLeGtNibbleExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  EXPECT_EQ(0xf000u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfLtNibble, 1u, List<uint32_t>{0x12345678u},
          6u)[0]);
  EXPECT_EQ(0xff00u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfLeNibble, 1u, List<uint32_t>{0x12345678u},
          6u)[0]);
  EXPECT_EQ(0x00ffu,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfGtNibble, 1u, List<uint32_t>{0x12345678u},
          6u)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1660, WorldGenerationComputeKernel::DenseUint32HalfLtNibble), input, 6u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1661, WorldGenerationComputeKernel::DenseUint32HalfLeNibble), input, 6u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1662, WorldGenerationComputeKernel::DenseUint32HalfGtNibble), input, 6u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
}


TEST(WorldGenerationComputeTest, DenseUint32HalfGeBetweenOutsideNibbleExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  EXPECT_EQ(0x0fffu,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfGeNibble, 1u, List<uint32_t>{0x12345678u},
          6u)[0]);
  EXPECT_EQ(0xff00u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfBetweenNibble, 1u,
          List<uint32_t>{0x12345678u}, 0x0605u)[0]);
  EXPECT_EQ(0x00ffu,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfOutsideNibble, 1u,
          List<uint32_t>{0x12345678u}, 0x0605u)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1680, WorldGenerationComputeKernel::DenseUint32HalfGeNibble), input, 6u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1681, WorldGenerationComputeKernel::DenseUint32HalfBetweenNibble), input,
          0x0605u, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1682, WorldGenerationComputeKernel::DenseUint32HalfOutsideNibble), input,
          0x0605u, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
}


TEST(WorldGenerationComputeTest, DenseUint32HalfPopcountParityReverseBitsNibbleExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  EXPECT_EQ(0x2231u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfPopcountNibble, 1u,
          List<uint32_t>{0x12345678u}, 0u)[0]);
  EXPECT_EQ(0x0011u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfParityNibble, 1u,
          List<uint32_t>{0x12345678u}, 0u)[0]);
  EXPECT_EQ(0xa6e1u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfReverseBitsNibble, 1u,
          List<uint32_t>{0x12345678u}, 0u)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1700, WorldGenerationComputeKernel::DenseUint32HalfPopcountNibble), input,
          0u, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1701, WorldGenerationComputeKernel::DenseUint32HalfParityNibble), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1702, WorldGenerationComputeKernel::DenseUint32HalfReverseBitsNibble),
          input, 0u, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
}


TEST(WorldGenerationComputeTest, DenseUint32HalfIsolateClearSmearLsbNibbleExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  EXPECT_EQ(0x1218u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfIsolateLsbNibble, 1u,
          List<uint32_t>{0x12345678u}, 0u)[0]);
  EXPECT_EQ(0x4460u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfClearLsbNibble, 1u,
          List<uint32_t>{0x12345678u}, 0u)[0]);
  EXPECT_EQ(0x577fu,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfSmearLsbNibble, 1u,
          List<uint32_t>{0x12345678u}, 0u)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1720, WorldGenerationComputeKernel::DenseUint32HalfIsolateLsbNibble), input,
          0u, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1721, WorldGenerationComputeKernel::DenseUint32HalfClearLsbNibble), input,
          0u, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1722, WorldGenerationComputeKernel::DenseUint32HalfSmearLsbNibble), input,
          0u, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
}


TEST(WorldGenerationComputeTest, DenseUint32HalfIsolateClearSmearMsbNibbleExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  EXPECT_EQ(0x4448u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfIsolateMsbNibble, 1u,
          List<uint32_t>{0x12345678u}, 0u)[0]);
  EXPECT_EQ(0x1230u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfClearMsbNibble, 1u,
          List<uint32_t>{0x12345678u}, 0u)[0]);
  EXPECT_EQ(0x777fu,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfSmearMsbNibble, 1u,
          List<uint32_t>{0x12345678u}, 0u)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1740, WorldGenerationComputeKernel::DenseUint32HalfIsolateMsbNibble), input,
          0u, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1741, WorldGenerationComputeKernel::DenseUint32HalfClearMsbNibble), input,
          0u, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1742, WorldGenerationComputeKernel::DenseUint32HalfSmearMsbNibble), input,
          0u, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
}


TEST(WorldGenerationComputeTest, DenseUint32HalfLsbMaskKeepOddEvenBitsNibbleExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  EXPECT_EQ(0x131fu,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfLsbMaskNibble, 1u,
          List<uint32_t>{0x12345678u}, 0u)[0]);
  EXPECT_EQ(0x5450u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfKeepOddBitsNibble, 1u,
          List<uint32_t>{0x12345678u}, 0u)[0]);
  EXPECT_EQ(0x0228u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfKeepEvenBitsNibble, 1u,
          List<uint32_t>{0x12345678u}, 0u)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1760, WorldGenerationComputeKernel::DenseUint32HalfLsbMaskNibble), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1761, WorldGenerationComputeKernel::DenseUint32HalfKeepOddBitsNibble),
          input, 0u, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1762, WorldGenerationComputeKernel::DenseUint32HalfKeepEvenBitsNibble),
          input, 0u, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
}


TEST(WorldGenerationComputeTest, DenseUint32HalfSwapGrayEncodeDecodeNibbleExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  EXPECT_EQ(0xa9b4u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfSwapAdjacentBitsNibble, 1u,
          List<uint32_t>{0x12345678u}, 0u)[0]);
  EXPECT_EQ(0x754cu,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfGrayEncodeNibble, 1u,
          List<uint32_t>{0x12345678u}, 0u)[0]);
  EXPECT_EQ(0x645fu,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfGrayDecodeNibble, 1u,
          List<uint32_t>{0x12345678u}, 0u)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1780, WorldGenerationComputeKernel::DenseUint32HalfSwapAdjacentBitsNibble),
          input, 0u, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1781, WorldGenerationComputeKernel::DenseUint32HalfGrayEncodeNibble), input,
          0u, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1782, WorldGenerationComputeKernel::DenseUint32HalfGrayDecodeNibble), input,
          0u, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
}


TEST(WorldGenerationComputeTest, DenseUint32HalfClzCtzCloNibbleExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  EXPECT_EQ(0x1110u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfClzNibble, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);
  EXPECT_EQ(0x0103u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfCtzNibble, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);
  EXPECT_EQ(0x0001u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfCloNibble, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1800, WorldGenerationComputeKernel::DenseUint32HalfClzNibble), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1801, WorldGenerationComputeKernel::DenseUint32HalfCtzNibble), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1802, WorldGenerationComputeKernel::DenseUint32HalfCloNibble), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
}


TEST(WorldGenerationComputeTest, DenseUint32HalfCtoBitLengthIsPowerOfTwoNibbleExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  EXPECT_EQ(0x1030u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfCtoNibble, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);
  EXPECT_EQ(0x3334u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfBitLengthNibble, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);
  EXPECT_EQ(0x0001u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfIsPowerOfTwoNibble, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1820, WorldGenerationComputeKernel::DenseUint32HalfCtoNibble), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1821, WorldGenerationComputeKernel::DenseUint32HalfBitLengthNibble), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1822, WorldGenerationComputeKernel::DenseUint32HalfIsPowerOfTwoNibble), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
}


TEST(WorldGenerationComputeTest, DenseUint32HalfNextPrevPow2BroadcastLsbNibbleExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  EXPECT_EQ(0x8888u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfNextPow2Nibble, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);
  EXPECT_EQ(0x4448u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfPrevPow2Nibble, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);
  EXPECT_EQ(0xf0f0u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfBroadcastLsbNibble, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1840, WorldGenerationComputeKernel::DenseUint32HalfNextPow2Nibble), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1841, WorldGenerationComputeKernel::DenseUint32HalfPrevPow2Nibble), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1842, WorldGenerationComputeKernel::DenseUint32HalfBroadcastLsbNibble), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
}


TEST(WorldGenerationComputeTest, DenseUint32HalfBroadcastMsbParityFillRotl1NibbleExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  EXPECT_EQ(0x000fu,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfBroadcastMsbNibble, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);
  EXPECT_EQ(0x00ffu,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfParityFillNibble, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);
  EXPECT_EQ(0xace1u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfRotl1Nibble, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1860, WorldGenerationComputeKernel::DenseUint32HalfBroadcastMsbNibble), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1861, WorldGenerationComputeKernel::DenseUint32HalfParityFillNibble), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1862, WorldGenerationComputeKernel::DenseUint32HalfRotl1Nibble), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
}


TEST(WorldGenerationComputeTest, DenseUint32HalfRotr1NegateIncWrapNibbleExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  EXPECT_EQ(0xa3b4u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfRotr1Nibble, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);
  EXPECT_EQ(0xa987u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfNegateNibble, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);
  EXPECT_EQ(0x6789u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfIncWrapNibble, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1880, WorldGenerationComputeKernel::DenseUint32HalfRotr1Nibble), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1881, WorldGenerationComputeKernel::DenseUint32HalfNegateNibble), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1882, WorldGenerationComputeKernel::DenseUint32HalfIncWrapNibble), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
}


TEST(WorldGenerationComputeTest, DenseUint32HalfDecNandNorNibbleExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  EXPECT_EQ(0x4567u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfDecWrapNibble, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);
  EXPECT_EQ(0xfdd7u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfNandNibble, 1u, List<uint32_t>{0x12345678u},
          0xau)[0]);
  EXPECT_EQ(0x0105u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfNorNibble, 1u, List<uint32_t>{0x12345678u},
          0xau)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1900, WorldGenerationComputeKernel::DenseUint32HalfDecWrapNibble), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1901, WorldGenerationComputeKernel::DenseUint32HalfNandNibble), input, 0xau,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1902, WorldGenerationComputeKernel::DenseUint32HalfNorNibble), input, 0xau,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
}


TEST(WorldGenerationComputeTest, DenseUint32HalfXnorMajMuxNibbleExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  EXPECT_EQ(0x032du,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfXnorNibble, 1u, List<uint32_t>{0x12345678u},
          0xau)[0]);
  EXPECT_EQ(0x5678u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfMajNibble, 1u, List<uint32_t>{0x12345678u},
          0x5au)[0]);
  EXPECT_EQ(0x0228u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfMuxNibble, 1u, List<uint32_t>{0x12345678u},
          0x5au)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1920, WorldGenerationComputeKernel::DenseUint32HalfXnorNibble), input, 0xau,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1921, WorldGenerationComputeKernel::DenseUint32HalfMajNibble), input, 0x5au,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1922, WorldGenerationComputeKernel::DenseUint32HalfMuxNibble), input, 0x5au,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
}


TEST(WorldGenerationComputeTest, DenseUint32HalfImpliesNeqFillAbsDiffNibbleExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  EXPECT_EQ(0xabafu,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfImpliesNibble, 1u, List<uint32_t>{0x12345678u},
          0xau)[0]);
  EXPECT_EQ(0xf0ffu,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfNeqFillNibble, 1u, List<uint32_t>{0x12345678u},
          0x6u)[0]);
  EXPECT_EQ(0x2345u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfAbsDiffNibble, 1u, List<uint32_t>{0x12345678u},
          0x3u)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1940, WorldGenerationComputeKernel::DenseUint32HalfImpliesNibble), input, 0xau,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1941, WorldGenerationComputeKernel::DenseUint32HalfNeqFillNibble), input, 0x6u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1942, WorldGenerationComputeKernel::DenseUint32HalfAbsDiffNibble), input, 0x3u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
}


TEST(WorldGenerationComputeTest, DenseUint32HalfEqOddEvenFillNibbleExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  EXPECT_EQ(0x0f00u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfEqFillNibble, 1u, List<uint32_t>{0x12345678u},
          0x6u)[0]);
  EXPECT_EQ(0xf0f0u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfOddFillNibble, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);
  EXPECT_EQ(0x0f0fu,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfEvenFillNibble, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1960, WorldGenerationComputeKernel::DenseUint32HalfEqFillNibble), input, 0x6u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1961, WorldGenerationComputeKernel::DenseUint32HalfOddFillNibble), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1962, WorldGenerationComputeKernel::DenseUint32HalfEvenFillNibble), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
}


TEST(WorldGenerationComputeTest, DenseUint32HalfLtLeGtFillNibbleExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  EXPECT_EQ(0xf000u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfLtFillNibble, 1u, List<uint32_t>{0x12345678u},
          0x6u)[0]);
  EXPECT_EQ(0xff00u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfLeFillNibble, 1u, List<uint32_t>{0x12345678u},
          0x6u)[0]);
  EXPECT_EQ(0x00ffu,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfGtFillNibble, 1u, List<uint32_t>{0x12345678u},
          0x6u)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1980, WorldGenerationComputeKernel::DenseUint32HalfLtFillNibble), input, 0x6u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1981, WorldGenerationComputeKernel::DenseUint32HalfLeFillNibble), input, 0x6u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(1982, WorldGenerationComputeKernel::DenseUint32HalfGtFillNibble), input, 0x6u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
}


TEST(WorldGenerationComputeTest, DenseUint32HalfGeSatIncSatDecNibbleExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  EXPECT_EQ(0x0fffu,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfGeFillNibble, 1u, List<uint32_t>{0x12345678u},
          0x6u)[0]);
  EXPECT_EQ(0x6789u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfSatIncNibble, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);
  EXPECT_EQ(0x4567u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfSatDecNibble, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(2000, WorldGenerationComputeKernel::DenseUint32HalfGeFillNibble), input, 0x6u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(2001, WorldGenerationComputeKernel::DenseUint32HalfSatIncNibble), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(2002, WorldGenerationComputeKernel::DenseUint32HalfSatDecNibble), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
}


TEST(WorldGenerationComputeTest, DenseUint32HalfRotl2Rotl3AvgNibbleExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  EXPECT_EQ(0x59d2u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfRotl2Nibble, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);
  EXPECT_EQ(0xa3b4u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfRotl3Nibble, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);
  EXPECT_EQ(0x4556u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfAvgNibble, 1u, List<uint32_t>{0x12345678u},
          0x4u)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(2020, WorldGenerationComputeKernel::DenseUint32HalfRotl2Nibble), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(2021, WorldGenerationComputeKernel::DenseUint32HalfRotl3Nibble), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(2022, WorldGenerationComputeKernel::DenseUint32HalfAvgNibble), input, 0x4u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
}













TEST(WorldGenerationComputeTest, DenseUint32HalfRotlRotrBitPairNibbleExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  EXPECT_EQ(0x59d2u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfRotlWrapNibble, 1u, List<uint32_t>{0x12345678u},
          0x2u)[0]);
  EXPECT_EQ(0xa3b4u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfRotrWrapNibble, 1u, List<uint32_t>{0x12345678u},
          0x1u)[0]);
  EXPECT_EQ(0x59d2u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfBitPairSwapNibble, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(2260, WorldGenerationComputeKernel::DenseUint32HalfRotlWrapNibble), input, 0x2u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(2261, WorldGenerationComputeKernel::DenseUint32HalfRotrWrapNibble), input, 0x1u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(2262, WorldGenerationComputeKernel::DenseUint32HalfBitPairSwapNibble), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
}

TEST(WorldGenerationComputeTest, DenseUint32HalfShlShrFunnelNibbleExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  EXPECT_EQ(0x48c0u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfShlWrapNibble, 1u, List<uint32_t>{0x12345678u},
          0x2u)[0]);
  EXPECT_EQ(0x1112u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfShrWrapNibble, 1u, List<uint32_t>{0x12345678u},
          0x2u)[0]);
  EXPECT_EQ(0x6ae2u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfFunnelShiftNibble, 1u, List<uint32_t>{0x12345678u},
          0xa2u)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(2240, WorldGenerationComputeKernel::DenseUint32HalfShlWrapNibble), input, 0x2u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(2241, WorldGenerationComputeKernel::DenseUint32HalfShrWrapNibble), input, 0x2u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(2242, WorldGenerationComputeKernel::DenseUint32HalfFunnelShiftNibble), input, 0xa2u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
}

TEST(WorldGenerationComputeTest, DenseUint32HalfFloorRoundRemNibbleExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  EXPECT_EQ(0x1222u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfFloorDivNibble, 1u, List<uint32_t>{0x12345678u},
          0x3u)[0]);
  EXPECT_EQ(0x2223u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfRoundDivNibble, 1u, List<uint32_t>{0x12345678u},
          0x3u)[0]);
  EXPECT_EQ(0x2012u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfRemNibble, 1u, List<uint32_t>{0x12345678u},
          0x3u)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(2220, WorldGenerationComputeKernel::DenseUint32HalfFloorDivNibble), input, 0x3u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(2221, WorldGenerationComputeKernel::DenseUint32HalfRoundDivNibble), input, 0x3u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(2222, WorldGenerationComputeKernel::DenseUint32HalfRemNibble), input, 0x3u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
}

TEST(WorldGenerationComputeTest, DenseUint32HalfModAddModSubCeilDivNibbleExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  EXPECT_EQ(0x1201u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfModAddNibble, 1u, List<uint32_t>{0x12345678u},
          0x35u)[0]);
  EXPECT_EQ(0x0120u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfModSubNibble, 1u, List<uint32_t>{0x12345678u},
          0x35u)[0]);
  EXPECT_EQ(0x2233u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfCeilDivNibble, 1u, List<uint32_t>{0x12345678u},
          0x3u)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(2200, WorldGenerationComputeKernel::DenseUint32HalfModAddNibble), input, 0x35u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(2201, WorldGenerationComputeKernel::DenseUint32HalfModSubNibble), input, 0x35u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(2202, WorldGenerationComputeKernel::DenseUint32HalfCeilDivNibble), input, 0x3u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
}

TEST(WorldGenerationComputeTest, DenseUint32HalfIfNeThresholdModMulNibbleExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  EXPECT_EQ(0xd6ddu,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfIfNePickNibble, 1u, List<uint32_t>{0x12345678u},
          0xd6u)[0]);
  EXPECT_EQ(0xddddu,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfThresholdPickNibble, 1u, List<uint32_t>{0x12345678u},
          0xd5u)[0]);
  EXPECT_EQ(0x1021u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfModMulNibble, 1u, List<uint32_t>{0x12345678u},
          0x35u)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(2180, WorldGenerationComputeKernel::DenseUint32HalfIfNePickNibble), input, 0xd6u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(2181, WorldGenerationComputeKernel::DenseUint32HalfThresholdPickNibble), input, 0xd5u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(2182, WorldGenerationComputeKernel::DenseUint32HalfModMulNibble), input, 0x35u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
}

TEST(WorldGenerationComputeTest, DenseUint32HalfSatWeightedMaskSelectIfEqPickNibbleExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  EXPECT_EQ(0xffffu,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfSatWeightedAddNibble, 1u, List<uint32_t>{0x12345678u},
          0xa5u)[0]);
  EXPECT_EQ(0xbbb8u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfMaskSelectNibble, 1u, List<uint32_t>{0x12345678u},
          0xb4u)[0]);
  EXPECT_EQ(0x5d78u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfIfEqPickNibble, 1u, List<uint32_t>{0x12345678u},
          0xd6u)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(2160, WorldGenerationComputeKernel::DenseUint32HalfSatWeightedAddNibble), input, 0xa5u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(2161, WorldGenerationComputeKernel::DenseUint32HalfMaskSelectNibble), input, 0xb4u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(2162, WorldGenerationComputeKernel::DenseUint32HalfIfEqPickNibble), input, 0xd6u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
}

TEST(WorldGenerationComputeTest, DenseUint32HalfBlendWeightedAddBitMergeNibbleExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  EXPECT_EQ(0x6788u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfBlendNibble, 1u, List<uint32_t>{0x12345678u},
          0xa5u)[0]);
  EXPECT_EQ(0xd27cu,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfWeightedAddNibble, 1u, List<uint32_t>{0x12345678u},
          0xa5u)[0]);
  EXPECT_EQ(0x4448u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfBitMergeNibble, 1u, List<uint32_t>{0x12345678u},
          0xc3u)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(2140, WorldGenerationComputeKernel::DenseUint32HalfBlendNibble), input, 0xa5u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(2141, WorldGenerationComputeKernel::DenseUint32HalfWeightedAddNibble), input, 0xa5u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(2142, WorldGenerationComputeKernel::DenseUint32HalfBitMergeNibble), input, 0xc3u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
}

TEST(WorldGenerationComputeTest, DenseUint32HalfNeq3AnyEqAllDistinctFillNibbleExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  EXPECT_EQ(0xfff0u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfNeq3FillNibble, 1u, List<uint32_t>{0x12345678u},
          0x88u)[0]);
  EXPECT_EQ(0xf000u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfAnyEqFillNibble, 1u, List<uint32_t>{0x12345678u},
          0x5au)[0]);
  EXPECT_EQ(0x0fffu,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfAllDistinctFillNibble, 1u, List<uint32_t>{0x12345678u},
          0x5au)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(2120, WorldGenerationComputeKernel::DenseUint32HalfNeq3FillNibble), input, 0x88u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(2121, WorldGenerationComputeKernel::DenseUint32HalfAnyEqFillNibble), input, 0x5au,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(2122, WorldGenerationComputeKernel::DenseUint32HalfAllDistinctFillNibble), input, 0x5au,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
}

TEST(WorldGenerationComputeTest, DenseUint32HalfSatMulAddMulSubEqual3FillNibbleExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  EXPECT_EQ(0xbdffu,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfSatMulAddNibble, 1u, List<uint32_t>{0x12345678u},
          0x12u)[0]);
  EXPECT_EQ(0xd71bu,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfMulSubNibble, 1u, List<uint32_t>{0x12345678u},
          0x5au)[0]);
  EXPECT_EQ(0x000fu,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfEqual3FillNibble, 1u, List<uint32_t>{0x12345678u},
          0x88u)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(2100, WorldGenerationComputeKernel::DenseUint32HalfSatMulAddNibble), input, 0x12u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(2101, WorldGenerationComputeKernel::DenseUint32HalfMulSubNibble), input, 0x5au,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(2102, WorldGenerationComputeKernel::DenseUint32HalfEqual3FillNibble), input, 0x88u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
}

TEST(WorldGenerationComputeTest, DenseUint32HalfAvg3Xor3MulAddNibbleExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  EXPECT_EQ(0x6777u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfAvg3Nibble, 1u, List<uint32_t>{0x12345678u},
          0x5au)[0]);
  EXPECT_EQ(0xa987u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfXor3Nibble, 1u, List<uint32_t>{0x12345678u},
          0x5au)[0]);
  EXPECT_EQ(0x71b5u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfMulAddNibble, 1u, List<uint32_t>{0x12345678u},
          0x5au)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(2080, WorldGenerationComputeKernel::DenseUint32HalfAvg3Nibble), input, 0x5au,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(2081, WorldGenerationComputeKernel::DenseUint32HalfXor3Nibble), input, 0x5au,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(2082, WorldGenerationComputeKernel::DenseUint32HalfMulAddNibble), input, 0x5au,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
}

TEST(WorldGenerationComputeTest, DenseUint32HalfRange3Sum3SatSum3NibbleExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  EXPECT_EQ(0x5555u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfRange3Nibble, 1u, List<uint32_t>{0x12345678u},
          0x5au)[0]);
  EXPECT_EQ(0x4567u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfSum3Nibble, 1u, List<uint32_t>{0x12345678u},
          0x5au)[0]);
  EXPECT_EQ(0xffffu,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfSatSum3Nibble, 1u, List<uint32_t>{0x12345678u},
          0x5au)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(2060, WorldGenerationComputeKernel::DenseUint32HalfRange3Nibble), input, 0x5au,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(2061, WorldGenerationComputeKernel::DenseUint32HalfSum3Nibble), input, 0x5au,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(2062, WorldGenerationComputeKernel::DenseUint32HalfSatSum3Nibble), input, 0x5au,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
}

TEST(WorldGenerationComputeTest, DenseUint32HalfMidMin3Max3NibbleExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  EXPECT_EQ(0x5678u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfMidNibble, 1u, List<uint32_t>{0x12345678u},
          0x5au)[0]);
  EXPECT_EQ(0x5555u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfMin3Nibble, 1u, List<uint32_t>{0x12345678u},
          0x5au)[0]);
  EXPECT_EQ(0xaaaau,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HalfMax3Nibble, 1u, List<uint32_t>{0x12345678u},
          0x5au)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(2040, WorldGenerationComputeKernel::DenseUint32HalfMidNibble), input, 0x5au,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(2041, WorldGenerationComputeKernel::DenseUint32HalfMin3Nibble), input, 0x5au,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(2042, WorldGenerationComputeKernel::DenseUint32HalfMax3Nibble), input, 0x5au,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
}

TEST(WorldGenerationComputeTest, DenseUint32AllEqualRangeBroadcastAtExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  EXPECT_EQ(0xffffffffu,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32AllNibblesEqual, 1u, List<uint32_t>{0x11111111u},
          0u)[0]);
  EXPECT_EQ(0u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32AllNibblesEqual, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);
  EXPECT_EQ(0x7u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32NibbleRange, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);
  EXPECT_EQ(0u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32NibbleRange, 1u, List<uint32_t>{0x55555555u},
          0u)[0]);
  EXPECT_EQ(0x88888888u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32BroadcastNibbleAt, 1u,
          List<uint32_t>{0x12345678u}, 0u)[0]);
  EXPECT_EQ(0x11111111u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32BroadcastNibbleAt, 1u,
          List<uint32_t>{0x12345678u}, 7u)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(810, WorldGenerationComputeKernel::DenseUint32AllNibblesEqual), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(811, WorldGenerationComputeKernel::DenseUint32NibbleRange), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(812, WorldGenerationComputeKernel::DenseUint32BroadcastNibbleAt), input, 3u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
}

TEST(WorldGenerationComputeTest, DenseUint32ExpandHighReplaceMaskNibblesExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  EXPECT_EQ(0x20406080u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32ExpandHighNibbles, 1u, List<uint32_t>{0x2468u},
          0u)[0]);
  // replace nibble 8 with A in 0x12345678 → 0x1234567a
  EXPECT_EQ(0x1234567au,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32ReplaceNibble, 1u, List<uint32_t>{0x12345678u},
          0x0a08u)[0]);
  // keep odd nibbles (bits 0,2,4,6) of 0x12345678 → 0x02040608
  EXPECT_EQ(0x02040608u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32MaskNibbles, 1u, List<uint32_t>{0x12345678u},
          0x55u)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(790, WorldGenerationComputeKernel::DenseUint32ExpandHighNibbles), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(791, WorldGenerationComputeKernel::DenseUint32ReplaceNibble), input, 0x0a08u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(792, WorldGenerationComputeKernel::DenseUint32MaskNibbles), input, 0x55u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
}

TEST(WorldGenerationComputeTest, DiscardFailedOpenTicketsByKernelIsSelective) {
  WorldGenerationComputeCapabilities caps = WorldGenerationComputeService::probeCapabilities();
  caps.gpuOpenGlCompute = true;
  WorldGenerationComputeService service(caps);
  service.setDeferredCpuCompletion(true);
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(780, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{1u}, 0u,
      WorldGenerationComputeBackend::GpuOpenGlCompute));
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(781, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{2u}, 0u,
      WorldGenerationComputeBackend::GpuOpenGlCompute));
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(782, WorldGenerationComputeKernel::DenseUint32Add), List<uint32_t>{3u}, 0u,
      WorldGenerationComputeBackend::GpuOpenGlCompute));

  EXPECT_EQ(0u,
      service.discardFailedOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_EQ(2u, service.completeOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
#ifndef STAR_ENABLE_WORLDGEN_GL_COMPUTE
  EXPECT_EQ(2u, service.failedOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_EQ(2u,
      service.discardFailedOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_EQ(0u, service.failedOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_EQ(0u, service.countOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_EQ(1u, service.inFlightByKernel(WorldGenerationComputeKernel::DenseUint32Add));
  EXPECT_EQ(0u,
      service.discardFailedOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Add));
#else
  size_t failedXor = service.failedOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor);
  size_t readyXor = service.readyOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor);
  EXPECT_EQ(2u, failedXor + readyXor);
  size_t discardedFailed =
      service.discardFailedOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor);
  size_t discardedReady =
      service.discardReadyOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor);
  EXPECT_EQ(2u, discardedFailed + discardedReady);
  EXPECT_EQ(0u, service.countOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_EQ(1u, service.inFlightByKernel(WorldGenerationComputeKernel::DenseUint32Add));
#endif
}

TEST(WorldGenerationComputeTest, DenseUint32NibbleSumMaxMinExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  EXPECT_EQ(0x24u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32NibbleSum, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);
  EXPECT_EQ(0x8u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32NibbleMax, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);
  EXPECT_EQ(0x1u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32NibbleMin, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);
  EXPECT_EQ(0u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32NibbleMin, 1u, List<uint32_t>{0x00f00f00u},
          0u)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(makeIdentity(690, WorldGenerationComputeKernel::DenseUint32NibbleSum),
          input, 0u, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(makeIdentity(691, WorldGenerationComputeKernel::DenseUint32NibbleMax),
          input, 0u, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(makeIdentity(692, WorldGenerationComputeKernel::DenseUint32NibbleMin),
          input, 0u, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
}

TEST(WorldGenerationComputeTest, DenseUint32NibbleXorOrAndExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  EXPECT_EQ(0x8u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32NibbleXor, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);
  EXPECT_EQ(0xfu,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32NibbleOr, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);
  EXPECT_EQ(0u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32NibbleAnd, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);
  EXPECT_EQ(0x1u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32NibbleAnd, 1u, List<uint32_t>{0x11111111u},
          0u)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(makeIdentity(670, WorldGenerationComputeKernel::DenseUint32NibbleXor),
          input, 0u, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(makeIdentity(671, WorldGenerationComputeKernel::DenseUint32NibbleOr),
          input, 0u, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(makeIdentity(672, WorldGenerationComputeKernel::DenseUint32NibbleAnd),
          input, 0u, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
}

TEST(WorldGenerationComputeTest, DenseUint32ExpandEvenInterleaveHalvesExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  EXPECT_EQ(0xaau,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32ExpandLow16ToEven, 1u, List<uint32_t>{0x0fu},
          0u)[0]);
  EXPECT_EQ(0x6u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32InterleaveHalves, 1u,
          List<uint32_t>{0x00010002u}, 0u)[0]);
  EXPECT_EQ(0x00010002u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32DeinterleaveHalves, 1u, List<uint32_t>{0x6u},
          0u)[0]);
  auto roundTrip = WorldGenerationComputeService::runKernel(
      WorldGenerationComputeKernel::DenseUint32DeinterleaveHalves, 1u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32InterleaveHalves, 1u,
          List<uint32_t>{0x12345678u}, 0u),
      0u);
  ASSERT_EQ(1u, roundTrip.size());
  EXPECT_EQ(0x12345678u, roundTrip[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(650, WorldGenerationComputeKernel::DenseUint32ExpandLow16ToEven), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(651, WorldGenerationComputeKernel::DenseUint32InterleaveHalves), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(652, WorldGenerationComputeKernel::DenseUint32DeinterleaveHalves), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
}

TEST(WorldGenerationComputeTest, DenseUint32CompressExpandOddBitsExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  // 0xff odd mask 0x55 -> compress -> 0x0f; expand restores 0x55
  EXPECT_EQ(0x0fu,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32CompressOddBits, 1u, List<uint32_t>{0xffu},
          0u)[0]);
  EXPECT_EQ(0x0fu,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32CompressEvenBits, 1u, List<uint32_t>{0xffu},
          0u)[0]);
  EXPECT_EQ(0x55u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32ExpandLow16ToOdd, 1u, List<uint32_t>{0x0fu},
          0u)[0]);
  auto roundTrip = WorldGenerationComputeService::runKernel(
      WorldGenerationComputeKernel::DenseUint32ExpandLow16ToOdd, 1u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32CompressOddBits, 1u, List<uint32_t>{0x55aa55aau},
          0u),
      0u);
  ASSERT_EQ(1u, roundTrip.size());
  EXPECT_EQ(0x55aa55aau & 0x55555555u, roundTrip[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(630, WorldGenerationComputeKernel::DenseUint32CompressOddBits), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(631, WorldGenerationComputeKernel::DenseUint32CompressEvenBits), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(632, WorldGenerationComputeKernel::DenseUint32ExpandLow16ToOdd), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
}

TEST(WorldGenerationComputeTest, DenseUint32KeepOddEvenSwapAdjacentExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  EXPECT_EQ(0x01014545u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32KeepOddBits, 1u, List<uint32_t>{0x01234567u},
          0u)[0]);
  EXPECT_EQ(0x00220022u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32KeepEvenBits, 1u, List<uint32_t>{0x01234567u},
          0u)[0]);
  // 0b0110 (6) -> swap pairs -> 0b1001 (9)
  EXPECT_EQ(0x9u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32SwapAdjacentBits, 1u, List<uint32_t>{0x6u},
          0u)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(makeIdentity(610, WorldGenerationComputeKernel::DenseUint32KeepOddBits),
          input, 0u, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(611, WorldGenerationComputeKernel::DenseUint32KeepEvenBits), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(612, WorldGenerationComputeKernel::DenseUint32SwapAdjacentBits), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
}

TEST(WorldGenerationComputeTest, DenseUint32SmearMsbByteOrAndExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  EXPECT_EQ(0x3fu,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32SmearMsb, 1u, List<uint32_t>{0x38u}, 0u)[0]);
  EXPECT_EQ(0u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32SmearMsb, 1u, List<uint32_t>{0u}, 0u)[0]);
  EXPECT_EQ(0x7u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32ByteOr, 1u, List<uint32_t>{0x01020304u},
          0u)[0]);
  EXPECT_EQ(0x0fu,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32ByteAnd, 1u, List<uint32_t>{0x1f2f3f0fu},
          0u)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(makeIdentity(590, WorldGenerationComputeKernel::DenseUint32SmearMsb),
          input, 0u, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(makeIdentity(591, WorldGenerationComputeKernel::DenseUint32ByteOr),
          input, 0u, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(makeIdentity(592, WorldGenerationComputeKernel::DenseUint32ByteAnd),
          input, 0u, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
}

TEST(WorldGenerationComputeTest, DenseUint32SmearLsbClearMsbByteXorExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  EXPECT_EQ(0x3fu,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32SmearLsb, 1u, List<uint32_t>{0x38u}, 0u)[0]);
  EXPECT_EQ(0xffffffffu,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32SmearLsb, 1u, List<uint32_t>{0u}, 0u)[0]);
  EXPECT_EQ(0x18u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32ClearMsb, 1u, List<uint32_t>{0x38u}, 0u)[0]);
  EXPECT_EQ(0u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32ClearMsb, 1u, List<uint32_t>{0u}, 0u)[0]);
  EXPECT_EQ(0x4u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32ByteXor, 1u, List<uint32_t>{0x01020304u},
          0u)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(makeIdentity(570, WorldGenerationComputeKernel::DenseUint32SmearLsb),
          input, 0u, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(makeIdentity(571, WorldGenerationComputeKernel::DenseUint32ClearMsb),
          input, 0u, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(makeIdentity(572, WorldGenerationComputeKernel::DenseUint32ByteXor),
          input, 0u, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
}

TEST(WorldGenerationComputeTest, DenseUint32ClearLsbGrayEncodeDecodeExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  EXPECT_EQ(0x30u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32ClearLsb, 1u, List<uint32_t>{0x38u}, 0u)[0]);
  EXPECT_EQ(0u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32ClearLsb, 1u, List<uint32_t>{0u}, 0u)[0]);
  EXPECT_EQ(0x6u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32GrayEncode, 1u, List<uint32_t>{0x4u}, 0u)[0]);
  // Gray(7)=4; decode(4)=7
  EXPECT_EQ(0x7u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32GrayDecode, 1u, List<uint32_t>{0x4u}, 0u)[0]);
  auto roundTrip = WorldGenerationComputeService::runKernel(
      WorldGenerationComputeKernel::DenseUint32GrayDecode, 1u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32GrayEncode, 1u, List<uint32_t>{0x12345678u}, 0u),
      0u);
  ASSERT_EQ(1u, roundTrip.size());
  EXPECT_EQ(0x12345678u, roundTrip[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(makeIdentity(550, WorldGenerationComputeKernel::DenseUint32ClearLsb),
          input, 0u, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(makeIdentity(551, WorldGenerationComputeKernel::DenseUint32GrayEncode),
          input, 0u, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(makeIdentity(552, WorldGenerationComputeKernel::DenseUint32GrayDecode),
          input, 0u, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
}

TEST(WorldGenerationComputeTest, DenseUint32IsolateLsbMsbLsbMaskExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  EXPECT_EQ(0x8u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32IsolateLsb, 1u, List<uint32_t>{0x38u}, 0u)[0]);
  EXPECT_EQ(0u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32IsolateLsb, 1u, List<uint32_t>{0u}, 0u)[0]);
  EXPECT_EQ(0x20u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32IsolateMsb, 1u, List<uint32_t>{0x38u}, 0u)[0]);
  EXPECT_EQ(0u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32IsolateMsb, 1u, List<uint32_t>{0u}, 0u)[0]);
  EXPECT_EQ(0xfu,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32LsbMask, 1u, List<uint32_t>{0x38u}, 0u)[0]);
  EXPECT_EQ(0xffffffffu,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32LsbMask, 1u, List<uint32_t>{0u}, 0u)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(makeIdentity(530, WorldGenerationComputeKernel::DenseUint32IsolateLsb),
          input, 0u, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(makeIdentity(531, WorldGenerationComputeKernel::DenseUint32IsolateMsb),
          input, 0u, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(makeIdentity(532, WorldGenerationComputeKernel::DenseUint32LsbMask),
          input, 0u, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
}

TEST(WorldGenerationComputeTest, DenseUint32Dup16ReverseBitsInBytesExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  EXPECT_EQ(0x56785678u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32DupLow16, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);
  EXPECT_EQ(0x12341234u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32DupHigh16, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);
  // 0x01 -> bits 00000001 reversed within byte -> 10000000 = 0x80
  EXPECT_EQ(0x80u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32ReverseBitsInBytes, 1u, List<uint32_t>{0x01u},
          0u)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(makeIdentity(510, WorldGenerationComputeKernel::DenseUint32DupLow16),
          input, 0u, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(makeIdentity(511, WorldGenerationComputeKernel::DenseUint32DupHigh16),
          input, 0u, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(512, WorldGenerationComputeKernel::DenseUint32ReverseBitsInBytes), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
}

TEST(WorldGenerationComputeTest, DenseUint32ReverseNibblesByteMaxMinExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  EXPECT_EQ(0x87654321u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32ReverseNibbles, 1u,
          List<uint32_t>{0x12345678u}, 0u)[0]);
  EXPECT_EQ(0xefu,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32ByteMax, 1u, List<uint32_t>{0x01ef0203u},
          0u)[0]);
  EXPECT_EQ(0x01u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32ByteMin, 1u, List<uint32_t>{0x01ef0203u},
          0u)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(490, WorldGenerationComputeKernel::DenseUint32ReverseNibbles), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(makeIdentity(491, WorldGenerationComputeKernel::DenseUint32ByteMax),
          input, 0u, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(makeIdentity(492, WorldGenerationComputeKernel::DenseUint32ByteMin),
          input, 0u, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
}

TEST(WorldGenerationComputeTest, DenseUint32SwapHalvesCloCtoExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  EXPECT_EQ(0x56781234u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32SwapHalves, 1u, List<uint32_t>{0x12345678u},
          0u)[0]);
  EXPECT_EQ(32u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32Clo, 1u, List<uint32_t>{0xffffffffu}, 0u)[0]);
  EXPECT_EQ(0u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32Clo, 1u, List<uint32_t>{0u}, 0u)[0]);
  EXPECT_EQ(4u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32Clo, 1u, List<uint32_t>{0xf0ffffffu}, 0u)[0]);
  EXPECT_EQ(32u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32Cto, 1u, List<uint32_t>{0xffffffffu}, 0u)[0]);
  EXPECT_EQ(3u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32Cto, 1u, List<uint32_t>{0x7u}, 0u)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(makeIdentity(470, WorldGenerationComputeKernel::DenseUint32SwapHalves),
          input, 0u, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(makeIdentity(471, WorldGenerationComputeKernel::DenseUint32Clo),
          input, 0u, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(makeIdentity(472, WorldGenerationComputeKernel::DenseUint32Cto),
          input, 0u, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
}

TEST(WorldGenerationComputeTest, DenseUint32IsNonzeroMaskHighSatMulExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  EXPECT_EQ(0xffffffffu,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32IsNonzero, 1u, List<uint32_t>{1u}, 0u)[0]);
  EXPECT_EQ(0u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32IsNonzero, 1u, List<uint32_t>{0u}, 0u)[0]);
  EXPECT_EQ(0xab000000u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32MaskHigh, 1u, List<uint32_t>{0xabcdef01u},
          8u)[0]);
  EXPECT_EQ(0xabcdef01u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32MaskHigh, 1u, List<uint32_t>{0xabcdef01u},
          32u)[0]);
  EXPECT_EQ(0u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32MaskHigh, 1u, List<uint32_t>{0xabcdef01u},
          0u)[0]);
  EXPECT_EQ(0xffffffffu,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32SatMul, 1u, List<uint32_t>{0x10000u},
          0x10000u)[0]);
  EXPECT_EQ(6u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32SatMul, 1u, List<uint32_t>{2u}, 3u)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(makeIdentity(460, WorldGenerationComputeKernel::DenseUint32IsNonzero),
          input, 0u, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(makeIdentity(461, WorldGenerationComputeKernel::DenseUint32MaskHigh),
          input, 8u, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(makeIdentity(462, WorldGenerationComputeKernel::DenseUint32SatMul),
          input, 3u, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
}

TEST(WorldGenerationComputeTest, DenseUint32RorBytesSignMaskIsZeroExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  EXPECT_EQ(0x04010203u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32RorBytes, 1u, List<uint32_t>{0x01020304u},
          1u)[0]);
  EXPECT_EQ(0x01020304u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32RorBytes, 1u, List<uint32_t>{0x01020304u},
          0u)[0]);
  EXPECT_EQ(0xffffffffu,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32SignMask, 1u, List<uint32_t>{0x80000000u},
          0u)[0]);
  EXPECT_EQ(0u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32SignMask, 1u, List<uint32_t>{0x7fffffffu},
          0u)[0]);
  EXPECT_EQ(0xffffffffu,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32IsZero, 1u, List<uint32_t>{0u}, 0u)[0]);
  EXPECT_EQ(0u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32IsZero, 1u, List<uint32_t>{1u}, 0u)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(makeIdentity(440, WorldGenerationComputeKernel::DenseUint32RorBytes),
          input, 1u, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(makeIdentity(441, WorldGenerationComputeKernel::DenseUint32SignMask),
          input, 0u, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(makeIdentity(442, WorldGenerationComputeKernel::DenseUint32IsZero),
          input, 0u, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
}

TEST(WorldGenerationComputeTest, DenseUint32NegAbsRolBytesExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  EXPECT_EQ(0xffffffffu,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32Neg, 1u, List<uint32_t>{1u}, 0u)[0]);
  EXPECT_EQ(0u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32Neg, 1u, List<uint32_t>{0u}, 0u)[0]);
  EXPECT_EQ(5u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32Abs, 1u, List<uint32_t>{0xfffffffbu}, 0u)[0]);
  EXPECT_EQ(0x80000000u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32Abs, 1u, List<uint32_t>{0x80000000u}, 0u)[0]);
  EXPECT_EQ(0x02030401u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32RolBytes, 1u, List<uint32_t>{0x01020304u},
          1u)[0]);
  EXPECT_EQ(0x01020304u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32RolBytes, 1u, List<uint32_t>{0x01020304u},
          0u)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(makeIdentity(430, WorldGenerationComputeKernel::DenseUint32Neg),
          input, 0u, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(makeIdentity(431, WorldGenerationComputeKernel::DenseUint32Abs),
          input, 0u, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(makeIdentity(432, WorldGenerationComputeKernel::DenseUint32RolBytes),
          input, 1u, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
}

TEST(WorldGenerationComputeTest, DenseUint32BitSetClearToggleExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  EXPECT_EQ(0x5u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32BitSet, 1u, List<uint32_t>{0x4u}, 0u)[0]);
  EXPECT_EQ(0x4u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32BitClear, 1u, List<uint32_t>{0x5u}, 0u)[0]);
  EXPECT_EQ(0x5u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32BitToggle, 1u, List<uint32_t>{0x4u}, 0u)[0]);
  EXPECT_EQ(0x4u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32BitToggle, 1u, List<uint32_t>{0x5u}, 0u)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(makeIdentity(410, WorldGenerationComputeKernel::DenseUint32BitSet),
          input, 1u, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(makeIdentity(411, WorldGenerationComputeKernel::DenseUint32BitClear),
          input, 1u, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(makeIdentity(412, WorldGenerationComputeKernel::DenseUint32BitToggle),
          input, 1u, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
}

TEST(WorldGenerationComputeTest, DenseUint32SextExtractByteExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  EXPECT_EQ(0xffffff80u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32Sext8, 1u, List<uint32_t>{0x80u}, 0u)[0]);
  EXPECT_EQ(0x7fu,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32Sext8, 1u, List<uint32_t>{0x7fu}, 0u)[0]);
  EXPECT_EQ(0xffff8000u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32Sext16, 1u, List<uint32_t>{0x8000u}, 0u)[0]);
  EXPECT_EQ(0x1234u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32Sext16, 1u, List<uint32_t>{0x1234u}, 0u)[0]);
  EXPECT_EQ(0xefu,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32ExtractByte, 1u, List<uint32_t>{0xabcdef01u},
          1u)[0]);
  EXPECT_EQ(0x01u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32ExtractByte, 1u, List<uint32_t>{0xabcdef01u},
          0u)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(makeIdentity(390, WorldGenerationComputeKernel::DenseUint32Sext8),
          input, 0u, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(makeIdentity(391, WorldGenerationComputeKernel::DenseUint32Sext16),
          input, 0u, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(392, WorldGenerationComputeKernel::DenseUint32ExtractByte), input, 1u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
}

TEST(WorldGenerationComputeTest, DenseUint32Ilog2BroadcastByteByteSumExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  EXPECT_EQ(0u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32Ilog2, 1u, List<uint32_t>{0u}, 0u)[0]);
  EXPECT_EQ(0u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32Ilog2, 1u, List<uint32_t>{1u}, 0u)[0]);
  EXPECT_EQ(3u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32Ilog2, 1u, List<uint32_t>{8u}, 0u)[0]);
  EXPECT_EQ(31u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32Ilog2, 1u, List<uint32_t>{0x80000000u}, 0u)[0]);
  EXPECT_EQ(0xababababu,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32BroadcastByte, 1u, List<uint32_t>{0x12abu}, 0u)[0]);
  EXPECT_EQ(10u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32ByteSum, 1u, List<uint32_t>{0x01020304u}, 0u)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(makeIdentity(370, WorldGenerationComputeKernel::DenseUint32Ilog2),
          input, 0u, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(371, WorldGenerationComputeKernel::DenseUint32BroadcastByte), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(makeIdentity(372, WorldGenerationComputeKernel::DenseUint32ByteSum),
          input, 0u, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
}

TEST(WorldGenerationComputeTest, DenseUint32HasZeroByteNextPow2MaskLowExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  EXPECT_EQ(0xffffffffu,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HasZeroByte, 1u, List<uint32_t>{0x00112233u},
          0u)[0]);
  EXPECT_EQ(0u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32HasZeroByte, 1u, List<uint32_t>{0x11223344u},
          0u)[0]);
  EXPECT_EQ(0u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32NextPow2, 1u, List<uint32_t>{0u}, 0u)[0]);
  EXPECT_EQ(8u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32NextPow2, 1u, List<uint32_t>{5u}, 0u)[0]);
  EXPECT_EQ(0u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32NextPow2, 1u, List<uint32_t>{0xffffffffu}, 0u)[0]);
  EXPECT_EQ(0x01u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32MaskLow, 1u, List<uint32_t>{0xabcdef01u}, 8u)[0]);
  EXPECT_EQ(0xabcdef01u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32MaskLow, 1u, List<uint32_t>{0xabcdef01u}, 32u)[0]);
  EXPECT_EQ(0u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32MaskLow, 1u, List<uint32_t>{0xabcdef01u}, 0u)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(
          makeIdentity(350, WorldGenerationComputeKernel::DenseUint32HasZeroByte), input, 0u,
          WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(makeIdentity(351, WorldGenerationComputeKernel::DenseUint32NextPow2),
          input, 0u, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(makeIdentity(352, WorldGenerationComputeKernel::DenseUint32MaskLow),
          input, 8u, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
}

TEST(WorldGenerationComputeTest, AnyOpenTicketsByKernelTracksPresence) {
  WorldGenerationComputeService service;
  service.setDeferredCpuCompletion(true);
  EXPECT_FALSE(service.anyOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(560, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{1u}, 0u));
  EXPECT_TRUE(service.anyOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_FALSE(service.anyOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Add));
  EXPECT_EQ(1u, service.drainOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_FALSE(service.anyOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
}

TEST(WorldGenerationComputeTest, FailedOpenTicketsByKernelCountsDispatchFailures) {
  // Forced GPU capability admits tickets; without a live GL device, complete fails.
  WorldGenerationComputeCapabilities caps = WorldGenerationComputeService::probeCapabilities();
  caps.gpuOpenGlCompute = true;
  WorldGenerationComputeService service(caps);
  service.setDeferredCpuCompletion(true);
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(580, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{1u}, 0u,
      WorldGenerationComputeBackend::GpuOpenGlCompute));
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(581, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{2u}, 0u,
      WorldGenerationComputeBackend::GpuOpenGlCompute));
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(582, WorldGenerationComputeKernel::DenseUint32Add), List<uint32_t>{3u}, 0u,
      WorldGenerationComputeBackend::GpuOpenGlCompute));

  EXPECT_EQ(0u, service.failedOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_EQ(2u, service.completeOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
#ifndef STAR_ENABLE_WORLDGEN_GL_COMPUTE
  EXPECT_EQ(2u, service.failedOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_EQ(0u, service.failedOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Add));
  EXPECT_EQ(0u, service.readyOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_EQ(1u, service.inFlightByKernel(WorldGenerationComputeKernel::DenseUint32Add));
#else
  // With GL builds, dispatch may succeed if a probe already created a context.
  EXPECT_EQ(0u, service.failedOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Add));
  EXPECT_EQ(1u, service.inFlightByKernel(WorldGenerationComputeKernel::DenseUint32Add));
  size_t failedXor = service.failedOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor);
  size_t readyXor = service.readyOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor);
  EXPECT_EQ(2u, failedXor + readyXor);
#endif
}

TEST(WorldGenerationComputeTest, AnyFailedOpenTicketsByKernelTracksFailedPresence) {
  WorldGenerationComputeCapabilities caps = WorldGenerationComputeService::probeCapabilities();
  caps.gpuOpenGlCompute = true;
  WorldGenerationComputeService service(caps);
  service.setDeferredCpuCompletion(true);
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(720, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{1u}, 0u,
      WorldGenerationComputeBackend::GpuOpenGlCompute));
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(721, WorldGenerationComputeKernel::DenseUint32Add), List<uint32_t>{2u}, 0u,
      WorldGenerationComputeBackend::GpuOpenGlCompute));
  EXPECT_FALSE(service.anyFailedOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_EQ(1u, service.completeOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
#ifndef STAR_ENABLE_WORLDGEN_GL_COMPUTE
  EXPECT_TRUE(service.anyFailedOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_FALSE(service.anyFailedOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Add));
  EXPECT_TRUE(service.anyInFlightByKernel(WorldGenerationComputeKernel::DenseUint32Add));
#else
  bool failedXor = service.anyFailedOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor);
  bool readyXor = service.anyReadyOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor);
  EXPECT_TRUE(failedXor || readyXor);
  EXPECT_TRUE(service.anyInFlightByKernel(WorldGenerationComputeKernel::DenseUint32Add));
#endif
}

TEST(WorldGenerationComputeTest, AnyCancelledOpenTicketsByKernelTracksCancelledPresence) {
  WorldGenerationComputeService service;
  service.setDeferredCpuCompletion(true);
  EXPECT_FALSE(
      service.anyCancelledOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(740, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{1u}, 0u));
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(741, WorldGenerationComputeKernel::DenseUint32Add), List<uint32_t>{2u}, 0u));
  EXPECT_FALSE(
      service.anyCancelledOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_EQ(1u, service.cancelOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_TRUE(
      service.anyCancelledOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_FALSE(
      service.anyCancelledOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Add));
  EXPECT_TRUE(service.anyInFlightByKernel(WorldGenerationComputeKernel::DenseUint32Add));
}

TEST(WorldGenerationComputeTest, ListFailedOpenTicketsByKernelReturnsFailedIdsOnly) {
  WorldGenerationComputeCapabilities caps = WorldGenerationComputeService::probeCapabilities();
  caps.gpuOpenGlCompute = true;
  WorldGenerationComputeService service(caps);
  service.setDeferredCpuCompletion(true);
  auto xorA = service.submit(
      makeIdentity(620, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{1u}, 0u,
      WorldGenerationComputeBackend::GpuOpenGlCompute);
  auto xorB = service.submit(
      makeIdentity(621, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{2u}, 0u,
      WorldGenerationComputeBackend::GpuOpenGlCompute);
  auto add = service.submit(
      makeIdentity(622, WorldGenerationComputeKernel::DenseUint32Add), List<uint32_t>{3u}, 0u,
      WorldGenerationComputeBackend::GpuOpenGlCompute);
  ASSERT_TRUE((bool)xorA);
  ASSERT_TRUE((bool)xorB);
  ASSERT_TRUE((bool)add);

  EXPECT_TRUE(service.listFailedOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor)
          .empty());
  EXPECT_EQ(2u, service.completeOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
#ifndef STAR_ENABLE_WORLDGEN_GL_COMPUTE
  auto failedXor = service.listFailedOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor);
  ASSERT_EQ(2u, failedXor.size());
  EXPECT_TRUE(failedXor[0] == *xorA || failedXor[0] == *xorB);
  EXPECT_TRUE(failedXor[1] == *xorA || failedXor[1] == *xorB);
  EXPECT_NE(failedXor[0].id, failedXor[1].id);
  EXPECT_TRUE(service.listFailedOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Add)
          .empty());
  EXPECT_EQ(1u, service.inFlightByKernel(WorldGenerationComputeKernel::DenseUint32Add));
#else
  size_t failedXor =
      service.listFailedOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor).size();
  size_t readyXor =
      service.listReadyOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor).size();
  EXPECT_EQ(2u, failedXor + readyXor);
  EXPECT_EQ(1u, service.inFlightByKernel(WorldGenerationComputeKernel::DenseUint32Add));
#endif
}

TEST(WorldGenerationComputeTest, CancelledOpenTicketsByKernelCountsRevokedMatches) {
  WorldGenerationComputeService service;
  service.setDeferredCpuCompletion(true);
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(540, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{1u}, 0u));
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(541, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{2u}, 0u));
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(542, WorldGenerationComputeKernel::DenseUint32Add), List<uint32_t>{3u}, 0u));

  EXPECT_EQ(0u, service.cancelledOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_EQ(2u, service.cancelOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_EQ(2u, service.cancelledOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_EQ(0u, service.cancelledOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Add));
  EXPECT_EQ(0u, service.inFlightByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_EQ(1u, service.inFlightByKernel(WorldGenerationComputeKernel::DenseUint32Add));
}

TEST(WorldGenerationComputeTest, ListCancelledOpenTicketsByKernelReturnsCancelledIdsOnly) {
  WorldGenerationComputeService service;
  service.setDeferredCpuCompletion(true);
  auto xorA = service.submit(
      makeIdentity(640, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{1u}, 0u);
  auto xorB = service.submit(
      makeIdentity(641, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{2u}, 0u);
  auto add = service.submit(
      makeIdentity(642, WorldGenerationComputeKernel::DenseUint32Add), List<uint32_t>{3u}, 0u);
  ASSERT_TRUE((bool)xorA);
  ASSERT_TRUE((bool)xorB);
  ASSERT_TRUE((bool)add);

  EXPECT_TRUE(
      service.listCancelledOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor)
          .empty());
  EXPECT_EQ(2u, service.cancelOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  auto cancelledXor =
      service.listCancelledOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor);
  ASSERT_EQ(2u, cancelledXor.size());
  EXPECT_TRUE(cancelledXor[0] == *xorA || cancelledXor[0] == *xorB);
  EXPECT_TRUE(cancelledXor[1] == *xorA || cancelledXor[1] == *xorB);
  EXPECT_NE(cancelledXor[0].id, cancelledXor[1].id);
  EXPECT_TRUE(
      service.listCancelledOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Add)
          .empty());
  EXPECT_EQ(1u, service.inFlightByKernel(WorldGenerationComputeKernel::DenseUint32Add));
}

TEST(WorldGenerationComputeTest, ReadyOpenTicketsByKernelCountsCompletedMatches) {
  WorldGenerationComputeService service;
  service.setDeferredCpuCompletion(true);
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(520, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{1u}, 0u));
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(521, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{2u}, 0u));
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(522, WorldGenerationComputeKernel::DenseUint32Add), List<uint32_t>{3u}, 0u));

  EXPECT_EQ(0u, service.readyOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_EQ(2u, service.completeOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_EQ(2u, service.readyOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_EQ(0u, service.readyOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Add));
  EXPECT_EQ(0u, service.inFlightByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_EQ(1u, service.inFlightByKernel(WorldGenerationComputeKernel::DenseUint32Add));
}

TEST(WorldGenerationComputeTest, AnyReadyOpenTicketsByKernelTracksReadyPresence) {
  WorldGenerationComputeService service;
  service.setDeferredCpuCompletion(true);
  EXPECT_FALSE(service.anyReadyOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(700, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{1u}, 0u));
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(701, WorldGenerationComputeKernel::DenseUint32Add), List<uint32_t>{2u}, 0u));
  EXPECT_FALSE(service.anyReadyOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_EQ(1u, service.completeOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_TRUE(service.anyReadyOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_FALSE(service.anyReadyOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Add));
  EXPECT_TRUE(service.anyInFlightByKernel(WorldGenerationComputeKernel::DenseUint32Add));
}

TEST(WorldGenerationComputeTest, ListReadyOpenTicketsByKernelReturnsReadyIdsOnly) {
  WorldGenerationComputeService service;
  service.setDeferredCpuCompletion(true);
  auto xorA = service.submit(
      makeIdentity(600, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{1u}, 0u);
  auto xorB = service.submit(
      makeIdentity(601, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{2u}, 0u);
  auto add = service.submit(
      makeIdentity(602, WorldGenerationComputeKernel::DenseUint32Add), List<uint32_t>{3u}, 0u);
  ASSERT_TRUE((bool)xorA);
  ASSERT_TRUE((bool)xorB);
  ASSERT_TRUE((bool)add);

  EXPECT_TRUE(service.listReadyOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor)
          .empty());
  EXPECT_EQ(2u, service.completeOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  auto readyXor = service.listReadyOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor);
  ASSERT_EQ(2u, readyXor.size());
  EXPECT_TRUE(readyXor[0] == *xorA || readyXor[0] == *xorB);
  EXPECT_TRUE(readyXor[1] == *xorA || readyXor[1] == *xorB);
  EXPECT_NE(readyXor[0].id, readyXor[1].id);
  EXPECT_TRUE(service.listReadyOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Add)
          .empty());
  EXPECT_EQ(1u, service.inFlightByKernel(WorldGenerationComputeKernel::DenseUint32Add));
}

TEST(WorldGenerationComputeTest, InFlightByKernelCountsOnlyPendingMatches) {
  WorldGenerationComputeService service;
  service.setDeferredCpuCompletion(true);
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(500, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{1u}, 0u));
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(501, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{2u}, 0u));
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(502, WorldGenerationComputeKernel::DenseUint32Add), List<uint32_t>{3u}, 0u));

  EXPECT_EQ(2u, service.inFlightByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_EQ(1u, service.inFlightByKernel(WorldGenerationComputeKernel::DenseUint32Add));
  EXPECT_EQ(0u, service.inFlightByKernel(WorldGenerationComputeKernel::DenseUint32Mul));
  EXPECT_EQ(3u, service.inFlight());

  EXPECT_EQ(2u, service.completeOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_EQ(0u, service.inFlightByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_EQ(1u, service.inFlightByKernel(WorldGenerationComputeKernel::DenseUint32Add));
  EXPECT_EQ(1u, service.inFlight());
}

TEST(WorldGenerationComputeTest, ListInFlightByKernelReturnsPendingIdsOnly) {
  WorldGenerationComputeService service;
  service.setDeferredCpuCompletion(true);
  auto xorA = service.submit(
      makeIdentity(660, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{1u}, 0u);
  auto xorB = service.submit(
      makeIdentity(661, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{2u}, 0u);
  auto add = service.submit(
      makeIdentity(662, WorldGenerationComputeKernel::DenseUint32Add), List<uint32_t>{3u}, 0u);
  ASSERT_TRUE((bool)xorA);
  ASSERT_TRUE((bool)xorB);
  ASSERT_TRUE((bool)add);

  auto pendingXor = service.listInFlightByKernel(WorldGenerationComputeKernel::DenseUint32Xor);
  ASSERT_EQ(2u, pendingXor.size());
  EXPECT_TRUE(pendingXor[0] == *xorA || pendingXor[0] == *xorB);
  EXPECT_TRUE(pendingXor[1] == *xorA || pendingXor[1] == *xorB);
  EXPECT_NE(pendingXor[0].id, pendingXor[1].id);
  auto pendingAdd = service.listInFlightByKernel(WorldGenerationComputeKernel::DenseUint32Add);
  ASSERT_EQ(1u, pendingAdd.size());
  EXPECT_EQ(add->id, pendingAdd[0].id);
  EXPECT_TRUE(service.listInFlightByKernel(WorldGenerationComputeKernel::DenseUint32Mul).empty());

  EXPECT_EQ(2u, service.completeOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_TRUE(service.listInFlightByKernel(WorldGenerationComputeKernel::DenseUint32Xor).empty());
  EXPECT_EQ(1u, service.listInFlightByKernel(WorldGenerationComputeKernel::DenseUint32Add).size());
}

TEST(WorldGenerationComputeTest, AnyInFlightByKernelTracksPendingPresence) {
  WorldGenerationComputeService service;
  service.setDeferredCpuCompletion(true);
  EXPECT_FALSE(service.anyInFlightByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(680, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{1u}, 0u));
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(681, WorldGenerationComputeKernel::DenseUint32Add), List<uint32_t>{2u}, 0u));
  EXPECT_TRUE(service.anyInFlightByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_TRUE(service.anyInFlightByKernel(WorldGenerationComputeKernel::DenseUint32Add));
  EXPECT_FALSE(service.anyInFlightByKernel(WorldGenerationComputeKernel::DenseUint32Mul));
  EXPECT_EQ(1u, service.completeOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_FALSE(service.anyInFlightByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_TRUE(service.anyOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_TRUE(service.anyInFlightByKernel(WorldGenerationComputeKernel::DenseUint32Add));
}

TEST(WorldGenerationComputeTest, CompareShadowNamedResolvesKernelAndRejectsUnknown) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  WorldGenerationComputeInputIdentity id;
  id.worldEpoch = 7;
  id.sequence = 480;
  id.kernelVersion = 1;
  id.kernel = WorldGenerationComputeKernel::DenseUint32Add;

  auto ok = service.compareShadowNamed("DenseUint32Xor", id, input, 0x11u,
      WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal, ok.verdict);
  EXPECT_EQ(WorldGenerationComputeKernel::DenseUint32Xor, ok.identity.kernel);

  auto bad = service.compareShadowNamed("nope", id, input, 0u,
      WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::PrimaryFailed, bad.verdict);
  EXPECT_EQ(WorldGenerationComputeKernel::DenseUint32Add, bad.identity.kernel);

  auto nullName = service.compareShadowNamed(nullptr, id, input, 0u,
      WorldGenerationComputeBackend::CpuReference, WorldGenerationComputeBackend::CpuReference);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::PrimaryFailed, nullName.verdict);
}

TEST(WorldGenerationComputeTest, FingerprintShadowSummaryIsStableAndSensitive) {
  WorldGenerationComputeShadowSummary a;
  a.total = 4;
  a.equal = 3;
  a.diverged = 1;
  a.primaryFailed = 0;
  a.shadowUnavailable = 0;
  a.cancelled = 0;
  a.totalDivergenceCount = 7;
  auto first = WorldGenerationComputeService::fingerprintShadowSummary(a);
  EXPECT_EQ(first, WorldGenerationComputeService::fingerprintShadowSummary(a));

  WorldGenerationComputeShadowSummary b = a;
  b.diverged = 2;
  EXPECT_NE(first, WorldGenerationComputeService::fingerprintShadowSummary(b));
  b = a;
  b.totalDivergenceCount = 8;
  EXPECT_NE(first, WorldGenerationComputeService::fingerprintShadowSummary(b));
}

TEST(WorldGenerationComputeTest, CompleteOpenTicketsByKernelLeavesReadyUntilPoll) {
  WorldGenerationComputeService service;
  service.setDeferredCpuCompletion(true);
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(450, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{0x10u},
      0x01u));
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(451, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{0x20u},
      0x01u));
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(452, WorldGenerationComputeKernel::DenseUint32Add), List<uint32_t>{0x30u},
      0x01u));

  EXPECT_EQ(0u, service.completeOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Mul));
  EXPECT_EQ(2u, service.completeOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_EQ(0u, service.completeOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_EQ(2u, service.countOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_EQ(1u, service.inFlight()); // Add still Pending

  auto xorOpen = service.listOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor);
  List<WorldGenerationComputeResult> outs;
  auto statuses = service.pollBatch(xorOpen, &outs);
  ASSERT_EQ(2u, statuses.size());
  EXPECT_EQ(WorldGenerationComputePollStatus::Ready, statuses[0]);
  EXPECT_EQ(WorldGenerationComputePollStatus::Ready, statuses[1]);
  EXPECT_EQ(0u, service.countOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_EQ(1u, service.countOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Add));
}

TEST(WorldGenerationComputeTest, FingerprintStagingSnapshotIsStableAndSensitive) {
  WorldGenerationComputeStagingSnapshot a;
  a.stagingBytes = 16;
  a.peakStagingBytes = 32;
  a.maxStagingBytes = 64;
  a.pooledBufferBytes = 8;
  a.pooledBufferCount = 1;
  a.openTickets = 2;
  a.peakOpenTickets = 3;
  a.inFlight = 1;
  a.gpuInFlight = 0;
  a.estimatedVramBytes = 0;
  a.gpuCapable = false;
  auto first = WorldGenerationComputeService::fingerprintStagingSnapshot(a);
  EXPECT_EQ(first, WorldGenerationComputeService::fingerprintStagingSnapshot(a));

  WorldGenerationComputeStagingSnapshot b = a;
  b.openTickets = 4;
  EXPECT_NE(first, WorldGenerationComputeService::fingerprintStagingSnapshot(b));
  b = a;
  b.gpuCapable = true;
  EXPECT_NE(first, WorldGenerationComputeService::fingerprintStagingSnapshot(b));
}

TEST(WorldGenerationComputeTest, SubmitNamedResolvesKernelAndRejectsUnknown) {
  WorldGenerationComputeService service;
  service.setDeferredCpuCompletion(true);
  WorldGenerationComputeInputIdentity id;
  id.worldEpoch = 7;
  id.sequence = 420;
  id.kernelVersion = 1;
  id.kernel = WorldGenerationComputeKernel::DenseUint32Add; // overwritten by name

  auto ticket = service.submitNamed("DenseUint32Xor", id, List<uint32_t>{0x10u}, 0x01u);
  ASSERT_TRUE((bool)ticket);
  EXPECT_EQ(1u, service.countOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_EQ(0u, service.countOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Add));

  WorldGenerationComputeResult result;
  EXPECT_TRUE(service.complete(*ticket));
  EXPECT_EQ(WorldGenerationComputePollStatus::Ready, service.poll(*ticket, &result));
  ASSERT_EQ(1u, result.values.size());
  EXPECT_EQ(0x11u, result.values[0]);

  EXPECT_FALSE((bool)service.submitNamed("nope", id, List<uint32_t>{1u}, 0u));
  EXPECT_FALSE((bool)service.submitNamed(nullptr, id, List<uint32_t>{1u}, 0u));
}

TEST(WorldGenerationComputeTest, DrainOpenTicketsByKernelIsSelective) {
  WorldGenerationComputeService service;
  service.setDeferredCpuCompletion(true);
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(400, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{1u}, 0u));
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(401, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{2u}, 0u));
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(402, WorldGenerationComputeKernel::DenseUint32Add), List<uint32_t>{3u}, 0u));

  EXPECT_EQ(0u, service.drainOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Mul));
  EXPECT_EQ(2u, service.drainOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_EQ(0u, service.countOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_EQ(1u, service.countOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Add));
  EXPECT_EQ(1u, service.drainOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Add));
  EXPECT_EQ(0u, service.openTickets());
}

TEST(WorldGenerationComputeTest, CancelOpenTicketsByKernelIsSelective) {
  WorldGenerationComputeService service;
  service.setDeferredCpuCompletion(true);
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(380, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{1u}, 0u));
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(381, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{2u}, 0u));
  ASSERT_TRUE((bool)service.submit(
      makeIdentity(382, WorldGenerationComputeKernel::DenseUint32Add), List<uint32_t>{3u}, 0u));

  EXPECT_EQ(0u, service.cancelOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Mul));
  EXPECT_EQ(2u, service.cancelOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_EQ(0u, service.cancelOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_EQ(2u, service.countOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_EQ(1u, service.countOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Add));

  auto xorOpen = service.listOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor);
  auto xorStatuses = service.pollBatch(xorOpen);
  ASSERT_EQ(2u, xorStatuses.size());
  EXPECT_EQ(WorldGenerationComputePollStatus::Cancelled, xorStatuses[0]);
  EXPECT_EQ(WorldGenerationComputePollStatus::Cancelled, xorStatuses[1]);
  EXPECT_EQ(0u, service.countOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_EQ(1u, service.countOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Add));
}

TEST(WorldGenerationComputeTest, ListAndCountOpenTicketsByKernel) {
  WorldGenerationComputeService service;
  service.setDeferredCpuCompletion(true);
  auto xorTicket = service.submit(
      makeIdentity(360, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{1u}, 1u);
  auto addTicket = service.submit(
      makeIdentity(361, WorldGenerationComputeKernel::DenseUint32Add), List<uint32_t>{2u}, 1u);
  auto xorTicket2 = service.submit(
      makeIdentity(362, WorldGenerationComputeKernel::DenseUint32Xor), List<uint32_t>{3u}, 1u);
  ASSERT_TRUE((bool)xorTicket);
  ASSERT_TRUE((bool)addTicket);
  ASSERT_TRUE((bool)xorTicket2);

  EXPECT_EQ(2u, service.countOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  EXPECT_EQ(1u, service.countOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Add));
  EXPECT_EQ(0u, service.countOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Mul));

  auto xorOpen =
      service.listOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor);
  ASSERT_EQ(2u, xorOpen.size());
  bool sawXor0 = false;
  bool sawXor2 = false;
  for (auto const& ticket : xorOpen) {
    if (ticket.id == xorTicket->id)
      sawXor0 = true;
    if (ticket.id == xorTicket2->id)
      sawXor2 = true;
  }
  EXPECT_TRUE(sawXor0);
  EXPECT_TRUE(sawXor2);

  auto addOpen =
      service.listOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Add);
  ASSERT_EQ(1u, addOpen.size());
  EXPECT_EQ(addTicket->id, addOpen[0].id);

  EXPECT_TRUE(service.cancel(*xorTicket));
  // Cancel marks the entry; poll is what retires the open-ticket id.
  EXPECT_EQ(2u, service.countOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
  service.poll(*xorTicket);
  EXPECT_EQ(1u, service.countOpenTicketsByKernel(WorldGenerationComputeKernel::DenseUint32Xor));
}

TEST(WorldGenerationComputeTest, RunNamedKernelAndIsBackendSupported) {
  auto named = WorldGenerationComputeService::runNamedKernel("DenseUint32Xor", 1u,
      List<uint32_t>{0x10u}, 0x01u);
  ASSERT_EQ(1u, named.size());
  EXPECT_EQ(0x11u, named[0]);
  EXPECT_TRUE(WorldGenerationComputeService::runNamedKernel("nope", 1u, List<uint32_t>{1u}, 0u)
          .empty());
  EXPECT_TRUE(WorldGenerationComputeService::runNamedKernel("DenseUint32Xor", 99u,
          List<uint32_t>{1u}, 0u)
          .empty());
  EXPECT_TRUE(WorldGenerationComputeService::runNamedKernel(nullptr, 1u, List<uint32_t>{1u}, 0u)
          .empty());

  WorldGenerationComputeCapabilities caps;
  caps.cpuReference = true;
  caps.gpuOpenGlCompute = false;
  EXPECT_TRUE(WorldGenerationComputeService::isBackendSupported(
      WorldGenerationComputeBackend::CpuReference, caps));
  EXPECT_FALSE(WorldGenerationComputeService::isBackendSupported(
      WorldGenerationComputeBackend::GpuOpenGlCompute, caps));
  caps.gpuOpenGlCompute = true;
  EXPECT_TRUE(WorldGenerationComputeService::isBackendSupported(
      WorldGenerationComputeBackend::GpuOpenGlCompute, caps));
  caps.cpuReference = false;
  EXPECT_FALSE(WorldGenerationComputeService::isBackendSupported(
      WorldGenerationComputeBackend::CpuReference, caps));
}

TEST(WorldGenerationComputeTest, DenseUint32BswapParityBitTestExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  EXPECT_EQ(0x04030201u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32Bswap, 1u, List<uint32_t>{0x01020304u}, 0u)[0]);
  EXPECT_EQ(0u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32Parity, 1u, List<uint32_t>{0x3u}, 0u)[0]);
  EXPECT_EQ(1u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32Parity, 1u, List<uint32_t>{0x7u}, 0u)[0]);
  EXPECT_EQ(0xffffffffu,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32BitTest, 1u, List<uint32_t>{0x2u}, 1u)[0]);
  EXPECT_EQ(0u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32BitTest, 1u, List<uint32_t>{0x2u}, 0u)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(makeIdentity(340, WorldGenerationComputeKernel::DenseUint32Bswap),
          input, 0u, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(makeIdentity(341, WorldGenerationComputeKernel::DenseUint32Parity),
          input, 0u, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(makeIdentity(342, WorldGenerationComputeKernel::DenseUint32BitTest),
          input, 1u, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
}

TEST(WorldGenerationComputeTest, DrainOpenTicketsAndPeakOpenTickets) {
  WorldGenerationComputeCapabilities caps;
  caps.maxInFlight = 4;
  WorldGenerationComputeService service(caps);
  service.setDeferredCpuCompletion(true);
  EXPECT_EQ(0u, service.drainOpenTickets());
  EXPECT_EQ(0u, service.stagingSnapshot().peakOpenTickets);
  ASSERT_TRUE((bool)service.submit(makeIdentity(330), List<uint32_t>{1u}, 0u));
  ASSERT_TRUE((bool)service.submit(makeIdentity(331), List<uint32_t>{2u}, 0u));
  EXPECT_EQ(2u, service.openTickets());
  EXPECT_EQ(2u, service.stagingSnapshot().peakOpenTickets);
  EXPECT_EQ(2u, service.drainOpenTickets());
  EXPECT_EQ(0u, service.openTickets());
  EXPECT_EQ(2u, service.stagingSnapshot().peakOpenTickets);
  service.resetPeakOpenTickets();
  EXPECT_EQ(0u, service.stagingSnapshot().peakOpenTickets);
  ASSERT_TRUE((bool)service.submit(makeIdentity(332), List<uint32_t>{3u}, 0u));
  EXPECT_EQ(1u, service.stagingSnapshot().peakOpenTickets);
  EXPECT_EQ(1u, service.drainOpenTickets());
}

TEST(WorldGenerationComputeTest, DenseUint32OrNotAvgIsPow2ExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  EXPECT_EQ(0xfffffff5u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32OrNot, 1u, List<uint32_t>{0x5u}, 0xau)[0]);
  EXPECT_EQ(3u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32Avg, 1u, List<uint32_t>{2u}, 5u)[0]);
  EXPECT_EQ(0xffffffffu,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32Avg, 1u, List<uint32_t>{0xffffffffu},
          0xffffffffu)[0]);
  EXPECT_EQ(0xffffffffu,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32IsPow2, 1u, List<uint32_t>{8u}, 0u)[0]);
  EXPECT_EQ(0u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32IsPow2, 1u, List<uint32_t>{0u}, 0u)[0]);
  EXPECT_EQ(0u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32IsPow2, 1u, List<uint32_t>{6u}, 0u)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(makeIdentity(320, WorldGenerationComputeKernel::DenseUint32OrNot),
          input, 0x0f0f0f0fu, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(makeIdentity(321, WorldGenerationComputeKernel::DenseUint32Avg), input,
          3u, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(makeIdentity(322, WorldGenerationComputeKernel::DenseUint32IsPow2),
          input, 0u, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
}

TEST(WorldGenerationComputeTest, FingerprintCapabilitiesAndSupportedKernelCount) {
  EXPECT_EQ(328u, WorldGenerationComputeService::supportedKernelCount());
  EXPECT_EQ(0u, WorldGenerationComputeService::supportedKernelCount(2u));
  EXPECT_EQ(WorldGenerationComputeService::supportedKernels().size(),
      WorldGenerationComputeService::supportedKernelCount());

  WorldGenerationComputeCapabilities a;
  a.cpuReference = true;
  a.gpuOpenGlCompute = false;
  a.maxWorkGroupInvocations = 64;
  a.maxInFlight = 8;
  a.maxImmutableBufferBytes = 16ull * 1024ull * 1024ull;
  a.maxStagingBytes = 32ull * 1024ull * 1024ull;
  WorldGenerationComputeCapabilities b = a;
  EXPECT_EQ(WorldGenerationComputeService::fingerprintCapabilities(a),
      WorldGenerationComputeService::fingerprintCapabilities(b));
  b.gpuOpenGlCompute = true;
  EXPECT_NE(WorldGenerationComputeService::fingerprintCapabilities(a),
      WorldGenerationComputeService::fingerprintCapabilities(b));
  b = a;
  b.maxInFlight = 16;
  EXPECT_NE(WorldGenerationComputeService::fingerprintCapabilities(a),
      WorldGenerationComputeService::fingerprintCapabilities(b));
}

TEST(WorldGenerationComputeTest, DenseUint32AndNotSatAddSatSubExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  uint32_t const mask = 0x0f0f0f0fu;
  EXPECT_EQ(0x50u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32AndNot, 1u, List<uint32_t>{0x55u}, mask)[0]);
  EXPECT_EQ(0xffffffffu,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32SatAdd, 1u, List<uint32_t>{0xfffffffeu}, 2u)[0]);
  EXPECT_EQ(5u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32SatAdd, 1u, List<uint32_t>{2u}, 3u)[0]);
  EXPECT_EQ(0u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32SatSub, 1u, List<uint32_t>{2u}, 5u)[0]);
  EXPECT_EQ(3u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32SatSub, 1u, List<uint32_t>{5u}, 2u)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(makeIdentity(310, WorldGenerationComputeKernel::DenseUint32AndNot),
          input, mask, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(makeIdentity(311, WorldGenerationComputeKernel::DenseUint32SatAdd),
          input, 0xffffffffu, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(makeIdentity(312, WorldGenerationComputeKernel::DenseUint32SatSub),
          input, 1u, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
}

TEST(WorldGenerationComputeTest, TicketIsOpenAndClearRejectedDuplicates) {
  WorldGenerationComputeService service;
  service.setDeferredCpuCompletion(true);
  auto ticket = service.submit(makeIdentity(300), List<uint32_t>{7u}, 1u);
  ASSERT_TRUE((bool)ticket);
  EXPECT_TRUE(service.ticketIsOpen(*ticket));
  EXPECT_EQ(0u, service.rejectedDuplicateCount());
  EXPECT_EQ(1u, service.failContext());
  WorldGenerationComputeResult result;
  EXPECT_EQ(WorldGenerationComputePollStatus::Ready, service.poll(*ticket, &result));
  EXPECT_FALSE(service.ticketIsOpen(*ticket));
  EXPECT_EQ(1u, service.rejectedDuplicateCount());
  EXPECT_EQ(WorldGenerationComputePollStatus::UnknownTicket, service.poll(*ticket, &result));
  EXPECT_EQ(1u, service.clearRejectedDuplicates());
  EXPECT_EQ(0u, service.rejectedDuplicateCount());
  EXPECT_EQ(0u, service.clearRejectedDuplicates());
  // After clearing, the id is simply unknown again (not a tracked reject).
  EXPECT_EQ(WorldGenerationComputePollStatus::UnknownTicket, service.poll(*ticket, &result));
}

TEST(WorldGenerationComputeTest, DenseUint32NandNorMulHiExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  uint32_t const mask = 0x0f0f0f0fu;
  EXPECT_EQ(~(0x55u & mask),
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32Nand, 1u, List<uint32_t>{0x55u}, mask)[0]);
  EXPECT_EQ(~(0x55u | mask),
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32Nor, 1u, List<uint32_t>{0x55u}, mask)[0]);
  EXPECT_EQ((uint32_t)(((uint64_t)0xffffffffu * 0x10001ull) >> 32),
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32MulHi, 1u, List<uint32_t>{0xffffffffu},
          0x10001u)[0]);
  EXPECT_EQ(0u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32MulHi, 1u, List<uint32_t>{2u}, 3u)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(makeIdentity(290, WorldGenerationComputeKernel::DenseUint32Nand), input,
          mask, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(makeIdentity(291, WorldGenerationComputeKernel::DenseUint32Nor), input,
          mask, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(makeIdentity(292, WorldGenerationComputeKernel::DenseUint32MulHi), input,
          0x10001u, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
}

TEST(WorldGenerationComputeTest, CancelAllAndVerdictNames) {
  EXPECT_STREQ("Pending",
      WorldGenerationComputeService::pollStatusName(WorldGenerationComputePollStatus::Pending));
  EXPECT_STREQ("Equal",
      WorldGenerationComputeService::shadowVerdictName(WorldGenerationComputeShadowVerdict::Equal));
  EXPECT_STREQ("Accepted",
      WorldGenerationComputeService::commitVerdictName(WorldGenerationComputeCommitVerdict::Accepted));
  EXPECT_EQ(nullptr,
      WorldGenerationComputeService::pollStatusName((WorldGenerationComputePollStatus)99));

  WorldGenerationComputeCapabilities caps;
  caps.maxInFlight = 4;
  WorldGenerationComputeService service(caps);
  service.setDeferredCpuCompletion(true);
  EXPECT_EQ(0u, service.cancelAll());
  ASSERT_TRUE((bool)service.submit(makeIdentity(280), List<uint32_t>{1u}, 0u));
  ASSERT_TRUE((bool)service.submit(makeIdentity(281), List<uint32_t>{2u}, 0u));
  EXPECT_EQ(2u, service.openTickets());
  EXPECT_EQ(2u, service.cancelAll());
  EXPECT_EQ(2u, service.openTickets());
  auto open = service.listOpenTickets();
  auto statuses = service.pollBatch(open);
  ASSERT_EQ(2u, statuses.size());
  EXPECT_EQ(WorldGenerationComputePollStatus::Cancelled, statuses[0]);
  EXPECT_EQ(WorldGenerationComputePollStatus::Cancelled, statuses[1]);
  EXPECT_EQ(0u, service.openTickets());
  EXPECT_EQ(0u, service.cancelAll());
}

TEST(WorldGenerationComputeTest, DenseUint32GtGeuXnorExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  uint32_t const bound = 0x80000000u;
  EXPECT_EQ(0xffffffffu,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32Gt, 1u, List<uint32_t>{bound + 1u}, bound)[0]);
  EXPECT_EQ(0u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32Gt, 1u, List<uint32_t>{bound}, bound)[0]);
  EXPECT_EQ(0xffffffffu,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32Geu, 1u, List<uint32_t>{bound}, bound)[0]);
  EXPECT_EQ(0u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32Geu, 1u, List<uint32_t>{bound - 1u}, bound)[0]);
  EXPECT_EQ(~(0x55u ^ 0x11u),
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32Xnor, 1u, List<uint32_t>{0x55u}, 0x11u)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(makeIdentity(270, WorldGenerationComputeKernel::DenseUint32Gt), input,
          bound, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(makeIdentity(271, WorldGenerationComputeKernel::DenseUint32Geu), input,
          bound, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(makeIdentity(272, WorldGenerationComputeKernel::DenseUint32Xnor), input,
          0x11u, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
}

TEST(WorldGenerationComputeTest, BackendNameAndSummarizeShadowReports) {
  EXPECT_STREQ("CpuReference",
      WorldGenerationComputeService::backendName(WorldGenerationComputeBackend::CpuReference));
  EXPECT_STREQ("GpuOpenGlCompute",
      WorldGenerationComputeService::backendName(WorldGenerationComputeBackend::GpuOpenGlCompute));
  auto cpu = WorldGenerationComputeService::parseBackendName("CpuReference");
  ASSERT_TRUE(cpu.isValid());
  EXPECT_EQ(WorldGenerationComputeBackend::CpuReference, *cpu);
  auto gpu = WorldGenerationComputeService::parseBackendName("GpuOpenGlCompute");
  ASSERT_TRUE(gpu.isValid());
  EXPECT_EQ(WorldGenerationComputeBackend::GpuOpenGlCompute, *gpu);
  EXPECT_FALSE(WorldGenerationComputeService::parseBackendName("nope").isValid());
  EXPECT_FALSE(WorldGenerationComputeService::parseBackendName(nullptr).isValid());

  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  List<WorldGenerationComputeService::ShadowBatchItem> items;
  WorldGenerationComputeService::ShadowBatchItem ok;
  ok.identity = makeIdentity(260, WorldGenerationComputeKernel::DenseUint32Xor);
  ok.parameter = 1u;
  items.append(ok);
  WorldGenerationComputeService::ShadowBatchItem alsoOk;
  alsoOk.identity = makeIdentity(261, WorldGenerationComputeKernel::DenseUint32Add);
  alsoOk.parameter = 1u;
  items.append(alsoOk);
  auto reports = service.compareShadowBatch(input, items);
  auto summary = WorldGenerationComputeService::summarizeShadowReports(reports);
  EXPECT_EQ(2u, summary.total);
  EXPECT_EQ(2u, summary.equal);
  EXPECT_EQ(0u, summary.diverged);
  EXPECT_EQ(0u, summary.totalDivergenceCount);

  WorldGenerationComputeShadowReport diverged;
  diverged.verdict = WorldGenerationComputeShadowVerdict::Diverged;
  diverged.divergenceCount = 3;
  WorldGenerationComputeShadowReport unavailable;
  unavailable.verdict = WorldGenerationComputeShadowVerdict::ShadowUnavailable;
  List<WorldGenerationComputeShadowReport> mixed;
  mixed.append(diverged);
  mixed.append(unavailable);
  mixed.append(reports[0]);
  summary = WorldGenerationComputeService::summarizeShadowReports(mixed);
  EXPECT_EQ(3u, summary.total);
  EXPECT_EQ(1u, summary.equal);
  EXPECT_EQ(1u, summary.diverged);
  EXPECT_EQ(1u, summary.shadowUnavailable);
  EXPECT_EQ(3u, summary.totalDivergenceCount);
}

TEST(WorldGenerationComputeTest, DenseUint32LtLeuAbsDiffExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  uint32_t const bound = 0x80000000u;
  EXPECT_EQ(0xffffffffu,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32Lt, 1u, List<uint32_t>{1u}, bound)[0]);
  EXPECT_EQ(0u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32Lt, 1u, List<uint32_t>{bound}, bound)[0]);
  EXPECT_EQ(0xffffffffu,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32Leu, 1u, List<uint32_t>{bound}, bound)[0]);
  EXPECT_EQ(0u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32Leu, 1u, List<uint32_t>{bound + 1u}, bound)[0]);
  EXPECT_EQ(3u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32AbsDiff, 1u, List<uint32_t>{5u}, 2u)[0]);
  EXPECT_EQ(3u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32AbsDiff, 1u, List<uint32_t>{2u}, 5u)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(makeIdentity(250, WorldGenerationComputeKernel::DenseUint32Lt), input,
          bound, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(makeIdentity(251, WorldGenerationComputeKernel::DenseUint32Leu), input,
          bound, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(makeIdentity(252, WorldGenerationComputeKernel::DenseUint32AbsDiff),
          input, 1u, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
}

TEST(WorldGenerationComputeTest, FingerprintIdentityAndListOpenTickets) {
  auto a = makeIdentity(240, WorldGenerationComputeKernel::DenseUint32Eq);
  auto b = makeIdentity(241, WorldGenerationComputeKernel::DenseUint32Eq);
  EXPECT_NE(WorldGenerationComputeService::fingerprintIdentity(a),
      WorldGenerationComputeService::fingerprintIdentity(b));
  EXPECT_EQ(WorldGenerationComputeService::fingerprintIdentity(a),
      WorldGenerationComputeService::fingerprintIdentity(a));
  auto flipped = a;
  flipped.kernel = WorldGenerationComputeKernel::DenseUint32Ne;
  EXPECT_NE(WorldGenerationComputeService::fingerprintIdentity(a),
      WorldGenerationComputeService::fingerprintIdentity(flipped));

  WorldGenerationComputeCapabilities caps;
  caps.maxInFlight = 4;
  WorldGenerationComputeService service(caps);
  service.setDeferredCpuCompletion(true);
  EXPECT_TRUE(service.listOpenTickets().empty());
  auto t0 = service.submit(makeIdentity(242), List<uint32_t>{1u}, 0u);
  auto t1 = service.submit(makeIdentity(243), List<uint32_t>{2u}, 0u);
  ASSERT_TRUE((bool)t0);
  ASSERT_TRUE((bool)t1);
  auto open = service.listOpenTickets();
  ASSERT_EQ(2u, open.size());
  bool saw0 = false;
  bool saw1 = false;
  for (auto ticket : open) {
    if (ticket.id == t0->id)
      saw0 = true;
    if (ticket.id == t1->id)
      saw1 = true;
  }
  EXPECT_TRUE(saw0);
  EXPECT_TRUE(saw1);
  EXPECT_TRUE(service.cancel(*t0));
  EXPECT_EQ(WorldGenerationComputePollStatus::Cancelled, service.poll(*t0));
  open = service.listOpenTickets();
  ASSERT_EQ(1u, open.size());
  EXPECT_EQ(t1->id, open[0].id);
}

TEST(WorldGenerationComputeTest, DenseUint32EqNeBitReverseExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  uint32_t const needle = 0x55aa55aau;
  EXPECT_EQ(0xffffffffu,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32Eq, 1u, List<uint32_t>{needle}, needle)[0]);
  EXPECT_EQ(0u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32Eq, 1u, List<uint32_t>{needle}, needle + 1u)[0]);
  EXPECT_EQ(0u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32Ne, 1u, List<uint32_t>{needle}, needle)[0]);
  EXPECT_EQ(0xffffffffu,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32Ne, 1u, List<uint32_t>{needle}, needle + 1u)[0]);
  EXPECT_EQ(1u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32BitReverse, 1u, List<uint32_t>{0x80000000u},
          0u)[0]);
  EXPECT_EQ(0x80000000u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32BitReverse, 1u, List<uint32_t>{1u}, 0u)[0]);
  auto twice = WorldGenerationComputeService::runKernel(
      WorldGenerationComputeKernel::DenseUint32BitReverse, 1u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32BitReverse, 1u, input, 0u),
      0u);
  ASSERT_EQ(input.size(), twice.size());
  for (size_t i = 0; i < input.size(); ++i)
    EXPECT_EQ(input[i], twice[i]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(makeIdentity(230, WorldGenerationComputeKernel::DenseUint32Eq), input,
          needle, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(makeIdentity(231, WorldGenerationComputeKernel::DenseUint32Ne), input,
          needle, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(makeIdentity(232, WorldGenerationComputeKernel::DenseUint32BitReverse),
          input, 0u, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
}

TEST(WorldGenerationComputeTest, DenseUint32ClzCtzPopcountExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  EXPECT_EQ(32u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32Clz, 1u, List<uint32_t>{0u}, 0u)[0]);
  EXPECT_EQ(32u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32Ctz, 1u, List<uint32_t>{0u}, 0u)[0]);
  EXPECT_EQ(0u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32Popcount, 1u, List<uint32_t>{0u}, 0u)[0]);
  EXPECT_EQ(0u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32Clz, 1u, List<uint32_t>{0x80000000u}, 0u)[0]);
  EXPECT_EQ(31u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32Ctz, 1u, List<uint32_t>{0x80000000u}, 0u)[0]);
  EXPECT_EQ(1u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32Popcount, 1u, List<uint32_t>{0x80000000u}, 0u)[0]);
  EXPECT_EQ(31u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32Clz, 1u, List<uint32_t>{1u}, 0u)[0]);
  EXPECT_EQ(0u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32Ctz, 1u, List<uint32_t>{1u}, 0u)[0]);
  EXPECT_EQ(32u,
      WorldGenerationComputeService::runKernel(
          WorldGenerationComputeKernel::DenseUint32Popcount, 1u, List<uint32_t>{0xffffffffu}, 0u)[0]);

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(makeIdentity(210, WorldGenerationComputeKernel::DenseUint32Clz), input,
          0u, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(makeIdentity(211, WorldGenerationComputeKernel::DenseUint32Ctz), input,
          0u, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(makeIdentity(212, WorldGenerationComputeKernel::DenseUint32Popcount),
          input, 0u, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
}

TEST(WorldGenerationComputeTest, KernelNameRoundTripAndCompleteBatch) {
  for (auto kernel : WorldGenerationComputeService::supportedKernels()) {
    char const* name = WorldGenerationComputeService::kernelName(kernel);
    ASSERT_TRUE(name != nullptr);
    auto parsed = WorldGenerationComputeService::parseKernelName(name);
    ASSERT_TRUE(parsed.isValid());
    EXPECT_EQ(kernel, *parsed);
  }
  EXPECT_EQ(nullptr,
      WorldGenerationComputeService::kernelName((WorldGenerationComputeKernel)0));
  EXPECT_FALSE(WorldGenerationComputeService::parseKernelName("nope").isValid());
  EXPECT_FALSE(WorldGenerationComputeService::parseKernelName(nullptr).isValid());

  WorldGenerationComputeCapabilities caps;
  caps.maxInFlight = 4;
  WorldGenerationComputeService service(caps);
  service.setDeferredCpuCompletion(true);
  List<WorldGenerationComputeTicket> tickets;
  for (uint64_t i = 0; i < 2; ++i) {
    auto ticket = service.submit(makeIdentity(220 + i), List<uint32_t>{(uint32_t)(i + 1)}, 0x11u);
    ASSERT_TRUE((bool)ticket);
    tickets.append(*ticket);
  }
  auto done = service.completeBatch(List<WorldGenerationComputeTicket>{tickets[0],
      WorldGenerationComputeTicket{424242}, tickets[1]});
  ASSERT_EQ(3u, done.size());
  EXPECT_TRUE(done[0]);
  EXPECT_FALSE(done[1]);
  EXPECT_TRUE(done[2]);
  List<WorldGenerationComputeResult> outs;
  auto statuses = service.pollBatch(tickets, &outs);
  ASSERT_EQ(2u, statuses.size());
  EXPECT_EQ(WorldGenerationComputePollStatus::Ready, statuses[0]);
  EXPECT_EQ(WorldGenerationComputePollStatus::Ready, statuses[1]);
  EXPECT_EQ(1u ^ 0x11u, outs[0].values[0]);
  EXPECT_EQ(2u ^ 0x11u, outs[1].values[0]);
}

TEST(WorldGenerationComputeTest, CancelAndPollBatchAreIndependent) {
  WorldGenerationComputeCapabilities caps;
  caps.maxInFlight = 4;
  WorldGenerationComputeService service(caps);
  service.setDeferredCpuCompletion(true);

  List<WorldGenerationComputeTicket> tickets;
  for (uint64_t i = 0; i < 3; ++i) {
    auto ticket = service.submit(makeIdentity(200 + i), List<uint32_t>{(uint32_t)i}, 1u);
    ASSERT_TRUE((bool)ticket);
    tickets.append(*ticket);
  }

  auto cancels = service.cancelBatch(List<WorldGenerationComputeTicket>{tickets[0], tickets[1],
      WorldGenerationComputeTicket{999999}});
  ASSERT_EQ(3u, cancels.size());
  EXPECT_TRUE(cancels[0]);
  EXPECT_TRUE(cancels[1]);
  EXPECT_FALSE(cancels[2]);

  List<WorldGenerationComputeResult> outs;
  auto statuses = service.pollBatch(tickets, &outs);
  ASSERT_EQ(3u, statuses.size());
  ASSERT_EQ(3u, outs.size());
  EXPECT_EQ(WorldGenerationComputePollStatus::Cancelled, statuses[0]);
  EXPECT_EQ(WorldGenerationComputePollStatus::Cancelled, statuses[1]);
  EXPECT_EQ(WorldGenerationComputePollStatus::Pending, statuses[2]);

  EXPECT_EQ(1u, service.pump());
  statuses = service.pollBatch(List<WorldGenerationComputeTicket>{tickets[2]}, &outs);
  ASSERT_EQ(1u, statuses.size());
  EXPECT_EQ(WorldGenerationComputePollStatus::Ready, statuses[0]);
  ASSERT_EQ(1u, outs.size());
  ASSERT_EQ(1u, outs[0].values.size());
  EXPECT_EQ(2u ^ 1u, outs[0].values[0]);
}

TEST(WorldGenerationComputeTest, DenseUint32RotlRotrNotExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  uint32_t const rot = 5u;
  for (size_t i = 0; i < input.size(); ++i) {
    uint32_t v = input[i];
    EXPECT_EQ((v << rot) | (v >> ((32u - rot) & 31u)),
        WorldGenerationComputeService::runKernel(
            WorldGenerationComputeKernel::DenseUint32Rotl, 1u, List<uint32_t>{v}, rot)[0]);
    EXPECT_EQ((v >> rot) | (v << ((32u - rot) & 31u)),
        WorldGenerationComputeService::runKernel(
            WorldGenerationComputeKernel::DenseUint32Rotr, 1u, List<uint32_t>{v}, rot)[0]);
    EXPECT_EQ(~v,
        WorldGenerationComputeService::runKernel(
            WorldGenerationComputeKernel::DenseUint32Not, 1u, List<uint32_t>{v}, 0u)[0]);
  }
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(makeIdentity(190, WorldGenerationComputeKernel::DenseUint32Rotl), input,
          rot, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(makeIdentity(191, WorldGenerationComputeKernel::DenseUint32Rotr), input,
          37u /* & 31 == 5 */, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(makeIdentity(192, WorldGenerationComputeKernel::DenseUint32Not), input,
          0u, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
  // Round-trip: rotl then rotr by the same amount restores the corpus element.
  auto rotated = WorldGenerationComputeService::runKernel(
      WorldGenerationComputeKernel::DenseUint32Rotl, 1u, input, rot);
  auto restored = WorldGenerationComputeService::runKernel(
      WorldGenerationComputeKernel::DenseUint32Rotr, 1u, rotated, rot);
  ASSERT_EQ(input.size(), restored.size());
  for (size_t i = 0; i < input.size(); ++i)
    EXPECT_EQ(input[i], restored[i]);
}

TEST(WorldGenerationComputeTest, DenseUint32ShlShrExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  uint32_t const shift = 3u;
  for (size_t i = 0; i < input.size(); ++i) {
    EXPECT_EQ(input[i] << (shift & 31u),
        WorldGenerationComputeService::runKernel(
            WorldGenerationComputeKernel::DenseUint32Shl, 1u, List<uint32_t>{input[i]}, shift)[0]);
    EXPECT_EQ(input[i] >> (shift & 31u),
        WorldGenerationComputeService::runKernel(
            WorldGenerationComputeKernel::DenseUint32Shr, 1u, List<uint32_t>{input[i]}, shift)[0]);
  }
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(makeIdentity(170, WorldGenerationComputeKernel::DenseUint32Shl), input,
          shift, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(makeIdentity(171, WorldGenerationComputeKernel::DenseUint32Shr), input,
          35u /* & 31 == 3 */, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
}

TEST(WorldGenerationComputeTest, DenseUint32MinMaxSubExactCpuAndShadow) {
  WorldGenerationComputeService service;
  auto input = edgeCorpus();
  uint32_t const bound = 0x80000000u;

  for (size_t i = 0; i < input.size(); ++i) {
    EXPECT_EQ(input[i] < bound ? input[i] : bound,
        WorldGenerationComputeService::runKernel(
            WorldGenerationComputeKernel::DenseUint32Min, 1u, List<uint32_t>{input[i]}, bound)[0]);
    EXPECT_EQ(input[i] > bound ? input[i] : bound,
        WorldGenerationComputeService::runKernel(
            WorldGenerationComputeKernel::DenseUint32Max, 1u, List<uint32_t>{input[i]}, bound)[0]);
    EXPECT_EQ(input[i] - 1u,
        WorldGenerationComputeService::runKernel(
            WorldGenerationComputeKernel::DenseUint32Sub, 1u, List<uint32_t>{input[i]}, 1u)[0]);
  }

  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(makeIdentity(140, WorldGenerationComputeKernel::DenseUint32Min), input,
          bound, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(makeIdentity(141, WorldGenerationComputeKernel::DenseUint32Max), input,
          bound, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal,
      service.compareShadow(makeIdentity(142, WorldGenerationComputeKernel::DenseUint32Sub), input,
          1u, WorldGenerationComputeBackend::CpuReference,
          WorldGenerationComputeBackend::CpuReference)
          .verdict);
}

TEST(WorldGenerationComputeTest, OwnerThreadRejectsForeignMutations) {
  // GPU-02: submission/dispatch stay on the adopted owner thread (asset-free).
  WorldGenerationComputeService service;
  EXPECT_FALSE(service.hasOwnerThread());
  service.adoptOwnerThread();
  EXPECT_TRUE(service.hasOwnerThread());
  EXPECT_TRUE(service.isOwnerThread());

  auto ticket = service.submit(makeIdentity(130), sampleInput(), 1u);
  ASSERT_TRUE(ticket);
  EXPECT_EQ(1u, service.openTickets()); // observation OK from owner

  std::atomic<bool> foreignSubmitOk{true};
  std::atomic<bool> foreignPollUnknown{false};
  std::atomic<bool> foreignCancelOk{true};
  std::atomic<size_t> foreignFail{1};
  std::thread foreign([&] {
    foreignSubmitOk = (bool)service.submit(makeIdentity(131), sampleInput(), 2u);
    WorldGenerationComputeResult result;
    foreignPollUnknown =
        service.poll(*ticket, &result) == WorldGenerationComputePollStatus::UnknownTicket;
    foreignCancelOk = service.cancel(*ticket);
    foreignFail = service.failContext();
  });
  foreign.join();

  EXPECT_FALSE(foreignSubmitOk);
  EXPECT_TRUE(foreignPollUnknown);
  EXPECT_FALSE(foreignCancelOk);
  EXPECT_EQ(0u, foreignFail);
  EXPECT_EQ(1u, service.openTickets());

  WorldGenerationComputeResult result;
  EXPECT_EQ(WorldGenerationComputePollStatus::Ready, service.poll(*ticket, &result));
  service.clearOwnerThread();
  EXPECT_FALSE(service.hasOwnerThread());
  EXPECT_TRUE(service.submit(makeIdentity(132), sampleInput(), 3u));
}

TEST(WorldGenerationComputeTest, StagingSnapshotTracksPeakPoolAndGpuEstimate) {
  // GPU-02 → RAM-01: observe staging/pool/VRAM estimate without assets.
  WorldGenerationComputeCapabilities caps = WorldGenerationComputeService::probeCapabilities();
  caps.maxInFlight = 4;
  WorldGenerationComputeService service(caps);
  service.setDeferredCpuCompletion(true);
  service.resetPeakStagingBytes();

  auto before = service.stagingSnapshot();
  EXPECT_EQ(0u, before.stagingBytes);
  EXPECT_EQ(0u, before.peakStagingBytes);
  EXPECT_EQ(caps.maxStagingBytes, before.maxStagingBytes);
  EXPECT_EQ(0u, before.inFlight);
  EXPECT_EQ(caps.gpuOpenGlCompute, before.gpuCapable);

  auto input = sampleInput();
  uint64_t bytes = (uint64_t)input.size() * sizeof(uint32_t);
  auto a = service.submit(makeIdentity(120), input, 1u);
  auto b = service.submit(makeIdentity(121), input, 2u);
  ASSERT_TRUE(a);
  ASSERT_TRUE(b);

  auto mid = service.stagingSnapshot();
  EXPECT_EQ(2u * bytes, mid.stagingBytes);
  EXPECT_EQ(2u * bytes, mid.peakStagingBytes);
  EXPECT_EQ(2u, mid.inFlight);
  EXPECT_EQ(2u, mid.openTickets);
  EXPECT_EQ(0u, mid.gpuInFlight);
  EXPECT_EQ(0u, mid.estimatedVramBytes);

  EXPECT_TRUE(service.complete(*a));
  WorldGenerationComputeResult result;
  EXPECT_EQ(WorldGenerationComputePollStatus::Ready, service.poll(*a, &result));
  auto afterPoll = service.stagingSnapshot();
  EXPECT_EQ(bytes, afterPoll.stagingBytes);
  EXPECT_EQ(2u * bytes, afterPoll.peakStagingBytes); // peak retained
  EXPECT_GE(afterPoll.pooledBufferCount, 1u);
  EXPECT_GE(afterPoll.pooledBufferBytes, bytes);

  service.resetPeakStagingBytes();
  EXPECT_EQ(bytes, service.stagingSnapshot().peakStagingBytes);

  // Forced GPU capability still estimates VRAM only for GPU-backend tickets.
  caps.gpuOpenGlCompute = true;
  WorldGenerationComputeService gpuService(caps);
  gpuService.setDeferredCpuCompletion(true);
  auto gpuTicket = gpuService.submit(makeIdentity(122), input, 1u,
      WorldGenerationComputeBackend::GpuOpenGlCompute);
  // Without live GL device, GPU submit may be refused even with the bit set.
  if (gpuTicket) {
    auto gpuSnap = gpuService.stagingSnapshot();
    EXPECT_EQ(1u, gpuSnap.gpuInFlight);
    EXPECT_EQ(bytes, gpuSnap.estimatedVramBytes);
  } else {
    // Capability alone is insufficient without dispatch; snapshot stays CPU-shaped.
    auto refused = gpuService.stagingSnapshot();
    EXPECT_EQ(0u, refused.gpuInFlight);
    EXPECT_EQ(0u, refused.estimatedVramBytes);
  }
}

TEST(WorldGenerationComputeTest, CommitRevalidatesIdentityAndInputFingerprint) {
  // GPU-04: equivalence alone is not publication — commit checks live inputs.
  WorldGenerationComputeService service;
  auto input = sampleInput();
  uint64_t fp = WorldGenerationComputeService::fingerprintInput(input);
  EXPECT_EQ(fp, WorldGenerationComputeService::fingerprintInput(input));

  auto ticket = service.submit(makeIdentity(110), input, 0x11u);
  ASSERT_TRUE(ticket);

  WorldGenerationComputeResult result;
  auto accepted = service.commit(*ticket, makeIdentity(110), fp, &result);
  EXPECT_EQ(WorldGenerationComputeCommitVerdict::Accepted, accepted.verdict);
  ASSERT_EQ(input.size(), result.values.size());
  EXPECT_EQ(input[0] ^ 0x11u, result.values[0]);
  EXPECT_EQ(WorldGenerationComputeCommitVerdict::UnknownTicket,
      service.commit(*ticket, makeIdentity(110), fp, &result).verdict);

  auto ticketB = service.submit(makeIdentity(111), input, 0x22u);
  ASSERT_TRUE(ticketB);
  auto staleId = makeIdentity(111);
  staleId.sequence = 999;
  auto idMismatch = service.commit(*ticketB, staleId, fp, &result);
  EXPECT_EQ(WorldGenerationComputeCommitVerdict::StaleIdentity, idMismatch.verdict);
  EXPECT_TRUE(result.values.empty());
  EXPECT_EQ(WorldGenerationComputeCommitVerdict::UnknownTicket,
      service.commit(*ticketB, makeIdentity(111), fp, &result).verdict);

  auto ticketC = service.submit(makeIdentity(112), input, 0x33u);
  ASSERT_TRUE(ticketC);
  List<uint32_t> mutated = input;
  mutated[0] = 0xdeadbeefu;
  auto inputMismatch = service.commit(*ticketC, makeIdentity(112),
      WorldGenerationComputeService::fingerprintInput(mutated), &result);
  EXPECT_EQ(WorldGenerationComputeCommitVerdict::StaleInput, inputMismatch.verdict);
  EXPECT_TRUE(result.values.empty());

  service.setDeferredCpuCompletion(true);
  auto ticketD = service.submit(makeIdentity(113), input, 1u);
  ASSERT_TRUE(ticketD);
  EXPECT_EQ(WorldGenerationComputeCommitVerdict::NotReady,
      service.commit(*ticketD, makeIdentity(113), fp, &result).verdict);
  EXPECT_TRUE(service.complete(*ticketD));
  EXPECT_EQ(WorldGenerationComputeCommitVerdict::Accepted,
      service.commit(*ticketD, makeIdentity(113), fp, &result).verdict);
}
