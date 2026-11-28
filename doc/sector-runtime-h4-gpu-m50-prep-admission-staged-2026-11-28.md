# H4 GPU milestone-50 — preparation admission → staged (cloud-safe)

**Date:** 2026-11-28  
**Branch:** `cursor/h4-gpu-shadow-9f77`  
**Milestone:** **25%** (not claimed)  
**DenseUint32 horizontal:** STOPPED  
**Prior tip absorbed by INT:** `f7440265` → `8ea0acfa`

## Objective

Next cloud-safe vertical after publish-gate / dual-context stubs: admit
Terrain/Biome preparation work into the WorldThread compute runtime, select a
backend with a **heuristic** cost stub, and stage Ready results for review —
without live sector/tile/entity/`generateQueue` writes.

## Delivered

| Piece | Change |
| --- | --- |
| `WorldGenerationComputeAdmission` | Terrain/Biome only; DenseUint32 → `RefusedKernel` |
| `selectBackend` | heuristic: low-latency/small → CPU; large+GPU/rehearsal → GPU |
| Runtime | `admitPreparation`, `stageReadyOpen`, `autoStageReady` (default **false**) |
| Publish path | stage only; `applyToLiveWorld` still always refuses |
| Test | `PreparationAdmissionStagesWithoutLiveApply` |

## Cloud validation

- `WorldGenerationComputeTest.*` → **324/324 PASS** (flag OFF; stub GL)
- No `/dev/dri` / no real GL required

## Config (opt-in)

```json
"worldGenerationCompute": {
  "worldThreadPump": true,
  "gpuFenceRehearsal": true,
  "deferredCompletion": true,
  "autoStageReady": false
}
```

`autoStageReady: true` stages Ready admitted tickets during `pumpUpdate`.
Still never applies to the live world.

## Real blockers (unchanged for 50%)

1. **Live publish** — staged review only; sector adoption not implemented.
2. **Measured transport cost** — `selectBackend` is heuristic, not GPU-05
   acceptance (upload/dispatch/fence/readback not timed in production).
3. **GL dual-context Desktop** — renderer share unsupported.
4. **2º vendor parked** — NVIDIA alone does not authorize 50%.
5. **Project review** — harness gain ≠ production setores/s.

## Compatibility

- Mods/saves/multiplayer untouched when disabled (defaults)
- No DenseUint32 / Half* APIs in this admission path
- No 50% claim; GPU-05 remains **0%** (prep stub only)

## Next

1. Wire preparation-feeder candidates into `admitPreparation` behind a flag
   (still stage-only)
2. Desktop: dual-context + measured cost samples for GPU-05 first cut
3. Keep 2º vendor collection when a second GPU host is available
