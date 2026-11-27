#include "StarWorldGenerationCompute.hpp"

#include "gtest/gtest.h"

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
