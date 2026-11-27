# H0 / VAL-02: handoff de task com sombra sem publicação

Data: 26/11/2026 UTC. Branch `Experimental`, base `adb2234d`.

[Sombra template stale](sector-runtime-h0-sector-shadow-stale-template-2026-11-26.md)
· [Progressão](sector-runtime/progress.md)

## Estado: em validação

Contrato: `TaskHandoffRetiresStaleWithoutPublishingShadowPayload` e
`TaskHandoffMatchesAndRetiresWithoutPublishing` — WorkerPool produz a sombra
admitida; o dono revalida `matches`, retira a task como Stale/Cancelled e
descarta o payload. Sem API de publicação.

Root: `SectorShadowTaskHandoffFillsTerrestrialThenRetiresStale` — composto +
sombra terrestre 32×32, handoff, expectativa stale por `tileRevision`, retire
Stale e discard.

CPU-01/H0/RAM-01 em 25%. Sem Actions.

## Validação

ASan/UBSan: **787/787** contrato (10 sombras × 10); Root 1×5; **70/70** RealBlockInfo.

## Próximo passo

Concorrência de handoff em
[handoffs concorrentes](sector-runtime-h0-sector-shadow-concurrent-handoff-2026-11-26.md).
Segue contexto profundo/assets. Não disparar Actions.
