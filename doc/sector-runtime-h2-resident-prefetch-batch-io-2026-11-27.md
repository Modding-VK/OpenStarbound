# H2: retenção, prefetch, lote regional e promoção de decode

Data: 27/11/2026 UTC. Branch `cursor/h2-products-prefetch-9f77`, base `388241da`.

[RAM-02 cache](sector-runtime-h2-product-revision-cache-2026-11-27.md)
· [Progressão](sector-runtime/progress.md)

## Estado: primeiros marcos (25%) nos seis contratos de H2

| Contrato | Artefato | Escopo do recorte |
| --- | --- | --- |
| RAM-02 | `WorldGenerationProductRevisionCache` | chave/invalidação (já em 25%) |
| RAM-03 | retenção soft/hard + histerese + benefício/demanda | drena especulação antes do visível |
| RAM-05 | `acquireConsumer` / `releaseConsumer` | consumidor ativo não é evicto/invalidado |
| RAM-04 | `planWorldGenerationPrefetch` | horizonte por velocidade/wrap + orçamento |
| WT-03 | `WorldGenerationRegionDemandBatch` | plano/admit/ready da região antes de wait |
| IO-01 | `WorldGenerationDecodePromotionTicket` | Prefetch→Visible sem segundo decode |

Sem publicação no mundo vivo, sem `WorldStorage` load, sem scripts/entidades.

## Validação

`world_generation_contract_tests` NoAssets:

- ProductRevisionCache **10**
- PrefetchPlan **5**
- RegionDemandBatch **4**
- DecodePromotion **4**
- **Total 23/23**

## Limitações

Não há payloads BaseTiles reais na cache, executor H1, benchmark de startup
nem decode SQLite. Aceitação integral (50%+) permanece aberta.

## Próximo passo

Primeiro recorte de CPU-02 entregue em
[PreparedSector](sector-runtime-h2-prepared-sector-2026-11-27.md).
H2 independente restante (payloads reais, WorldStorage, WorldThread) depende
de H1/infra. Não disparar Actions.
