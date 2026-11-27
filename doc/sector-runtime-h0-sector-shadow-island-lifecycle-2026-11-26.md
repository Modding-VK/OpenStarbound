# H0 / VAL-02: IslandSurface + lifecycle (invalidate/abandon/fail)

Data: 26/11/2026 UTC. Branch `Experimental`, base `16e3b4bc`.

[Ocean failure](sector-runtime-h0-sector-shadow-ocean-failure-2026-11-26.md)
· [Progressão](sector-runtime/progress.md)

## Estado: em validação

Três casos sob composto + sombra IslandSurface 32×32:

- `IslandSurfaceShadowTaskInvalidateDiscardsFilled` — invalidate → Cancelled
- `AbandonedPreparationDiscardsFilledIslandSurfaceShadow` — preparation abandon
- `ProducerFailureDiscardsFilledIslandSurfaceShadow` — producer Failed

Sem publicação no mundo vivo. CPU-01/H0/RAM-01 em 25%. Sem Actions.

## Validação

ASan/UBSan: 1×5; **86/86** RealBlockInfo.

## Próximo passo

Noise-cache lifecycle e demais gates H0. Não disparar Actions.
