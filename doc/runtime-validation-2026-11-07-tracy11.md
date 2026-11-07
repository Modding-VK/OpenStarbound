# Validação pós-correção de BaseTiles — Tracy 11

**Branch/build:** `Experimental`, após `8688e087`  
**Objetivo:** validar em Windows a correção de BaseTiles sem tiles instalados e
observar o preview de decode privado de tiles salvos.

## Artefatos recebidos

| Artefato | Tamanho | SHA-256 |
| --- | ---: | --- |
| `tracy11.z01` | 33.554.432 bytes | `e476d221db1b6510e8f5d550ec01cc5e98b1991388fce6ac401feabf215229e4` |
| `tracy11.zip` | 26.956.804 bytes | `af6a737532959b9f03839bc088599b4936ae49fc64b72ee3bb6f371ea0f67d29` |
| `tracy11.tracy` reconstruído | 60.502.048 bytes | `90c48aae39c6364135f44e7db90b9a4b459b5086d6a8e3518e14f0c0448a3700` |
| `starbound.log` | 82.962 bytes | `5cf90e192024d3e01c401626e6288b2ede7d9a97fe1e4d4b8574adfe6fe35a9e` |
| `exploration.zip` | 17.609.305 bytes | `3a51159695c99fc7bcb1755b5698bbdaa5bc3e73267194df3eb36aa0b49a82cc` |

`exploration.zip` passou no teste de integridade. O Tracy multipart foi
reconstruído com êxito a partir de `.z01` seguido de `.zip`; a extração contém
um trace válido de 60.502.048 bytes. A exportação headless oficial é suficiente
para a análise abaixo; o Tracy Profiler gráfico continua opcional para inspeção
visual de uma janela temporal específica.

## Exportação oficial Tracy (atualização posterior)

Após a reconstrução, o trace foi lido pelo `tracy-csvexport` oficial 0.11.2,
compilado de `wolfpld/tracy@b8c2e25c3d02009059e09f623072921677dd6bb4`.
Foram exportados eventos individuais (`-u`) e calculados percentis por zona
com correspondência exata de nome. O CSV bruto permanece fora do Git.

| Zona | Eventos | p50 | p95 | p99 | Máximo |
| --- | ---: | ---: | ---: | ---: | ---: |
| `WorldServerThread update` | 13.769 | 12,648 ms | 43,377 ms | 75,525 ms | 441,008 ms |
| `WorldServer entities` | 13.681 | 8,987 ms | 32,580 ms | 62,853 ms | 434,482 ms |
| `WorldStorage generate queue` | 3.005 | 1,094 ms | 19,232 ms | 44,423 ms | 170,677 ms |
| `Base tiles worker` | 852 | 1,829 ms | 6,182 ms | 8,672 ms | 115,745 ms |
| `Generate microdungeons` | 1.489 | 0,577 ms | 1,127 ms | 26,171 ms | 170,420 ms |
| `Generate cave liquid` | 609 | 2,739 ms | 4,342 ms | 5,916 ms | 6,627 ms |

`WorldServer entities` é o maior contribuinte observado na thread dona e
explica grande parte da cauda de `WorldServerThread update`. Isso não autoriza
externalizar NPCs: o backlog dessa externalização continua suspenso. O dado
orienta primeiro uma decomposição de perfil de entidades e o avanço de
preparação/commit setorial sem executar estado vivo fora da dona.

`Base tiles worker` é curto no p95 e ocorreu somente 852 vezes; junto das
amostras de pool ociosa, isso confirma que elevar workers não é a próxima
alavanca. A cauda de microdungeons e a fila serial justificam priorizar
pré-requisitos visíveis, resultados prontos e sua espera até commit.

## Cenário e término

Os relatórios agregados pertencem a
`CelestialWorld:383738451:-48324174:-267567852:4`, instância 16: 133 janelas,
267,102 s. O log termina com desconexão, parada do mundo e encerramento normal
da aplicação. Não contém `Access violation`, `prepareBaseTileArray`,
`WorldStorageException` ou `MapException` após o início da sessão.

