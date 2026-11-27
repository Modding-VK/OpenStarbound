# H3 / CPU-04: plano CaveLiquid em sombra

Data: 27/11/2026 UTC. Branch `cursor/h3-kernels-publication-9f77`, base
`e55c008f` (Experimental). Merge alvo previsto:
`cursor/integration-sector-runtime-7342`.

[CPU-04](sector-runtime/cpu-and-stages.md) · [Progressão](sector-runtime/progress.md)

## Objetivo

Primeiro recorte acionável de H3: extrair o flood-fill de CaveLiquid para um
produto privado versionado com delta ordenado de pressão, sem instalar no
mundo vivo.

## Escopo do recorte

| Artefato | Papel |
| --- | --- |
| `WorldGenerationCaveLiquidPlanIdentity` | task CaveLiquidPlan + revisões + parâmetros de fill |
| `planWorldGenerationCaveLiquidSeeds` | grade de sementes pela densidade legada |
| `planWorldGenerationCaveLiquidDelta` | flood-fill/poison/nível sobre amostras privadas |
| `WorldGenerationCaveLiquidPlan` | handle `tryPlan` / `tryConsume` / `discard` sem publish |

Amostras (`WorldGenerationCaveLiquidSample`) trazem `solidCollision` já
resolvido pelo dono — sem `MaterialDatabase`/`WorldStorage`/`Root`.

## Validação

`world_generation_contract_tests` NoAssets, filtro
`WorldGenerationCaveLiquidPlanTest.*`: **7/7**.

## Limitações

Não há amarração ao `generateCaveLiquid` legado, Finalize, TMX/MD, staging
WT-04 nem IO. Aceitação 50%+ exige equivalência com o path vivo e o restante
das etapas de CPU-04.

## Próximo passo

Finalize normalização entregue em
[finalize-normalize](sector-runtime-h3-finalize-normalize-2026-11-27.md).
Seguir flora intents ou equivalência viva. Não disparar Actions.
