# H4 GPU lote76 — HalfOddPopFill/PopMod3/Maj3Fill + rejectedDuplicatesDocket

**Date:** 2026-11-28
**Branch:** `cursor/h4-gpu-shadow-9f77`
**Tip:** `045ab532`
**Base tip absorbed by INT:** `2796b301` → `a89ad754`
**Milestone:** 25% (unchanged)

## Objective
Extend DenseUint32 Half* catalog with odd/mod3/majority pop fills and one reject-set list alias (asset-free).

## Delivered
1. `DenseUint32HalfOddPopFillNibble` (437)
2. `DenseUint32HalfPopMod3Nibble` (438)
3. `DenseUint32HalfMaj3FillNibble` (439)
4. `rejectedDuplicatesDocket()` — alias of `rejectedDuplicatesList()`

## Evidence
- `world_generation_contract_tests --gtest_filter=WorldGenerationComputeTest.*` → **312/312 PASS**

## Compatibility
CPU+GL parity under `STAR_ENABLE_WORLDGEN_GL_COMPUTE`; no assets/smoke.

## Next
lote77: HalfZeroPopFill/FullPopFill/PopEqCountFill + Evacuate.
