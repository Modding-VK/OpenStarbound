# H4 GPU milestone-50 — limited live publish (in-memory sink)

**Date:** 2026-11-28  
**Branch:** `cursor/h4-gpu-shadow-9f77`  
**Milestone:** **25%** (not claimed)  
**DenseUint32:** STOPPED / refused by live-publish kernel allowlist  
**Desktop dual_context consumed:** INT `462e2cbc` PASS

## Decision

NVIDIA-only **does not** authorize H4 50% (see criteria ledger). Next highest
impact executable without a second GPU: **limited live publish** with
safeguards — not a WorldStorage write.

## Delivered

| Piece | Behavior |
| --- | --- |
| Default | `applyToLiveWorld` → `RefusedLiveWorld` |
| Opt-in | `livePublishLimited` (config / `setLivePublishLimited`) |
| Allowlist | TerrainField* / BiomeIdBucket only |
| Cap | ≤ 1 048 576 words; fingerprint must match values |
| Sink | `WorldGenerationComputeLivePublication` in-memory only |
| Never | tiles, entities, `generateQueue`, mods/saves |

## Config

```json
"worldGenerationCompute": {
  "worldThreadPump": true,
  "livePublishLimited": false
}
```

## Cloud validation

- Extended `PublishGateStagesAndRefusesLiveWorld` + `LivePublishLimitedRefusesDenseAndOversized`
- Full `WorldGenerationComputeTest.*` run on stub build

## Compatibility

- Default off → no behavior change for dedicated servers
- Even when on: no WorldServer mutation in this tip
- No 50% claim

## Next

1. Optional: preparation feeder consumes the live sink
2. 2º vendor when available
3. Full sector adoption remains a separate audited tip
