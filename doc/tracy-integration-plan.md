# Plano de integração do Tracy

## Objetivo e estado

Planejamento em 25/10/2026 para a branch `perf/threading-audit`. O Tracy deve
mostrar, em uma linha do tempo por thread, onde o jogador espera durante a
exploração: atualização do mundo, fila de setores, preparação paralela de tiles,
microdungeons, ativação de entidades, scripts, persistência, rede e desenho. Ele
complementa os resumos do `ExplorationProfile` descritos em
[exploration-profiling.md](exploration-profiling.md); não substitui as contagens
de idade/profundidade da fila nem as comparações com a instrumentação desligada.

Esta integração não requer Pluto. O projeto mantém Lua 5.3, seus formatos de
script e sua API atual. A instrumentação interna da engine é independente da
implementação de Lua. A API opcional de zonas dentro dos scripts pode ser
avaliada separadamente após confirmar a compatibilidade do `TracyLua.hpp` com
Lua 5.3 e seus contextos no OpenStarbound.

Todo código externo usado nesta integração deve seguir a política de
[procedência e integridade](dependency-sources.md). O código do xStarbound serve
como referência de arquitetura; o Tracy será obtido diretamente do projeto
oficial que o mantém.

Referência: xStarbound `b9b8c1a` fixa Tracy em
`b8c2e25c3d02009059e09f623072921677dd6bb4`, com `TRACY_ON_DEMAND` e
`TRACY_ONLY_LOCALHOST`. Suas zonas cobrem ticks de mundo, entidades, scripts,
armazenamento, cliente e renderização. Copiar a seleção de pontos úteis, não as
partes de Lua/Pluto nem a arquitetura de geração do xStarbound.

## 1. Dependência e build

1. Acrescentar `STAR_ENABLE_TRACY` em `source/CMakeLists.txt`, padrão `OFF`. Na
   configuração normal, não baixar Tracy, compilar seu cliente ou acrescentar
   definições e bibliotecas ao executável.
2. Buscar apenas quando a opção estiver ativa, diretamente de `wolfpld/tracy`,
   com revisão e checksum fixos de uma distribuição oficial. Preferir
   `FetchContent` condicionado à opção, em vez de um submódulo buscado em todos
   os checkouts: o workflow atual faz checkout
   recursivo e o download automático já é um custo relevante. Registrar a
   licença BSD de três cláusulas na documentação/distribuição diagnóstica.
3. Configurar `TRACY_ENABLE`, `TRACY_ON_DEMAND` e `TRACY_ONLY_LOCALHOST`. Começar
   sem coleta automática de call stacks por zona; ativá-la somente se a primeira
   captura não localizar o caminho caro. A geração de símbolos do build de
   diagnóstico deve ser preservada para interpretar stacks quando habilitados.
4. Criar um único cabeçalho interno de macros para zonas, marcações de frame e
   textos de contexto. Sem Tracy, as macros não avaliam argumentos nem criam
   objetos. Usar nomes fixos de zona e anexar coordenadas/IDs como texto
   somente nos pontos que precisarem de contexto.
5. Propagar includes e definições às bibliotecas `OBJECT` instrumentadas
   (`star_core`, `star_game` e, para o cliente, `star_rendering` e
   `star_frontend`). Vincular `Tracy::TracyClient` nos executáveis `starbound`
   e `starbound_server`. Não basta vinculá-lo apenas ao executável: os arquivos
   compilados nas bibliotecas `OBJECT` também precisam enxergar seus headers e
   definições.
6. Criar uma configuração diagnóstica Windows separada da distribuição normal.
   A execução automática do workflow continua com Tracy desligado. Uma entrada
   manual ou workflow próprio deverá produzir executável com símbolos e indicar
   claramente o commit, as opções usadas e a versão do Tracy. O instalador
   usual não deve ser substituído silenciosamente por essa build.

## 2. Primeiras zonas e correlação

| Thread/caminho | Pontos iniciais | Questão respondida |
| --- | --- | --- |
| `WorldServerThread` | tick completo, espera entre ticks, mensagens, `WorldServer::update`, envio de pacotes | O tempo observado é execução do mundo, espera ou comunicação? |
| Fila de geração | ordenação, escolha do setor, etapas `BaseTiles`/`MicroDungeons`/`Finalize`, dependências vizinhas, commit | Qual etapa mantém o setor visível pendente? |
| Workers de setores | espera/trabalho, reconstrução do snapshot do template, `prepareBaseTileArray`, entrega do resultado | Há trabalho simultâneo útil ou o mundo espera pelo worker/commit? |
| Persistência | leitura do B-tree, descompressão, desserialização, serialização, compressão, commit/sync | A demora vem de dados salvos ou geração inédita? |
| Entidades e Lua | `updateAllEntities`, tipo de entidade, callback periódico, inicialização, scripts do mundo, criação de entidades e escrita de log | O vilarejo demora por NPCs/objetos/scripts ou por logging síncrono ao ativar a área? |
| Cliente e desenho | recepção e aplicação de pacotes, iluminação, `painter.environment`, tiles, entidades | O servidor concluiu antes de a área aparecer na tela? |

