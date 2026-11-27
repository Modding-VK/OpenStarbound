# H3 / CPU-04: normalização Finalize em sombra

Data: 27/11/2026 UTC. Branch `cursor/h3-kernels-publication-9f77`.
Antecedente: [CaveLiquid plan](sector-runtime-h3-cave-liquid-plan-2026-11-27.md).
Merge alvo: `cursor/integration-sector-runtime-7342`.

## Objetivo

Segundo recorte de CPU-04: separar a normalização de Finalize (fonte/pressão
de oceano, color variants não-reais, `collisionCacheDirty`) num delta ordenado
privado. Sem flora/objetos/tesouros e sem publicação.

## Escopo

| Artefato | Papel |
| --- | --- |
| `WorldGenerationTaskStage::FinalizePlan` | estágio reservável sem write |
| `WorldGenerationFinalizePlanIdentity` | task + revisões + setor |
| `planWorldGenerationFinalizeNormalizeDelta` | ops puras sobre amostras |
| `WorldGenerationFinalizePlan` | `tryPlan` / `tryConsume` / `discard` |
| `planWorldGenerationFinalizePlacementIntents` | ordena candidatos owner-resolved; sem spawn |

## Validação

NoAssets: Finalize **8/8** + CaveLiquid **7/7** + estágio coexistente **1**.

## Limitações

Sem `replaceBiomeBlocks`/`placeBiomeGrass`, sem consulta a PlantDatabase/
ObjectDatabase, sem equivalência ao `prepareSector` vivo, sem WT-04.
CPU-04 permanece em 25%.

## Próximo

Equivalência CaveLiquid/Finalize contra fixture, ou MD-02 estático em sombra.
Publicação só com `apis_ready` adequado em H0/H1.
