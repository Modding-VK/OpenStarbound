# Validação acelerada de setores padrão — Tracy 19

**Build:** `d8aed14dc43b8f2dd3ff83d0994f1544bcb3036e`  
**Objetivo:** medir o limite prático da entrega de setores padrão ao combinar
modo agressivo, orçamento máximo de 40 ms, alimentador mais amplo e capacidade
de pendências BaseTiles de 4×. O escopo não inclui ampliar o planejamento
especulativo de MicroDungeons.

## Artefatos

| Artefato | Tamanho | SHA-256 |
| --- | ---: | --- |
| `tracy19.tracy` | 5.221.649 bytes | `0d191a165dcd6bac8a1eb33a78eda3e44a4814e0bb6e8bcdc9a13ec77c8f0e66` |
| `exploration.zip` | 5.582.177 bytes | `51cd0875e557c0b573b2e09a66457613c3ab3be74eda21cc21cd572b3e3752ff` |
| `starbound.log` | 82.583 bytes | `6ad3ed05d6b1fea9981b566cdf8d67dc8d3b171ea02c468b68d4c7dbb34ff318` |

O Tracy é lido diretamente pelo `tracy-csvexport` oficial 0.11.2
(`wolfpld/tracy@b8c2e25c3d02009059e09f623072921677dd6bb4`). O log confirma a
build acima, encerramento normal e não contém `Access violation`,
`MapException` ou `WorldStorageException`.

## Escopo efetivamente observado

O relatório primário do mundo `CelestialWorld:...:9:3` contém **duas sessões**
de profiling, ambas finalizadas, totalizando 178,615 s. Isso impede compará-lo
causalmente com uma única rota anterior, mas não invalida os contadores, que
foram agregados por sessão.

| Sessão | Duração | Completos / expirados | BaseTiles agendados / aplicados / commitados |
| --- | ---: | ---: | ---: |
| 1 | 66,194 s | 416 / 120 | 502 / 502 / 498 |
| 2 | 112,421 s | 1.055 / 410 | 857 / 857 / 759 |
| Total | 178,615 s | 1.471 / 530 | 1.359 / 1.359 / 1.257 |

O preview de capacidade foi realmente exercitado: o teto registrado é **44**
pendências BaseTiles (pool automática de 11 workers × multiplicador 4). Ainda
assim, a profundidade média observada foi apenas **3,72**
(`10.780 / 2.901` amostras) e a pool teve média de **11,0** workers. Logo, a
capacidade de 44 não ficou saturada; aumentá-la novamente não entrega mais
setores nesta rota.

O alimentador admitiu 971 de 1.290 candidatos BaseTiles. Não há resultado
BaseTiles stale relatado. A fila explícita funcionou: 1.257 resultados prontos
foram enfileirados e commitados pela thread dona.

## Gargalo atual

| Indicador visível | Ocorrências |
| --- | ---: |
| Bloqueio em MicroDungeons | 1.282 |
| Bloqueio em BaseTiles | 315 |
| Bloqueio por limite de nível | 79 |
| Bloqueio por deadline | 1 |
| Continuidade de cadeia tentada | 717 |
| Orçamento esgotado | 6 |

Apesar do orçamento máximo de 40 ms, ele se esgotou somente seis vezes. A
world thread não está limitando esta coleta por falta de janela serial, e
BaseTiles já fica pronto rapidamente. MicroDungeons é aproximadamente quatro
vezes mais frequente como primeiro bloqueio causal; o maior ganho agora não é
elevar workers BaseTiles, nem elevar o orçamento além de 40 ms.

O próprio teste preserva um sinal conhecido: o alimentador de planos de
MicroDungeons, mesmo desligado na configuração de dependência, ainda registra
297 admissões, 123 commits e 167 resultados stale por outros caminhos de
planejamento. Isso reforça que não se deve ampliar essa produção especulativa
antes de uma entrega de planos prontos revalidada.

## Tracy

| Zona | Eventos | p50 | p95 | p99 | Máximo |
| --- | ---: | ---: | ---: | ---: | ---: |
| `WorldServerThread update` | 6.995 | 1,342 ms | 4,583 ms | 9,347 ms | 128,922 ms |
| `WorldStorage generate queue` | 1.733 | 1,147 ms | 5,422 ms | 10,763 ms | 127,946 ms |
| `WorldServer entities` | 6.953 | 0,411 ms | 1,306 ms | 2,175 ms | 8,611 ms |
| `Generate base tiles` | 1.067 | 0,027 ms | 0,069 ms | 0,107 ms | 0,328 ms |
| `Generate microdungeons` | 1.631 | 0,522 ms | 0,893 ms | 1,239 ms | 127,605 ms |
| `Generate cave liquid` | 728 | 0,011 ms | 0,040 ms | 0,208 ms | 2,129 ms |

Os máximos de ~128 ms em update/fila coincidem com os máximos de
MicroDungeons. Eles são outliers, não uma justificativa para ampliar o budget
serial: os p95/p99 da fila seguem abaixo de 11 ms mesmo nesta configuração
agressiva.

## Decisão

1. Manter `generationBaseTilesDeliveryCapacityPreview: true` e multiplicador 4
   como preview seguro para a próxima validação; o caminho é estável e não
   apresentou stale BaseTiles.
2. Não elevar o multiplicador, workers nem orçamento serial por enquanto: há
   folga clara na capacidade BaseTiles e no orçamento de 40 ms.
3. Próxima alteração de código: criar fila explícita de **planos prontos de
   MicroDungeons**, com ticket/revisões, priorização por pedido visível e
   aplicação exclusiva pela thread dona. Medir pronto→aplicação, stale e
   expirações contra uma rota única e limpa.
4. A advertência isolada `ExplorationProfile ... BadVariantType` durante a
   escrita de um relatório permanece um defeito de observabilidade conhecido;
   não interrompeu a execução, mas deve ser corrigida antes de usar relatórios
   como série longitudinal automatizada.
