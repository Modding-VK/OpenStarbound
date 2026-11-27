# H0 / VAL-02: sombras concorrentes no ledger

Data: 26/11/2026 UTC. Branch `Experimental`, base `4323d53a`.

[Sombra noises/caches](sector-runtime-h0-sector-shadow-noise-caches-2026-11-26.md)
· [Progressão](sector-runtime/progress.md)

## Estado: em validação

`ConcurrentSectorShadowsReleaseIndependently` (contrato) admite dois arrays
BaseTiles para setores `{0,0}` e `{1,0}` no mesmo ledger, prova payloads
independentes e liberação parcial.

`ConcurrentSectorShadowsMatchIndependentTerrestrialSectors` (Root) captura dois
compostos com sequences distintas, preenche as sombras a partir de layouts
privados e compara BaseTiles de ambos os setores antes de descartar em ordem.

Sem publicação, workers ou Actions. CPU-01/H0/RAM-01 em 25%.

## Validação

ASan/UBSan: 6 sombras × 10; **783/783** contrato; Root 1×5; **67/67** RealBlockInfo.

## Próximo passo

IslandSurface/colunas privadas sob composto+sombra, ou pressão de admissão com
duas reservas. Não disparar Actions.
