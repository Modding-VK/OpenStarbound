# H3 / MD-02: codec TMX comprimido em sombra

Data: 27/11/2026 UTC. Branch `cursor/h3-kernels-publication-9f77`.
Antecedente: [TMX Json decode](sector-runtime-h3-tmx-static-decode-2026-11-27.md).

## Objetivo

Estender o decode sombra para tilelayers base64 com `gzip` / `zlib` / `zstd`
(e base64 sem compressão), removendo flip bits de GID como o `TMXMap` vivo —
sem Root/Assets/tilesets e sem publish.

## Escopo

| Artefato | Papel |
| --- | --- |
| `worldGenerationTmxDecodeCompressedLayerBytes` | base64 → bytes via gzip/zlib/zstd |
| flip mask `0xe0000000` | strip horizontal/vertical/diagonal |
| array `data` | permanece suportado (sem compression) |

## Validação

NoAssets `WorldGenerationTmxStaticDecodeTest.*`: **9/9** (zlib/gzip/zstd + corrupt).

## Maturidade

MD-02 permanece em **25%**: falta carga via Assets, cache/reload e
equivalência a `TMXMap`+tilesets vivos. Não concede 50%.

## Próximo

Entregue: [Assets-fixture sombra](sector-runtime-h3-tmx-shadow-assets-2026-11-27.md).
Seguinte: cache/reload ou equivalência viva só com live-publish.
