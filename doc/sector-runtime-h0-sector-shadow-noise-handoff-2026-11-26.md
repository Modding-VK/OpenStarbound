# H0 / VAL-02: noises/caches + handoff de task

Data: 26/11/2026 UTC. Branch `Experimental`, base `96deff7a`.

[Oceano handoff](sector-runtime-h0-sector-shadow-ocean-handoff-2026-11-26.md)
· [Progressão](sector-runtime/progress.md)

## Estado: em validação

`NoiseCacheShadowTaskHandoffRetiresStale` — árvore Cache/Perlin privada,
composto + sombra 32×32, handoff, Stale, discard. Sem publicação.

CPU-01/H0/RAM-01 em 25%. Sem Actions.

## Validação

ASan/UBSan: 1×5; **80/80** RealBlockInfo.

## Próximo passo

Oceano invalidate em
[ocean invalidate](sector-runtime-h0-sector-shadow-ocean-invalidate-2026-11-26.md).
Segue gates H0 antes de H1. Não disparar Actions.
