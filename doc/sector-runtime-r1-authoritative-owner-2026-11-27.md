# R1: dona autoritativa (contrato exclusivo, sem thread OS)

Data: 27/11/2026 UTC. Branch `cursor/r1-regional-owners-9f77`, base `bfd2c23a`.

[Outline/epoch](sector-runtime-r1-border-outline-epoch-2026-11-27.md)
· [Progressão](sector-runtime/progress.md)

## Estado: em validação (permanece 25%)

`WorldRegionAuthoritativeOwnerLedger` promove dona Active (harness) a
**Authoritative**: escrita exclusiva ligada só a `WorldRegionOwnerIdentity`.
APIs `publishAuthoritative*` / `declareAuthoritative*` /
`confirmAuthoritative*` exigem promoção. Drain/retire demove.

Sem thread OS, sem WorldStorage vivo, sem R2. Maturidade REG-01 **não** sobe
a 50% (ainda sem thread dedicada / carga real).

API estável para dependentes: `WorldRegionAuthoritativeOwner` (sem sufixo
`-harness`).

## Validação

`WorldRegionAuthorityTest` 4/4; suite R1 regional 30/30.

## Próximo

Thread OS / WorldStorage / WorldServerThread — só com ordem Project.
