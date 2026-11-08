# Validação dinâmica e entrega de planos prontos — Tracy 20

**Build:** `658eee9952074fe563e1e42dbd949547b5a4d1a4`

## Artefatos

| Artefato | Tamanho | SHA-256 |
| --- | ---: | --- |
| `tracy20.z01` | 33.554.432 bytes | `7e41613cfd15c0219f893dedd3541529a0cda4f7dbf5879d73a87d753520cecd` |
| `tracy20.z02` | 33.554.432 bytes | `2793b5c5bda9a32afd079e31895048619f24e8c3bf4b7e34d9c9b94554dc9543` |
| `tracy20.zip` | 19.692.823 bytes | `6168df73109a99401c2d521687aa41b2f506e89729850f01511bdfc0f85d3329` |
| Tracy reconstruído | 86.788.676 bytes | `2609de78896426feae0f645935640e62ddbc36dd320d79b137d41a1ea6d06e9c` |
| `exploration.zip` | 9.901.722 bytes | `1b24f475546a885885a9cab756af0c4cc712b5e1188e93724b9d0004ed0e8cb9` |
| `starbound.log` | 134.307 bytes | `12b23e40146b7963bb97160353ef23801731cb6e53cedea5b1dff3805040ca28` |
| `starbound.config` | 10.797 bytes | `8ad34b0130073510a32f7188b9c04d51f226da09b66a67582f232fb71aeffdd3` |

O ZIP multipartido precisou ser concatenado para extração; `unzip` reporta o
offset típico desse formato, mas valida `tracy20.tracy` e o
`tracy-csvexport` oficial 0.11.2 o lê integralmente. O jogo encerrou de forma
normal, sem crash de mundo.

## Sessão e política dinâmica

Há uma sessão finalizada de 464,721 s no mundo `CelestialWorld:...:9`.
Configuração relevante: 11 workers automáticos, `performanceMode: dynamic`,
teto de 40 ms, ceiling de update de 32 ms, fila de resultados BaseTiles e
MicroDungeons ativa; o feeder de dependência de MicroDungeons permaneceu
desligado.

| Medida | Resultado |
| --- | ---: |
| Setores completos / expirados | 2.159 / 653 |
| Conclusões por segundo | 4,65 |
| Passagens dinâmicas / limitadas por pressão | 5.305 / 4.161 |
| Orçamento efetivo médio | 25,52 ms |
| Orçamento desejado médio / headroom médio | 33,98 / 27,06 ms |
| Pool BaseTiles: workers médios / ativos médios | 11,0 / 0,07 |
| Planos MicroDungeons pendentes médios | 1,06 |

O modo dinâmico funcionou como especificado: preservou o budget base e limitou
o burst à folga de update. Nesta rota, portanto, ele não é um modo de saturação
de CPU: a pool ficou quase ociosa. A comparação com Tracy 19 não é causal
(rota e duração diferentes), mas os dados descartam falta de workers BaseTiles
ou de orçamento serial como gargalo imediato.

## Fila de planos prontos

| Medida | Resultado |
| --- | ---: |
| Planos agendados / resultados aplicados | 2.861 / 2.828 |
| Prontos enfileirados | 2.861 |
| Commitados pela fila visível | 1.359 |
| Selecionados visíveis | 1.367 |
| Resultados stale / retry | 24 / 24 |
| Taxa stale | 0,84% |
| Bloqueios visíveis: MicroDungeons / BaseTiles | 2.858 / 1.022 |

A fila explícita é eficaz e segura neste recorte: o stale caiu de forma
substancial frente aos testes anteriores de alimentação especulativa, e quase
metade dos planos foi entregue diretamente pela fila de visibilidade. Contudo,
a própria fila raramente teve backlog (média 0,90); o gargalo passou a ser a
**admissão insuficiente de planos úteis**, não a entrega de resultados prontos.

O próximo teste deve ativar, sem aumentar limite inicialmente:

```json
"generationDependencyMicroDungeonPreparationPreview": true,
"generationDependencyMicroDungeonCandidateLimit": 4
```

Manter a fila de planos prontos ativa e comparar a mesma rota. Avançar somente
se ocupação dos workers subir, stale permanecer baixo e expirações não piorarem.

## Aplicação serial e Tracy

| Zona | Eventos | p50 | p95 | p99 | Máximo |
| --- | ---: | ---: | ---: | ---: | ---: |
| `WorldServerThread update` | 31.920 | 5,924 ms | 15,128 ms | 184,352 ms | 184,352 ms |
| `WorldStorage generate queue` | 7.707 | 1,030 ms | 11,655 ms | 182,195 ms | 182,195 ms |
| `WorldServer entities` | 31.876 | 3,864 ms | 9,776 ms | 111,261 ms | 111,261 ms |
| `Generate microdungeons` | 6.152 | 0,560 ms | 0,956 ms | 148,258 ms | 148,258 ms |
| `Microdungeon plan candidates` | 3.093 | 5,399 ms | 9,681 ms | 16,899 ms | 16,899 ms |
| `Generate cave liquid` | 2.622 | 0,017 ms | 3,843 ms | 6,332 ms | 6,332 ms |

A nova instrumentação mostra que a aplicação comum é barata, mas alguns planos
de dungeon têm custo serial alto: `factory` teve 997 amostras, 2.261,614 ms no
total, p95 superior de 102,307 ms e máximo 142,887 ms. Revalidação, fachada e
limpeza são desprezíveis; publicação de tiles teve somente 28 amostras e
6,225 ms no total. Os outliers de MicroDungeons explicam os p99 de update e
fila. Não ampliar o budget acima de 40 ms antes de atacar esses outliers.

## Observabilidade

Apesar de esta build já conter fallback no writer, o log ainda registrou três
`BadVariantType` na categoria `storage`. A exceção acontecia ao testar campos
opcionais durante a montagem da categoria, antes de `makeCategory()` e do
fallback de escrita. A correção posterior usa objetos tipados locais para o
teste de presença e elimina esse `toObject()` inseguro. Ela precisa de
validação na próxima execução.

## Decisão

1. Manter a fila de planos prontos e sua instrumentação.
2. Testar feeder de dependência de MicroDungeons limitado a quatro candidatos,
   pois a pool está ociosa e stale está baixo nesta nova entrega.
3. Não aumentar BaseTiles, workers ou orçamento máximo nesta etapa.
4. Após o feeder estar ocupado, isolar os planos `factory` acima de 25 ms por
   dungeon/asset/footprint sem gravar payloads de mod, para atacar os outliers
   seriais de forma compatível.
