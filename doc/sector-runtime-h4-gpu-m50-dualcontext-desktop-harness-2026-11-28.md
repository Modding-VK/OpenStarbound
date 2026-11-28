# H4 GPU milestone-50 — dual-context Desktop harness (path 1)

**Date:** 2026-11-28  
**Branch:** `cursor/h4-gpu-shadow-9f77`  
**Milestone:** **25%** (not claimed)  
**DenseUint32 horizontal:** STOPPED  
**Prior tip absorbed by INT:** `d746256c` → `d5983159`  
**Auth window:** Desktop GPU até **2026-11-29**

## Choice

Among (1) dual-context Desktop, (2) limited live publish, (3) 2º vendor:

**Chose (1)** — auth expires tomorrow; Desktop-V-Pessoal idle + approved;
cloud has no `/dev/dri`. 2º vendor still needs a second GPU host (parked).
Live publish remains a later gate after dual-context evidence.

## Delivered (this tip — harness, not claim)

| Piece | Change |
| --- | --- |
| Probe | `--- m50 dual-context WorldThread ---` with rehearsal **OFF** |
| Evidence lines | `dual_context: role=WorldThreadHidden(...) renderer_share=0 status='...'` |
| GlDevice | `contextRoleName`; status reflects live role |
| Test (GL-ON) | `RealGlDualContextWorldThreadHidden` |

Cloud stub suite still passes. Desktop must run GL-ON to close the observation.

## Desktop procedure (Desktop-V-Pessoal)

Checkout tip of this branch (or INT after absorb). Build:

```bat
cmake -S source -B ../build-h4-gl ^
  -DSTAR_ENABLE_WORLDGEN_GL_COMPUTE=ON -DSTAR_BUILD_GUI=ON ^
  -DBUILD_TESTING=ON -DSTAR_ENABLE_TRACY=OFF
cmake --build ../build-h4-gl --config RelWithDebInfo --target world_generation_gl_compute_probe
cmake --build ../build-h4-gl --config RelWithDebInfo --target world_generation_contract_tests
```

Run:

```bat
set STAR_WORLDGEN_GL_GAIN_REPEATS=65536
set STAR_WORLDGEN_GL_GAIN_ITERS=8
set STAR_WORLDGEN_GL_GAIN_PASSES=32
world_generation_gl_compute_probe.exe > h4-m50-gl-probe-dualcontext-run.txt

world_generation_contract_tests.exe --gtest_filter=WorldGenerationComputeTest.RealGlDualContextWorldThreadHidden:WorldGenerationComputeTest.RealGlDeferredTerrainFieldCompletesViaPump > h4-m50-gl-contract-dualcontext-run.txt
```

### Required log lines

- `dual_context: role=WorldThreadHidden(...) renderer_share=0`
- `dual_context: WorldThreadHidden OK; renderer_share unsupported (expected)`
- `PASS: ... + dual_context`
- Contract: `RealGlDualContextWorldThreadHidden` PASS
- Manifest: bytes, SHA256, UTC, `git rev-parse --short HEAD`, vendor/renderer — no personal paths

## What this does / does not close

**Closes (when Desktop PASS):** observation that WorldThread owns a real hidden
GL compute context with live GpuOpenGlCompute (rehearsal off).

**Does not close:** renderer↔compute **share** (`renderer_share` stays 0);
live sector publish; 2º vendor; 50% claim; GPU-05 measured cost.

## Next after Desktop evidence

1. Consume log SHA256 into progress + flip `desktop_validation` note
2. Then path (2) or (3) as Project prioritizes
