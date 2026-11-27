# H3 / CPU-04: flora/spawn em sombra

Data: 27/11/2026 UTC. Branch `cursor/h3-kernels-publication-9f77`.
Antecedente: [Finalize normalize/intents](sector-runtime-h3-finalize-normalize-2026-11-27.md).

## Objetivo

Recorte adicional de CPU-04 além do marco 25%: separar seleção de grass-mod
(`placeBiomeGrass`-style) e intents de spawn de bioma dos efeitos de escrever
mods / construir plantas, objetos e tesouros. Sem Root, PlantDatabase,
ObjectDatabase, WorldStorage vivo ou publish.

## Escopo

| Artefato | Papel |
| --- | --- |
| `WorldGenerationFloraTileSample` | tile + vizinhança owner-capturada |
| `planWorldGenerationFloraGrassIntents` | floor/ceiling + density + staticRandom |
| `planWorldGenerationFloraSpawnIntents` | ordena candidatos; ignora dungeon |
| `WorldGenerationFloraSpawnPlan` | `tryPlan` / `tryConsume` / `discard` |

## Validação

NoAssets `WorldGenerationFloraSpawnPlanTest.*`: **6/6**.

## Maturidade

CPU-04 permanece em **25%**: falta `replaceBiomeBlocks`, equivalência viva ao
`prepareSector`, hue shifts autoritativos e publicação. Este recorte não
concede 50%.

## Próximo

Decode TMX real (MD-02) em sombra, ou equivalência viva só com
`apis_ready` live-publish.
