# CPU performance baseline

## Goal

Record repeatable measurements before changing simulation behavior. The tools
measure aggregate server-world update cost and terrestrial sector generation;
neither represents a full multiplayer session.

## Accepted exploration reference — 2026-10-23

Use source revision `684e6ae0ee211d7f470f17813347c32cf9a39614` on
`perf/threading-audit` as the reference client for subsequent optimization
comparisons. The user tested this Windows x86_64 build and confirmed that the
lighting regression was corrected. The accompanying log identifies NVIDIA
RTX 3050 / driver 621.01 and Frackin' Universe 6.5.8 with additional mods;
it records world visits and a graceful shutdown. See `lighting-regression.md`.

This is a functional reference for the reported lighting issue, not a measured
performance baseline or a claim that all gameplay and multiplayer paths have
been validated. It already includes the earlier optimization commits. Preserve
the old upstream reference `e604e078` separately when measuring cumulative
changes against the user's original client. Neither uploaded log supplies
frame-time distributions or per-stage exploration timings.

For the next local comparison:

1. Keep the reference client and a copy of its configuration, mod versions,
   player, and universe. Use separate working copies restored from the same
   snapshot for each build, so one run cannot pre-generate the other's terrain.
2. Record two scenarios separately: revisiting already generated terrain and
   entering previously ungenerated terrain. Use the same route, travel speed,
   zoom, resolution, graphics settings, and recording method in each comparison.
3. Keep a log and recording per run, identified by source revision and scenario.
   If frame-time capture is available, retain the samples and report median,
   p95/p99, and stall counts using the same threshold and duration. Do not infer
   a CPU bottleneck or a speedup from average FPS or startup timings alone.
4. Use the opt-in stage benchmarks below for attribution. Their controlled
   workloads complement the interactive runs; they do not measure the entire
   rendering, disk-loading, and multiplayer experience.

The latest log also reports a missing `osbprintedmap8x4` object while loading
the ship. Track it separately as an unresolved asset compatibility issue; its
presence does not establish a cause for the exploration stalls.

## Build

For measurements during actual gameplay, use the optional runtime diagnostics
documented in [exploration-profiling.md](exploration-profiling.md), including the
Windows configuration and two exploration scenarios. They are disabled by default
and are independent of the benchmark profiling switches below.

Build the developer-only target from the source directory:

```sh
cmake --build build/linux-release --target world_benchmark
```

It is excluded from the default build. Running it requires the game's packed
assets and a dungeon definition available through the normal asset loader. Do
not commit proprietary game assets to the repository.

## Run consistently

Use the same machine, build configuration, asset set, dungeon, seed, step count,
and server fidelity when comparing revisions. For example:

```sh
dist/world_benchmark DUNGEON_NAME --seed 123456789 --steps 5000 --times 5
```

Replace `DUNGEON_NAME` with a dungeon ID available in the selected game assets.

Use an identical warm-up run before recording results, then compare the median
of several measured runs. Keep Lua profiling disabled for normal timing because
profiling adds overhead. Run a separate `--profiling` pass when investigating
script cost; Lua writes a `.luaprofile` report under the game's `storage/lua`
directory when its Lua root shuts down. Record the commit, compiler, build type,
CPU model, dungeon, seed, fidelity, and each run's elapsed time and average FPS
with the result.

The tool always reports aggregate simulation throughput and entity count. Add
`--stage-profiling` to also print per-phase sample count, total, mean, and maximum
time in milliseconds for setup/actions, entities, world script contexts, damage,
wiring, sky, weather, liquids, falling blocks, block damage, world storage,
generation, and cleanup/networking. The `worldStorageGeneration` phase includes
the synchronous sector generation performed by `generateQueue`.

The phase timers are disabled by default. When enabled, they add clock reads to
each world update, so use the unprofiled run for throughput comparisons and a
separate profiled run to locate expensive phases. Script callbacks triggered
inside entity updates are timed under `entities`; the `scriptContexts` phase
covers the separate world script contexts. Phase totals omit benchmark-side
work such as the periodic `signalRegion` calls.

## Terrestrial sector generation

Build the opt-in planet generation benchmark:

```sh
cmake --build build/linux-release --target planet_generation_benchmark
```

Run it with the same game assets, planet type, size, and seed when comparing
revisions:

```sh
dist/planet_generation_benchmark --type garden --size small --seed 123456789 --sectors 32
```

Run it from the game installation directory containing the usual `sbinit.config`.
The benchmark uses that file's asset sources, so include the same mod stack,
including Frackin' Universe, in each comparison.

