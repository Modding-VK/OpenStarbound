# H0 / VAL-02: setor vertical {0,1} — matriz completa

Data: 27/11/2026 UTC. Branch `cursor/h0-foundations-9f77`, base `5559e373`.

[Concurrent+reload matrix](sector-runtime-h0-sector-shadow-concurrent-reload-matrix-2026-11-27.md)
· [Progressão](sector-runtime/progress.md)

## Estado: em validação

Setor `{0,1}` / `tileRegion` 0–32×32–64 sob composto + sombra:

- produce/discard, handoff Stale, invalidate Cancelled
- preparation abandon, producer Failed
- hotReload, loadMods deferido, loadMods+reload
- templateRevision stale
- handoff concorrente com `{0,0}`

Sem publicação. CPU-01/H0/RAM-01 em 25%. Sem Actions.

## Validação

ASan/UBSan: 1×5; **125/125** RealBlockInfo.

## Próximo passo

Diagonal em
[diagonal adjacent](sector-runtime-h0-sector-shadow-diagonal-adjacent-2026-11-27.md).
Segue VAL-02 além de BaseTiles. Não disparar Actions.
