# Validação do planejamento MicroDungeons da cadeia — Tracy 16

**Build observada:** `db6bd2c65bd70de1bb52cc84730d733cda679c2e`  
**Objetivo:** validar o preview de planejamento privado de MicroDungeons na
fronteira de dependências visíveis e confirmar o isolamento de relatórios por
lançamento.

## Artefatos

| Artefato | Tamanho | SHA-256 |
| --- | ---: | --- |
| `starbound.config` | 10.405 bytes | `82d7b97a8176708618393691de3e611cf61ccb8e4f006abdab4a6988ad4d1084` |
| `exploration.zip` | 29.577.444 bytes | `ee9e3c8d2ecbf6f68d8e5f56a931d39c087054fa5c5a381d237e4b6c8aaea6b9` |
| `starbound.log` | 105.039 bytes | `7e369c4a4068f040fe265ff245825d33bb512ca7df17bc8160a32be05971c75b` |
| `16.z01` | 33.554.432 bytes | `daf0493972fee9e2cb24dbce7b2ecb2e0edf2f313196808cc9193e04b7ab5922` |
| `16.zip` | 25.171.132 bytes | `88db18219de1a4c94c18a6c3b98c4b3f77c447df2b648d8277d97d8f7248bdf8` |
| `16.tracy` reconstruído | 58.722.909 bytes | `770e3c99b32f300b0d5d15c124a7b7734bef38409aa8b5fb40cbc49d2be91817` |

O ZIP passou na integridade. O log confirma a build e encerramento normal, sem
`Access violation`, `MapException` ou `WorldStorageException`. A configuração
confirma os previews de BaseTiles, dependências, risco de expiração e
MicroDungeons, com limite de quatro candidatos.

## Isolamento de relatórios

A correção de rotação funcionou: o relatório anterior de `4:3` foi preservado
como `.jsonl.1`, enquanto a sessão atual do mundo
`CelestialWorld:383738451:-48324174:-267567852:6:1`, instância 19, possui uma
única sessão (`sessionStartedMonotonicMilliseconds=695039920`) e `final: true`.
Não foi necessário reconstruir fronteiras temporais.

## Resultado observado

| Medida | Resultado |
| --- | ---: |
| Duração / janelas | 142,616 s / 71 |
| Setores completos / expirados | 702 / 269 |
| Profundidade média da fila | 42,872 |
| Espera média / máxima de setor | 6,577 / 16,999 s |
| Promoções / tentativas / avanços de risco | 1.938 / 1.785 / 1.066 |
| BaseTiles agendados / aplicados / commitados | 1.335 / 1.332 / 1.320 |
| Planejamentos MicroDungeons do feeder: tentativa / admissão | 2.944 / 812 |
| Planos MicroDungeons agendados / aplicados / stale | 1.471 / 1.036 / 382 |
| Bloqueios BaseTiles / MicroDungeons / vizinhos | 691 / 1.054 / 3.359 |

O preview foi exercitado e não causou crash. Contudo, 382 planos stale são
trabalho privado perdido demais para adoção. Esta sessão é outro mundo/rota em
relação à run 15; não é permitido comparar suas taxas diretamente.

## Tracy oficial

A captura multipartes foi reconstruída com `zip -FF`, passou em `unzip -t` e
foi exportada com `tracy-csvexport` oficial 0.11.2,
`wolfpld/tracy@b8c2e25c3d02009059e09f623072921677dd6bb4`.

| Zona | Eventos | p50 | p95 | p99 | Máximo |
| --- | ---: | ---: | ---: | ---: | ---: |
| `WorldServerThread update` | 14.438 | 3,997 ms | 12,856 ms | 31,536 ms | 253,343 ms |
| `WorldStorage generate queue` | 3.556 | 1,139 ms | 17,149 ms | 54,690 ms | 151,193 ms |
| `WorldServer entities` | 14.402 | 2,443 ms | 6,950 ms | 10,231 ms | 174,801 ms |
| `Base tiles worker` | 3.680 | 2,280 ms | 10,021 ms | 17,252 ms | 184,991 ms |
| `Base tiles worker prepare` | 1.840 | 2,278 ms | 9,790 ms | 13,447 ms | 21,878 ms |
| `Generate microdungeons` | 304.132 | 0,001 ms | 0,001 ms | 0,004 ms | 146,877 ms |
| `Generate cave liquid` | 1.268 | 0,020 ms | 4,225 ms | 6,203 ms | 8,941 ms |

O planejamento de MicroDungeons tem cauda mínima na maioria dos eventos; os
picos coincidem com o caminho serial mais amplo. A fila de geração, por outro
lado, mantém cauda de p95 17,149 ms e p99 54,690 ms. Evitar snapshots stale é,
portanto, redução de trabalho desperdiçado compatível com a prioridade de
entrega, não autorização para externalizar aplicação/entidades.

## Causa e correção seguinte

O planejador assíncrono captura e revalida um halo 3×3. A fronteira de
dependências usada pelo feeder garantia BaseTiles apenas pela regra cardinal;
uma diagonal do halo ainda podia mudar durante o planejamento, invalidando o
resultado. A correção posterior a esta coleta exige `BaseTiles` em todos os
setores válidos do halo antes de admitir o plano e separa nas métricas os
resultados do feeder em `.microDungeonPlanCommitted` e `.microDungeonPlanStale`.

O novo recorte permanece opt-in. O próximo teste deve repetir esta rota com a
build corrigida e comparar especificamente admissões versus stale do feeder,
expirações, `sectorWait` e Tracy.
