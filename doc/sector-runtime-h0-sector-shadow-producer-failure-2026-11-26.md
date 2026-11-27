# H0 / VAL-02: falha de produtor com sombra

Data: 26/11/2026 UTC. Branch `Experimental`, base `4b395dec`.

[Abandono Root terrestre](sector-runtime-h0-sector-shadow-preparation-abandon-root-2026-11-26.md)
· [Progressão](sector-runtime/progress.md)

## Estado: em validação

`ProducerFailureRetiresFailedAndDiscardsShadow` — o produtor chama `fail` e
lança; o dono retém o payload, retira `Failed` e descarta. Sem publicação.

CPU-01/H0/RAM-01 em 25%. Sem Actions.

## Validação

ASan/UBSan: 1×10; **791/791** contrato.

## Próximo passo

Root em
[falha de produtor Root](sector-runtime-h0-sector-shadow-producer-failure-root-2026-11-26.md).
Segue gates H0 antes de H1. Não disparar Actions.
