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
| `16.tracy` | acima do limite de transferência do analisador | não analisado neste ambiente; reenviar multipartes para exportação oficial |

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

## Causa e correção seguinte

O planejador assíncrono captura e revalida um halo 3×3. A fronteira de
dependências usada pelo feeder garantia BaseTiles apenas pela regra cardinal;
uma diagonal do halo ainda podia mudar durante o planejamento, invalidando o
resultado. A correção posterior a esta coleta exige `BaseTiles` em todos os
setores válidos do halo antes de admitir o plano e separa nas métricas os
resultados do feeder em `.microDungeonPlanCommitted` e `.microDungeonPlanStale`.

O novo recorte permanece opt-in. O próximo teste deve repetir esta rota com a
build corrigida e comparar especificamente admissões versus stale do feeder,
expirações, `sectorWait` e Tracy. O `16.tracy` precisa ser enviado em ZIP
multipartes (por exemplo `.z01` + `.zip`) se continuar acima de 32 MiB.
