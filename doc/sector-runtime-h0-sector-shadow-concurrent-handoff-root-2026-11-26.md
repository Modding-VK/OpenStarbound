# H0 / VAL-02: handoffs concorrentes terrestres (Root)

Data: 26/11/2026 UTC. Branch `Experimental`, base `aa8dd5fc`.

[Handoffs concorrentes](sector-runtime-h0-sector-shadow-concurrent-handoff-2026-11-26.md)
· [Progressão](sector-runtime/progress.md)

## Estado: em validação

`ConcurrentSectorShadowTaskHandoffsFillIndependentTerrestrialSectors` captura
dois compostos, admite duas sombras, preenche setores `{0,0}` e `{1,0}`, faz
handoff paralelo e retira Cancelled/Stale com discard independente. Sem
publicação.

CPU-01/H0/RAM-01 em 25%. Sem Actions.

## Validação

ASan/UBSan: 1×5; **71/71** RealBlockInfo (com **788** contrato do recorte
anterior).

## Próximo passo

Contexto profundo/assets de CPU-01. Não disparar Actions.