Reutilizar os limites entre etapas já nomeadas pelo `ExplorationProfile`, mas
colocar zonas adicionais no caminho de montagem de dungeons compostas:
carregamento da definição, busca de conectores, teste de posicionamento,
pintura/aplicação de tiles, criação de entidades e `flush`. Colocar zonas de
espera/lock separadas do trabalho útil. Não criar uma zona para cada tile nem
incluir nomes dinâmicos de mod como nomes de zona; isso tornaria a captura cara
e difícil de comparar.

Medir escrita de log em uma zona pequena, sem registrar o texto no Tracy e sem
fazer logging a partir da instrumentação. O `ExplorationProfile` já atribui
tempo de logging síncrono à entidade ativa; o Tracy permitirá ver em qual thread
a escrita aconteceu e se bloqueou o tick do mundo.

As zonas de uma tarefa assíncrona pertencem à thread que executa a tarefa.
Correlacionar pedido, worker e commit com um identificador de setor em texto ou
evento, sem tentar manter uma zona RAII aberta entre threads. Manter os
contadores atuais de profundidade, idade e tempo de espera da fila: uma linha do
tempo de CPU sozinha não mede toda a espera do jogador.

## 3. Lua 5.3 e extensão posterior

Começar com zonas C++ em torno das chamadas de Lua. O `TracyLua.hpp` fixado
usa funções da API C presentes no Lua 5.3 do projeto, como `lua_getstack`,
`lua_getinfo` e `lua_tolstring`; isto indica viabilidade sem Pluto, mas ainda é
necessário verificar a integração com o ciclo de vida dos contextos. Só então,
se os tempos agregados não bastarem,
oferecer uma tabela `tracy` para scripts de diagnóstico, condicionada à build
instrumentada. Scripts de mods existentes não precisam ser alterados. A ausência
de Tracy na build normal não pode impedir o carregamento desses mods; scripts
novos devem verificar a existência da tabela antes de usá-la.

Evitar inserir hooks permanentes em toda instrução Lua na primeira versão.
Comparar separadamente o custo das zonas por callback e da API opcional dentro
dos scripts. Não migrar Lua, bytecode, bibliotecas padrão ou protocolos de rede.

## 4. Medição e critérios para encerrar a implementação

Usar o mesmo snapshot de jogador/universo e a mesma pilha de mods, incluindo
Frackin' Universe, para percursos em área já gerada e inédita. Incluir o
vilarejo Mantizi e capturar entrada, exploração e recarregamento como eventos
distintos. Registrar commit, configuração de `sectorWorkers`, fidelidade,
resolução, zoom e horário da gravação. Examinar no Tracy as threads do mundo,
dos setores, do cliente e da iluminação, junto dos relatórios de fila do
`ExplorationProfile`.

Com `TRACY_ON_DEMAND`, iniciar o visualizador/captura local antes de entrar no
trecho investigado; eventos anteriores à conexão não fazem parte do traço.
Salvar o arquivo da captura com o commit e o cenário. O visualizador é uma
ferramenta de diagnóstico separada da instalação normal do jogo.

Critérios de conclusão: build normal permanece com Tracy ausente; build
diagnóstica abre captura local sob demanda; zonas identificam separadamente
geração, espera por worker, microdungeon, entidades/scripts, persistência e
renderização; nomes e IDs permitem ligar um pico à região observada; o fluxo
normal de mods, iluminação, saves e multiplayer permanece funcional. Medir o
custo da instrumentação comparando a build normal e a diagnóstica sem captura,
antes de interpretar diferenças pequenas de tempo. A captura não é um benchmark
de FPS ou de throughput por si só.

## Sequência de implementação sugerida

1. Integração CMake opcional e build diagnóstica, sem zonas de jogo.
2. Zonas grossas na thread do mundo, setor worker, cliente e persistência.
3. Zonas finas em microdungeons, criação de entidades e callbacks Lua após a
   primeira captura apontar o caminho dominante.
4. API de Tracy para Lua 5.3 somente se as zonas C++ não responderem à pergunta
   sobre qual mod/script consome tempo.
5. Revisar custo das zonas e manter na build normal apenas código compilado
   para zero trabalho quando Tracy estiver desligado.
