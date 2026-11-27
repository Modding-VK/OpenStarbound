# H0 / VAL-02: payload ObjectPool

Data: 27/11/2026 UTC. Branch `cursor/h0-foundations-9f77`, base `1c5579f1`.

[Plant payload](sector-runtime-h0-candidate-plant-payload-2026-11-27.md)
· [Progressão](sector-runtime/progress.md)

## Estado: validado localmente

`ObjectPool` compara entradas ordenadas (peso bit-a-bit, nome, Json).
Todos os kinds de `BiomeItem` têm payload sob `SupportedPayload`, exceto
`TileDamageParameters` nos variants de planta (ainda excluído).

H0/VAL-02 em 25%. Sem Actions. Sem H1–H5/R*.

## Validação

ASan/UBSan: **13/13** `WorldGenerationCandidateSequenceTest`.

Artefato:
`/opt/cursor/artifacts/candidate-object-pool-payload-2026-11-27.log`
(SHA-256 `6326e95fc9c7c33492234ac530babc827e09868dbd7335ba2132645c9bc24e23`).

## Próximo passo

TileDamageParameters (exige link/toJson), MOD-01 ou VAL-01 H0.
Não disparar Actions.
