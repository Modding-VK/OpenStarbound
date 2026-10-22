# CPU performance baseline

## Goal

Record repeatable measurements before changing simulation behavior. The tools
measure aggregate server-world update cost and terrestrial sector generation;
neither represents a full multiplayer session.

## Build

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

## Uploaded startup reference

The pre-change OpenStarbound 0.1.15.1 log (source ID
`e604e078e89310f1212dd20f5cd01bd59d25300b`) reports Frackin' Universe 6.5.8 and
`Root: Loaded ItemDatabase` in 9.653 seconds. The game then entered a world and
shut down cleanly. This was a Windows x86_64 run with a larger mod stack, so use
it only as a reference when the same setup is repeated; it contains no runtime
frame-rate or CPU measurements. The log already has three missing object-script
warnings from the ATI bridge scan and a missing StarExtensions warning for Zoom
Keybinds.
