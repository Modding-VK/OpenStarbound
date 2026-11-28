# H4 GPU milestone-50 — WorldStorage sector adopt: not cloud-safe → standby

**Date:** 2026-11-28  
**Branch:** `cursor/h4-gpu-shadow-9f77`  
**Prior tip absorbed by INT:** `7a4d7140` → `8a38889f`  
**Milestone:** **25%** (not claimed)  
**DenseUint32:** STOPPED

## Decision

Evaluated **WorldStorage sector adopt** after limited live publish sink.

**Verdict: not cloud-safe** in this environment / tip class. H4 enters
**documented standby** focused on the remaining 50% gate that is hardware-bound:
**≥2º vendor** ([multidriver path](sector-runtime-h4-gpu-m50-multidriver-path-2026-11-28.md)).

## Why WorldStorage adopt is not cloud-safe here

| Risk | Detail |
| --- | --- |
| Assets / world host | Cloud Project VM has no Starbound base assets; WorldStorage sector apply needs a real storage/world host |
| Semantic gap | Live publish sink holds Terrain/Biome **uint32 fields**, not `ServerTileSectorArray` BaseTiles; mapping into live tiles is a new product contract |
| Compatibility | Writing tiles / `generateQueue` / generationLevel touches mods/saves/multiplayer — AGENTS.md high-impact; needs audited tip + independent review |
| Existing shadow | `WorldGenerationSectorShadow` is BaseTiles-shaped and **must not** install into the live world — wrong vehicle for “WorldStorage adopt” |
| False stub | An `adoptToWorldStorage()` that only refuses would be another micro-stub series — rejected |

## What remains cloud-complete (no new claim)

- WorldThread runtime + dual_context Desktop PASS `462e2cbc`
- Limited live publish **sink** (opt-in, in-memory, default off) tip `7a4d7140`
- NVIDIA gain_pipeline / driver_evidence — **not** 50% alone ([ledger](sector-runtime-h4-gpu-m50-criteria-ledger-nvidia-2026-11-28.md))

## Standby: 2º vendor

H4 does **not** idle on inventing stubs. Standby means:

1. No further DenseUint32 / micro-prep on this front until Project assigns a
   second GPU host **or** authorizes a Desktop WorldStorage adopt spike with
   assets + review.
2. When a 2º vendor host appears: follow
   [multidriver path](sector-runtime-h4-gpu-m50-multidriver-path-2026-11-28.md)
   on tip ≥ `8a38889f` / H4 tip of this standby.
3. WorldStorage adopt stays **OPEN** on the criteria ledger until an audited
   tip with real storage host exists.

## Explicit non-claims

- No H4 **50%** claim
- No WorldStorage mutation in this tip (docs only)
- No renderer share claim

## Resume conditions

| Trigger | Action |
| --- | --- |
| 2º vendor worker available | Run multidriver probe; H4 consumes SHA256 |
| Project authorizes WorldStorage adopt on Desktop with assets | Separate audited tip; not this standby |
| User/Project redirects H4 | Follow new path; update `h4.md` |
