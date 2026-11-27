# H2 / CPU-02: identidade PreparedSector e handle residente

Data: 27/11/2026 UTC. Branch `cursor/h2-products-prefetch-9f77`.

[RAM-02 cache](sector-runtime-h2-product-revision-cache-2026-11-27.md)
· [Retenção/prefetch/batch/IO](sector-runtime-h2-resident-prefetch-batch-io-2026-11-27.md)
· [Progressão](sector-runtime/progress.md)
· [CPU-02](sector-runtime/cpu-and-stages.md)

## Objetivo

Primeiro recorte executável de CPU-02: produto versionado privado
(`WorldGenerationPreparedSectorIdentity` + `WorldGenerationPreparedSector`)
que retém uma entrada na cache residente e só consome após revalidação do
dono. Preparar não publica no mundo vivo.

## Escopo do recorte

| Artefato | Papel |
| --- | --- |
| `WorldGenerationPreparedProduct` | enum de produtos (BaseTiles, campo, candidatos, vazio, delta) |
| `WorldGenerationPreparedSectorIdentity` | task + revisões + região + `fromBaseTiles` |
| `WorldGenerationPreparedSector` | `tryPrepare` / `tryConsume` / `discard` com consumer na cache |

Sem payloads de tiles, sem campos terreno reais, sem candidatos ordenados,
sem install no `WorldServer`.

## Validação

`world_generation_contract_tests` NoAssets, filtro PreparedSector + H2
adjacente: **27/27** (PreparedSector **4** + cache/prefetch/batch/decode **23**).

## Limitações

Não há conteúdo compartilhado entre consumidores além da chave/retenção.
Aceitação 50%+ exige produtos reais, versionamento entre etapas e equivalência
com o pipeline H1.

## Próximo passo

H2 independente esgotado nesta branch: payloads BaseTiles/`PreparedSector`
completos, WorldStorage e WorldThread dependem de H1/infra. Não disparar Actions.
