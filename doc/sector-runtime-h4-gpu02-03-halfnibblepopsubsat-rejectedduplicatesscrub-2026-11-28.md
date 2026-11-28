# H4 GPU lote74 — HalfPopSubSat/PopMul/EvenPopFill + rejectedDuplicatesScrub

**Date:** 2026-11-28
**Branch:** `cursor/h4-gpu-shadow-9f77`
**Tip:** `279bf77d`
**Base tip absorbed by INT:** `ebb7fe61` → `e07fcdbc`
**Milestone:** 25% (unchanged)

## Objective
Extend DenseUint32 Half* catalog with sat-sub/mul/even-pop fills and one reject-set clear alias (asset-free).

## Delivered
1. `DenseUint32HalfPopSubSatNibble` (431)
2. `DenseUint32HalfPopMulNibble` (432)
3. `DenseUint32HalfEvenPopFillNibble` (433)
4. `rejectedDuplicatesScrub()` — alias of `clearRejectedDuplicates()`

## Evidence
- `world_generation_contract_tests --gtest_filter=WorldGenerationComputeTest.*` → **308/308 PASS**

## Compatibility
CPU+GL parity under `STAR_ENABLE_WORLDGEN_GL_COMPUTE`; no assets/smoke.

## Next
lote75: HalfNePopFill/PopMin/PopMax + Cipher; then idle.
