# R2: ops além da sombra via WorldRegionAuthoritativeOwner

Data: 27/11/2026 UTC. Branch `cursor/r2-regional-ops-9f77`, base `dd9e3b58`.
Dep R1: `WorldRegionAuthoritativeOwner` @ `00285fe7` (sem `-harness`).

## Objetivo

Gate R2-only: CrossOp / EntityHandoff / Recovery só atuam quando a região
tem `WorldRegionAuthorityGrade::Authoritative`. Harness Active sozinho é
recusado.

## Implementação

- `WorldRegionAuthoritativeOps` consome `WorldRegionAuthoritativeOwnerLedger`
- Import mínimo R1 na árvore R2: `StarWorldRegionOwner.*` +
  `StarWorldRegionAuthority.*` (reconciliação no merge de integração)
- Ledgers sombra REG-03/04/05 preservados por baixo do gate

## Validação

`WorldRegionAuthoritativeOpsTest` **4/4** + regressão CrossOp/Handoff/Recovery
**12/12**; ASan/UBSan. Sem Actions / thread OS / WorldStorage vivo.

## Progresso

REG-03/04/05 permanecem **25%** (gate autoritativo é avanço além da sombra,
mas requisitos completos do contrato — rede, índice vivo, custo medido — ainda
abertos). Programa **5,1%** inalterado. Pacote R2 segue ready-for-integration.
