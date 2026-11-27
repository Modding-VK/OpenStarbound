# H0 / VAL-02: oceano + invalidate de task

Data: 26/11/2026 UTC. Branch `Experimental`, base `9daa3a7a`.

[Noise handoff](sector-runtime-h0-sector-shadow-noise-handoff-2026-11-26.md)
· [Progressão](sector-runtime/progress.md)

## Estado: em validação

`OceanShadowTaskInvalidateDiscardsFilled` — terrestre com oceano, composto +
sombra 32×32 preenchida, invalidate durante handoff, Cancelled, discard. Sem
publicação no mundo vivo.

CPU-01/H0/RAM-01 em 25%. Sem Actions.

## Validação

ASan/UBSan: 1×5; **81/81** RealBlockInfo.

## Próximo passo

Oceano abandon/fail em
[ocean abandon](sector-runtime-h0-sector-shadow-ocean-abandon-2026-11-26.md) e
[ocean failure](sector-runtime-h0-sector-shadow-ocean-failure-2026-11-26.md).
Segue Island/noise lifecycle. Não disparar Actions.
