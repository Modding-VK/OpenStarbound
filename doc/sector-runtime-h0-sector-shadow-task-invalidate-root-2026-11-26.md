# H0 / VAL-02: invalidate Root terrestre

Data: 26/11/2026 UTC. Branch `Experimental`, base `4093b453`.

[loadMods+reload sob sombra](sector-runtime-h0-sector-shadow-loadmods-reload-2026-11-26.md)
· [Invalidate no handoff](sector-runtime-h0-sector-shadow-task-invalidate-2026-11-26.md)
· [Progressão](sector-runtime/progress.md)

## Estado: em validação

`TaskInvalidateDiscardsFilledTerrestrialShadow` preenche 32×32, invalida a
task durante o handoff, retém equivalência, retira Cancelled e descarta. Sem
publicação.

CPU-01/H0/RAM-01 em 25%. Sem Actions.

## Validação

ASan/UBSan: 1×10; **77/77** RealBlockInfo.

## Próximo passo

Island handoff em
[IslandSurface handoff](sector-runtime-h0-sector-shadow-island-handoff-2026-11-26.md).
Segue gates H0 antes de H1. Não disparar Actions.
