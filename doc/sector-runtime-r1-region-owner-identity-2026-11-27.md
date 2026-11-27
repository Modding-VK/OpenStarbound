# R1 / REG-01: identidade e lifecycle da dona (primeiro recorte)

Data: 27/11/2026 UTC. Branch `cursor/r1-regional-owners-9f77`, base `e55c008f`.

[Threads regionais](sector-runtime/regional-threads.md)
· [Progressão](sector-runtime/progress.md)

## Estado: em validação

`WorldRegionOwnerRegistry` — harness REG-01 sem thread OS, sem Lua e sem
publicação em `WorldServer`/`WorldStorage`:

- identidade `(worldEpoch, regionGeneration, region)` por valor
- `Queued` não cria dona
- primeiro `Tiles`/`Entities` cria dona da região 2×2
- último unload inicia drain; `completeDrain` aposenta a geração
- falha de criação bloqueia ativação regional

CPU-01/H0/RAM-01 permanecem em 25%. REG-01 sobe **0→25%**. Sem Actions.

## Validação

`WorldRegionOwnerTest`: **6/6** no alvo `world_generation_contract_tests`.

## Próximo passo

REG-01: drain com trabalho pendente, ligação a níveis reais de carga, e depois
REG-02 (estado local / bordas). Não disparar Actions.
