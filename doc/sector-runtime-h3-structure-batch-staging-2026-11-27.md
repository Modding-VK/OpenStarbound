# H3 / MD-05: aplicação em batches com staging invisível

Data: 27/11/2026 UTC. Branch `cursor/h3-kernels-publication-9f77`.
Antecedente: [MD-04 commands](sector-runtime-h3-structure-command-plan-2026-11-27.md).

## Objetivo

Primeiro recorte de MD-05: fatiar o plano de comandos em batches ordenados,
aplicar em staging privado (invisível), retomar via token e só “commit” da
unidade lógica após todas as batches — sem publish vivo na facade.

## Escopo

| Artefato | Papel |
| --- | --- |
| `planWorldGenerationStructureBatches` | fatias ordenadas + fase |
| `WorldGenerationStructureResumeToken` | nextBatch/appliedCommands |
| `WorldGenerationStructureBatchStaging` | apply/resume/tryCommit/discard |

`tryCommit` exige staging completo + reservas frescas; sucesso não instala tiles.

## Validação

NoAssets `WorldGenerationStructureBatchStagingTest.*`: **6/6** (+ MD-04 **6/6**).

## Limitações

Sem writer vivo, sem equivalência de notificações/colisões, sem WT-04.
Publicação autoritativa permanece fora.

## Próximo

WT-04 staging/publicação mais barata em sombra, ou IO-02 sombra.
