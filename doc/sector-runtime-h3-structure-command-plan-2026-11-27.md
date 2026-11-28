# H3 / MD-04: footprint e comandos de estrutura em sombra

Data: 27/11/2026 UTC. Branch `cursor/h3-kernels-publication-9f77`.
Antecedentes: [TMX static](sector-runtime-h3-tmx-static-product-2026-11-27.md),
[TMX binding](sector-runtime-h3-tmx-binding-plan-2026-11-27.md).

## Objetivo

Primeiro recorte de MD-04: descrever comandos (materiais, mods, líquidos,
proteção, clearTileEntities, objetos/flora/NPC/veículo/stagehand/drops/fios/
marcações) e footprint read/write/entityReach com reservas regionais
ordenadas, sem publicação viva.

## Escopo

| Artefato | Papel |
| --- | --- |
| `WorldGenerationStructureCommandDraft` | efeito proposto pelo dono |
| `planWorldGenerationStructureFootprint` | sets normalizados com wrap X |
| `planWorldGenerationStructureReservations` | regiões ordenadas + revisão |
| `WorldGenerationStructureCommandPlan` | `tryPlan` / `tryConsume` / `discard` |

`tryConsume` recusa footprint truncado e revisão regional stale. Sucesso não
instala na facade.

## Validação

NoAssets `WorldGenerationStructureCommandPlanTest.*`: **6/6**.

## Limitações

Sem writer vivo, sem callbacks opacos reais, sem MD-05 staging/retomada.
Publicação autoritativa permanece fora.

## Próximo

MD-05 staging invisível ou WT-04 publicação mais barata em sombra.
