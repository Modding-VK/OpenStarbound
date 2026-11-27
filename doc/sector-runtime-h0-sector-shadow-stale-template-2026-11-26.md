# H0 / VAL-02: sombra com template stale

Data: 26/11/2026 UTC. Branch `Experimental`, base `ad1af326`.

[IslandSurface+pressão](sector-runtime-h0-sector-shadow-island-pressure-2026-11-26.md)
· [Progressão](sector-runtime/progress.md)

## Estado: em validação

`TemplateRevisionBumpFailsOwnerMatchesWithoutDroppingPayload` (contrato) e
`SectorShadowDiscardsWhenTemplateRevisionGoesStale` (Root): após preencher a
sombra, um bump de `generationRevision` (addCustomTerrainRegion) faz a
expectativa viva falhar em `matches` enquanto o payload privado permanece até
`discard`. Os static inputs capturados ficam no snapshot pré-bump.

Sem publicação, workers ou Actions. CPU-01/H0/RAM-01 em 25%.

## Validação

ASan/UBSan: 8 sombras × 10 → **785/785** contrato; stale × 5; **69/69** RealBlockInfo.

## Próximo passo

Handoff de task coberto em
[handoff task+sombra](sector-runtime-h0-sector-shadow-task-handoff-2026-11-26.md).
Segue contexto profundo/assets. Não disparar Actions.
