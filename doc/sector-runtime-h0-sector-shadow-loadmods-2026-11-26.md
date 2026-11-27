# H0 / VAL-02: sombra vs loadMods

Data: 26/11/2026 UTC. Branch `Experimental`, base `029af606`.

[Sombra vs hotReload](sector-runtime-h0-sector-shadow-asset-hotreload-2026-11-26.md)
· [Progressão](sector-runtime/progress.md)

## Estado: em validação

`SectorShadowSurvivesLoadModsUntilOwnerDiscards` preenche a sombra, chama
`loadMods(..., false)` (epoch muda, assets ainda deferidos), prova LookupSources
antigas inválidas e identidade do produto ainda `matches`, descarta, depois
`reload` aplica o override (`marker` 9). Sem publicação.

CPU-01/H0/RAM-01 em 25%. Sem Actions.

## Validação

ASan/UBSan: 1×5; **73/73** RealBlockInfo (após suite completa).

## Próximo passo

Gates H0 restantes antes de H1, ou pressão/mailbox sem Actions. Não disparar
Actions.
