# Validação da cache de definições — Tracy 24

**Build:** `23e551fbe4f317b029d04f7ee5d8d29f568212eb`  
**Objetivo:** identificar a causa interna de factories de MicroDungeon lentas
na fase de definição, sem expor nomes, paths ou payloads de mods.

## Artefatos

| Artefato | Tamanho | SHA-256 |
| --- | ---: | --- |
| `24.tracy` | 8.770.548 bytes | `b553699140228d61d574e610ba13f0759fb42da9adc8285ad891e2335cee74f8` |
| `exploration.zip` | 14.186.784 bytes | `0ec4a6b66111146d55153b7710e7c75d03f06c1e046cb146d15c3261a34e0aad` |
| `starbound.log` | 61.435 bytes | `9e014bd39f504024c1f166510f16d81f1eb178d5f93b7472e987ea727bc0a2d2` |
| `starbound.config` | 10.797 bytes | `8ad34b0130073510a32f7188b9c04d51f226da09b66a67582f232fb71aeffdd3` |

A execução encerrou normalmente. A configuração permaneceu em modo `dynamic`,
com workers automáticos, fila de resultados prontos e feeder especulativo de
MicroDungeons desligado.

## Evidência de cache

A sessão isolada iniciou em `751978035.136`, possui 20 janelas e registro
final. A partição adicionada no build foi emitida para quatro factories lentas:

| Hash | Resultado | Total | Cache miss | Lookup | Resolução | Leitura e construção | Build |
| ---: | --- | ---: | --- | ---: | ---: | ---: | ---: |
| `-5547472491861117508` | não gerado | 159,391 ms | sim | 159,056 ms | 0,001 ms | 159,046 ms | 0 |
| `-1320161944544667942` | 1.996 tiles | 51,150 ms | sim | 16,886 ms | 0,001 ms | 16,882 ms | 32,759 ms |
| `4164075777730300935` | 1.237 tiles | 36,742 ms | sim | 14,592 ms | 0,001 ms | 14,588 ms | 21,570 ms |
| `-5547472491861117508` | 2.249 tiles | 31,791 ms | não | 0,000 ms | 0 | 0 | 25,217 ms |

O custo de `cacheLookup` inclui o lock e toda a operação de cache; como ele é
praticamente igual a `readAndBuild` apenas nos misses, a evidência aponta para
leitura/parsing/construção da definição, não para contenção do mutex nem para a
resolução do path. A repetição do hash `-554...` quente em seguida custou
0,0005 ms na definição, confirmando que a cache evita o outlier.

Isso estabelece churn/evicção da cache de definições como hipótese operacional
forte. A coleta atual não mede todos os hits, misses e evicções; portanto, ela
não demonstra ainda qual capacidade é suficiente nem autoriza aumentar um
limite de memória sem observação adicional.

## Tracy e entrega

| Zona | Eventos | p50 | p95 | p99 | Máximo |
| --- | ---: | ---: | ---: | ---: | ---: |
| `WorldServerThread update` | 2.244 | 5,937 ms | 16,942 ms | 37,160 ms | 232,275 ms |
| `WorldStorage generate queue` | 523 | 1,009 ms | 23,491 ms | 47,078 ms | 226,164 ms |
| `Generate microdungeons` | 391 | 0,586 ms | 3,668 ms | 39,162 ms | 217,145 ms |
| `MicroDungeonFactory generate` | 144 | 0,056 ms | 7,644 ms | 51,148 ms | 159,387 ms |
| `WorldServer entities` | 2.186 | 4,130 ms | 9,739 ms | 12,561 ms | 61,924 ms |

O máximo da factory de 159,387 ms coincide com a leitura/construção de
definição. Há também picos de `WorldServerThread update` anteriores e
independentes; a cache explica esse outlier de geração, não todos os stalls.

Foram concluídos 233 setores e expiraram 34. BaseTiles permaneceu subutilizado
(11 workers configurados, 2 worker-passos ativos em 24 amostras), enquanto
MicroDungeons ainda bloqueou 199 tentativas visíveis. Aumentar workers não é a
correção para o miss de definição.

## Decisão e próximo passo

Instrumentar a `DungeonDefinitions` com contadores globais e de cardinalidade
fixa para hits, misses, evicções, ocupação e bytes estimados, sem nomes de
assets. Com essa evidência, implementar uma capacidade de cache explicitamente
orçada — potencialmente configurável no modo agressivo/dinâmico — e validar a
mesma rota contra p95/p99, memória, saves e mods. Não pré-carregar nem mover a
leitura de assets para workers nesta fase: ambos alterariam o contrato de
memória/asset e precisam de auditoria própria.

O perfil transitório `Nowhere` ainda recuperou um `BadVariantType` de
`storage`; a execução não falhou, mas a causa raiz segue pendente.
