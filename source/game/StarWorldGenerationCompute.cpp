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
      || kernel == WorldGenerationComputeKernel::DenseUint32Not;
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
      WorldGenerationComputeKernel::DenseUint32Not};
  for (auto kernel : all)
    out.append(kernel);
  return out;
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

WorldGenerationComputeStagingSnapshot WorldGenerationComputeService::stagingSnapshot() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  WorldGenerationComputeStagingSnapshot snap;
  snap.stagingBytes = m_stagingBytes;
  snap.peakStagingBytes = m_peakStagingBytes;
  snap.maxStagingBytes = m_capabilities.maxStagingBytes;
  snap.openTickets = m_entries.size();
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

}
