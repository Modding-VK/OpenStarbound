# H0 / VAL-02: handoffs concorrentes de task+sombra

Data: 26/11/2026 UTC. Branch `Experimental`, base `2b1836c7`.

[Handoff task+sombra](sector-runtime-h0-sector-shadow-task-handoff-2026-11-26.md)
· [Progressão](sector-runtime/progress.md)

## Estado: em validação

`ConcurrentTaskHandoffsRetireIndependentlyWithoutPublishing` admite duas
sombras, despacha duas tasks no WorkerPool, retira Cancelled/Stale em ordem e
descarta payloads de forma independente. Sem publicação.

CPU-01/H0/RAM-01 em 25%. Sem Actions.

## Validação

ASan/UBSan: 1 caso × 10; **788/788** contrato.

## Próximo passo

Root em
[handoffs concorrentes Root](sector-runtime-h0-sector-shadow-concurrent-handoff-root-2026-11-26.md).
Segue contexto profundo/assets. Não disparar Actions.
