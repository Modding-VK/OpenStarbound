# H0 / VAL-02: adapter BiomeItemPlacement → candidate record

Data: 27/11/2026 UTC. Branch `cursor/h0-foundations-9f77`, base `b7df0947`.

[Candidate sequence](sector-runtime-h0-candidate-sequence-2026-11-27.md)
· [Progressão](sector-runtime/progress.md)

## Estado: validado localmente

`worldGenerationCandidateRecord` copia posição/prioridade (+ id opcional)
sem reter `BiomeItem`. `sortWorldGenerationCandidateRecords` usa
`stable_sort` por prioridade ascendente, alinhado a
`BiomeItemPlacement::operator<`. Sem publicação no mundo vivo.

H0/VAL-02 em 25%. Sem Actions.

## Validação

ASan/UBSan: **8/8** `WorldGenerationCandidateSequenceTest`.

Artefato local:
`/opt/cursor/artifacts/candidate-placement-adapter-2026-11-27.log`
(SHA-256 `df5e6ed9e94080269349fa22dca9ad1d5f9216ce8704e825900e479dbd92b2b0`).

## Próximo passo

Ligar a planos/queues reais sem payload profundo; manter gates de hash.
Não disparar Actions.
