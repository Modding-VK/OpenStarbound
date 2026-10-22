# CPU performance baseline

## Goal

Record repeatable measurements before changing simulation behavior. The first
baseline is the aggregate server-world update cost from `world_benchmark`; it
does not measure individual update stages or represent a full multiplayer
session.

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

## Current audit constraints

`WorldServer::update` processes entities before script contexts, damage,
wiring, weather, liquids, and the remaining world work. `EntityMap` sorts
entities by type and updates its spatial index after each entity callback.
Entity callbacks can therefore observe spatial changes made by earlier
callbacks in the same tick. Parallelizing this loop directly could change
simulation and mod-visible behavior; first measure and document dependencies,
then design a controlled snapshot/commit boundary if profiling justifies it.

## First engine change under evaluation

`ItemDatabase` retains each item's immutable base configuration after the asset
scan. `itemConfig` reuses that JSON value instead of retrieving the asset and
recursively merging the same static configuration for every request. Per-request
Lua builders still run with the caller's parameters, level, and seed; generated
item data is not cached. This removes repeated setup work but has no measured
performance claim yet. Compare the same gameplay scenario and record the build,
mods, seed, entity count, world update rate, and profiler settings before drawing
a conclusion.
