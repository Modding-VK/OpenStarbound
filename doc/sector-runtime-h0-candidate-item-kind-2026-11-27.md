# H0 / VAL-02: discriminante BiomeItem no candidate record

Data: 27/11/2026 UTC. Branch `cursor/h0-foundations-9f77`, base `c37519b6`.

[Queue snapshot](sector-runtime-h0-candidate-queue-snapshot-2026-11-27.md)
· [Progressão](sector-runtime/progress.md)

## Estado: validado localmente

`WorldGenerationCandidateRecord` v2 inclui `itemKind`
(`BiomeItem::typeIndex`) sem copiar payload. Campo `ItemKind` no
comparador. Sem publicação no mundo vivo.

H0/VAL-02 em 25%. Sem Actions.

## Validação

ASan/UBSan: **10/10** `WorldGenerationCandidateSequenceTest`.

Artefato:
`/opt/cursor/artifacts/candidate-item-kind-2026-11-27.log`
(SHA-256 `84f2982ea7b3a173243168d532e581f823e5f0c789514afeaefbad9015017b32`).

## Próximo passo

Payload profundo BiomeItem, planos MicroDungeon completos, MOD-01 ou
VAL-01 — conforme Project. Não disparar Actions.
