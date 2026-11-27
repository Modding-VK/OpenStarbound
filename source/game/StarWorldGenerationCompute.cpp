#include "StarWorldGenerationCompute.hpp"
#include "StarWorldGenerationComputeGl.hpp"

#include <algorithm>

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
  if (version != 1u)
    return false;
  return kernel == WorldGenerationComputeKernel::DenseUint32Xor
      || kernel == WorldGenerationComputeKernel::DenseUint32Add;
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

void WorldGenerationComputeService::setDeferredCpuCompletion(bool deferred) noexcept {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_deferredCpu = deferred;
}

size_t WorldGenerationComputeService::openTickets() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_entries.size();
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

size_t WorldGenerationComputeService::stagingBytes() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return (size_t)m_stagingBytes;
}

size_t WorldGenerationComputeService::pooledBuffers() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_bufferPool.size();
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
  entry.stagingBytes = bytes;
  m_stagingBytes += bytes;

  // GPU completions use the same sync/deferred switch as CPU for the first
  // harness: deferred leaves Pending until pump()/complete(); sync runs now.
  if (!m_deferredCpu)
    completeEntry(entry);
  else
    entry.state = EntryState::Pending;

  m_entries.emplace(ticket.id, std::move(entry));
  return ticket;
}

Maybe<WorldGenerationComputeTicket> WorldGenerationComputeService::submit(
    WorldGenerationComputeInputIdentity identity, List<uint32_t> input, uint32_t parameter,
    WorldGenerationComputeBackend backend) {
  std::lock_guard<std::mutex> lock(m_mutex);
  return submitLocked(std::move(identity), std::move(input), parameter, backend);
}

Maybe<List<WorldGenerationComputeTicket>> WorldGenerationComputeService::submitBatch(
    List<WorldGenerationComputeBatchItem> items, WorldGenerationComputeBackend backend) {
  std::lock_guard<std::mutex> lock(m_mutex);
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
  auto it = m_entries.find(ticket.id);
  if (it == m_entries.end())
    return false;
  if (it->second.state != EntryState::Pending)
    return false;
  completeEntry(it->second);
  return it->second.state == EntryState::Ready || it->second.state == EntryState::Cancelled
      || it->second.state == EntryState::Failed;
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
  return pollLocked(ticket, out);
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

size_t WorldGenerationComputeService::failContext() {
  std::lock_guard<std::mutex> lock(m_mutex);
  size_t affected = 0;
  for (auto& pair : m_entries) {
    Entry& entry = pair.second;
    entry.contextFailed = true;
    entry.publicationRevoked = true;
    entry.state = EntryState::Cancelled;
    entry.output.clear();
    ++affected;
  }
  return affected;
}

WorldGenerationComputeShadowReport WorldGenerationComputeService::compareShadow(
    WorldGenerationComputeInputIdentity identity, List<uint32_t> const& input, uint32_t parameter,
    WorldGenerationComputeBackend primary, WorldGenerationComputeBackend shadow) {
  WorldGenerationComputeShadowReport report;
  report.identity = identity;
  report.primaryBackend = primary;
  report.shadowBackend = shadow;
  report.comparedValues = input.size();

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

  size_t n = std::min(primaryValues.size(), shadowValues.size());
  report.comparedValues = n;
  for (size_t i = 0; i < n; ++i) {
    if (primaryValues[i] != shadowValues[i]) {
      if (!report.divergenceCount) {
        report.firstDivergenceIndex = i;
        report.primarySample = primaryValues[i];
        report.shadowSample = shadowValues[i];
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

}
