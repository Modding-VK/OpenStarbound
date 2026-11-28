# H4 GPU milestone-50 — WorldThread compute runtime (cloud-safe)

**Date:** 2026-11-28  
**Branch:** `cursor/h4-gpu-shadow-9f77`  
**Milestone:** **25%** (not claimed)  
**DenseUint32 horizontal:** STOPPED

## Objective

Next vertical after NVIDIA `gain_pipeline` / parked 2º vendor: wire GPU-02
owners + non-blocking fence pump into `WorldServerThread` without live-world
writes. This is the cloud-safe prep that unblocks H5 GPU-05 selection work
once results can complete on the world tick without blocking.

## Delivered

| Piece | Change |
| --- | --- |
| `WorldGenerationComputeRuntime` | start/pump/stop; config `worldGenerationCompute.worldThreadPump` (default **false**) |
| `WorldServerThread::run` | starts runtime on the world thread; stops on exit |
| `WorldServerThread::update` | `pumpUpdate()` after `WorldServer::update` (non-blocking) |
| Test | `WorldThreadRuntimePumpRehearsalCloudSafe` |

Default remains inactive (dedicated servers / normal configs unchanged). When
enabled, `gpuFenceRehearsal` defaults **true** so cloud/CI materializes on CPU
after `pumpFences` without claiming `gpuOpenGlCompute`.

## Cloud validation

- `WorldGenerationComputeTest.*` → **320/320 PASS** (flag OFF)
- No `/dev/dri` / no real GL required for the rehearsal path

## Real blockers (documented; not solved here)

1. **No live publish** — runtime never commits GPU/CPU compute results into
   tiles, entities, or `generateQueue`. Sector adoption is a later gate.
2. **2º vendor parked** — NVIDIA `gpu_faster=1` alone does not authorize 50%;
   multi-driver still open ([path](sector-runtime-h4-gpu-m50-multidriver-path-2026-11-28.md)).
3. **Real GL on WorldThread** — GlDevice uses a hidden SDL context (not the
   renderer). Dual-context with a client renderer + dedicated-server headless
   behavior need Desktop/lab validation; this tip does not claim them.
4. **H5 GPU-05** — heterogeneous backend selection still needs cost model +
   WorldThread-visible completion signals from real workloads (not rehearsal).
5. **Project review** — harness gain ≠ production setores/s.

## Config (opt-in)

```json
"worldGenerationCompute": {
  "worldThreadPump": true,
  "gpuFenceRehearsal": true,
  "deferredCompletion": true
}
```

## Compatibility

- Mods/saves/multiplayer untouched when disabled (default)
- No DenseUint32 / Half* APIs
- No 50% claim

## Next

1. Optional Desktop: enable `worldThreadPump` with rehearsal false on GL-ON build
2. Keep 2º vendor collection when a second GPU host is available (non-blocking)
3. Later: admission from preparation pipeline without live writes until commit gate
