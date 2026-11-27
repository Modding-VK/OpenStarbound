# R2 / REG-05: falha e checkpoint em sombra

Data: 27/11/2026 UTC. Branch `cursor/r2-regional-ops-9f77`, base `b6e8604b`.

[REG-04 sombra](sector-runtime-r2-entity-handoff-shadow-2026-11-27.md)
· [Contrato REG-05](sector-runtime/regional-threads.md#reg-05--falha-checkpoint-e-custo-de-escala)

## Objetivo

Primeiro recorte de REG-05: falha invalida intenções não confirmadas do passo
afetado; região parcialmente publicada não parece revertida até checkpoint
consistente ou reload legado. Sem medição de custo de threads nem mundo vivo.

## Implementação

- `WorldRegionRecoveryShadow`
- `noteUnconfirmed` / `fail` / `checkpoint` / `returnToLegacy` / `reloadLegacy`
- Checkpoint recusado enquanto houver unconfirmed no passo

## Validação

`WorldRegionRecoveryTest` — **3/3**; ASan/UBSan. Sem Actions.

## Progresso

REG-05: **0% → 25%**. Programa: **4,6% → 5,1%** (10 marcos / 196).
R2 entrada: **25%** (3/12 marcos). Lifecycle R1 e custo real seguem abertos.
