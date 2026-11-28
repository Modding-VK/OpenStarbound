# H4 GPU lote75 — HalfNePopFill/PopMin/PopMax + rejectedDuplicatesCipher

**Date:** 2026-11-28
**Branch:** `cursor/h4-gpu-shadow-9f77`
**Tip:** `c77ead4b`
**Base tip absorbed by INT:** `ebb7fe61` → `e07fcdbc`
**Milestone:** 25% (unchanged)

## Objective
Extend DenseUint32 Half* catalog with ne-pop fill and min/max popcount writes plus one reject-set fingerprint alias (asset-free).

## Delivered
1. `DenseUint32HalfNePopFillNibble` (434)
2. `DenseUint32HalfPopMinNibble` (435)
3. `DenseUint32HalfPopMaxNibble` (436)
4. `rejectedDuplicatesCipher()` — alias of `rejectedDuplicatesFingerprint()`

## Evidence
- `world_generation_contract_tests --gtest_filter=WorldGenerationComputeTest.*` → **310/310 PASS**

## Compatibility
CPU+GL parity under `STAR_ENABLE_WORLDGEN_GL_COMPUTE`; no assets/smoke.

## Next
Idle: `phase: ready-for-integration` for Integrator absorb window.
