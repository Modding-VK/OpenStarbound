# H3 / MD-02: cache/reload TMX estático em sombra

Data: 27/11/2026 UTC. Branch `cursor/h3-kernels-publication-9f77`.
Antecedente: [Assets-fixture](sector-runtime-h3-tmx-shadow-assets-2026-11-27.md).

## Objetivo

Recorte adicional de MD-02: cache privado de `WorldGenerationTmxStaticProduct`
chaveado por assetKey/geração/versões, com orçamento de bytes retidos, contadores
de hit/miss/evict e `reload()` explícito que limpa entradas para um reload de
mods não reutilizar produto stale — sem Root vivo e sem publish.

## Escopo

| Artefato | Papel |
| --- | --- |
| `WorldGenerationTmxShadowCacheKey` | assetKey + assetGeneration + format/product |
| `WorldGenerationTmxShadowCache` | get/insert/getOrDecode + budget LRU |
| `reload()` | limpa + bump de epoch |
| `invalidate` / `invalidateAll` | remoção explícita |

## Validação

NoAssets `WorldGenerationTmxShadowCacheTest.*`: **8/8**. Regressão
`WorldGenerationTmxShadowAssetsTest.*`: **6/6**.

## Maturidade

MD-02 permanece em **25%**: falta equivalência a `TMXMap`+`tilesetDatabase`
vivos. Não concede 50%.

## Próximo

Equivalência viva só com `apis_ready` live-publish, ou outro recorte H3 sombra
autorizado.
