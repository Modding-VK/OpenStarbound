# H3 / MD-02: decode Json TMX estático em sombra

Data: 27/11/2026 UTC. Branch `cursor/h3-kernels-publication-9f77`.
Antecedente: [TMX static product](sector-runtime-h3-tmx-static-product-2026-11-27.md).

## Objetivo

Recorte adicional de MD-02: decodificar Json TMX controlado (tilelayers com
`data` array + objectgroups) para `WorldGenerationTmxStaticProduct` sem
Root/Assets/tilesets e sem materializar brushes. CPU-only / NoAssets.

## Escopo

| Artefato | Papel |
| --- | --- |
| `tryDecodeWorldGenerationTmxStaticBlueprint` | Json → blueprint |
| `tryDecodeWorldGenerationTmxStaticProduct` | Json → produto estático |
| tilelayer | só array não comprimido; zlib/gzip/zstd recusados |
| tilesets | ignorados (sem `tilesetDatabase`) |
| objectgroup | nome/posição/tamanho + connector props |

## Validação

NoAssets `WorldGenerationTmxStaticDecodeTest.*`: **6/6**.

## Maturidade

MD-02 permanece em **25%**: falta decode comprimido, carga via Assets,
cache/reload e equivalência a `TMXMap` vivo. Não concede 50%.

## Próximo

Codec comprimido sombra, ou equivalência viva / publish só com
`apis_ready` live-publish.
