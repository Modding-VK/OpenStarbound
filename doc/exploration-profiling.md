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
| `generation.baseTiles.worker` | Tempo de CPU gasto por um worker na preparação isolada dos tiles básicos quando `worldThreading.sectorWorkers` está habilitado. Não representa espera da thread do mundo. |
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

O servidor também informa as fases já existentes de dano, fiação, céu, clima,
líquidos, blocos em queda e envio de atualizações. `tileSectorsFromStorage` não
significa necessariamente geração completa: setores salvos parcialmente ainda
podem exigir etapas de geração. Amostras de geração incluem setores vizinhos,
portanto não equivalem ao número de blocos visíveis atravessados pelo jogador.

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
| `painter.lightWait` | Espera pelo resultado da iluminação; não é medição do uso de CPU do worker. |
| `painter.lightUpload` | Ajuste do mapa de luz, upload e parâmetros do shader. |
| `painter.worldDraw` | Preparação/envio das camadas do mundo, entidades e partículas. |
| `painter.cleanup`, `painter.total` | Limpeza de caches e duração total da chamada ao pintor. |

São durações de relógio: incluem bloqueios e esperas. O desenho é assíncrono na
GPU; esses valores não são tempos de execução da GPU, tempos completos de frame,
FPS ou percentis. Não foi acrescentada sincronização da GPU para medir.

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
