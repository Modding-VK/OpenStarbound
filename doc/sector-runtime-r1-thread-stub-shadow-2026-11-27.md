# R1: stub sombra de thread OS (REG-01)

Data: 27/11/2026 UTC. Branch `cursor/r1-regional-owners-9f77`, base `00285fe7`.

[Authoritative owner](sector-runtime-r1-authoritative-owner-2026-11-27.md)
· [Progressão](sector-runtime/progress.md)

## Estado: em validação (permanece 25%)

`WorldRegionOwnerThreadStubLedger` — lifecycle Running/Joining/Stopped para
dona **Authoritative**, sem `pthread`, Lua ou WorldServer. Id opaco ≠
identidade. Falha de criação bloqueia o stub sem derrubar a autoridade.

API sombra: `WorldRegionOwnerThreadStub-shadow`. Sem R2. Sem Actions.

## Validação

`WorldRegionThreadStubTest` 4/4; suite R1 regional 34/34.

## Próximo

R1-only residual de harness/sombra esgotado. Próximo: `pthread` real /
WorldStorage / WorldServerThread — só com ordem Project.
