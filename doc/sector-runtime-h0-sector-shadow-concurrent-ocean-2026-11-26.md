# H0 / VAL-02: sombras oceano concorrentes no ledger

Data: 26/11/2026 UTC. Branch `Experimental`, base `1d9b85c3`.

[Noise/adjacent stale](sector-runtime-h0-sector-shadow-noise-adjacent-stale-2026-11-26.md)
· [Progressão](sector-runtime/progress.md)

## Estado: em validação

`ConcurrentOceanSectorShadowsMatchIndependentSectors` — dois setores oceano
`{0,0}` e `{1,0}` admitidos juntos no ledger, BaseTiles independentes, discard
parcial e total. Sem publicação.

CPU-01/H0/RAM-01 em 25%. Sem Actions.

## Validação

ASan/UBSan: 1×5; **106/106** RealBlockInfo.

## Próximo passo

Handoffs concorrentes oceano em
[concurrent ocean handoff](sector-runtime-h0-sector-shadow-concurrent-ocean-handoff-2026-11-27.md).
Segue gates H0 restantes. Não disparar Actions.
