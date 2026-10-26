# Diagnóstico da exploração no Windows

## Objetivo e referência

A compilação `684e6ae0` continua sendo a referência funcional, com iluminação
confirmada pelo usuário. Esta instrumentação acrescenta medições opcionais ao
cliente e ao servidor local. Ela não implementa geração paralela nem modifica a
ordem de execução da simulação.

O diagnóstico está desativado por padrão. Quando desativado, os novos medidores
não consultam o relógio, acumulam amostras ou emitem relatórios. Há verificações
condicionais nos pontos instrumentados. Quando ativado, a coleta e a gravação
do log têm custo: use essa execução para localizar gargalos, e execuções com
diagnóstico desativado para comparar desempenho final.
Os marcadores desta ampliação não mudam o formato dos saves ou pacotes, a API
Lua disponível aos mods, nem a ordem das etapas de geração. Os resumos JSON
ficam maiores quando a coleta está ligada; o limite de marcadores por janela
evita crescimento ilimitado.

## Gerar e habilitar

1. No GitHub Actions, execute o workflow **Build**, selecione a branch
   `perf/threading-audit` e habilite **Windows**. Use o artefato dessa execução;
   o executável de referência `684e6ae0` ainda não possui esses medidores.
2. Com o jogo fechado, guarde uma cópia da instalação de referência e da pasta
   `storage`. Instale o novo client pelo procedimento já usado anteriormente.
3. Edite `storage/starbound.config`. Acrescente ou atualize esta propriedade no
   objeto principal, preservando as demais opções e as vírgulas do JSON:

   ```json
   "explorationProfiling" : {
     "enabled" : true,
     "intervalSeconds" : 5,
     "slowOperationMilliseconds" : 50,
     "entityDetailLimit" : 12
   }
   ```

4. Abra o jogo e entre no mundo. O log deve conter linhas com o prefixo
   `[ExplorationProfile] enabled` e, durante a partida, relatórios com o mesmo
   prefixo seguidos de JSON. O nível de log precisa permitir mensagens `Info`
   (padrão). Não é necessário habilitar Lua profiling ou comandos de administrador.
5. Para desativar, feche o jogo, troque `enabled` para `false` e reinicie.

A configuração é lida na criação dos componentes; reinicie o processo para
alterá-la de forma consistente. O intervalo é limitado a 1–60 segundos e o
limiar de operação lenta a 1–60000 milissegundos. Em single-player, a opção do
cliente também habilita o diagnóstico do servidor local. Em multiplayer remoto,
o servidor precisa de sua própria configuração em `starbound_server.config` e
do executável instrumentado para fornecer as medições do servidor; a opção do
cliente não é transmitida pela rede.

## Próxima coleta

Para separar as etapas internas dos NPCs, habilitar também `npcDetails: true`
conforme [npc-profiling.md](npc-profiling.md). Essa extensão adiciona `npcStages`
e contadores de população/pathfinding. Os tempos de `stages` e `entityTypes`
também passam a incluir limites superiores aproximados de p50/p95/p99 por
histogramas fixos; não somar percentis entre janelas.

Faça duas sessões separadas, conservando os mesmos mods, resolução, zoom,
configurações gráficas e meio/velocidade de deslocamento:

- **A — área já gerada:** reinicie o jogo, entre num planeta conhecido, aguarde
  cerca de 30 segundos e percorra por 60–120 segundos uma rota já explorada.
- **B — área inédita:** reinicie o jogo e percorra por 60–120 segundos uma região
  ainda não gerada. Registre o planeta, a posição inicial e a direção do percurso.

Em cada sessão, grave o percurso, anote aproximadamente quando ocorrem travadas
e encerre normalmente. Copie `storage/starbound.log` antes da próxima execução,
nomeando os arquivos, por exemplo, `exploracao-area-gerada.log` e
`exploracao-area-nova.log`. Envie o log completo e o vídeo correspondente.
Trechos de entrada/teleporte devem ser analisados separadamente do percurso.

Para repetir a comparação entre builds, restaure **cópias de trabalho** do mesmo
snapshot de jogador e universo antes de cada execução. Uma área nova deixa de
ser nova depois da primeira visita. Preserve o snapshot original. O cache do
sistema operacional também afeta leitura; use o mesmo procedimento de aquecimento
e repetições. Os dois cenários identificam custos distintos, e não constituem por
si só uma comparação de velocidade entre builds.

