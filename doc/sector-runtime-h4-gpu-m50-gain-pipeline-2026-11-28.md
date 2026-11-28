# H4 GPU milestone-50 — terrain gain pipeline (no 50% claim)

**Date:** 2026-11-28  
**Branch:** `cursor/h4-gpu-shadow-9f77`  
**Aligns with INT:** `77c2aa47` (`ShadowVerdict::Equal`); Desktop gain tip `509aaed0`  
**Desktop evidence:** `gain_pipeline` **`gpu_faster=1`** (ratio≈17–23, NVIDIA); single-kernel still 0

## Objective

1. Align H4 probe with INT `Equal` enum fix (no dirty local needed)
2. Add a transfer-amortized terrain GPU path where compute can dominate wall time
3. Keep milestone at **25%** — `gpu_faster=1` achieved on one vendor; multi-driver still open

## Why single-kernel timing lost

Desktop wall `gpu_submit_poll_us` includes SSBO upload + dispatch + ClientWaitSync + readback per kernel. TerrainFieldMax2/BiomeIdBucket are tiny ALU; transfer wins → `gpu_faster=0`.

## Delivered

| Piece | Change |
| --- | --- |
| Probe | Remaining lowercase `ShadowVerdict::equal` → `Equal` (match INT `77c2aa47`) |
| `GlDevice::dispatchPipeline` | One upload; `passes` × [Max2→ClampU16→BiomeIdBucket]; one wait; one readback |
| Probe `gain_pipeline` | Default n≈851968 (65536×edge), iters=8, passes=16; env `STAR_WORLDGEN_GL_GAIN_*` |
| Test | `TerrainGainPipelineCpuReferenceMatchesStackedKernels` (+ GL check when claimed) |

## Desktop result (consumed)

Tip `509aaed0`: `gpu_faster=1`, ratio≈17.4 (passes=32) / ≈23.3 (passes=64), equal=1.  
Store: `internal/h4-desktop-gl-probe-gain-pipeline-2026-11-28.md`.

**Next:** [multidriver path](sector-runtime-h4-gpu-m50-multidriver-path-2026-11-28.md) — 2º vendor.

## Cloud validation

- Flag OFF: WorldGenerationComputeTest **319/319 PASS**
- No DenseUint32 horizontal work

## Compatibility

- Stub `dispatchPipeline` returns false
- Normal builds unchanged
- No WorldThread wiring; no 50% claim