Os avisos de assets de mods ausentes e a falha conhecida de escrita da categoria
`storage` por `BadVariantType` continuam no log, mas não são caminho fatal desta
sessão e não são atribuídos ao preview.

## Resultado da correção

| Medida | Resultado | Leitura permitida |
| --- | ---: | --- |
| Snapshots de tile decode agendados | 1.203 | O caminho assíncrono foi exercitado. |
| Resultados instalados após ticket | 1.031 | Publicação dona funcionou na maior parte das tarefas. |
| Cancelados em unload | 156 | Resultados não foram publicados após descarga. |
| `generation.baseTiles.tilesUnavailable` | 0 | Nenhum BaseTiles foi admitido antes de tiles instalados. |
| Limite de pendências de decode | 2.774 ocorrências | O preview atingiu seu teto configurado/derivado; isso é pressão de admissão, não falha. |
| BaseTiles prontos/commitados | 851 / 851 | A primeira fila de resultados prontos foi exercitada. |

Assim, a falha do relatório anterior — `copySector()` nulo chegando ao worker
de BaseTiles — **não se reproduziu** no cenário observado. Isto valida a
correção para essa rota, mas não substitui equivalência de save/mods em todas as
rotas nem autoriza ativação por padrão.

## Desempenho observado

| Indicador | Resultado |
| --- | ---: |
| Setores completos | 1.662 (aprox. 6,22/s) |
| Profundidade média da fila | 70,94 setores |
| Maior idade da fila | 59,38 s |
| Orçamento adaptativo médio | 17,69 ms |
| Tempo médio do update antes de geração | 12,00 ms |
| Maior `serverUpdate` | 440,03 ms |
| Workers BaseTiles configurados | 11 |
| Workers BaseTiles ativos nas amostras | 0,247 em média |
| Pendências BaseTiles nas amostras | 1,746 em média |
| Maior atraso pronto→commit de BaseTiles | 1.238,57 ms |

O decode de tiles não é o gargalo dominante nesta rota: 1.031 instalações
ocorreram sem crash, enquanto a pool de BaseTiles ficou subalimentada e a fila
autoritativa permaneceu antiga. O log mostra pedidos em `generationLevel=3`
(`CaveLiquid`) repetidamente `notSelected`; quando tentados, alguns bloquearam
em microdungeons de vizinhos. A interpretação permitida é que seleção,
dependências e commits ainda limitam a entrega. Não há medição contínua de CPU
do sistema nesses dados.

## Próximos passos

1. Manter os previews desligados por padrão e repetir em mundo salvo com a rota
   de unload/reload e dois jogadores, usando os mesmos mods.
2. Instrumentar atraso de decode `scheduled → ready → installed` e amostras de
   pendências, pois a coleta atual demonstra volume, mas não separa o tempo de
   worker do tempo de espera de owner.
3. Evoluir a seleção de commits prontos e a cadeia mínima visível antes de
   aumentar mais workers: há capacidade de BaseTiles ociosa com backlog alto.
4. Adicionar recortes Tracy/JSONL para componentes de entidades (NPC, objetos,
   stagehands e scripts) e comparar suas caudas com `WorldServer entities`, sem
   mover sua execução para workers.
5. Comparar explicitamente `savedTileDecodePendingLimit` em 16, 22 e 32 apenas
   após a rota de equivalência; o contador de limite mostra que o teto merece
   experimento, não elevação automática.

Complementa [runtime-crash-base-tiles-2026-11-07.md](runtime-crash-base-tiles-2026-11-07.md),
[sector-io-ready-queue-audit-2026-11-07.md](sector-io-ready-queue-audit-2026-11-07.md) e o
[checklist operacional](regional-runtime-execution-checklist.md).
