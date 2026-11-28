# H4 GPU lote77 — HalfZeroPopFill/FullPopFill/PopEqCountFill + rejectedDuplicatesEvacuate

**Date:** 2026-11-28
**Branch:** `cursor/h4-gpu-shadow-9f77`
**Tip:** `6d7d1765`
**Base tip absorbed by INT:** `2796b301` → `a89ad754`
**Milestone:** 25% (unchanged)

## Objective
Extend DenseUint32 Half* catalog with zero/full/eq-count pop fills and one reject-set clear alias (asset-free).

## Delivered
1. `DenseUint32HalfZeroPopFillNibble` (440)
2. `DenseUint32HalfFullPopFillNibble` (441)
3. `DenseUint32HalfPopEqCountFillNibble` (442)
4. `rejectedDuplicatesEvacuate()` — alias of `clearRejectedDuplicates()`

## Evidence
- `world_generation_contract_tests --gtest_filter=WorldGenerationComputeTest.*` → **314/314 PASS**

## Compatibility
CPU+GL parity under `STAR_ENABLE_WORLDGEN_GL_COMPUTE`; no assets/smoke.

## Next
lote78: HalfOnlyAPop/OnlyBPop/PopRatio + Glyph; then idle.
