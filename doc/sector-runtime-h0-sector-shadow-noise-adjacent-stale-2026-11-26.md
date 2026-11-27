# H0 / VAL-02: noise/adjacente + stale de template

Data: 26/11/2026 UTC. Branch `Experimental`, base `29dff4de`.

[Ocean/Island stale](sector-runtime-h0-sector-shadow-ocean-island-stale-2026-11-26.md)
· [Progressão](sector-runtime/progress.md)

## Estado: em validação

- `NoiseCacheSectorShadowDiscardsWhenTemplateRevisionGoesStale`
- `AdjacentSectorShadowDiscardsWhenTemplateRevisionGoesStale`

Bump de `generationRevision`; sombra deixa de `matches`; inputs capturados no
snapshot pré-bump. Sem publicação. CPU-01/H0/RAM-01 em 25%. Sem Actions.

## Validação

ASan/UBSan: 1×5; **105/105** RealBlockInfo.

## Próximo passo

Oceano concorrente em
[concurrent ocean](sector-runtime-h0-sector-shadow-concurrent-ocean-2026-11-26.md).
Segue gates H0 restantes. Não disparar Actions.
