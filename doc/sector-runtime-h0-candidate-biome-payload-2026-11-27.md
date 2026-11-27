# H0 / VAL-02: payload BiomeItem (MicroDungeon/Treasure)

Data: 27/11/2026 UTC. Branch `cursor/h0-foundations-9f77`, base `d6a6419a`.

[Candidate itemKind](sector-runtime-h0-candidate-item-kind-2026-11-27.md)
· [Progressão](sector-runtime/progress.md)

## Estado: validado localmente

Comparador diagnóstico de `BiomeItem`: `MicroDungeonNames` (StringSet) e
`TreasureBoxSet` (String). Grass/Bush/Tree/ObjectPool →
`UnsupportedPayload` sob `SupportedPayload`; `KindOnly` ignora conteúdo.
`compareWorldGenerationCandidatePlacements` alinha records + payload.
Sem Root/Assets, sem mudança de seleção, sem publicação no mundo vivo.

H0/VAL-02 em 25%. Sem Actions.

## Validação

ASan/UBSan: **12/12** `WorldGenerationCandidateSequenceTest`.

Artefato:
`/opt/cursor/artifacts/candidate-biome-payload-2026-11-27.log`
(SHA-256 `09418f3a345d1019248f778c884ae5ed12587d07f4895f04dd692660d58f4392`).

## Próximo passo

Fechado parcialmente pelo
[payload Grass/Bush/Tree](sector-runtime-h0-candidate-plant-payload-2026-11-27.md).
Não disparar Actions.
