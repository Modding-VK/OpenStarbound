# H0 / VAL-02: snapshot de PlacementQueue → candidate records

Data: 27/11/2026 UTC. Branch `cursor/h0-foundations-9f77`, base `7020b127`.

[Placement adapter](sector-runtime-h0-candidate-placement-adapter-2026-11-27.md)
· [Progressão](sector-runtime/progress.md)

## Estado: validado localmente

`worldGenerationCandidateRecordsFromQueue` lê `entries()` em IDs
ascendentes, sem copiar `BiomeItem` nem mutar a fila. Após `take`, o
comparador detecta comprimento; sort por prioridade permanece opt-in.

H0/VAL-02 em 25%. Sem Actions. Sem publicação no mundo vivo.

## Validação

ASan/UBSan: **9/9** `WorldGenerationCandidateSequenceTest`.

Artefato:
`/opt/cursor/artifacts/candidate-queue-snapshot-2026-11-27.log`
(SHA-256 `a949895564343897e1d85fb8a2798a70ea3922cea85536e6d4bc5fb8e4eb3144`).

## Próximo passo

Payload BiomeItem / planos MicroDungeon / streaming (VAL-01) ou MOD-01 —
conforme fila H0. Não disparar Actions.
