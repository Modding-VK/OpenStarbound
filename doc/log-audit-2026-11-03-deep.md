# Auditoria aprofundada dos 41 fragmentos de log

Análise de 03/11/2026, branch Experimental. Evidência bruta preservada fora do
Git em `/workspace/log-evidence/starbound-2026-11-03/`. Fonte: arquivos
`starbound-part-0001.log` até `starbound-part-0041.log`, lidos na ordem numérica.
Foram decodificados 27.055 relatórios JSON válidos, sem falhas de parsing nem
regressões de horário nessa sequência; 6.634 são relatórios do servidor.

Build registrada: `aeaca59c010899734d6c003e6431113272613776`. O commit
`ab7d4e57` (retenção de resultados e isolamento do cache de colisão) é ancestral
dessa build. Portanto, os atrasos ocorreram com aquela correção presente.
A divisão posterior dos relatórios em arquivos por categoria não foi validada
por esta sessão. Números abaixo são medições instrumentadas de uma sessão,
sem atribuição automática a CPU, disco físico ou hardware específico.

## 1. O setor antigo frequentemente não recebe tentativa completa

Os 162 registros `[SectorQueue]` mostram:

| Observação | Registros |
|---|---:|
| `fullAttempts=0` | 160 |
| `advances=0` | 162 |
| `lastBlock=notSelected` | 112 |
| `lastBlock=none` | 48 |
| `lastBlock=generation.baseTiles` | 2 |
| Nível 3, `CaveLiquid` | 158 |
| Nível 2, `MicroDungeons` | 3 |
| Nível 1, `BaseTiles` | 1 |

São observações limitadas do setor mais antigo; contadores reiniciam ao trocar
o setor acompanhado e após emissão. Não representam todos os setores nem
todas as tentativas desde a criação do pedido.

Às 18:27:58.847, o setor `[407,134]` tinha idade de 30,1 s, nível 3, três
tentativas, nenhuma tentativa completa e nenhum avanço no intervalo medido.
Às 18:27:59.742, o perfil registrou idade máxima de 30.732 ms para esse setor
(`starbound-part-0017.log`, linha 887). Nessa janela foram aplicados 27 resultados
de BaseTiles e concluídos 16 setores, com tick máximo de 33,4 ms.

No código, `WorldStorage::generateQueue` ordena a fila por proximidade. Ao
encontrar o primeiro setor que não completa, ativa `schedulingOnly`; os próximos
pedidos nessa passagem só visam `BaseTiles`. Um setor já no nível 3 pode ser
visitado, receber retorno sem avanço e continuar sem tentativa de `Complete`.
A ordenação por distância é refeita nas passagens seguintes.

Isso sustenta um problema de oportunidade de execução das etapas superiores.
Ainda não comprova que simplesmente alterar a ordem preserve geração idêntica:
microdungeons e finalização têm efeitos e dependências entre setores.

## 2. Execução do worker e latência até aplicação são diferentes

Instância de servidor 32, 18:27:04.356–18:49:14.498, 259 relatórios:

| Medida | Resultado |
|---|---:|
| Tarefas BaseTiles agendadas / resultados aplicados | 7.328 / 7.323 |
| Aplicados pela drenagem de resultados prontos | 7.320 |
| Execução do worker: média / máximo | 2,30 / 144,71 ms |
| Espera na fila do pool: média / máximo | 0,258 / 6,079 ms |
| Resultado pronto até recolhimento: média / máximo | 69,20 / 425,43 ms |
| Passagens visíveis / passagens sem avanço | 11.438 / 1.811 |
| Bloqueios por vizinho / limite de nível | 20.486 / 6.601 |
| Admissões adiadas | 899 |

As médias usam soma de duração dividida por amostras do respectivo campo.
Os contadores de bloqueio incluem tentativas repetidas. `readyResultsCommitted`
é subconjunto de `resultsApplied`, não deve ser somado a ele. A diferença entre
agendamentos e aplicações não prova perda de tarefas.

A amostragem do pool mostra dois workers configurados. O baixo atraso medido
para começar trabalho não demonstra saturação contínua do pool. Aumentar workers
não resolve diretamente a falta de tentativas completas nas etapas superiores.

Os eventos detalhados perderam 3.748 entradas por limite de instrumentação nessa
instância. Assim, parear somente eventos `reserved/committed` produz uma amostra
incompleta. A estimativa anterior de cerca de 72 ms reserva→commit é latência,
não custo médio de execução de BaseTiles.

## 3. Etapas posteriores e expiração

Na instância 32, `generation.microDungeons` acumulou 39,63 s em 6.142 amostras
(média 6,45 ms; máximo 226,70 ms). `generation.finalize` acumulou 23,42 s em
3.673 amostras (média 6,38 ms; máximo 96,26 ms). São candidatos reais a decomposição
em etapas e planejamento paralelo, sujeitos a ordem determinística e footprints.
Esses totais não devem ser somados aos escopos superiores que já os contêm.

Houve 2.784 expirações de entradas de fila. O código remove pedidos cujo TTL
chega a zero; isso pode ser normal quando o jogador se afasta. Não equivale a
2.784 setores corrompidos ou perdidos.

