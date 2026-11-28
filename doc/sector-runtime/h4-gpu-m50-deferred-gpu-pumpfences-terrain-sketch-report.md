# H4 GPU milestone-50 — deferred GPU tickets → pumpFences + terrain/biome sketch

**Date:** 2026-11-28
**Branch:** `cursor/h4-gpu-shadow-9f77`
**Tip:** `PENDING`
**Milestone:** 25% (target 50%; **not claimed**)
**DenseUint32 horizontal:** STOPPED

## Objective
Cloud-safe vertical follow-up after GL owner/fence harness:
1. Wire deferred `GpuOpenGlCompute` tickets through `createFence` / `pumpFences`
2. Sketch TerrainField/Biome CPU kernels without claiming 50% or Desktop probe

## Delivered
1. `setGpuFenceRehearsal(bool)` — admit deferred GPU tickets without `gpuOpenGlCompute` (CPU materialize)
2. `Entry.hasGlFence` / `glFence` — armed on deferred GPU submit; retired on Ready/Failed/Cancel
3. `pump()` calls `GlDevice::pumpFences` then completes signaled GPU tickets (non-blocking)
4. Terrain/biome sketch (CPU): `TerrainFieldMax2/Min2/ClampU16`, `BiomeIdBucket` (443–446)
5. Tests: DeferredGpuTicketsCompleteViaPumpFencesRehearsal, GpuSubmitRejectedWithoutCapabilityOrRehearsal, TerrainBiomeFieldSketchExactCpuAndShadow

## Evidence
- `WorldGenerationComputeTest.*` → **318/318 PASS**
- No real GL claim; rehearsal never sets `gpuOpenGlCompute`
- Awaiting **Desktop V Pessoal** for STAR_ENABLE_WORLDGEN_GL_COMPUTE probe, GL shaders for terrain/biome, measurable gain, multi-driver shadow

## Compatibility
- Normal builds remain CPU-first; rehearsal is opt-in and explicit
- No new DenseUint32/Half*/rejectedDuplicates APIs
- No WorldThread live wiring yet; no 50% claim

## Next
1. Desktop V: enable GL compute, attach real GLsync on deferred GPU dispatch path
2. GL shaders for TerrainField/Biome + timing harness vs CPU
3. WorldThread adopts service+GlDevice owners and pumps fences on update
