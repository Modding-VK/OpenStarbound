# H3 / WT-04: passagem de publicação com orçamento em sombra

Data: 27/11/2026 UTC. Branch `cursor/h3-kernels-publication-9f77`.
Antecedente: [MD-05 batch staging](sector-runtime-h3-structure-batch-staging-2026-11-27.md).

## Objetivo

Primeiro recorte de WT-04: orçamento de passagem (batches/comandos) sobre
staging invisível, dirty/notificações só na fronteira lógica confirmada, e
descarte stale antes de efeitos — sem publish vivo.

## Escopo

| Artefato | Papel |
| --- | --- |
| `WorldGenerationPublicationPassBudget` | maxBatches/maxCommands |
| `WorldGenerationPublicationDeferredDirty` | dirty diferido pós-fronteira |
| `WorldGenerationPublicationPass` | `run` / `discardStale` |

Saídas: `BudgetExhausted`, `Complete`, `StaleDiscarded`, `Idle`.

## Validação

NoAssets `WorldGenerationPublicationPassTest.*`: **5/5**.

## Limitações

Sem WorldThread real, sem invalidação de colisão viva, sem IO/cliente.
Fronteira confirmada permanece privada ao staging sombra.

## Próximo

IO-02 sombra, ou amarração WT-04 ao WorldThread quando APIs live-publish
existirem. Não disparar Actions.
