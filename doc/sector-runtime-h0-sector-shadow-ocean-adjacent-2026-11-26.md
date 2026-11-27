# H0 / VAL-02: sombra oceano e setor adjacente

Data: 26/11/2026 UTC. Branch `Experimental`, base `575aaf06`.

[Sombra por setor](sector-runtime-h0-sector-shadow-2026-11-26.md) · [Progressão](sector-runtime/progress.md)

## Estado: em validação

Dois casos Root opt-in sobre `WorldGenerationSectorShadow`:

- `SectorShadowOceanTerrestrialBaseTilesThenDiscards` — builder terrestre com
  oceano; BaseTiles privados do composto preservam líquido e comparam 32×32;
- `SectorShadowAdjacentSectorTerrestrialBaseTilesThenDiscards` — setor `{1,0}`
  com `tileRegion` 32×0–64×32; amostras em coordenadas mundiais, array privado
  local, `matches` e descarte sem publicação.

Sem Actions, sem workers/mailbox, sem ganho de setores/s. CPU-01/H0/RAM-01 em 25%.

## Validação

ASan/UBSan: 3 sombras × 5 repetições; **65/65** RealBlockInfo.

## Próximo passo

Noises/caches privados alinhados ao composto + sombra, ainda sem publicação.
Não disparar Actions.