O setor `[499,134]` aparece com idade máxima de 24.819 ms às 18:46:47.928
(`starbound-part-0018.log`, linha 362), e com conclusão após espera de 3.723 ms
às 18:46:57.951 (linha 371). A diferença indica que não é seguro tratar ambos
como a mesma permanência na fila. Falta uma identidade de episódio do pedido
para distinguir expiração/reentrada de outros resets de contagem.

## 4. Objetos, monstros e NPCs têm distribuições diferentes

Na instância 32, entre tempos agregados por tipo de entidade, monstros
representam 62,12%, NPCs 22,06% e objetos 3,13%. A externalização de NPCs não
cobre automaticamente a maior parcela dessa distribuição.

Na instância 18 (14:40:34.451–18:20:39.938), objetos representam 77,36%, monstros
15,81%, veículos 3,01% e NPCs 2,09%. Entre roteadores lentos dessa instância,
o ID 11 aparece 9.794 vezes, acumula 2.887,34 s de Lua nas amostras retidas e
atinge 987,07 ms em uma atualização. IDs só identificam entidades dentro da
instância; não unificar IDs iguais de instâncias diferentes sem outra evidência.

As chamadas de escrita em containers acumulam 988,98 s nessa instância.
`object.container.decodeItem` acumula 697,02 s em 197.570.249 amostras;
`callScriptedEntity` acumula 447,48 s. São escopos inclusivos que podem se
sobrepor. Os 697 s correspondem a cerca de 70,5% do total agregado do escopo
de escrita, mas o log agregado não atribui cada chamada a um ITD específico.
O número de chamadas sugere investigar reutilização de dados decodificados,
consultas em lote e revisões de inventário, além do paralelismo por rede.

O percentual anterior de 97,93% refere-se somente ao Lua das entidades lentas
retidas; não representa 97,93% de todo o scripting da sessão.

## 5. Pathfinding: vida da busca não é tempo de CPU

`PathFinder::explore` registra `npc.pathfinding.searchWall` desde a primeira
exploração até conclusão ou abandono. Uma busca pode sobreviver a vários ticks;
seu tempo inclui intervalos sem execução. Um valor muito maior que um tick não
prova defeito no medidor nem bloqueio contínuo de CPU.

Na instância 32 houve 1.206 buscas iniciadas, 1.033 sucessos, 102 falhas e 71
abandonos. A execução de `pathfinding.explore` acumulou 2,87 s, máximo de
0,5445 ms por chamada. Na instância 18 houve 1.848 buscas iniciadas, 1.517
falhas, 106 sucessos e 224 abandonos; exploração acumulada de 81,45 s, máximo
de 20,54 ms. Isso recomenda investigar tentativas repetidas em caminhos
inacessíveis naquele ambiente. Falha de busca não implica erro da engine.

## 6. Persistência e cliente também têm picos

Na instância 18, `storage.sync.total` chegou a 1.623,31 ms e
`storage.sync.commit` a 1.590,39 ms. `WorldServerThread::run` chama `sync()`
após `update()` na mesma thread. Portanto, `serverUpdate` sozinho não cobre
todo o tempo durante o qual essa thread pode ficar ocupada. O escopo de commit
inclui trabalho do banco e I/O; não identifica sozinho lentidão física do disco.

No painter, a média por amostra de `painter.total` foi 6,21 ms e a de
`painter.environment` 4,45 ms. Estrelas acumularam média de 2,915 ms por amostra;
os orbiters têm duas amostras por render e média de 0,761 ms por chamada.
O ambiente atingiu pico de 203,37 ms. Isso é custo do cliente; os logs agregados
não estabelecem causalidade com a fila de geração do servidor. Também não são
medições isoladas de GPU.

## 7. Limitações e próximos recortes

- A sequência temporal deve ser construída lendo os fragmentos numericamente.
  Saída agregada de busca paralela pode reordenar arquivos; somas permanecem
  úteis, mas primeiro/último registro dessa saída não delimitam uma sessão.
- A instrumentação limita detalhes de tarefas, pacotes e dependências; ausência
  de um detalhe não prova ausência de trabalho. A observação do grafo foi
  truncada 2.150 vezes na instância 32.
- O grafo observado consumiu 2,32 s nessa instância, máximo de 9,28 ms por
  observação. O overhead completo de profiling/serialização/escrita não foi
  isolado. São necessários cenários comparáveis com coleta ligada/desligada.
- Corrigir justiça e progresso das etapas superiores é prioridade fundamentada.
  Uma proposta deve preservar desempate determinístico e conflitos de estruturas;
  prioridade herdada pelos pré-requisitos é parte do desenho a avaliar.
- Acrescentar identidade de permanência na fila e motivo terminal ao rastreamento
  permite distinguir conclusão, expiração, unload e reentrada por setor.
- Persistência merece medição do ciclo inteiro da thread; redes de objetos e
  monstros precisam de frentes próprias no plano de paralelismo.

Auditoria de logs e leitura estática do código. Nenhuma mudança no runtime,
compilação, execução de jogo ou disparo de Actions foi realizado nesta etapa.
