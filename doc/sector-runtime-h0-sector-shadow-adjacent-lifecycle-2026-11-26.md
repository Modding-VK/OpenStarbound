# H0 / VAL-02: setor adjacente + abandon/fail

Data: 26/11/2026 UTC. Branch `Experimental`, base `0469767b`.

[Adjacent handoff](sector-runtime-h0-sector-shadow-adjacent-handoff-2026-11-26.md)
· [Progressão](sector-runtime/progress.md)

## Estado: em validação

Setor `{1,0}` sob composto + sombra:

- `AbandonedPreparationDiscardsFilledAdjacentShadow`
- `ProducerFailureDiscardsFilledAdjacentShadow`

Sem publicação. CPU-01/H0/RAM-01 em 25%. Sem Actions.

## Validação

ASan/UBSan: 1×5; **93/93** RealBlockInfo.

## Próximo passo

hotReload oceano/Island em
[ocean-island hotreload](sector-runtime-h0-sector-shadow-ocean-island-hotreload-2026-11-26.md).
Segue loadMods sob variantes. Não disparar Actions.
