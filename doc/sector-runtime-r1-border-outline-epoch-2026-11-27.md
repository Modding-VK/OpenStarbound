# R1: contorno de borda + retireAll (harness residual)

Data: 27/11/2026 UTC. Branch `cursor/r1-regional-owners-9f77`, base `94acf8eb`.

[Inventário/checkpoint](sector-runtime-r1-local-checkpoint-2026-11-27.md)
· [Progressão](sector-runtime/progress.md)

## Estado: em validação (permanece 25%)

1. **REG-02:** `WorldRegionBorderOutline` — perímetro via `regionBounds`
   (parcial); `ForeignCell` rejeita tile fora da dona.
2. **REG-01:** `retireAll()` — fronteira de epoch simulada sem thread OS.
3. **WT-05:** `publishPerimeterSkeleton` / `retireAllOwners` no coordenador.

Sem thread OS, sem mundo vivo, sem R2. Sem Actions. Maturidade **não** sobe
de 25% (ainda sem autoridade).

## Validação

26 testes Owner/Border/Runtime/LocalState/Checkpoint.

## Próximo

Harness sombra residual esgotado de novo. Próximo marco: WorldStorage /
WorldServerThread / donas autoritativas — só com ordem do Project.
