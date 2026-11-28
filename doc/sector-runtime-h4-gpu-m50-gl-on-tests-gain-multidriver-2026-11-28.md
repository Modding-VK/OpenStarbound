# H4 GPU milestone-50 — GL-ON test alignment + gain/multi-driver evidence

**Date:** 2026-11-28  
**Branch:** `cursor/h4-gpu-shadow-9f77`  
**Desktop probe tip:** `29672021` (INT merge containing `c624921b`) — PASS harness  
**Milestone:** remains **25%** (no 50% claim)

## Objective

1. Close the three contract tests that FAIL on real GL because they assumed stub/rehearsal
2. Extend probe with amortized timing + `driver_evidence` fingerprints for multi-driver collection
3. Keep DenseUint32 horizontal STOPPED; do not claim 50% until gain + multi-driver criteria land

## Desktop FAIL → fix

| Test | FAIL on RTX | Fix |
| --- | --- | --- |
| `DeferredGpuTicketsCompleteViaPumpFencesRehearsal` | `pump()=0` with live GLsync | Force `gpuOpenGlCompute=false` so rehearsal path stays cloud+Desktop stable |
| `GpuSubmitRejectedWithoutCapabilityOrRehearsal` | submit accepted (`gpu=1`) | Construct service with explicit `gpuOpenGlCompute=false` |
| `GlOwnerThreadAndFenceHarnessNonBlocking` | residual fences + idle GLsync already Signaled | `shutdown()` first; accept Pending **or** Signaled on first poll |

Added (GL-ON only): `RealGlDeferredTerrainFieldCompletesViaPump` — loops non-blocking `pump()` until Ready.

`WorldGenerationComputeGlDevice::shutdown()` now resets the probe `once_flag` so later probes can recreate the context (test / harness isolation).

## Gain / multi-driver (evidence, not claim)

Probe additions:

- Timing: warmup + `iters` (default 8) × larger corpus (default repeats 16384); prints `ratio_cpu_over_gpu` and `gpu_faster`
- Env: `STAR_WORLDGEN_GL_TIMING_REPEATS`, `STAR_WORLDGEN_GL_TIMING_ITERS`
- `driver_evidence`: vendor/renderer/version + `caps_fp` + `shadow_fp` for Terrain/Biome

**50% gain gate (still open):** need `gpu_faster=1` on a production-relevant corpus (or documented sector-batch wall time), not the 13-value edge corpus alone. Desktop RTX tip `29672021` showed transfer-dominated times (`gpu_submit_poll_us` > `cpu_us`) — expected for tiny N.

**Multi-driver gate (still open):** collect `driver_evidence` on ≥2 vendors (e.g. NVIDIA done; Intel/AMD or second NVIDIA driver family). Require matching Terrain/Biome shadow verdicts / comparable `shadow_fp` semantics; record vendor strings + hashes in a follow-up report.

## Cloud validation

- Flag OFF: `WorldGenerationComputeTest.*` (includes fixed rehearsal/reject/fence tests)
- Real GL path covered by Desktop probe + new ifdef test when flag ON

## Compatibility

- Normal builds unchanged in claim behavior
- No DenseUint32 / Half* / rejectedDuplicates APIs
- No 50% claim in this tip

## Next

1. Desktop V: rebuild tip, re-run focused contract tests (expect 0 FAIL) + probe with larger timing env
2. Second driver/machine: capture `driver_evidence` line
3. Only then evaluate GPU-03/04 50% inputs with Project
