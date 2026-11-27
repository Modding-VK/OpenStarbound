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

Sem payloads de tiles reais, sem candidatos ordenados concretos, sem install
no `WorldServer`. Consumo das formas H1 (payload/path/handoff) em
[consume](sector-runtime-h2-prepared-sector-consume-2026-11-27.md).

## Validação

`world_generation_contract_tests` NoAssets, filtro PreparedSector + H2
adjacente: ver relatório de consumo (PreparedSector **7** + adjacentes).

## Limitações

Não há conteúdo compartilhado entre consumidores além da chave/retenção.
Aceitação 50%+ exige produtos reais, versionamento entre etapas e equivalência
com o store H1 no Integrador.

## Próximo passo

Consumo H1-shaped entregue no handle H2. Unificação store↔handle e install
autorizado ficam para Integrador / próxima ordem Project. Não disparar Actions.
