# H4 GPU milestone-50 — GL owner thread + non-blocking fence harness

**Date:** 2026-11-28
**Branch:** `cursor/h4-gpu-shadow-9f77`
**Tip:** `PENDING`
**Milestone target:** 50% (in progress; not claimed)
**DenseUint32 horizontal coverage:** STOPPED (user order)

## Objective
Smallest vertical step toward GPU-02/GPU-05 acceptance for milestone 50%:
GL context owner-thread affinity and a fence harness that WorldThread can pump
without blocking on `glClientWaitSync`.

## Delivered
1. `WorldGenerationComputeGlDevice::adoptOwnerThread/clearOwnerThread/hasOwnerThread/isOwnerThread`
2. Fence harness: `createFence`, `pollFence`, `pumpFences` (non-blocking), `retireFence`, `pendingFenceCount`
3. Stub build (`STAR_ENABLE_WORLDGEN_GL_COMPUTE` off): Pending→Signaled via `pumpFences` (unit-testable without GPU)
4. GL build path: `glFenceSync` + `glClientWaitSync(..., timeout=0)` for polls; sync `dispatch` still waits (existing API)
5. Test `GlOwnerThreadAndFenceHarnessNonBlocking` (foreign thread refused; pump non-blocking)

## Evidence
- `world_generation_contract_tests --gtest_filter=WorldGenerationComputeTest.*` → **315/315 PASS**
- No `/dev/dri`, no `nvidia-smi`, no `glxinfo` in this cloud agent environment
- Real GL 4.3+ compute claim, multi-driver shadow, and measurable terrain/biome gain **not validated here**

## Compatibility
- Normal builds remain CPU-only (`gpuOpenGlCompute=false`)
- No new DenseUint32/Half*/rejectedDuplicates APIs
- No live-world writes; no WorldThread wiring yet (next vertical step)

## Blockers / Desktop V Pessoal
This VM cannot exercise real OpenGL 4.3+ compute or multi-driver shadow.
**Need Desktop V Personal (local GPU)** for:
- `STAR_ENABLE_WORLDGEN_GL_COMPUTE=ON` probe claiming `gpuOpenGlCompute`
- GLsync fence latency vs CPU reference
- Terrain/biome kernel gain measurements
- Multi-driver shadow matrix on real vendors

## Next vertical steps (ordered)
1. Wire service deferred GPU tickets to `createFence`/`pumpFences` (WorldThread-safe)
2. TerrainField/Biome exact integer kernels with CPU≡GL + timing harness (on Desktop V)
3. Multi-driver shadow slots filled from `lastProbeInfo()` vendor/renderer
