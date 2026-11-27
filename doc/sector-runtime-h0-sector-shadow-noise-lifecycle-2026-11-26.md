# H0 / VAL-02: noises/caches + lifecycle (invalidate/abandon/fail)

Data: 26/11/2026 UTC. Branch `Experimental`, base `bcfeddfc`.

[Island lifecycle](sector-runtime-h0-sector-shadow-island-lifecycle-2026-11-26.md)
· [Progressão](sector-runtime/progress.md)

## Estado: em validação

Três casos sob composto + sombra Cache/Perlin 32×32:

- `NoiseCacheShadowTaskInvalidateDiscardsFilled` — invalidate → Cancelled
- `AbandonedPreparationDiscardsFilledNoiseCacheShadow` — preparation abandon
- `ProducerFailureDiscardsFilledNoiseCacheShadow` — producer Failed

Sem publicação. CPU-01/H0/RAM-01 em 25%. Sem Actions.

## Validação

ASan/UBSan: 1×5; **89/89** RealBlockInfo.

## Próximo passo

Adjacente em
[adjacent handoff](sector-runtime-h0-sector-shadow-adjacent-handoff-2026-11-26.md).
Segue abandon/fail adjacente e assets. Não disparar Actions.