## Conteúdo dos relatórios

Cada coletor emite um resumo por intervalo, no limite entre operações, e um
resumo final ao ser destruído normalmente. Uma operação longa pode atrasar a
emissão além do intervalo configurado. Nenhum evento gera uma linha por tile,
entidade ou frame. A memória depende dos nomes fixos de etapas, não da duração
da partida. As estatísticas são zeradas após cada relatório.

Campos principais:

Na geração de setores, `generation.queueDepthTotal` dividido por
`generation.queueDepthSamples` dá a profundidade média observada da fila.
`generation.oldestQueuedAge` mede a idade do pedido mais antigo
em cada amostragem. `generation.sectorWait` mede o tempo entre o
primeiro pedido e a conclusão de cada setor. `generation.sectorsCompleted` e
`generation.sectorsExpired` distinguem trabalho concluído de pedidos removidos
após expirarem. Compare essas medidas com `generation.timeBudgetExhausted` e
com os tempos de `generation.microDungeons` para investigar estruturas que
aparecem lentamente durante a exploração.
Os campos de duração emitidos no JSON são em **milissegundos**. A soma das
idades e esperas de vários setores não equivale ao tempo de CPU, pois os
pedidos podem ficar pendentes simultaneamente.

Para investigar uma fila que não avança, os relatórios novos incluem
`oldestQueuedSector`, com coordenadas do setor e a maior idade observada na
janela, e `slowSectors`, com os setores concluídos mais lentos e suas esperas.
Cada coordenada de setor corresponde a uma região de 32 × 32 tiles; a lista
de conclusões é limitada por `entityDetailLimit` e não enumera toda a fila.

- `component`: `server`, `client` ou `painter`.
- `instance`: distingue coletores dentro do processo.
- `context`: identifica o mundo no servidor e o slot no cliente. O pintor não
  tem ID de mundo. Um intervalo de cliente/pintor pode abranger um teleporte;
  correlacione pelos horários e mensagens de entrada/saída do log.
- `windowMilliseconds`: duração real da janela, incluindo pausas entre chamadas.
- `stages`: número de amostras, tempo total, média, máximo e `slowSamples`
  (amostras com duração maior ou igual ao limiar).
- `counts`: setores encontrados no armazenamento, setores inicialmente vazios,
  bytes comprimidos recuperados, entidades carregadas e pacotes aplicados.
- `entityTypes`: custo agregado de cada categoria (`Object`, `Npc`, `Monster`,
  etc.), incluindo subtotais de Lua e logging.
- `slowEntities`: as atualizações individuais mais lentas da janela, limitadas
  por `entityDetailLimit` (1–50; padrão 12). Cada entrada informa tipo, ID,
  nome, subtipo quando disponível, posição, identificador da thread e tempos
  total, Lua e logging.
- `oldestQueuedSector`, `slowSectors`: coordenadas da fila de geração e
  esperas, disponíveis nos relatórios das builds instrumentadas mais recentes.
- `tilePackets`: até 64 marcadores de `TileArrayUpdate` por janela, com origem
  em tiles e horário monotônico; `tilePacketMarkersDropped` conta os demais.
- `final`: último resumo antes da destruição; a limpeza posterior não está incluída.

### Servidor e armazenamento

