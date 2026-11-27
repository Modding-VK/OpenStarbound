# H3 / MD-02: Assets-backed TMX decode em sombra (fixture)

Data: 27/11/2026 UTC. Branch `cursor/h3-kernels-publication-9f77`.
Antecedente: [codec TMX](sector-runtime-h3-tmx-static-codec-2026-11-27.md).

## Objetivo

Recorte adicional de MD-02: resolver `tilesets[].source` contra um store
in-memory (`WorldGenerationTmxShadowAssetStore`) sem Root/Assets vivo, anexar
`WorldGenerationTmxStaticTilesetRef` ao produto estático e misturar o hash do
tileset no `contentHash` — ainda sem publish.

## Escopo

| Artefato | Papel |
| --- | --- |
| `WorldGenerationTmxShadowAssetStore` | fixture Json path→tileset |
| `worldGenerationTmxTilesetAssetPath` | normaliza `…/tilesets/…` como o vivo |
| `tryDecode…WithAssets` | decode + refs firstGid/path/tileCount/hash |
| tilesets embutidos | recusados neste slice (só `source` externo) |

## Validação

NoAssets `WorldGenerationTmxShadowAssetsTest.*`: **6/6** (path, resolve,
missing, embedded, hash change, empty tilesets). Regressão
`WorldGenerationTmxStaticProductTest.*` + `WorldGenerationTmxStaticDecodeTest.*`:
**15/15**.

## Maturidade

MD-02 permanece em **25%**: falta cache/reload e equivalência a
`TMXMap`+`tilesetDatabase` vivos. Não concede 50%.

## Próximo

Cache/reload do produto TMX em sombra, ou equivalência viva só com
live-publish.
