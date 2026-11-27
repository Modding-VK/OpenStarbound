# H0 / VAL-02: oceano/Island + loadMods

Data: 26/11/2026 UTC. Branch `Experimental`, base `e4af1e0d`.

[Ocean/Island hotReload](sector-runtime-h0-sector-shadow-ocean-island-hotreload-2026-11-26.md)
· [Progressão](sector-runtime/progress.md)

## Estado: em validação

- `OceanSectorShadowSurvivesLoadModsUntilOwnerDiscards`
- `IslandSurfaceSectorShadowSurvivesLoadModsUntilOwnerDiscards`

loadMods (sem reload imediato) invalida LookupSources; sombra permanece até
discard; reload aplica o marker do mod. Sem publicação.

CPU-01/H0/RAM-01 em 25%. Sem Actions.

## Validação

ASan/UBSan: 1×5; **97/97** RealBlockInfo.

## Próximo passo

Noise assets em
[noise assets](sector-runtime-h0-sector-shadow-noise-assets-2026-11-26.md).
Segue adjacente sob assets. Não disparar Actions.
