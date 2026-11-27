# H3 / MD-02: produto TMX estático em sombra

Data: 27/11/2026 UTC. Branch `cursor/h3-kernels-publication-9f77`.
Merge alvo: `cursor/integration-sector-runtime-7342`.

[MD-02](sector-runtime/microdungeons-and-tmx.md) · [Progressão](sector-runtime/progress.md)

## Objetivo

Primeiro recorte de MD-02: produto intermediário privado com camadas GID,
objetos/footprints e conectores, versionado por assetGeneration/contentHash,
sem ligação RandomBrush/NpcBrush e sem escrita em facade.

## Escopo

| Artefato | Papel |
| --- | --- |
| `WorldGenerationTmxStaticIdentity` | assetKey + geração + hash + formatVersion |
| `WorldGenerationTmxStaticBlueprint` | input já resolvido pelo dono |
| `WorldGenerationTmxStaticProduct` | `tryBuild` / `tryConsume` / `discard` |

Sem parser TMX completo, sem Assets/Root, sem MD-03 RNG, sem publicação.

## Validação

NoAssets `WorldGenerationTmxStaticProductTest.*`: **6/6**.

## Limitações

Decode Json controlado (sem Assets/compressão):
[TMX decode](sector-runtime-h3-tmx-static-decode-2026-11-27.md). Aceitação
50%+ ainda exige compressão/Assets, cache/reload e ligação MD-03 viva.

## Publicação

`apis_ready` cobre sombra — não live-publish/commit. Publicação autoritativa
permanece fora deste recorte.

## Próximo

Codec comprimido sombra, ou equivalência viva só com live-publish.
