# H4 GPU milestone-50 — Terrain/Biome GL shaders + Desktop probe prep

**Date:** 2026-11-28  
**Branch:** `cursor/h4-gpu-shadow-9f77`  
**Authorization:** user Desktop GPU tests through **2026-11-29**; Desktop V Pessoal runs GL probe.  
**Milestone:** remains **25%** until Desktop evidence (no 50% claim).

## Objective

1. Cloud-safe prep for m50: GL compute shaders for TerrainField*/BiomeIdBucket matching CPU sketch
2. Extend opt-in probe harness with m50 shadows, wall timing, deferred GPU→pump path
3. Publish Desktop V checklist; coordinate with probe results (no DenseUint32 horizontal)

## Branch / prior tip

- Prior HEAD: `84378eb7` (deferred GPU → pumpFences + terrain CPU sketch docs)
- This tip: `4dc4c11e` — Terrain/Biome GL programs + probe m50 section + Desktop checklist

## Changes

| Area | Change |
| --- | --- |
| `StarWorldGenerationComputeGl.cpp` | Shaders + program wire for TerrainFieldMax2/Min2/ClampU16, BiomeIdBucket |
| `world_generation_gl_compute_probe.cpp` | m50 shadows, timing corpus, deferred TerrainFieldMax2 pump |
| Checklist | `doc/sector-runtime-h4-gpu-m50-desktop-v-probe-checklist-2026-11-28.md` |

## Cloud validation

- Flag **OFF** contract tests (normal build): WorldGenerationComputeTest suite
- No GPU on cloud VM — real GL claim deferred to Desktop V

## Evidence boundaries

- Shaders compile only when `STAR_ENABLE_WORLDGEN_GL_COMPUTE=ON` (Desktop)
- Timing lines are wall-clock submit/poll vs CPU `runKernel`; not Tracy; not a throughput claim
- Deferred path on real GL arms idle fence then dispatches on pump (sync dispatch inside completeEntry)

## Compatibility

- Normal builds unchanged (stub GL device; rehearsal path intact)
- No DenseUint32 / Half* / rejectedDuplicates added
- Mod/save/multiplayer untouched

## Next (Desktop V → H4)

1. Pull tip; run checklist through 2026-11-29
2. Return probe log excerpt + HEAD + GL renderer
3. H4 coordinates: only then evaluate measurable gain / 50% gate inputs
