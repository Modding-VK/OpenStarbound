# WorldThread, coordenação e publicação

[Índice](README.md) · [Agendador](dependency-scheduler.md) · [Regiões](regional-threads.md)

## Propósito e melhoria esperada

Remodelar a WorldThread para integrar mais resultados por unidade de tempo e
reduzir stalls de geração. A capacidade cresce ao retirar cálculo, cópias e
esperas do caminho crítico e tornar a publicação mais barata. Uma thread
isolada continua usando um núcleo por vez; a arquitetura distribui preparação
e, numa etapa posterior, propriedade regional.

## Estado atual e pontos de entrada

`StarWorldServerThread::update` protege o mundo e processa entradas/update.
`WorldServer::update` percorre simulação e chama `WorldStorage::generateQueue`
conforme a fidelidade. A função mantém orçamento próprio, mas pode terminar
antes de utilizá-lo. `queueUpdatePackets` transmite apenas setores ativos.
`WorldServer::generateRegion` mantém contrato síncrono de região pronta no
retorno. Esses caminhos precisam de tratamento distinto.

## Responsabilidades da coordenadora proposta

1. Aceitar interesse, operações externas e versões numa ordem identificável.
2. Atualizar demanda do grafo e receber conclusões CPU/GPU/IO.
3. Validar produtos e selecionar commits elegíveis por prioridade e justiça.
4. Executar efeitos globais/legados e confirmar níveis de geração.
5. Construir snapshots confirmados para rede/persistência.
6. Coordenar épocas, checkpoints e, futuramente, donos regionais.

O coordenador não deve recalcular o campo de terreno ou percorrer toda uma
árvore já conhecida para descobrir novamente por que um nó aguarda.

## WT-01 — Serviço de conclusões e drenagem

Introduzir pontos explícitos para drenar a mailbox: antes da seleção de
trabalho, após commits que liberem dependentes e antes de preparar os pacotes,
respeitando a fronteira de simulação aprovada. Não bloquear à espera de um job
individual enquanto outro trabalho elegível pode avançar.

Cada passagem deve terminar por motivo explícito: fila vazia, nenhum nó
elegível, espera de produtor, pausa de política, limite de etapas, deadline
ou erro. O agendador recebe esse resultado para distinguir ociosidade útil
de uma interrupção artificial.

Aceitação: resultado já pronto não ganha uma rodada extra sem motivo; testes
de completions fora de ordem conservam justiça e sequência dos efeitos.

## WT-02 — Orçamento produtivo e resposta do jogo

Separar tempo de simulação, preparação, publicação e montagem de pacotes.
Considerar o custo estimado da próxima operação e registrar overrun de uma
operação indivisível. Priorizar trabalho pronto quando ele acumular.

O modo de alto throughput permite serviço maior e mais frequente de geração,
conforme os alvos escolhidos pelo usuário. A frequência/`dt` da simulação e
dos scripts não aumenta automaticamente. Mais trabalho de geração não pode
aplicar duas vezes a física ou Lua de uma entidade no mesmo passo.

Aceitação: teto concedido, tempo gasto e causa de saída são comparáveis;
ganho em entrega não vem de simulação acelerada ou pacotes contraditórios.

## WT-03 — Entrada síncrona em lote

Gerar o plano de demanda da região inteira antes de aguardar sua conclusão.
Admitir cálculos puros independentes e consumir resultados por condição/evento,
substituindo a repetição com `Thread::yield()` por progresso organizado.
O chamador continua recebendo uma região pronta no retorno.

Aceitação: entrada no planeta reproduz output e estado ativo, mas consegue
executar vários produtores independentes antes da primeira espera. Benchmark
de startup fica separado do streaming contínuo.

## WT-04 — Publicação mais barata

Receber deltas tipados e operações preparadas. Agrupar invalidação de colisão,
marcações dirty e notificações somente quando a equivalência observável for
demonstrada. A ordem de objetos, entidades, promises e callbacks permanece
explícita. Um resultado obsoleto é descartado antes de qualquer efeito.

Para uma operação grande, construir staging invisível ou reduzir seu custo.
Interromper o writer atual no meio de efeitos vivos não cria atomicidade.
Persistência e clientes recebem somente fronteiras confirmadas.

Aceitação: economia mensurável de CPU por setor e equivalência de colisão,
líquidos, scripts, rede e saves. Ver [IO e publicação](io-persistence-network.md).

## WT-05 — Coordenadora do runtime regional

Depois de REG-01–05, cada região 2×2 elegível terá sua thread dona. A
WorldThread central evolui para coordenação de relógio, identidade, operações
globais, transferências e checkpoint. A pool de cálculo auxilia as regiões;
não executa alternadamente suas simulações como substituta da thread dedicada.

A migração preserva um caminho legado completo para mods sem contrato
regional. Uma passagem de modo só ocorre numa fronteira consistente de mundo,
nunca em meio a callback ou transação de estrutura.

## Dependências e métricas

WT-01/02 dependem de IDs e grafo do [agendador](dependency-scheduler.md).
WT-04 depende de [etapas puras](cpu-and-stages.md), [compatibilidade](compatibility.md)
e transações de [IO](io-persistence-network.md). WT-05 é uma entrega regional,
posterior às melhorias de preparação.

Medir pronto→commit, tempo exclusivo de publicação, commits por passagem,
operações por tile, motivos de saída, overrun, tempo de simulação, tempo de
pacotes e p95/p99 do update/frame. A redução de cada parcela deve aparecer
também na espera interesse→entrega definida em [validação](validation-and-telemetry.md).
