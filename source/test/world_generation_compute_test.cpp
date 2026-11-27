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
  ASSERT_EQ(31u, kernels.size());
  EXPECT_EQ(WorldGenerationComputeKernel::DenseUint32Xor, kernels[0]);
  EXPECT_EQ(WorldGenerationComputeKernel::DenseUint32SatSub, kernels[kernels.size() - 1]);
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
            || kernel == WorldGenerationComputeKernel::DenseUint32Rotr)
        ? 3u
        : 0x11u;
    items.append(item);
  }
  auto reports = service.compareShadowBatch(input, std::move(items));
  ASSERT_EQ(31u, reports.size());
  for (auto const& report : reports) {
    EXPECT_EQ(WorldGenerationComputeShadowVerdict::Equal, report.verdict)
        << (int)report.identity.kernel;
    EXPECT_FALSE(report.reproducer.valid);
  }
}

TEST(WorldGenerationComputeTest, FingerprintCapabilitiesAndSupportedKernelCount) {
  EXPECT_EQ(31u, WorldGenerationComputeService::supportedKernelCount());
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