The benchmark constructs a terrestrial planet, selects a run of surface
sectors away from the spawn area, and synchronously activates those sectors.
Its report separates `baseTiles`, `microDungeons`, `caveLiquid`, `finalize`, and
`terraform`, with sample count, total, mean, and maximum time. Counts include
adjacent lower generation levels created to satisfy the existing border
dependency rules. These timings identify which synchronous stage dominates
before moving work to workers; terrain caches and world mutations still need an
explicit snapshot and ordered-commit design before parallel execution.

## Current audit constraints

`WorldServer::update` processes entities before script contexts, damage,
wiring, weather, liquids, and the remaining world work. `EntityMap` sorts
entities by type and updates its spatial index after each entity callback.
Entity callbacks can therefore observe spatial changes made by earlier
callbacks in the same tick. Parallelizing this loop directly could change
simulation and mod-visible behavior; first measure and document dependencies,
then design a controlled snapshot/commit boundary if profiling justifies it.

## Frackin' Universe router trace — 2026-10-23

The Windows x86_64 run from source revision `2f7ca884` revisited the generated
world `CelestialWorld:383737864:-48324405:61106789:8` for 139.2 seconds of
profiler windows. Object updates consumed 94.93 seconds, of which 85.93 seconds
were measured inside periodic Lua updates. Logging inside entity updates was
below the profiler's measurable resolution in every reported entity category.

The slow-entity records attribute at least 71.87 seconds to three Frackin'
Universe `network_router` objects. This lower bound only includes individual
updates lasting at least 50 ms:

| Entity | Position | Slow updates | Mean | Maximum |
| --- | --- | ---: | ---: | ---: |
| 11 | `(863, 1011)` | 79 | 459.82 ms | 671.16 ms |
| 295 | `(921, 1043)` | 79 | 309.47 ms | 396.91 ms |
| 461 | `(944, 1095)` | 78 | 142.27 ms | 233.62 ms |

All these updates ran on the same world-server thread. The server update reached
1013.12 ms and its entity phase reached 1011.20 ms, consistent with multiple
router updates accumulating serially in one simulation tick. In comparison,
`worldStorageGeneration` totaled 1.40 seconds and peaked at 98.35 ms during the
whole capture. This run therefore demonstrates a persistent entity-script
bottleneck in an already generated area; it does not characterize new terrain
generation.

Inspection of Frackin' Universe tag `6.5.8` at revision `329e714b` explains the
scaling. `kheAA_router.lua` rebuilds its input and output graph on each routing
pass, scans each source item against each target container, and may scan every
target slot. Those inner loops repeatedly call container functions across the
Lua/C++ boundary. `checkFilter` also calls `world.containerItems(entity.id())`
for each candidate source item even though `update` already stored that same
result in `self.routerItems`. The engine converts complete item descriptions to
and from JSON at several of these boundaries.

Parallel updates are unsafe for this workload because routers can read and
mutate the same containers and synchronously call scripts on connected entities.
A safe sequence is to remove redundant work while retaining serial semantics,
then add an atomic batch-transfer API only if profiling still justifies an
opt-in mod change. As a first engine-side correction, both Lua implementations
of `containerItemAt` retain a const reference to the bag's item list instead of
copying the complete `List<ItemPtr>` for every queried slot. The returned JSON,
slot validation, call order, and mutation behavior remain unchanged.

A later `e3ab55fa` run reproduced this independently of terrain generation.
Router entity 11 at `(863, 1011)` produced 11 slow updates, totaling at least
5.11 seconds and peaking at 676.56 ms. Router entity 524 at `(944, 1095)`
produced 10 slow updates, totaling at least 1.34 seconds and peaking at
172.34 ms. Almost the entire duration was inside Lua, logging remained zero,
and both ran on the same world-server thread. During the new-terrain portion
of the same session, entity updates instead averaged 0.16 ms and never exceeded
0.59 ms.

## Low-risk changes under evaluation

`jsonMerge` also returns the base value directly when both inputs are objects
and the merger is empty. This avoids copying an immutable object map for a
no-op merge. It applies to callers outside `ItemDatabase` as well and has no
measured performance claim yet.

`LuaRoot::ScriptCache::loadContextScript` now checks the compiled-script map
while holding its existing lock instead of reacquiring the recursive lock
through helper methods. Compiled bytecode remains cached, while each builder
still gets a fresh context and executes normally. The effect has not been
measured.

The server's per-tick tile-entity check now reads the stored material-space
list by reference for `Object` entities. It still compares the same values and
applies tile changes through the same path, while avoiding a temporary list
copy. The effect has not been measured.

`world.entityQuery` now writes matching IDs directly into the returned Lua
table when no result ordering is requested. Queries requesting `nearest` or
`random` keep the intermediate entity list needed for sorting. The effect has
not been measured.

