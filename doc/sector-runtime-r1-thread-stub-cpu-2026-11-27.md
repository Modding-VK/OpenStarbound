# R1: stub OS thread em CPU (sem WorldServer)

Data: 27/11/2026 UTC. Branch `cursor/r1-regional-owners-9f77`, base `f90055ff`.

[Thread stub sombra](sector-runtime-r1-thread-stub-shadow-2026-11-27.md)
· [Progressão](sector-runtime/progress.md)

## Estado: em validação (permanece 25%)

`WorldRegionOwnerThreadStubLedger` agora usa `Star::Thread::invoke` (OS thread
real) que executa trabalho de CPU até join. Sem WorldServerThread, WorldStorage,
Lua ou rede. Id opaco ≠ identidade.

API: `WorldRegionOwnerThreadStubCpu`. Sem R2. Sem Actions.

## Validação

`WorldRegionThreadStubTest` 5/5 (inclui ticks de CPU > 0); suite R1 35/35.

## Próximo / standby

Contrato REG-01 completo (simulação regional / carga WorldStorage) exige
**mundo vivo** — fora deste estágio. Standby até ordem Project.
