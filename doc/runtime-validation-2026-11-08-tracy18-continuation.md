# Validação da continuidade de cadeia visível — Tracy 18

**Build:** `8fd9f5c1da7b4f267e4e33f3affe3f466e85c0d9`  
**Objetivo:** validar a correção de precedência entre continuidade produtiva e
risco de expiração.

## Artefatos

| Artefato | Tamanho | SHA-256 |
| --- | ---: | --- |
| `tracy18.tracy` | 3.523.881 bytes | `6d04598cab10f83b08db0ea2c98ff7f66dfb795225cf9dd3e574450bcfe2fb37` |
| `starbound.config` | 10.461 bytes | `7096fe932d43a72e452bf87e40505597c5b7602bf6326ac24995320783549122` |
| `exploration.zip` | 4.109.674 bytes | `2bc02cc78d1756cee35cfa03015084ccf704883de0ea25109f379a4ee4013a66` |
| `starbound.log` | 66.485 bytes | `978d4c0bdc86e88ab909243d74b69e934d7f9f2a5ef83a47447bf3c5bed80e6e` |

O ZIP passou na integridade. O log confirma a build e parada normal, sem
`Access violation`, `MapException` ou `WorldStorageException`. A configuração
mantém ativos os previews de risco, continuidade, BaseTiles e MicroDungeons.

## Sessão e resultado

A sessão primária do mundo
`CelestialWorld:383738451:-48324174:-267567852:9:3`, instância 19, está
isolada (uma sessão, 44 janelas / 87,213 s, `final: true`).

| Medida | Resultado |
| --- | ---: |
| Setores completos / expirados | 508 / 227 |
| Profundidade média / espera máxima | 54,243 / 13,252 s |
| Continuidade agendada / retomada / tentada / descartada | 443 / 435 / 434 / 9 |
| Risco: promoções / tentativas / avanços | 839 / 762 / 376 |
| MicroDungeons feeder: admissão / commitado / stale | 578 / 236 / 336 |
| Bloqueador visível: MicroDungeons / BaseTiles / limite | 828 / 268 / 36 |

A continuidade deixou de ser inalcançável: 98,2% das cadeias agendadas foram
retomadas e 98,0% receberam tentativa. A correção cumpre o contrato limitado.
Não é uma comparação causal com Tracy 17, porque mundo/rota/duração diferem.

MicroDungeons continua o bloqueador visível principal. O feeder ainda perde
58,1% das admissões como stale (336 de 578); portanto, aumentar candidatos ou
workers não é justificado. O próximo recorte deve drenar planos prontos por
fila explícita, revalidada e ordenada pela visibilidade.

## Tracy oficial

Exportado com `tracy-csvexport` 0.11.2,
`wolfpld/tracy@b8c2e25c3d02009059e09f623072921677dd6bb4`.

| Zona | Eventos | p50 | p95 | p99 | Máximo |
| --- | ---: | ---: | ---: | ---: | ---: |
| `WorldServerThread update` | 4.128 | 1,070 ms | 4,746 ms | 10,336 ms | 80,558 ms |
| `WorldStorage generate queue` | 1.001 | 1,059 ms | 7,294 ms | 10,880 ms | 23,043 ms |
| `WorldServer entities` | 4.056 | 0,346 ms | 0,626 ms | 0,843 ms | 3,909 ms |
| `Base tiles worker` | 1.004 | 1,788 ms | 3,874 ms | 5,802 ms | 48,189 ms |
| `Generate microdungeons` | 867 | 0,011 ms | 0,784 ms | 1,861 ms | 6,252 ms |
| `Generate cave liquid` | 358 | 0,016 ms | 1,766 ms | 3,832 ms | 4,837 ms |

Os percentis de update/fila são baixos nesta rota; não há justificativa para
aumentar o orçamento serial antes de resolver o desperdício de planos stale.

## Próximo passo

Implementar uma fila de resultados prontos de MicroDungeons: o worker continua
produzindo somente plano privado; a dona revalida, prioriza setores visíveis e
aplica o plano uma vez. A validação deve medir atraso pronto→aplicação, stale e
expirações contra a mesma rota.