| Etapa | Interpretação |
| --- | --- |
| `serverUpdate` | Duração de uma atualização do mundo; não inclui espera entre ticks nem toda a atividade de `WorldServerThread`. |
| `entities` | Atualização das entidades, incluindo scripts disparados por elas. `entityTypes` divide essa fase por categoria. |
| `scriptContexts` | Contextos de script do mundo; não representa todos os scripts. |
| `worldStorageTick` | Manutenção e descarregamento de setores. |
| `worldStorageGeneration` | Processamento da fila de geração, incluindo carregamento e dependências. |
| `generation.queueSort` | Ordenação da fila por proximidade dos jogadores. |
| `generation.baseTiles`, `.microDungeons`, `.caveLiquid`, `.finalize`, `.terraform` | Chamadas das etapas de geração; dependências de setores vizinhos são processadas pelo caminho existente. |
| `generation.baseTiles.worker` | Tempo de relógio gasto por um worker na preparação isolada dos tiles básicos quando `worldThreading.sectorWorkers` está habilitado. Inclui a reconstrução do snapshot privado na primeira tarefa de cada worker ou após uma revisão do template; as tarefas seguintes reutilizam esse snapshot. Não representa espera da thread do mundo. |
| `generation.timeBudgetExhausted` | Número de atualizações que alcançaram o orçamento de geração com setores ainda pendentes. O trabalho é retomado no tick seguinte, entre etapas completas. |
| `storage.tiles.read`, `storage.entities.read` | Busca no banco de setores, incluindo consultas sem resultado. Pode usar caches; não é tempo de leitura física do disco. |
| `storage.*.decompress` | Descompressão dos dados recuperados para carregar setores. |
| `storage.*.deserialize` | Decodificação dos tiles/entidades armazenados, incluindo validação de tiles. |
| `storage.tiles.install`, `.installDefault` | Instalação dos tiles carregados ou de um setor inicialmente vazio. |
| `storage.entities.create` | Construção das entidades a partir dos dados salvos, por lote de setor. |
| `storage.entities.init` | Inicialização e inserção das entidades, incluindo callbacks de scripts acionados nesse caminho. |
| `storage.entities.uniqueIndex` | Atualização do índice de identificadores únicos. |
| `storage.*.callbacks` | Notificação da mudança de nível de carregamento. |
| `storage.tiles.total`, `storage.entities.total` | Total de uma mudança de nível, após carregar as dependências adjacentes explícitas. |
| `storage.unloadAttempt` | Tentativa de descarregar setor; pode manter entidades ativas e inclui persistência quando realizada. |
| `storage.sync.total`, `.sector`, `.commit` | Salvamento periódico, trabalho por setor e commit do banco; pode ocorrer fora de `serverUpdate`. |
| `storage.sync.tiles` | Cópia, serialização e compressão dos tiles de um setor alterado. Setores sem alterações de tiles não entram nesta etapa. |
| `storage.sync.dirtyTiles`, `.cleanTiles` | Quantidade de setores de tiles gravados e ignorados durante o salvamento periódico. Entidades persistentes continuam sendo sincronizadas para preservar alterações feitas por scripts. |

O servidor também informa as fases já existentes de dano, fiação, céu, clima,
líquidos, blocos em queda e envio de atualizações. `tileSectorsFromStorage` não
significa necessariamente geração completa: setores salvos parcialmente ainda
podem exigir etapas de geração. Amostras de geração incluem setores vizinhos,
portanto não equivalem ao número de blocos visíveis atravessados pelo jogador.

### Espera dos workers de tiles básicos

Os contadores `generation.baseTiles.scheduled`, `.pendingPolls`,
`.pendingLimitHits`, `.resultsReady`, `.resultsApplied`, `.staleResults` e
`.readyResultsEvicted` distinguem agendamento, espera, saturação do limite,
resultado pronto, aplicação, invalidação do snapshot e descarte de resultado
pronto. `.staleTiles`, `.staleRevision`, `.staleTemplate` e
`.staleTemplateRevision` indicam quais verificações invalidaram o resultado;
um resultado pode aumentar mais de um desses contadores. Ao desativar
`explorationProfiling`, esses contadores deixam de ser coletados.

As etapas `generation.baseTiles.queueDelay`, `.pendingAge`, `.commitDelay` e
`.resultAge` medem, respectivamente, tempo entre agendamento e início no
worker, idade de uma tarefa ainda não pronta ao ser consultada, tempo entre
fim do worker e coleta do resultado pela thread do mundo, e tempo total do
agendamento à coleta. `.pendingAge` é amostrada por consulta e pode registrar
a mesma tarefa muitas vezes; seu total não é custo de CPU. O trabalho dentro
do worker continua em `generation.baseTiles.worker`. A profundidade média
observada nas tentativas assíncronas é
`generation.baseTiles.pendingDepthTotal / pendingDepthSamples`; ela não mede
o comprimento interno da fila do pool de threads.
Uma vez por segundo, se a preparação assíncrona for consultada, os campos
`generation.baseTiles.poolWorkersTotal`, `.poolActiveTotal` e
`.poolQueuedTotal` são amostrados. Divida cada total por `.poolSamples` para
obter a média observada. `active` é uma aproximação baseada no estado de espera
dos workers; esses valores funcionam mesmo com `/debug threads` oculto.

