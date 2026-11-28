# H4 GPU milestone-50 — criteria ledger (NVIDIA-scoped, no claim)

**Date:** 2026-11-28  
**Branch:** `cursor/h4-gpu-shadow-9f77`  
**H4 tip (this ledger tip):** pending push after live-publish limited  
**Desktop dual_context tip:** INT `462e2cbc` (contains `35f196c6`)  
**Milestone percentage:** **25%** — **not claimed as 50%**

## Contract ruling: NVIDIA-only does **not** authorize 50%

Canonical multi-driver path
([multidriver](sector-runtime-h4-gpu-m50-multidriver-path-2026-11-28.md)) and
progress rubric forbid claiming H4 **50%** with a single vendor. This ledger
records what is PASS under NVIDIA so Project can see gap-to-50% without
raising the percentage.

## Criteria matrix

| Criterion | Status | Evidence |
| --- | --- | --- |
| CPU reference + shadow Equal | PASS | contract suite |
| Terrain/Biome GL shadow Equal (NVIDIA) | PASS | Desktop `509aaed0` / `462e2cbc` |
| `gain_pipeline` `gpu_faster=1` (NVIDIA) | PASS | ratio ~15–23; tips `509aaed0`, `462e2cbc` |
| WorldThread runtime + fence pump | PASS | tip `76ab0fa2`+ |
| Dual-context WorldThreadHidden (rehearsal off) | PASS | Desktop `462e2cbc`; probe SHA256 `9D52D121…`; contract `AB1C1B57…` |
| Renderer↔compute **share** | OPEN | `renderer_share=0` by design |
| Limited live publish (in-memory sink) | THIS TIP | opt-in; not WorldStorage |
| Live sector / tile / `generateQueue` adoption | OPEN | still refused |
| ≥2º vendor / driver family | OPEN | parked; blocks GPU-03/04 50% |
| Measured GPU-05 transport cost | OPEN | heuristic only |
| Project review (harness ≠ setores/s) | OPEN | required for adoption |

## Percentages

| Contract | Note | % |
| --- | --- | --- |
| GPU-01…04 | First cut remains; dual_context + limited sink do not complete 50% requirements | **25%** |
| H4 phase | Still 4/16 marcos | **25%** |
| Program | Unchanged | **5,6%** |

**Do not** interpret this ledger as a 50% claim or a “partial 50%” percentage.

## Next highest impact

1. Wire preparation feeder → limited sink (still no WorldStorage) **or**
2. Collect 2º vendor when a second GPU host exists
3. Keep renderer share and full sector adoption for later audited tips
