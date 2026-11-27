# H3 / IO-02: serviço de banco em sombra (harness)

Data: 27/11/2026 UTC. Branch `cursor/h3-kernels-publication-9f77`.
Antecedente: [WT-04 publication pass](sector-runtime-h3-publication-pass-2026-11-27.md).

## Objetivo

Primeiro recorte de IO-02: harness in-memory que serializa ops por mundo,
separa `contentVersion` do ticket de escrita, expõe leitores só no último
checkpoint e rejeita conteúdo stale — sem SQLite/`.world`/mundo vivo.

## Escopo

| Artefato | Papel |
| --- | --- |
| `WorldGenerationShadowDbService` | begin/put/commit/rollback/sync/close |
| `readCheckpoint` | nunca vê scratch não commitado |
| `encodeFrontierBlob` | bytes coerentes com fronteira WT-04 |

## Validação

NoAssets `WorldGenerationShadowDbTest.*`: **7/7**.

## Limitações

Sem disco, sem workers reais, sem `m_db` vivo. IO-03 primeiro recorte em
[shadow checkpoint](sector-runtime-h3-shadow-checkpoint-2026-11-27.md);
IO-04 unload continua aberto.

## Próximo

IO-04 unload/drain em sombra, ou amarração do frontier blob ao
PublicationPass confirmado.
