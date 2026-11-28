# R1: inventário local REG-02 + barreira IO-03

Data: 27/11/2026 UTC. Branch `cursor/r1-regional-owners-9f77`, base `c2b7ce75`.

[Bordas/coordenadora](sector-runtime-r1-border-runtime-2026-11-27.md)
· [Progressão](sector-runtime/progress.md)

## Estado: em validação

1. **REG-02 (evidência):** `WorldRegionLocalStateLedger` — kinds exclusivos
   (tiles/líquidos/colisão/entidades/fios/dano/Lua local); vizinho não muta.
2. **IO-03 0→25%:** `WorldRegionCheckpointBarrier` — passo lógico comum;
   dona rápida não avança checkpoint enquanto outra atrasa.
3. **WT-05:** RuntimeCoordinator expõe inventário + checkpoint.

Sem escrita em `.world`, sem thread OS. Sem Actions.

## Validação

22 testes novos/estendidos nas suites Owner/Border/Runtime/LocalState/Checkpoint.

## Próximo passo

Integração WorldStorage/WorldServerThread e thread dedicada — fora do harness
puro. R2 (REG-03+) permanece na branch própria. Não disparar Actions.