Se a fila de setores envelhecer com ticks curtos e sem estouro do orçamento,
compare `.pendingAge` e `.pendingLimitHits` com os tempos de `.queueDelay` e
`.commitDelay`. Uma tarefa que ainda não terminou não produz amostra de
`.queueDelay` nem de `.worker` até o resultado ser coletado. Assim, a ausência
dessas amostras não prova que o worker esteja ocioso.

`generation.blocked.deadline`, `.invalidSector`, `.neighbor`, `.levelLimit`
e `.baseTiles` identificam retornos sem conclusão em `generateSectorToLevel`.
As demais etapas têm os mesmos rótulos `.microDungeons`, `.caveLiquid`,
`.finalize` e `.terraform`. Chamadas para setores vizinhos são recursivas; um
impedimento pode aumentar o contador da etapa original e vários contadores
`.neighbor`. São eventos de tentativa, não setores únicos bloqueados.

Para identificar a origem de resultados obsoletos, o array de tiles acompanha
revisões **apenas nos setores com um worker de tiles básicos pendente**. Os
contadores `generation.baseTiles.staleRevision.*` e `.evictedRevision.*`
classificam incrementos de revisão ocorridos antes de um resultado ser
invalidado ou descartado. Os sufixos são `collisionQuery` (primeira passagem
de consulta de colisão), `collisionCache` (reconstrução/invalidação do cache),
`worldGeneration`, `liquid`, `entity`, `storage` e `other`. Um incremento é um
acesso mutável, não necessariamente uma alteração persistente nem um setor
único. `other` inclui caminhos fora dos escopos classificados.

`staleBaseTiles` e `evictedBaseTiles` mostram até `entityDetailLimit` eventos
de cada tipo por janela, priorizados pela idade da tarefa. Cada entrada tem
coordenadas do setor, idade, diferença da revisão e contadores por causa.
`generation.baseTiles.pendingRemovedOnUnload` conta entradas removidas quando
um setor descarrega, inclusive se o worker ainda estiver em execução. A
coleta de causas aumenta o trabalho por acesso mutável enquanto há setores
monitorados; compare desempenho final com `explorationProfiling` desligado.
Os rótulos identificam o contexto de execução no código, não provam sozinhos
que cada alteração de revisão exigia descartar o resultado.

### Chamadas de objetos e montagem de dungeons

`object.worldCall.containerRead`, `.containerWrite`, `.callScriptedEntity`,
`.sendEntityMessage` e `.loadRegion` medem chamadas síncronas da API Lua
**durante a atualização de um objeto**. Os tempos são partes do tempo Lua e da
atualização desse objeto; não devem ser somados ao total dos objetos.
`sendEntityMessage` mede o envio, não a latência até a resposta futura. O
perfil não registra nomes de mods, IDs de contêineres ou conteúdo de mensagens.
Outros callbacks e trabalhos assíncronos continuam fora dessas categorias.

Na primeira criação de um mundo ou em uma colocação posterior de dungeon,
`dungeon.definitionLoad`, `.generate`, `.findConnectablePart`, `.canPlace`,
`.placePart`, `.flushLiquid` e `.flush` separam as fases principais da
montagem. `dungeon.anchorCandidates`, `.connectorsVisited`,
`.candidatesTried` e `.partsPlaced` contam tentativas e peças aceitas. A
visita a uma dungeon já salva não refaz essa montagem. `.generate` inclui
suas subetapas; não some os tempos entre si. Dentro de `.flush`, as etapas
`.markRegions`, `.tiles`, `.objects`, `.entities` e `.wiresAndLiquids`
mostram onde a aplicação da estrutura consome tempo.

### Cliente e desenho

