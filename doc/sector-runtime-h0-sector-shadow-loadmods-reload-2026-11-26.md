# H0 / VAL-02: sombra vs loadMods com reload

Data: 26/11/2026 UTC. Branch `Experimental`, base `f2cadf47`.

[Falha de produtor Root](sector-runtime-h0-sector-shadow-producer-failure-root-2026-11-26.md)
· [Sombra vs loadMods](sector-runtime-h0-sector-shadow-loadmods-2026-11-26.md)
· [Progressão](sector-runtime/progress.md)

## Estado: em validação

`SectorShadowSurvivesLoadModsWithReloadUntilOwnerDiscards` — após o fill,
`loadMods(..., true)` aplica o override imediatamente (`marker` 9); fontes
antigas ficam inválidas; a identidade do produto da sombra ainda `matches`;
discard sem publicação.

CPU-01/H0/RAM-01 em 25%. Sem Actions.

## Validação

ASan/UBSan: 1×5; **76/76** RealBlockInfo.

## Próximo passo

Invalidate Root em
[invalidate Root terrestre](sector-runtime-h0-sector-shadow-task-invalidate-root-2026-11-26.md).
Segue gates H0 antes de H1. Não disparar Actions.
