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

## Uploaded startup reference

The pre-change OpenStarbound 0.1.15.1 log (source ID
`e604e078e89310f1212dd20f5cd01bd59d25300b`) reports Frackin' Universe 6.5.8 and
`Root: Loaded ItemDatabase` in 9.653 seconds. The game then entered a world and
shut down cleanly. This was a Windows x86_64 run with a larger mod stack, so use
it only as a reference when the same setup is repeated; it contains no runtime
frame-rate or CPU measurements. The log already has three missing object-script
warnings from the ATI bridge scan and a missing StarExtensions warning for Zoom
Keybinds.
