# H0 / VAL-02: sombra vs hotReload de assets

Data: 26/11/2026 UTC. Branch `Experimental`, base `a69debf2`.

[Handoffs concorrentes Root](sector-runtime-h0-sector-shadow-concurrent-handoff-root-2026-11-26.md)
· [Progressão](sector-runtime/progress.md)

## Estado: em validação

`SectorShadowSurvivesAssetHotReloadUntilOwnerDiscards` captura LookupSources +
composto, preenche a sombra, faz `hotReload` e mostra:

- fontes de lookup antigas ficam inválidas e o scope muda;
- a identidade do produto BaseTiles da sombra ainda `matches` (não é lease de
  assets);
- o dono descarta o payload privado sem publicação.

Documenta a fronteira: revalidar fontes/assets à parte antes de qualquer
caminho futuro de commit. CPU-01/H0/RAM-01 em 25%. Sem Actions.

## Validação

ASan/UBSan: 1×5; **72/72** RealBlockInfo.

## Próximo passo

Feito em [sombra vs loadMods](sector-runtime-h0-sector-shadow-loadmods-2026-11-26.md).
Segue gates H0 antes de H1. Não disparar Actions.
