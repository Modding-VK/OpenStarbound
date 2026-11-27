# R2 / REG-04: handoff de entidades em sombra

Data: 27/11/2026 UTC. Branch `cursor/r2-regional-ops-9f77`, base `946a5406`.

[REG-03 sombra](sector-runtime-r2-cross-op-shadow-2026-11-27.md)
· [Contrato REG-04](sector-runtime/regional-threads.md#reg-04--transferência-de-entidades)

## Objetivo

Primeiro recorte de REG-04: no máximo uma dona por `(EntityId, incarnation)`
num passo lógico; prepare → validate → commit atualiza região/`ownerGeneration`
em conjunto; mensagens stale/duplicadas são rejeitadas. Sem escrita viva e sem
externalização de NPCs.

## Implementação

- `WorldRegionEntityHandoffShadow`
- `seedOwner` exclusivo; um handoff Prepared por entidade
- `commit` exige geração esperada; `bumpOwnerGeneration` simula revisão
  autoritativa concorrente (sombra)
- Idempotência por passo: não re-prepara no mesmo `logicalStep` após commit

## Validação

`WorldRegionEntityHandoffTest` — **4/4** (+5 CrossOp regressão); ASan/UBSan.
Sem Actions.

## Progresso

REG-04: **0% → 25%**. Programa: **4,1% → 4,6%** (9 marcos / 196).
