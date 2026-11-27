# H0 / VAL-02: payload Grass/Bush/TreePair

Data: 27/11/2026 UTC. Branch `cursor/h0-foundations-9f77`, base `5b22ca46`.

[BiomeItem payload](sector-runtime-h0-candidate-biome-payload-2026-11-27.md)
· [Progressão](sector-runtime/progress.md)

## Estado: validado localmente

`SupportedPayload` compara Grass/Bush/TreePair por campos públicos
(nome/diretório/imagens/hues/flags/Json). `TileDamageParameters` e
`ObjectPool` continuam fora (UnsupportedPayload / exclusão documentada).
Sem Root/Assets, sem publicação no mundo vivo.

H0/VAL-02 em 25%. Sem Actions.

## Validação

ASan/UBSan: **13/13** `WorldGenerationCandidateSequenceTest`.

Artefato:
`/opt/cursor/artifacts/candidate-plant-payload-2026-11-27.log`
(SHA-256 `6326e95fc9c7c33492234ac530babc827e09868dbd7335ba2132645c9bc24e23`).

## Próximo passo

ObjectPool e/ou TileDamageParameters, ou MOD-01/VAL-01 H0. Sem H1–H5/R*.
Não disparar Actions.
