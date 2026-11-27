# R2 / REG-03: intenções multirregião em sombra

Data: 27/11/2026 UTC. Branch `cursor/r2-regional-ops-9f77`, base `e55c008f`.

[Contrato REG-03](sector-runtime/regional-threads.md#reg-03--tempo-e-operações-entre-regiões)
· [Progressão](sector-runtime/progress.md)

## Objetivo

Primeiro recorte de REG-03: ledger sombra de intenções entre regiões sob um
passo lógico comum, com reserva exclusiva do footprint em ordem lexicográfica,
commit/rejeição/timeout e conflito sem hold parcial. Sem escrita no mundo vivo,
sem Lua e sem dona regional autoritativa (R1 continua pendente).

## Implementação

- `WorldRegionCrossOpShadow` / `WorldRegionCrossIntent`
- Demanda versionada por `(worldEpoch, sequence)` e `logicalStep`
- Footprint deve tocar a região origem e cada alvo declarado
- Passo mais antigo que a barreira atual é recusado
- Conclusão “mais rápida” não reescreve um Conflict já decidido

## Validação

`WorldRegionCrossOpTest` — **5/5** no alvo `world_generation_contract_tests`
(ASan/UBSan). Sem Actions.

## Limitações

Não há mailbox de turno local, relógio de rede, checkpoint, handoff de
entidades (REG-04) nem medição de custo (REG-05). Não remove a barreira global
de passo; passos independentes permanecem entrega posterior.

## Progresso

REG-03: **0% → 25%**. Programa: **3,6% → 4,1%** (8 marcos / 196).

Próximo: REG-04 em
[entity handoff sombra](sector-runtime-r2-entity-handoff-shadow-2026-11-27.md).
