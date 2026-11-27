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

Stale oceano/Island em
[ocean-island stale](sector-runtime-h0-sector-shadow-ocean-island-stale-2026-11-26.md).
Segue gates H0 restantes. Não disparar Actions.
