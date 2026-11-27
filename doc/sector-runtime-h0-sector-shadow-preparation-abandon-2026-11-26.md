# H0 / VAL-02: abandono de preparação com sombra

Data: 26/11/2026 UTC. Branch `Experimental`, base `11cb9137`.

[Invalidate no handoff](sector-runtime-h0-sector-shadow-task-invalidate-2026-11-26.md)
· [Progressão](sector-runtime/progress.md)

## Estado: em validação

`AbandonedPreparationDiscardsShadowWithoutHandoff` admite a sombra sob
`WorldGenerationTaskPreparation`, sai sem `handoff` (dtor abandona a task
Queued) e descarta o payload. Sem publicação.

CPU-01/H0/RAM-01 em 25%. Sem Actions.

## Validação

ASan/UBSan: 1×10; **790/790** contrato.

## Próximo passo

Root em
[abandono Root terrestre](sector-runtime-h0-sector-shadow-preparation-abandon-root-2026-11-26.md).
Segue gates H0 antes de H1. Não disparar Actions.