Queued `BaseTiles` generation can now run in an isolated, process-wide worker
pool when `worldThreading.sectorWorkers` is greater than zero. Workers receive
owned sector copies and independently reconstructed world-template snapshots;
`WorldServerThread` validates and commits completed results. This avoids sharing
the mutable terrain-generation caches that caused an access violation in the
first worker implementation. The synchronous path remains the default.
Configuration, scope, and current limitations are documented in
[world-threading.md](world-threading.md).

A Windows run of `8f113215` with two workers confirmed the cache-isolation fix:
657 worker results completed across two visits to the same Frackin' Universe
planet, no access violation occurred, and the application stopped gracefully.
It also measured a mean worker duration of about 50.25 ms because that revision
rebuilt the complete template snapshot for every sector. The follow-up keeps
one snapshot per worker and source-template revision, preserving isolation while
removing that repeated construction cost. This run is correctness evidence; its
route and generated-sector mix are not a controlled performance comparison.

The subsequent Windows run of `e3ab55fa` validated worker-local template reuse.
The first two completed jobs, one for each worker, averaged 39.81 ms while their
private templates were reconstructed. The following 62 jobs averaged between
1.72 and 1.85 ms in their reporting windows; across all 64 jobs the mean was
3.03 ms and the maximum was 39.97 ms. This is a 94% reduction in aggregate mean
worker duration from `8f113215`, with no fatal error and a graceful shutdown.
During the 50.1-second new-terrain world instance, server updates averaged
0.77 ms and peaked at 19.40 ms. The capture contained 79 initially empty tile
sectors and 730 sectors loaded from storage, so it validates the cache reuse
but is not a throughput comparison against the earlier route.

The same capture exposed a queue-liveness regression after the fast worker
results had been validated. Worker completions stopped after 64 results, while
`generation.baseTiles` continued to be polled in later windows and the player
encountered an unloaded sector boundary. Generation queue entries did not
refresh their own TTL when repeatedly signaled. Expired entries could therefore
leave completed dependency jobs in the bounded pending map; four such orphans
prevented any new job from being scheduled. Queue signals now refresh that TTL,
sector unloads detach pending results, and a full map evicts its least recently
requested completed result before refusing new work.

## Uploaded startup reference

The pre-change OpenStarbound 0.1.15.1 log (source ID
`e604e078e89310f1212dd20f5cd01bd59d25300b`) reports Frackin' Universe 6.5.8 and
`Root: Loaded ItemDatabase` in 9.653 seconds. The game then entered a world and
shut down cleanly. This was a Windows x86_64 run with a larger mod stack, so use
it only as a reference when the same setup is repeated; it contains no runtime
frame-rate or CPU measurements. The log already has three missing object-script
warnings from the ATI bridge scan and a missing StarExtensions warning for Zoom
Keybinds.

## NPC diagnostic log supplied with the HUD report — 2026-10-25

The user supplied a `starbound.log` after enabling `explorationProfiling` with
`npcDetails: true`. It contains 69 server reporting windows across four world
contexts, plus 67 client and 67 painter windows. Across the server windows,
`entityTypes.Npc` accumulated about 41.1 s over 314,430 updates, while
`entityTypes.Object` accumulated about 46.3 s over 5,657,239 updates. The
largest individual NPC update was 65.2 ms and the largest object update was
537.6 ms. The NPC detail sections record 49,434 path exploration calls
(about 5.7 s inclusive) and 424,690 `lua.update` invocations (about 19.5 s
inclusive). These substage totals overlap the NPC totals and must not be added
to them.

Per-world breakdown (world numbers follow visit order in this log; timings
are accumulated measurements within each world, not elapsed session time):

| World | Server window | NPC updates / time | Object updates / time | Longest world tick | Longest sector queue wait |
| --- | --- | ---: | ---: | ---: | ---: |
| 1 | 16:54:07–16:55:08 | 72,570 / 9.8 s | 870,329 / 1.4 s | 169 ms | 29.7 s |
| 2 | 16:55:12–16:55:57 | 33,484 / 10.0 s | 292,224 / 0.6 s | 33 ms | 83 ms |
| 3 | 16:55:58–16:58:48 | 206,934 / 20.8 s | 3,072,701 / 8.2 s | 531 ms | 250 ms |
| 4 | 16:58:51–16:59:36 | 1,442 / 0.6 s | 1,421,985 / 36.2 s | 976 ms | 2.76 s |

