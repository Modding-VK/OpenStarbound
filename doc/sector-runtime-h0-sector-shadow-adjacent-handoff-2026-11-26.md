# H0 / VAL-02: setor adjacente + handoff/invalidate

Data: 26/11/2026 UTC. Branch `Experimental`, base `93debe36`.

[Noise lifecycle](sector-runtime-h0-sector-shadow-noise-lifecycle-2026-11-26.md)
· [Progressão](sector-runtime/progress.md)

## Estado: em validação

Setor `{1,0}` / `tileRegion` 32–64×0–32 sob composto + sombra:

- `AdjacentShadowTaskHandoffRetiresStale` — handoff + Stale
- `AdjacentShadowTaskInvalidateDiscardsFilled` — invalidate → Cancelled

Sem publicação. CPU-01/H0/RAM-01 em 25%. Sem Actions.

## Validação

ASan/UBSan: 1×5; **91/91** RealBlockInfo.

## Próximo passo

Abandon/fail adjacente e assets sob variantes. Não disparar Actions.
