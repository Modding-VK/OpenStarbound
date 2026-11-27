# H0 / VAL-02: invalidate de task durante handoff de sombra

Data: 26/11/2026 UTC. Branch `Experimental`, base `738601a0`.

[Sombra vs loadMods](sector-runtime-h0-sector-shadow-loadmods-2026-11-26.md)
· [Progressão](sector-runtime/progress.md)

## Estado: em validação

`TaskInvalidateDuringHandoffDiscardsWithoutPublishing` despacha a sombra, o
dono chama `invalidate`, o produtor observa `valid()` e encerra, a task é
retirada como Cancelled e o payload é descartado. Sem publicação.

CPU-01/H0/RAM-01 em 25%. Sem Actions.

## Validação

ASan/UBSan: 1×20; **789/789** contrato.

## Próximo passo

Abandono coberto em
[abandono de preparação](sector-runtime-h0-sector-shadow-preparation-abandon-2026-11-26.md).
Segue gates H0 antes de H1. Não disparar Actions.
