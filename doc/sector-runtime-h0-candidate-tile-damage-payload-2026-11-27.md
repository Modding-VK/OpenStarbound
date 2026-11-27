# H0 / VAL-02: TileDamageParameters no payload de plantas

Data: 27/11/2026 UTC. Branch `cursor/h0-foundations-9f77`, base `7b1bbbbc`.

[ObjectPool payload](sector-runtime-h0-candidate-object-pool-payload-2026-11-27.md)
· [Progressão](sector-runtime/progress.md)

## Estado: validado localmente

`WorldGenerationTileDamageParametersAccess` compara fatores/recovery/
health/harvest bit-a-bit sem Root. Grass/Bush/TreePair incluem
`tileDamageParameters`. Cobertura BiomeItem sob `SupportedPayload` completa
para VAL-02 diagnóstico de candidatos.

H0/VAL-02 em 25%. Sem Actions. Sem H1–H5/R*.

## Validação

ASan/UBSan: **13/13** `WorldGenerationCandidateSequenceTest`.

Artefato:
`/opt/cursor/artifacts/candidate-tile-damage-payload-2026-11-27.log`
(SHA-256 `ec65efeccca0e9877c55b99cde04bec5c337dfe82c66c35b055c7b9fcd1523c2`).

## Próximo passo

MOD-01 ou VAL-01 H0 (streaming/rota), ou planeta/mods — sem sair desta
frente. Não disparar Actions.
