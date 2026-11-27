# H0 / VAL-02: falha de produtor Root terrestre

Data: 26/11/2026 UTC. Branch `Experimental`, base `0177fb7b`.

[Falha de produtor](sector-runtime-h0-sector-shadow-producer-failure-2026-11-26.md)
· [Progressão](sector-runtime/progress.md)

## Estado: em validação

`ProducerFailureDiscardsFilledTerrestrialShadow` preenche 32×32, o produtor
falha/lança, o dono retém equivalência, retira `Failed` e descarta. Sem
publicação.

CPU-01/H0/RAM-01 em 25%. Sem Actions.

## Validação

ASan/UBSan: 1×5; **75/75** RealBlockInfo.

## Próximo passo

loadMods+reload em
[loadMods+reload sob sombra](sector-runtime-h0-sector-shadow-loadmods-reload-2026-11-26.md).
Segue gates H0 antes de H1. Não disparar Actions.
