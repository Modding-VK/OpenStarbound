# H4 GPU lote72 — HalfAvgBits/MaxPopPick/MinPopPick + rejectedDuplicatesToken

**Date:** 2026-11-28
**Branch:** `cursor/h4-gpu-shadow-9f77`
**Tip:** `0b2d7d4c`
**Base tip absorbed by INT:** `93588277` → `13bf2bc0`
**Milestone:** 25% (unchanged)

## Objective
Extend DenseUint32 Half* catalog with average-bits and max/min pop pick fills plus one reject-set fingerprint alias (asset-free).

## Delivered
1. `DenseUint32HalfAvgBitsNibble` (425) — (pop(n)+pop(p))/2
2. `DenseUint32HalfMaxPopPickNibble` (426) — pop(n)>=pop(p) ? n : p
3. `DenseUint32HalfMinPopPickNibble` (427) — pop(n)<=pop(p) ? n : p
4. `rejectedDuplicatesToken()` — alias of `rejectedDuplicatesFingerprint()`

## Evidence
- `world_generation_contract_tests --gtest_filter=WorldGenerationComputeTest.*` → **304/304 PASS**

## Compatibility
CPU+GL parity under `STAR_ENABLE_WORLDGEN_GL_COMPUTE`; no assets/smoke; no live-world write path change.

## Next
Idle: `phase: ready-for-integration` for Integrator absorb window.
