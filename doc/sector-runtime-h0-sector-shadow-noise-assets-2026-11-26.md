# H0 / VAL-02: noises/caches + hotReload/loadMods

Data: 26/11/2026 UTC. Branch `Experimental`, base `72d88206`.

[Ocean/Island loadMods](sector-runtime-h0-sector-shadow-ocean-island-loadmods-2026-11-26.md)
· [Progressão](sector-runtime/progress.md)

## Estado: em validação

- `NoiseCacheSectorShadowSurvivesAssetHotReloadUntilOwnerDiscards`
- `NoiseCacheSectorShadowSurvivesLoadModsUntilOwnerDiscards`

LookupSources invalidam; sombra Cache/Perlin permanece até discard. Sem
publicação. CPU-01/H0/RAM-01 em 25%. Sem Actions.

## Validação

ASan/UBSan: 1×5; **99/99** RealBlockInfo.

## Próximo passo

Adjacente assets em
[adjacent assets](sector-runtime-h0-sector-shadow-adjacent-assets-2026-11-26.md).
Segue gates H0 restantes. Não disparar Actions.
