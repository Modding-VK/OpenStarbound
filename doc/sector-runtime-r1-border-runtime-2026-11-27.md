# R1: REG-02 bordas + WT-05 coordenadora (primeiros recortes)

Data: 27/11/2026 UTC. Branch `cursor/r1-regional-owners-9f77`, base `94bd7b61`.

[REG-01 identidade](sector-runtime-r1-region-owner-identity-2026-11-27.md)
· [Progressão](sector-runtime/progress.md)

## Estado: em validação

1. **REG-01 (evidência adicional):** `pendingWork` bloqueia `completeDrain`;
   consultas `identityForSector` / `members`.
2. **REG-02 0→25%:** `WorldRegionBorderLedger` — snapshot imutável com idade/
   revisão; região ausente = indisponível; drain não publica borda nova.
3. **WT-05 0→25%:** `WorldRegionRuntimeCoordinator` — interesse → donos,
   relógio lógico, bordas confirmadas; sem publicação central de simulação.

Sem thread OS, Lua ou WorldServer. Sem Actions.

## Validação

`WorldRegionOwnerTest` 7 + `WorldRegionBorderTest` 4 + `WorldRegionRuntimeTest` 4
= **15/15**.

## Próximo passo

Inventário de estado local REG-02; drain/carga reais; IO-03 regional. Não
disparar Actions.
