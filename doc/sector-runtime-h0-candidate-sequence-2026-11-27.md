# H0 / VAL-02: comparador de sequência de candidatos

Data: 27/11/2026 UTC. Branch `cursor/h0-foundations-9f77`, base `812c667e`.

[Capstone 2×2 + tight ledger](sector-runtime-h0-sector-shadow-four-sector-tight-ledger-2026-11-27.md)
· [Progressão](sector-runtime/progress.md)

## Estado: validado localmente

Primeiro recorte VAL-02 além de BaseTiles: comparador diagnóstico de
sequências `WorldGenerationCandidateRecord` (placementId opcional, posição,
prioridade bit-a-bit). Detecta divergência de comprimento, ID, posição,
prioridade (incl. −0 / NaN) e ordem relativa. Sem `BiomeItem` profundo, sem
mudança de seleção/RNG/cópias, sem publicação no mundo vivo.

CPU-01/H0/RAM-01/VAL-02 permanecem em 25%. Sem Actions.

## Validação

ASan/UBSan: **6/6** `WorldGenerationCandidateSequenceTest`.

Artefato local (fora do Git):
`/opt/cursor/artifacts/candidate-sequence-2026-11-27.log`
(1521 bytes,
SHA-256 `c9d33b57bd63be774f5491129005529966b33a80bead74cceab539bd6caec55b`).

## Limitações

Não compara payload de `BiomeItem`; não liga o comparador ao gerador vivo;
não é VAL-01 de streaming nem MOD-01.

## Próximo passo

Fechado pelo
[adapter BiomeItemPlacement](sector-runtime-h0-candidate-placement-adapter-2026-11-27.md).
Não disparar Actions.
