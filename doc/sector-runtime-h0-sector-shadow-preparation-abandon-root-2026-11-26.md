# H0 / VAL-02: abandono de preparação Root terrestre

Data: 26/11/2026 UTC. Branch `Experimental`, base `c10c6da9`.

[Abandono de preparação](sector-runtime-h0-sector-shadow-preparation-abandon-2026-11-26.md)
· [Progressão](sector-runtime/progress.md)

## Estado: em validação

`AbandonedPreparationDiscardsFilledTerrestrialShadow` preenche 32×32 sob
TaskPreparation + composto, abandona a task sem handoff, mantém o payload até
`discard` e zera o ledger. Sem publicação.

CPU-01/H0/RAM-01 em 25%. Sem Actions.

## Validação

ASan/UBSan: 1×5; **74/74** RealBlockInfo.

## Próximo passo

Falha de produtor em
[falha de produtor](sector-runtime-h0-sector-shadow-producer-failure-2026-11-26.md).
Segue gates H0 antes de H1. Não disparar Actions.
