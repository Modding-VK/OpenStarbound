# H0 / VAL-02: setor adjacente + hotReload/loadMods

Data: 26/11/2026 UTC. Branch `Experimental`, base `4aac3d42`.

[Noise assets](sector-runtime-h0-sector-shadow-noise-assets-2026-11-26.md)
· [Progressão](sector-runtime/progress.md)

## Estado: em validação

Setor `{1,0}` sob composto + sombra:

- `AdjacentSectorShadowSurvivesAssetHotReloadUntilOwnerDiscards`
- `AdjacentSectorShadowSurvivesLoadModsUntilOwnerDiscards`

LookupSources invalidam; sombra permanece até discard. Sem publicação.
CPU-01/H0/RAM-01 em 25%. Sem Actions.

## Validação

ASan/UBSan: 1×5; **101/101** RealBlockInfo.

## Próximo passo

Gates H0 restantes antes de H1. Não disparar Actions.