| Etapa | Interpretação |
| --- | --- |
| `client.applyPacketBatch` | Aplicação dos pacotes recebidos; não inclui transporte nem decodificação anterior dos pacotes. |
| `client.createEntity`, `client.applyTileArray` | Partes da aplicação de pacotes referentes a entidades e regiões de tiles. |
| `client.initWorld` | Entrada num mundo, incluindo limpeza do anterior. |
| `client.update` | Atualização local, incluindo entidades, scripts e outros sistemas. |
| `client.prepareRenderData` | Preparação dos dados visuais; pode incluir cálculo de iluminação síncrono. |
| `painter.tileSetup` | Preparação dos chunks visuais, incluindo consultas/construção de caches. |
| `painter.environment` | Preparação e envio do desenho do céu e elementos ambientais. |
| `painter.environment.setup`, `.stars`, `.debris`, `.orbiters`, `.horizon`, `.sky`, `.flush` | Partes da etapa ambiental; `.orbiters` reúne os dois momentos de desenho dos orbitadores. |
| `painter.lightWait` | Espera pelo resultado da iluminação; não é medição do uso de CPU do worker. |
| `painter.lightUpload` | Ajuste do mapa de luz, upload e parâmetros do shader. |
| `painter.worldDraw` | Preparação/envio das camadas do mundo, entidades e partículas. |
| `painter.cleanup`, `painter.total` | Limpeza de caches e duração total da chamada ao pintor. |

São durações de relógio: incluem bloqueios e esperas. O desenho é assíncrono na
GPU; esses valores não são tempos de execução da GPU, tempos completos de frame,
FPS ou percentis. Não foi acrescentada sincronização da GPU para medir.

O servidor marca o pacote após colocá-lo na fila de saída; o cliente marca a
mesma origem de tiles depois de aplicar o pacote. Em jogo local, os horários
monotônicos do mesmo processo permitem estimar a diferença entre esses pontos.
O marcador do servidor **não** é confirmação de envio pela rede, e o do cliente
**não** é a primeira renderização do setor. Em máquinas diferentes, os
relógios monotônicos não são diretamente comparáveis. Os marcadores são
limitados a 64 por janela para controlar o tamanho do log.

## Como interpretar sem contar duas vezes

`serverUpdate` contém as fases do servidor. Essas fases podem conter as etapas
de armazenamento e geração. `storage.*.total`, `client.applyPacketBatch` e
`painter.total` também contêm suas subetapas. Scripts podem solicitar outros
carregamentos durante uma inicialização: nesse caso, os tempos também se
sobrepõem. **Não some todos os campos nem os máximos das diferentes etapas.**
`slowSamples` conta operações da etapa, não travadas únicas nem frames perdidos.

Procure primeiro janelas com máximos elevados nos totais e depois nas subetapas
correspondentes. Compare os horários com o vídeo. Um pico em `read`, em geração,
em inicialização ou em `tileSetup` orienta investigações diferentes. A ausência
de pico não elimina custos fora dos pontos medidos, como espera por locks,
decodificação de rede, apresentação do frame e trabalho assíncrono da GPU.

O log pode conter relatórios de vários mundos ativos. Relatórios usam o logger
normal e têm custo de serialização/E/S; a gravação de um coletor ocorre fora de
seus próprios timers, mas pode afetar o agendamento de outros trabalhos.

O campo `luaTotalMilliseconds` mede somente a chamada periódica `update` dos
componentes Lua enquanto uma entidade está sendo atualizada. Ele não inclui
todos os callbacks Lua possíveis (por exemplo, inicialização, mensagens ou
callbacks acionados em outras fases). `loggingTotalMilliseconds` mede o tempo
dentro de `Logger::log`, incluindo espera no mutex e sinks síncronos na mesma
thread; a formatação anterior à chamada não entra nesse subtotal. Lua e logging
são partes do tempo total da entidade e podem se sobrepor quando Lua registra
uma mensagem. Não some esses campos ao total.

O detalhamento adiciona duas leituras do relógio para cada atualização de
entidade e leituras adicionais para callbacks Lua e logging. Isso torna a
segunda camada mais intrusiva que o diagnóstico agregado anterior. Use-a apenas
para atribuição, preserve a mesma configuração entre coletas e confirme qualquer
ganho posterior com `enabled: false`.
Mantenha o diagnóstico desativado ao usar os benchmarks para comparações de
throughput; os modos próprios de profiling desses utilitários continuam separados.

A fila experimental de geração e sua configuração são descritas em
[world-threading.md](world-threading.md). Áreas já geradas não exercitam o
estágio assíncrono `BaseTiles`.

## Validação desta implementação

Os arquivos C++ alterados foram compilados no ambiente Linux. A execução visual
e a coleta no Windows com os mods do usuário permanecem pendentes. Não foram
executados testes automatizados nem feitas afirmações de ganho de desempenho.
