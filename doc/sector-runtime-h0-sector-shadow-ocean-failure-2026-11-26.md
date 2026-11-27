# H0 / VAL-02: oceano + falha de produtor

Data: 26/11/2026 UTC. Branch `Experimental`, base `d71d850e`.

[Ocean abandon](sector-runtime-h0-sector-shadow-ocean-abandon-2026-11-26.md)
· [Progressão](sector-runtime/progress.md)

## Estado: em validação

`ProducerFailureDiscardsFilledOceanShadow` — terrestre com oceano, composto +
sombra 32×32 preenchida, produtor falha após `fail()`, retire Failed, discard.
Sem publicação no mundo vivo.

CPU-01/H0/RAM-01 em 25%. Sem Actions.

## Validação

ASan/UBSan: 1×5; **83/83** RealBlockInfo.

## Próximo passo

Gates H0 restantes (Island/noise lifecycle). Não disparar Actions.
