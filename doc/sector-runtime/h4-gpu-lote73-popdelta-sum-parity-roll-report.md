# H4 GPU lote73 — HalfPopDelta/PopSum/PopParityFill + rejectedDuplicatesRoll

**Date:** 2026-11-28
**Branch:** `cursor/h4-gpu-shadow-9f77`
**Tip:** `73db4068`
**Base tip absorbed by INT:** `ebb7fe61` → `e07fcdbc`
**Milestone:** 25% (unchanged)

## Objective
Extend DenseUint32 Half* catalog with popcount delta/sum/parity fills and one reject-set list alias (asset-free).

## Delivered
1. `DenseUint32HalfPopDeltaNibble` (428)
2. `DenseUint32HalfPopSumNibble` (429)
3. `DenseUint32HalfPopParityFillNibble` (430)
4. `rejectedDuplicatesRoll()` — alias of `rejectedDuplicatesList()`

## Evidence
- `world_generation_contract_tests --gtest_filter=WorldGenerationComputeTest.*` → **306/306 PASS**

## Compatibility
CPU+GL parity under `STAR_ENABLE_WORLDGEN_GL_COMPUTE`; no assets/smoke.

## Next
lote74: HalfPopSubSat/PopMul/EvenPopFill + Scrub.
