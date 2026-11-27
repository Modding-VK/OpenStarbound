# H0 / VAL-02: oceano + abandono de preparação

Data: 26/11/2026 UTC. Branch `Experimental`, base `d71d850e`.

[Ocean invalidate](sector-runtime-h0-sector-shadow-ocean-invalidate-2026-11-26.md)
· [Progressão](sector-runtime/progress.md)

## Estado: em validação

`AbandonedPreparationDiscardsFilledOceanShadow` — terrestre com oceano, composto
+ sombra preenchida sob `TaskPreparation`, abandono ao sair do escopo, sombra
ativa até discard. Sem publicação.

CPU-01/H0/RAM-01 em 25%. Sem Actions.

## Validação

ASan/UBSan: 1×5; **83/83** RealBlockInfo (com fail oceano no mesmo lote).

## Próximo passo

Fail oceano e lifecycle Island/noise. Não disparar Actions.
