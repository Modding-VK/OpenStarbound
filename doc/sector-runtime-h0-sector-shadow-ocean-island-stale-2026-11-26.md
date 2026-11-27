# H0 / VAL-02: oceano/Island + stale de template

Data: 26/11/2026 UTC. Branch `Experimental`, base `26038f8b`.

[Adjacent assets](sector-runtime-h0-sector-shadow-adjacent-assets-2026-11-26.md)
· [Progressão](sector-runtime/progress.md)

## Estado: em validação

- `OceanSectorShadowDiscardsWhenTemplateRevisionGoesStale`
- `IslandSurfaceSectorShadowDiscardsWhenTemplateRevisionGoesStale`

Bump de `generationRevision` via custom terrain; sombra deixa de `matches` a
expectativa viva, permanece ativa até discard; inputs capturados ficam no
snapshot pré-bump. Sem publicação.

CPU-01/H0/RAM-01 em 25%. Sem Actions.

## Validação

ASan/UBSan: 1×5; **103/103** RealBlockInfo.

## Próximo passo

Gates H0 restantes antes de H1. Não disparar Actions.