World 1 completed 368 queued sectors and exhausted its generation time budget
178 times. It recorded 215 microdungeon stage samples totaling 1.48 s, with
a 162 ms maximum. Its oldest queued sector reached 35.1 s. The queue wait is
wall-clock age from enqueue to completion: simultaneous waits overlap, so the
718 s sum of all sector waits is **not** CPU time on the world thread.
The world identifier matches the planet in the earlier log where
`mantiziestate` was placed at `[478, 1000]`. This session loaded that existing
planet from storage and contains no new `Placing dungeon mantiziestate` event.
The player entered near x=5969, y=1023. The user reports that sectors of the
Mantizi Estate remained slow to appear during this visit. The measurements
support that observation: from 16:54:32 through 16:54:42, three consecutive
five-second reports recorded **zero** completed sectors while the sampled
generation queue averaged roughly 43–51 entries; its oldest request grew from
17.9 s to 27.7 s. The next report recorded a sector wait of 29.7 s. The log
has no per-sector coordinates, so it cannot attribute an individual delayed
sector to a particular dungeon piece or isolate loading from generation work.

World 4 has a distinct object/script bottleneck. Object updates consumed 36.2 s
of its 46.8 s accumulated `serverUpdate` time (about 77%); 33.5 s of object
time was reported inside Lua callbacks. There were 99 object updates over the
50 ms slow-operation threshold and 98 slow-entity records retained. Of those
records, 97 are `network_router` and one is `normalalveary`. The largest
`network_router` update was 538 ms. Object logging measured zero milliseconds
in this capture. Lua callback time includes synchronous engine calls invoked
by scripts, so these numbers alone cannot separate script computation from
interaction with storage, other objects, or the world. Slow-entity records are
bounded per reporting window and are not a complete per-object accounting.

World 4 completed 102 sectors, with a maximum queue wait of 2.76 s and 16
generation budget exhaustions. Its measured base-tile worker and microdungeon
stages were zero, so this log does not support attributing those particular
world-4 stalls to expensive terrain or dungeon generation. Object load and
sector wait overlap in the same visit, but the aggregate report does not prove
that a specific router delayed a specific sector. World 3 mainly measured NPC
time (20.8 s) and contained the longest individual NPC update (65.2 ms).

Client and painter reports span the entire session and have a single context,
so they cannot be mapped exactly to the four server worlds. Across the session,
`painter.environment` accumulated 108.8 s and `painter.total` 137.8 s; the
largest painter frame was 437 ms. The client recorded a 382 ms packet batch
maximum, mostly during world initialization. These are separate stages and
cannot be added to server-thread time to estimate one CPU core's utilization.

The log has no crash. Startup reports three missing mod script assets (chicken
nest, makeshift reactor, bunk bed), and two dungeon-object placement warnings
occur at 16:55:55. No evidence in this capture links those warnings to the
world-4 router stalls. A useful next capture would keep a reproducible route
and world snapshot, then split `network_router` Lua callback time into local
work and synchronous calls to other entities/storage; compare the same route
with router activity disabled or reduced.

This is an observational diagnostic capture, not a paired benchmark: four
world contexts contribute and no matching universe snapshot or route was
supplied. The object maximum indicates a separate long-tail investigation is
still relevant. Neither aggregate total identifies which event caused a
specific visible pause. Keep gameplay and build validation separate from this
log analysis.

## Restored-save Glitch village stall — follow-up log

The user restored backups of `player` and `universe`, revisited the same
planet (`383738425:-48324202:-255713414:2`), and observed sector loading stop
while approaching a Glitch village. The log is from source ID
`1b1e113cabfefe4ec6c6e2b7de672fcb3be7e3cc`. The player entered this
planet at 20:42:24 and shut the game down gracefully at 20:44:18. No new
dungeon-placement event appears during this visit.

The sector queue repeatedly stalled. At 20:43:09 and 20:43:14, five-second
reports show average queue depths of 46–47 and **zero** completed sectors;
the oldest pending sector reached 22.8 s. At 20:43:39, 20:43:44, 20:43:49,
and 20:43:54, four successive reports again show zero completed sectors with
average queue depths of 33–37. The oldest pending sector rose from 37.5 s to
52.5 s. One sector completed after a **54.95 s** wait at 20:43:59. At the
final report, another request was still 48.8 s old. This independently
corroborates the player's observation of missing sectors near the village.

These zero-completion windows mostly recorded **no generation time-budget
exhaustion**, and their maximum `serverUpdate` times were generally 11–34 ms.
Across this planet visit, 526 sectors completed; measured base-tile worker
durations for completed work totaled 562 ms across 150 samples (maximum
146 ms), microdungeons 788 ms, and tile-storage work 255 ms. Those totals
do not explain the long uncompleted requests. The profiling format does not
record pending worker count, worker queue age, completion-poll outcome,
stale-snapshot retries, or per-sector coordinates. The likely gap to
investigate is between asynchronous base-tile scheduling and the dependent
main-thread commit, but the log cannot establish which part blocked.

This is a second route through a restored copy of the same planet, not a
matched Mantizi-vs-Glitch timing comparison. A separate missing
`osbprintedmap8x4` object occurred on another world before arrival and is
not evidenced as the cause of this planet's queue stall. The three startup
missing-script warnings also predate the stall.
