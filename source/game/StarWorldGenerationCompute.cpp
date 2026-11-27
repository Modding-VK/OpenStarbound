#include "StarWorldGenerationCompute.hpp"

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
  // CPU reference is always available and does not raise the graphics floor.
  // OpenGL compute must be detected by a future probe against real 4.3+/extension
  // limits; creating a 4.1 context alone must never set gpuOpenGlCompute.
  WorldGenerationComputeCapabilities caps;
  caps.cpuReference = true;
  caps.gpuOpenGlCompute = false;
  caps.maxWorkGroupInvocations = 1;
  caps.maxImmutableBufferBytes = 16ull * 1024ull * 1024ull;
  return caps;
}

WorldGenerationComputeService::WorldGenerationComputeService(
    WorldGenerationComputeCapabilities capabilities)
  : m_capabilities(std::move(capabilities)) {
  if (!m_capabilities.cpuReference)
    m_capabilities.cpuReference = true;
}

bool WorldGenerationComputeService::supported(WorldGenerationComputeKernel kernel) noexcept {
  return kernel == WorldGenerationComputeKernel::DenseUint32Xor;
}

List<uint32_t> WorldGenerationComputeService::runCpuDenseUint32Xor(
    List<uint32_t> const& input, uint32_t parameter) {
  List<uint32_t> out;
  out.reserve(input.size());
  for (uint32_t value : input)
    out.append(value ^ parameter);
  return out;
}

void WorldGenerationComputeService::completeCpu(Entry& entry) {
  if (entry.publicationRevoked) {
    entry.state = EntryState::Cancelled;
    entry.output.clear();
    return;
  }
  if (entry.identity.kernel != WorldGenerationComputeKernel::DenseUint32Xor
      || entry.identity.kernelVersion != 1u) {
    entry.state = EntryState::Failed;
    entry.output.clear();
    return;
  }
  entry.output = runCpuDenseUint32Xor(entry.input, entry.parameter);
  entry.state = EntryState::Ready;
}

Maybe<WorldGenerationComputeTicket> WorldGenerationComputeService::submit(
    WorldGenerationComputeInputIdentity identity, List<uint32_t> input, uint32_t parameter,
    WorldGenerationComputeBackend backend) {
  if (!supported(identity.kernel) || identity.kernelVersion != 1u)
    return {};
  if (backend == WorldGenerationComputeBackend::GpuOpenGlCompute) {
    if (!m_capabilities.gpuOpenGlCompute)
      return {};
  } else if (backend != WorldGenerationComputeBackend::CpuReference
      || !m_capabilities.cpuReference) {
    return {};
  }

  uint64_t bytes = (uint64_t)input.size() * sizeof(uint32_t);
  if (bytes > m_capabilities.maxImmutableBufferBytes)
    return {};

  std::lock_guard<std::mutex> lock(m_mutex);
  WorldGenerationComputeTicket ticket{m_nextTicket++};
  Entry entry;
  entry.identity = identity;
  entry.backend = backend;
  entry.parameter = parameter;
  entry.input = std::move(input);
  // GPU-01 CPU reference completes synchronously. A later GPU path may leave
  // entries Pending until fence/poll; cancel still revokes publication.
  if (backend == WorldGenerationComputeBackend::CpuReference)
    completeCpu(entry);
  else
    entry.state = EntryState::Pending;
  m_entries.emplace(ticket.id, std::move(entry));
  return ticket;
}

WorldGenerationComputePollStatus WorldGenerationComputeService::poll(
    WorldGenerationComputeTicket ticket, WorldGenerationComputeResult* out) {
  std::lock_guard<std::mutex> lock(m_mutex);
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

  if (status != WorldGenerationComputePollStatus::Pending)
    m_entries.erase(it);
  return status;
}

bool WorldGenerationComputeService::cancel(WorldGenerationComputeTicket ticket) {
  std::lock_guard<std::mutex> lock(m_mutex);
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

size_t WorldGenerationComputeService::openTickets() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_entries.size();
}

}
