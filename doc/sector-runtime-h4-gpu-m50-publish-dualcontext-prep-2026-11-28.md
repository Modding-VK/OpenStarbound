# H4 GPU milestone-50 — publish-live gate + dual-context prep (cloud-safe)

**Date:** 2026-11-28  
**Branch:** `cursor/h4-gpu-shadow-9f77`  
**Milestone:** **25%** (not claimed)  
**DenseUint32 horizontal:** STOPPED  
**Prior tip absorbed by INT:** `76ab0fa2` → `191cb6e4`

## Objective

Next cloud-safe vertical after WorldThread runtime: introduce an explicit
publish-to-review gate for Ready compute results and dual-context observation
stubs. Live sector/tile/entity/`generateQueue` writes remain refused.

## Delivered

| Piece | Change |
| --- | --- |
| `WorldGenerationComputePublishGate` | `stageReady` via `commit` (consumes ticket); stages review record |
| `applyToLiveWorld` | **always** `RefusedLiveWorld` (cloud-safe stub) |
| GlDevice dual-context prep | `contextRole` / `rendererShareSupported` / `dualContextPrepStatus` |
| Runtime wiring | `publishGate()` when active; clears staged on stop |
| Tests | `PublishGateStagesAndRefusesLiveWorld`, `DualContextPrepStubsCloudSafe`, `WorldThreadRuntimePublishGateWiring` |

## Cloud validation

- `WorldGenerationComputeTest.*` → **323/323 PASS** (flag OFF; stub GL)
- No `/dev/dri` / no real GL required

## Dual-context prep status (observation only)

- Stub build: `role=Inactive`; `renderer_share=unsupported`
- GL build: `role=WorldThreadHidden_when_probed`; still `renderer_share=unsupported`
- Desktop must validate any future share with the client renderer; this tip
  does **not** implement sharing.

## Real blockers (unchanged for 50%)

1. **Live publish** — gate stages only; sector adoption not implemented.
2. **GL dual-context Desktop** — renderer share unsupported; hidden SDL context
   remains separate from the client renderer.
3. **2º vendor parked** — NVIDIA alone does not authorize 50%.
4. **H5 GPU-05** — heterogeneous selection still needs cost model + real
   WorldThread-visible completion from production workloads.
5. **Project review** — harness gain ≠ production setores/s.

## Compatibility

- Mods/saves/multiplayer untouched (gate does not write the world)
- No DenseUint32 / Half* APIs
- No 50% claim

## Next

1. Desktop: probe dual-context prep status on GL-ON WorldThread build
2. Optional: admission from preparation pipeline into staged records only
3. Keep 2º vendor collection when a second GPU host is available (non-blocking)
