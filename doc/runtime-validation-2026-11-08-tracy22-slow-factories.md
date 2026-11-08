# Validação de factories lentas — Tracy 22

**Build:** `8dfeea97bb5b6f8124876d763d89dd75be4a7c90`  
**Objetivo:** identificar, sem expor conteúdo de mods, as factories de
MicroDungeons que causam outliers seriais.

## Artefatos

| Artefato | Tamanho | SHA-256 |
| --- | ---: | --- |
| `22.tracy` | 15.259.933 bytes | `9f189abe3c03b648a8908b6a6c9d594203ea3f04c3437a649db8b0cc1fb63ff3` |
| `exploration.zip` | 13.384.347 bytes | `9cb74c524e8cdea0484c7deb6109ab24146986368ab034f2e8adcb66d157885a` |
| `starbound.log` | 74.983 bytes | `91e7da901198179238898664b548f948941a9c25a89e5c929a9ce643fee26bc8` |
| `starbound.config` | 10.797 bytes | `8ad34b0130073510a32f7188b9c04d51f226da09b66a67582f232fb71aeffdd3` |

Execução encerrada normalmente. O feeder especulativo de MicroDungeons estava
desligado, como decidido após Tracy 21.

## Sessão isolada

O arquivo primário ainda contém uma sessão anterior (Tracy 21), devido à falha
de isolamento de nome de relatório; esta análise seleciona somente a sessão
com início monotônico `745477244.543`, 37 janelas, 73,882 s e `final: true`.

| Medida | Resultado |
| --- | ---: |
| Setores completos / expirados | 354 / 115 |
| Conclusões por segundo | 4,79 |
| Planos agendados / aplicados / stale | 474 / 463 / 9 |
| Taxa stale | 1,90% |
| Entrega pronta de MicroDungeons | 262 commits |
| Pool: workers médios / ativos médios | 11,0 / 0,13 |
| Bloqueios visíveis: MicroDungeons / BaseTiles | 474 / 167 |

Sem feeder, o stale volta a ser baixo. A pool continua subutilizada, mas a
evidência anterior demonstra que preencher essa folga por snapshots de
dependência é contraproducente.

## Factories lentas observadas

`slowMicroDungeonFactories` reteve nove ocorrências acima de 25 ms. Os hashes
são identificadores numéricos estáveis; não representam nomes de assets ou
conteúdo de mods.

| Template hash | Footprint | Resultado | Ocorrências | Total | Máximo |
| ---: | ---: | --- | ---: | ---: |
| `-5547472491861117508` | 2.249 | gerado | 2 | 79,215 ms | 43,882 ms |
| `-7285595532071091871` | 0 | não gerado | 1 | 138,818 ms | 138,818 ms |
| `4443550473309009783` | 895 | gerado | 1 | 56,819 ms | 56,819 ms |
| `4164075777730300935` | 1.473 | gerado | 1 | 48,862 ms | 48,862 ms |
| `-8965889747190644131` | 755 | gerado | 1 | 40,215 ms | 40,215 ms |
| `7936031634133527863` | 0 | não gerado | 1 | 37,584 ms | 37,584 ms |
| `-3671785436117245938` | 894 | gerado | 1 | 29,256 ms | 29,256 ms |

O custo não depende apenas do footprint: a factory de 138,818 ms não gerou
estrutura. A próxima investigação deve instrumentar fases internas da factory
para distinguir seleção, parsing/template, tentativas de posicionamento e
falha, antes de modificar regras de geração.

## Tracy

| Zona | Eventos | p50 | p95 | p99 | Máximo |
| --- | ---: | ---: | ---: | ---: | ---: |
| `WorldServerThread update` | 4.422 | 4,731 ms | 19,991 ms | 150,232 ms | 658,542 ms |
| `WorldStorage generate queue` | 969 | 1,544 ms | 17,361 ms | 40,699 ms | 182,978 ms |
| `WorldServer entities` | 4.369 | 2,629 ms | 9,630 ms | 147,869 ms | 229,850 ms |
| `Generate microdungeons` | 946 | 0,579 ms | 1,359 ms | 40,327 ms | 181,135 ms |
| `Microdungeon plan candidates` | 474 | 5,774 ms | 9,625 ms | 11,056 ms | 11,504 ms |

O p99 de geração permanece alto por outliers de aplicação; o planejamento em
worker não é o custo dominante. Há também picos independentes em entidades,
que devem ser correlacionados antes de atribuir todo stutter à geração.

## Operação de relatórios

O perfil transitório `Nowhere` ainda emitiu `BadVariantType` na categoria
`storage`, e a sessão foi anexada ao arquivo primário anterior. A correção
posterior adiciona fallback externo e inclui o início de sessão no nome de
arquivo, além de limpar paths ao trocar `Nowhere` pelo id real do mundo.
