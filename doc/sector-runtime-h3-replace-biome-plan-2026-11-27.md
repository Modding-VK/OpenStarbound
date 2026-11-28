# H3 / CPU-04: replaceBiomeBlocks em sombra

Data: 27/11/2026 UTC. Branch `cursor/h3-kernels-publication-9f77`.
Antecedente: [Flora/spawn](sector-runtime-h3-flora-spawn-plan-2026-11-27.md).

## Objetivo

Recorte adicional de CPU-04: resolver placeholders de bioma
(`BiomeMaterialId` / `Biome1..5`) em delta ordenado de material/hue/mod, com
amostras de main/subBlocks e hue fornecidas pelo dono. Sem Root,
MaterialDatabase, mutação de tile ou `updateCollision`.

## Escopo

| Artefato | Papel |
| --- | --- |
| `WorldGenerationReplaceBiomeTileSample` | placeholders + biome index |
| `WorldGenerationReplaceBiomeBlockSample` | mainBlock / subBlocks / hues |
| `planWorldGenerationReplaceBiomeDelta` | resolve ou limpa sem biome |
| `WorldGenerationReplaceBiomePlan` | `tryPlan` / `tryConsume` / `discard` |

## Validação

NoAssets `WorldGenerationReplaceBiomePlanTest.*`: **6/6**.

## Maturidade

CPU-04 permanece em **25%**: falta equivalência viva ao `prepareSector`,
collision autoritativa e publish. Não concede 50%.

## Próximo

Decode TMX via Assets, ou equivalência viva só com live-publish.
Codec gzip/zlib/zstd já em [TMX codec](sector-runtime-h3-tmx-static-codec-2026-11-27.md).
