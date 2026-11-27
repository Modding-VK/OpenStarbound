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
  // GPU-04 edge corpus: zero, one, max, wrap-adjacent, and patterned values.
  return List<uint32_t>{0u, 1u, 0xffffffffu, 0x80000000u, 0x7fffffffu, 0xfffffffeu};
}

}

TEST(WorldGenerationComputeTest, ProbeKeepsCpuAndDoesNotClaimGpuWithoutDetection) {
  auto caps = WorldGenerationComputeService::probeCapabilities();
  EXPECT_TRUE(caps.cpuReference);
  EXPECT_FALSE(caps.gpuOpenGlCompute);
  EXPECT_GE(caps.maxImmutableBufferBytes, sizeof(uint32_t));
  EXPECT_GE(caps.maxInFlight, 1u);
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
  WorldGenerationComputeService service;
  auto ticket = service.submit(makeIdentity(3), sampleInput(), 1u,
      WorldGenerationComputeBackend::GpuOpenGlCompute);
  EXPECT_FALSE(ticket);
  EXPECT_EQ(0u, service.openTickets());
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

TEST(WorldGenerationComputeTest, ContextFailureRejectsDuplicates) {
  WorldGenerationComputeService service;
  service.setDeferredCpuCompletion(true);
  auto ticket = service.submit(makeIdentity(21), sampleInput(), 1u);
  ASSERT_TRUE(ticket);
  EXPECT_EQ(1u, service.failContext());
  WorldGenerationComputeResult result;
  EXPECT_EQ(WorldGenerationComputePollStatus::Cancelled, service.poll(*ticket, &result));
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
  WorldGenerationComputeService service;
  auto report = service.compareShadow(makeIdentity(62), edgeCorpus(), 1u,
      WorldGenerationComputeBackend::CpuReference,
      WorldGenerationComputeBackend::GpuOpenGlCompute);
  EXPECT_EQ(WorldGenerationComputeShadowVerdict::ShadowUnavailable, report.verdict);
  EXPECT_EQ(0u, report.divergenceCount);
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
