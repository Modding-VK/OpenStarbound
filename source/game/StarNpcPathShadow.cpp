#include "StarNpcPathShadow.hpp"
#include "StarConfiguration.hpp"
#include "StarRoot.hpp"
#include "StarTime.hpp"
#include "StarWorldServer.hpp"

#include <atomic>

namespace Star {
namespace {

unsigned shadowWorkerCount() {
  static unsigned count = [] {
    auto profiling = Root::singleton().configuration()->get("explorationProfiling", JsonObject());
    if (!profiling.getBool("enabled", false) || !profiling.getBool("npcDetails", false))
      return 0u;
    auto threading = Root::singleton().configuration()->get("worldThreading", JsonObject());
    return clamp<unsigned>(threading.getUInt("npcShadowWorkers", 0), 0, 2);
  }();
  return count;
}

WorkerPool& shadowPool() {
  static WorkerPool pool("NpcPathShadow", shadowWorkerCount());
  return pool;
}

atomic<unsigned>& pendingShadowTasks() {
  static atomic<unsigned> pending{0};
  return pending;
}

struct ShadowSearch {
  NpcShadowPathOutcome outcome = NpcShadowPathOutcome::Error;
  Maybe<PlatformerAStar::Path> path;
};

ShadowSearch searchSnapshot(shared_ptr<PlatformerAStar::PathSnapshot const> const& snapshot,
    Vec2F from, Vec2F to, ActorMovementParameters movement, PlatformerAStar::Parameters parameters) {
  try {
    PlatformerAStar::PathFinder finder(snapshot, from, to, std::move(movement), std::move(parameters), false);
    // Shadow work is deliberately bounded even when a mod leaves A* unbounded.
    for (unsigned attempt = 0; attempt < 8; ++attempt) {
      auto explored = finder.explore(256);
      if (explored) {
        if (finder.result())
          return {*explored ? NpcShadowPathOutcome::Route : NpcShadowPathOutcome::PartialRoute, finder.result()};
        return {NpcShadowPathOutcome::NoRoute, {}};
      }
    }
    return {NpcShadowPathOutcome::SearchLimit, {}};
  } catch (PlatformerAStar::PathReadUnavailable const&) {
    return {NpcShadowPathOutcome::MissingCoverage, {}};
  } catch (std::exception const&) {
    return {NpcShadowPathOutcome::Error, {}};
  } catch (...) {
    return {NpcShadowPathOutcome::Error, {}};
  }
}

bool sameSearch(ShadowSearch const& a, ShadowSearch const& b) {
  if (a.outcome != b.outcome)
    return false;
  if (a.outcome != NpcShadowPathOutcome::Route && a.outcome != NpcShadowPathOutcome::PartialRoute)
    return true;
  if (!a.path || !b.path || a.path->size() != b.path->size())
    return false;
  for (size_t i = 0; i < a.path->size(); ++i) {
    auto const& left = a.path->at(i);
    auto const& right = b.path->at(i);
    if (left.action != right.action || !(left.source == right.source) || !(left.target == right.target)
        || left.jumpVelocity != right.jumpVelocity || left.cost != right.cost)
      return false;
  }
  return true;
}

}

bool NpcPathShadow::enabled() {
  return shadowWorkerCount() != 0;
}

NpcShadowPathSubmission NpcPathShadow::submit(WorldServer* world, NpcShadowPathIdentity identity,
    Vec2F from, Vec2F to, ActorMovementParameters movement,
    PlatformerAStar::Parameters search, weak_ptr<atomic<bool>> alive) {
  if (!enabled())
    return {NpcShadowPathSubmission::Status::Disabled, {}};

  auto& pending = pendingShadowTasks();
  unsigned current = pending.load();
  unsigned limit = shadowWorkerCount() * 2;
  while (current < limit && !pending.compare_exchange_weak(current, current + 1)) {}
  if (current >= limit)
    return {NpcShadowPathSubmission::Status::Saturated, {}};

  try {
    double captureStart = Time::monotonicTime();
    // Capture the route's neighborhood, not a fixed large square around every
    // NPC. The target is unwrapped to the shortest horizontal world distance.
    Vec2F unwrappedTo = from + world->geometry().diff(to, from);
    auto region = RectI::integral(RectF::boundBoxOf(from, unwrappedTo)).padded(16);
    if (region.width() > 113 || region.height() > 113)
      throw PlatformerAStar::PathReadUnavailable("Path snapshot route exceeds coverage", false);
    auto snapshot = PlatformerAStar::PathSnapshot::capture(world, region);
    auto sectorRevisions = world->pathSectorRevisions(region);
    identity.captureStep = snapshot->captureStep();
    double captureEnd = Time::monotonicTime();

    auto reference = searchSnapshot(snapshot, from, to, movement, search);
    double referenceEnd = Time::monotonicTime();
    auto queuedAt = referenceEnd;

    auto promise = shadowPool().addProducer<NpcShadowPathReport>(
        [snapshot, identity, sectorRevisions = std::move(sectorRevisions), from, to,
         movement = std::move(movement), search = std::move(search),
         alive, reference = std::move(reference), captureStart, captureEnd, referenceEnd, queuedAt]() mutable {
          struct ReleasePending {
            ~ReleasePending() { pendingShadowTasks().fetch_sub(1); }
          } releasePending;

          NpcShadowPathReport report;
          report.identity = identity;
          report.region = snapshot->region();
          report.sectorRevisions = std::move(sectorRevisions);
          report.reference = reference.outcome;
          report.capturedTiles = (size_t)snapshot->region().width() * (size_t)snapshot->region().height();
          report.missingTiles = snapshot->missingTiles();
          report.capturedBlocks = snapshot->capturedBlocks();
          report.captureSeconds = captureEnd - captureStart;
          report.referenceSeconds = referenceEnd - captureEnd;
          report.queueSeconds = Time::monotonicTime() - queuedAt;

          auto token = alive.lock();
          if (!token || !token->load()) {
            report.worker = NpcShadowPathOutcome::Cancelled;
            return report;
          }
          double workerStart = Time::monotonicTime();
          auto worker = searchSnapshot(snapshot, from, to, std::move(movement), std::move(search));
          report.workerSeconds = Time::monotonicTime() - workerStart;
          report.worker = worker.outcome;
          report.matches = sameSearch(reference, worker);
          if (!token->load())
            report.worker = NpcShadowPathOutcome::Cancelled;
          return report;
        });
    return {NpcShadowPathSubmission::Status::Accepted, std::move(promise)};
  } catch (PlatformerAStar::PathReadUnavailable const&) {
    pending.fetch_sub(1);
    return {NpcShadowPathSubmission::Status::MissingCoverage, {}};
  } catch (...) {
    pending.fetch_sub(1);
    return {NpcShadowPathSubmission::Status::CaptureUnavailable, {}};
  }
}

}
