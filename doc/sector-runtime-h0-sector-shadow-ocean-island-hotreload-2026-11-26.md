# H0 / VAL-02: oceano/Island + hotReload de assets

Data: 26/11/2026 UTC. Branch `Experimental`, base `637e6b5f`.

[Adjacent lifecycle](sector-runtime-h0-sector-shadow-adjacent-lifecycle-2026-11-26.md)
· [Progressão](sector-runtime/progress.md)

## Estado: em validação

- `OceanSectorShadowSurvivesAssetHotReloadUntilOwnerDiscards`
- `IslandSurfaceSectorShadowSurvivesAssetHotReloadUntilOwnerDiscards`

LookupSources invalidam; identidade de produto / sombra permanecem até discard.
Sem publicação. CPU-01/H0/RAM-01 em 25%. Sem Actions.

## Validação

ASan/UBSan: 1×5; **95/95** RealBlockInfo.

## Próximo passo

loadMods sob oceano/Island/noise. Não disparar Actions.
