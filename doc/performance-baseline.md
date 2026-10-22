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

The tool reports aggregate simulation throughput and entity count. It does not
yet attribute time to entity updates, scripts, wiring, liquids, or world
generation. Those stages need separate instrumentation before optimizing any
one of them.

## Current audit constraints

`WorldServer::update` processes entities before script contexts, damage,
wiring, weather, liquids, and the remaining world work. `EntityMap` sorts
entities by type and updates its spatial index after each entity callback.
Entity callbacks can therefore observe spatial changes made by earlier
callbacks in the same tick. Parallelizing this loop directly could change
simulation and mod-visible behavior; first measure and document dependencies,
then design a controlled snapshot/commit boundary if profiling justifies it.
