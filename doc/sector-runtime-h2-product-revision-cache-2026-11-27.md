# H2 / RAM-02: cache versionada por produto/revisão

Data: 27/11/2026 UTC. Branch `cursor/h2-products-prefetch-9f77`, base `e55c008f`.

[Memory/prefetch](sector-runtime/memory-and-prefetch.md)
· [Progressão](sector-runtime/progress.md)

## Estado: primeiro marco (25%)

`WorldGenerationProductRevisionCache` — camada residente opt-in:

- chave mínima: kind + sector + `templateRevision` + `tileRevision` + `assetGeneration`
- hit só com igualdade exacta; revisão diferente é miss
- invalidação seletiva por template/tile/asset ou All
- capacidade com eviction da inserção mais antiga
- rejeita placeholder de 0 bytes (não imita setor salvo)

Não publica no mundo vivo, não substitui setor salvo modificado e não é
`PreparedSector` completo (CPU-02 recebeu identidade/handle em
[prepared-sector](sector-runtime-h2-prepared-sector-2026-11-27.md); payloads
reais ainda pendentes).

## Validação

`WorldGenerationProductRevisionCacheTest`: **7/7** no alvo
`world_generation_contract_tests` (NoAssets).

## Próximo passo

RAM-03 (retenção/benefício/pressão sobre esta cache) ou amarrar payloads
reais de BaseTiles privados sem publicação. Não disparar Actions.
