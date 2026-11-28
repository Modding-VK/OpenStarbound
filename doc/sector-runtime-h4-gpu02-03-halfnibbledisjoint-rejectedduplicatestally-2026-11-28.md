# H4 GPU lote70 — HalfDisjointFill/EqualPop/GtPop + rejectedDuplicatesTally

**Date:** 2026-11-28
**Branch:** `cursor/h4-gpu-shadow-9f77`
**Tip:** `c9126568`
**Base tip absorbed by INT:** `93588277` → `13bf2bc0`
**Milestone:** 25% (unchanged)

## Objective
Extend DenseUint32 Half* catalog with three fill kernels and one reject-set observation alias (asset-free).

## Delivered
1. `DenseUint32HalfDisjointFillNibble` (419) — fill when A∩B empty else 0
2. `DenseUint32HalfEqualPopNibble` (420) — fill when pop(A)==pop(B)
3. `DenseUint32HalfGtPopNibble` (421) — fill when pop(A)>pop(B)
4. `rejectedDuplicatesTally()` — alias of `rejectedDuplicatesCount()`

## Evidence
- `world_generation_contract_tests --gtest_filter=WorldGenerationComputeTest.*` → **300/300 PASS**
- Commits: kernels + API (this tip)

## Compatibility
CPU+GL parity under `STAR_ENABLE_WORLDGEN_GL_COMPUTE`; no assets/smoke; no live-world write path change.

## Next
lote71: HalfLtPop/GePop/LePop + reject-set Archive alias.
