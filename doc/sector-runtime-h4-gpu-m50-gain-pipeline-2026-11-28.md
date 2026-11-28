# H4 GPU milestone-50 — terrain gain pipeline (no 50% claim)

**Date:** 2026-11-28  
**Branch:** `cursor/h4-gpu-shadow-9f77`  
**Aligns with INT:** `77c2aa47` (`ShadowVerdict::Equal`)  
**Desktop evidence:** re-probe tip `3d37e0de` — single-kernel `gpu_faster=0` (ratio ~0.26–0.32)

## Objective

1. Align H4 probe with INT `Equal` enum fix (no dirty local needed)
2. Add a transfer-amortized terrain GPU path where compute can dominate wall time
3. Keep milestone at **25%** until Desktop reports `gpu_faster=1` on this harness (and multi-driver remains open)

## Why single-kernel timing lost

Desktop wall `gpu_submit_poll_us` includes SSBO upload + dispatch + ClientWaitSync + readback per kernel. TerrainFieldMax2/BiomeIdBucket are tiny ALU; transfer wins → `gpu_faster=0`.

## Delivered

| Piece | Change |
| --- | --- |
| Probe | Remaining lowercase `ShadowVerdict::equal` → `Equal` (match INT `77c2aa47`) |
| `GlDevice::dispatchPipeline` | One upload; `passes` × [Max2→ClampU16→BiomeIdBucket]; one wait; one readback |
| Probe `gain_pipeline` | Default n≈851968 (65536×edge), iters=8, passes=16; env `STAR_WORLDGEN_GL_GAIN_*` |
| Test | `TerrainGainPipelineCpuReferenceMatchesStackedKernels` (+ GL check when claimed) |

## Desktop next

```bat
set STAR_WORLDGEN_GL_GAIN_REPEATS=65536
set STAR_WORLDGEN_GL_GAIN_ITERS=8
set STAR_WORLDGEN_GL_GAIN_PASSES=32
world_generation_gl_compute_probe.exe
```

Look for `gain_pipeline: ... gpu_faster=1`. Still **not** a 50% claim alone (need Project review + multi-driver).

## Cloud validation

- Flag OFF: WorldGenerationComputeTest **319/319 PASS**
- No DenseUint32 horizontal work

## Compatibility

- Stub `dispatchPipeline` returns false
- Normal builds unchanged
- No WorldThread wiring; no 50% claim
