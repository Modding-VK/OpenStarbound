# Checklist operacional do runtime regional

**Branch:** `Experimental`  
**Estado:** fundações em andamento; nenhuma escrita paralela no mundo está autorizada ainda.

Este documento consolida as entregas distribuídas entre os planos de geração,
regiões, microdungeons, persistência, objetos e diagnóstico.

## Regra de publicação

Workers recebem snapshots imutáveis e retornam planos ou batches. Somente a
thread dona da região pode validar revisões, criar entidades, alterar tiles,
confirmar promises ou publicar efeitos de rede e persistência.

## Compatibilidade com mods — requisito de bloqueio

Compatibilidade não é uma otimização posterior. Uma etapa não pode ser ligada
por padrão nem declarada equivalente enquanto a comparação com os mods e
assets da rota de referência não tiver passado. Em particular:

- workers não chamam Lua, callbacks, APIs de mods, promises, bancos de
  entidades ou fachadas do mundo vivo;
- a ordem observável de geração, criação de entidades, promises, rede e saves
  permanece na thread dona até existir um contrato explícito para cada efeito;
- qualquer API nova é opt-in, preserva fallback legado e registra somente
  métricas de cardinalidade fixa, nunca nomes ou payloads de mods;
- testes de equivalência sempre incluem mundo salvo e novo, reload,
  multiplayer e um conjunto de mods representativo.

## Estado atual

A auditoria consolidada mais recente está em
[performance-audit-2026-11-07.md](performance-audit-2026-11-07.md). Ela é o
ponto de retomada para evidência, correção do crash, limites conhecidos e
sequência de implementação.

A fronteira de IO de setores e a fila explícita de resultados prontos foram
auditadas em
[sector-io-ready-queue-audit-2026-11-07.md](sector-io-ready-queue-audit-2026-11-07.md).

| Componente | Estado |
| --- | --- |
| BaseTiles em pool | Implementado, com identidade e validação de revisão |
| Admissão de tarefas | Estágios declarados validados; chaves por `(setor, etapa)` podem coexistir |
| Regiões lógicas em sombra | Implementado para observação e métricas |
| Planejador de microdungeon por snapshot | Extraído, ainda síncrono |
| `MicroDungeonPlan` task | Planejador em worker opt-in; publicação ainda serial |
| Ativação inicial com workers | Preview opt-in; aguarda conclusão e preserva contrato síncrono |
| Entrega orientada à CPU | Preview `legacy`/`adaptive`/`aggressive`: pool automática inicial, orçamento por backlog e recuo adaptativo sob pressão do update; sem redimensionamento em execução ainda |
| Integridade da fila de geração | Conclusão remove o setor processado por chave; observação regional tolera e mede entradas sem identidade, sem interromper o mundo |
| `WorldMutationBatch` | Contrato e validação pura criados; sem operações vivas |
| Escrita paralela | Não implementada |

## Escopo de plataformas nesta fase

A estabilização e a validação corrente cobrem Windows, Linux GCC/ARM e macOS.
Linux Clang fica explicitamente adiado até o encerramento do projeto: seu preset
de teste e o job correspondente não são critério de bloqueio nesta fase e não
devem receber trabalho de compatibilidade isolado. Essa suspensão não reduz a
exigência de compatibilidade funcional ou de mods nas plataformas que seguem
no escopo.

## Microdungeons

- [x] Conectar `PendingMicroDungeonPlan` ao pool de workers, desligado por padrão.
- [x] Capturar template, setor, halo, cópias de tiles e revisões no snapshot.
- [x] Revalidar identidade, template e revisões no retorno.
- [x] Registrar agendamento, fila, execução, descarte, retry e bytes retidos.
- [ ] Criar operações reais de tile em `WorldMutationBatch`.
- [ ] Aplicar batches somente pela região dona, inicialmente em preview.
- [ ] Calcular footprint completo de microdungeons compostas.
- [ ] Reservar regiões do footprint e ordenar conflitos deterministicamente.
- [ ] Separar entidades, fios, líquidos e promises do batch de tiles.
- [ ] Converter `DungeonGeneratorWorld` em fachada gravadora de comandos.

**Saída:** mesma seed, assets e mods geram resultados equivalentes com um ou
vários workers; resultados obsoletos nunca publicam.

### Recorte vertical seguinte — somente planejamento assíncrono

Este é o próximo item implementável, mas permanece **desligado por padrão**
até passar a validação comparativa. O worker receberá um snapshot que possui
os próprios dados; `WorldTemplatePtr`, `WorldServer`, `WorldStorage`, fachada
de dungeon, entidades e promises não podem ser lidos pelo produtor.

1. A thread dona captura `templateStore`, revisão/identidade do template, o
   setor solicitado, o halo de 3×3 setores e a revisão de cada setor do halo.
   As colocações pendentes são copiadas com seu índice estável.
2. O worker reconstrói seu `WorldTemplate` privado a partir do store e retorna
   somente descritores ordenados. Ele não instancia `DungeonGeneratorWorld` e
   não produz `WorldMutationBatch` neste recorte.
3. No retorno, a região dona exige tarefa válida, mesma encarnação do mundo,
   mesma identidade/revisão do template e todas as revisões do halo. Falha,
   unload ou divergência descartam o resultado e aposentam a reserva.
4. Apenas depois dessa revalidação, a thread dona chama o caminho legado de
   aplicação, na mesma ordem de hoje. Promises e entidades continuam nesse
   caminho autoritativo.

Critérios de aceitação: sequência idêntica de planos para a mesma seed, assets
e mods com 0, 1 e N workers; nenhum acesso do worker a tile vivo; e contagem
equilibrada de `reserved`, `committed`, `stale`, `cancelled` e `failed` após
unload/reload. A captura precisa informar bytes do halo, profundidade da fila e
tempo de fila/execução, sem nomes de dungeons ou payloads de mods.

## Líquidos e finalização

- [ ] Definir `CaveLiquidPlan` sobre snapshot com halo.
- [ ] Validar revisões e fronteiras antes de publicar líquidos.
- [ ] Criar protocolo de borda para líquidos e fios entre regiões.
- [ ] Separar preparação de `Finalize` da criação de objetos, plantas e colisão.
- [ ] Publicar entidades e colisão pela região dona.

### Backlog — oceanos tóxicos e clima pesado

Observação de campo em 07/11/2026: planetas tóxicos com grandes corpos de
líquido e planetas sob clima intenso apresentam carregamento e update lentos.
Este item fica fora do recorte atual de scheduler até haver uma medição isolada.

- [ ] Comparar planeta tóxico novo e planeta de referência equivalente: tempo
  de `CaveLiquid`, células ativas, transferências entre setores, pressão e
  tempo total de `liquids`.
- [ ] Separar no profiling de clima o custo de domínios, janelas visíveis,
  projéteis do servidor e partículas do cliente.
- [ ] Avaliar orçamento adaptativo e processamento fatiado para líquidos
  distantes, preservando prioridade nas regiões visíveis e a equivalência de
  simulação.
- [ ] Projetar `CaveLiquidPlan` sobre snapshot/halo antes de qualquer cálculo
  paralelo; o motor de líquidos vivo permanece na thread dona até existir
  validação de revisões e publicação ordenada.
- [ ] Avaliar coalescimento/orçamento de clima e partículas apenas após o
  profiling separar custo de servidor, renderização e scripts/mods.

## Agendamento e memória

### Remodelagem da WorldThread — coordenadora serial, preparação paralela

A `WorldThread` continua sendo a dona serial do mundo. O objetivo não é criar
escritores concorrentes de tiles, entidades, Lua, rede ou persistência; é
retirar dela cálculo privado, espera de IO e admissão serial desnecessária para
que coordene publicação determinística. A referência arquitetural é a separação
usual entre preparação paralela de chunks e integração autoritativa serial, sem
assumir compatibilidade de APIs ou comportamento de outro motor.

O modelo alvo possui quatro filas distintas:

1. **Interesse:** ativações de jogador, geração inicial e prefetch, com razão
   e prioridade explícitas.
2. **Preparação:** snapshots elegíveis para BaseTiles, planejamento de
   microdungeons, leitura privada e futuras etapas puras; limitada por workers,
   bytes e memória.
3. **Resultados prontos:** resultados privados revalidados, ordenados por
   visibilidade, idade e cadeia mínima de pré-requisitos.
4. **Commit:** publicação pela `WorldThread`, que valida encarnação/revisões e
   aplica tiles, entidades, promises, rede e persistência na ordem observável.

Uma dependência de vizinho ou microdungeon pode atrasar o commit de um setor,
mas não deve impedir a preparação de setores independentes até a capacidade
configurada. Prioridade nunca cria interesse por si só, nem permite a um worker
executar Lua, tocar estado vivo ou publicar resultado.

O profiling de 07/11/2026 com política `adaptive` confirma a necessidade desta
separação: a pool automática selecionou 11 workers, mas as amostras de
BaseTiles registraram somente 0,22 worker ativo em média, com fila autoritativa
média de 41,9 setores e espera máxima de 75,6 s. O orçamento atingiu 24 ms e
raramente recuou por pressão do update. Portanto, aumentar somente o número de
threads ou o orçamento não basta: admissão, dependências e entrega serial são
o próximo gargalo mensurável. As amostras de pool não são uma medição contínua
do uso de CPU do sistema.

- [x] Implementar preview de orçamento adaptativo para entrega de setores por
  backlog, com `legacy` como fallback e teto configurável.
- [x] Dimensionar a pool de preparação na criação pela capacidade de CPU, com
  reserva explícita e teto configurável para a thread dona.
- [x] Acrescentar ao orçamento adaptativo um sinal de tempo já gasto no update
  antes da geração; o burst recua ao orçamento base sob pressão.
- [ ] Acrescentar pressão de memória e redimensionar a pool em execução somente
  após contrato de drain seguro das tarefas pendentes.
- [x] Preview de seleção de commits BaseTiles prontos pela ordem autoritativa
  de pedidos visíveis; promoções de justiça/dependência já existentes são
  preservadas e prefetch usa desempate estável. Ainda requer validação Windows
  antes de adoção por padrão. A rota Tracy 12 confirmou 108 seleções visíveis
  por prioridade sem crash, mas não é A/B controlado; ver
  [runtime-validation-2026-11-07-tracy12.md](runtime-validation-2026-11-07-tracy12.md).
- [x] Executar A/B adicional do preview em mundo lunar: a variante ativa
  confirmou 333 seleções visíveis pela prioridade, sem crash. Durações e rota
  ainda não são equivalentes; a espera máxima de setor continua próxima de 16
  s nas duas variantes. Ver
  [runtime-validation-2026-11-07-ready-scheduler-ab-moon.md](runtime-validation-2026-11-07-ready-scheduler-ab-moon.md).
- [ ] Medir setores completos por segundo, idade/profundidade p50/p95/p99 da
  fila, atraso de commit e tempo do update como critérios primários de
  performance; utilização de CPU isolada não basta.
- [ ] Construir grafo executivo de BaseTiles, MicroDungeons, CaveLiquid e Finalize.
- [ ] Propagar prioridade visível aos pré requisitos.
- [x] Preview limitado de preparação BaseTiles para a cadeia de pré-requisitos
  de uma raiz visível, sem tornar vizinhos pedidos ativos nem mover etapas
  posteriores para workers. Tracy 14 confirmou planos e admissões sem crash,
  mas não reduziu a cauda de espera; ver
  [runtime-validation-2026-11-07-tracy14-dependency-preparation.md](runtime-validation-2026-11-07-tracy14-dependency-preparation.md).
- [x] Implementar preview de seleção por TTL restante: somente o pedido
  visível mais próximo de expirar dentro de uma janela configurável recebe uma
  tentativa reservada; o TTL e os contratos de geração permanecem inalterados.
  Requer A/B focado em expirações e `sectorWait`.
- [ ] Validar o preview de risco de expiração contra a mesma rota, duração,
  save e mods, comparando expirações, `sectorWait`, justiça e tempo do update.
  Tracy 15 confirma ativação e resultado favorável sem crash, mas é sessão curta
  sem controle `false`; ver
  [runtime-validation-2026-11-07-tracy15-expiry-risk.md](runtime-validation-2026-11-07-tracy15-expiry-risk.md).
- [x] Instrumentar marcos por pedido visível (Tiles e cada etapa de geração),
  mantendo vizinhos recursivos fora da telemetria de interesse do jogador.
- [x] Documentar protocolo A/B e critérios que condicionam prioridade futura
  de microdungeons/vizinhos; ver
  [regional-runtime-expiry-risk-validation-protocol.md](regional-runtime-expiry-risk-validation-protocol.md).
- [x] Separar relatórios de exploração entre lançamentos: arquivo primário
  existente é rotacionado antes da primeira escrita da nova sessão e cada
  relatório registra o início monotônico da sessão.
- [ ] Orçar bytes, tarefas pendentes e regiões globalmente.
- [ ] Separar admissão, execução e commit em filas distintas.
- [ ] Fazer a admissão alimentar preparação independente até os limites de
  workers/bytes/memória, sem esperar o commit de uma cadeia bloqueada.
- [x] Adicionar preview limitado de admissão independente de BaseTiles da fila
  visível, com carregamento somente de tiles, orçamento de tempo e os limites
  existentes de tarefas/bytes; resultados continuam aguardando commit da dona.
- [x] Introduzir seleção estável de resultados prontos BaseTiles antes do
  commit; medir atraso resultado-pronto→commit e trabalho útil perdido.
- [x] Materializar a primeira fila de resultados prontos para BaseTiles:
  promises concluídas recebem ticket/timestamp, preservam a reserva até commit
  e registram enfileiramento, pendência, commit, stale e cancelamento.
- [x] Auditar a fronteira de snapshot de tiles salvos e o contrato da fila de
  resultados prontos; primeiro recorte exclui banco, entidades e callbacks de
  worker.
- [ ] Reter resultados úteis sob pressão de memória sem livelock.
- [ ] Agregar dependências com cardinalidade limitada e contadores de perda.

## Runtime regional, persistência e compatibilidade

- [ ] Implementar mailboxes autoritativas entre regiões e handoff de entidades.
- [ ] Introduzir update local por região com barreira inicial medida.
- [x] Preview de decode de tiles salvos: a dona captura bytes imutáveis do
  B-tree com encarnação/ticket, worker apenas descomprime/desserializa e a dona
  revalida e instala. Limitado a carregamentos `Tiles` de prefetch/preparação.
- [x] Separar decode privado da instalação viva nesse recorte; banco, tiles
  vivos, entidades, callbacks, saves e rede continuam na thread dona.
- [x] Descartar promessa proprietária de decode pendente em unload e
  `unloadAll`; worker tardio não retém ponteiro para o mundo nem pode publicar.
- [x] Impedir admissão de BaseTiles enquanto o decode privado ainda não tiver
  instalado o array de tiles; defesa repetida no preparador. Ver
  [runtime-crash-base-tiles-2026-11-07.md](runtime-crash-base-tiles-2026-11-07.md).
- [x] Validar em Windows a rota que antes falhava: 1.203 decodes agendados,
  1.031 instalados e 156 cancelados em unload, sem novo crash. A equivalência
  de save/mods/unload/reload e dois jogadores continua pendente; detalhes em
  [runtime-validation-2026-11-07-tracy11.md](runtime-validation-2026-11-07-tracy11.md).
- [ ] Validar o preview em Windows com mundo novo/salvo, mods, unload/reload,
  prefetch, cancelamento e dois jogadores antes de ampliar IO assíncrono.
- [ ] Preservar checkpoint, rede, saves e reload sem duplicação.
- [ ] Definir API opt-in e fallback legado para mods, objetos e containers.
- [ ] Auditar monstros, veículos e stagehands antes de migrá-los.

## Ordem de execução atual

1. **Estabilização:** manter os contratos puros, a checagem de etapas e a
   integração com `main`; não ampliar o número de escritores do mundo.
2. **BaseTiles:** repetir o teste com mundos reais e assets licenciados antes
   de mudar limite, política de descarte ou validação de revisão.
3. **Microdungeons:** executar somente o recorte vertical de planejamento
   descrito acima; batches de mutação e a fachada gravadora vêm depois da
   equivalência do planejador.
4. **Agendador:** converter a observação atual em grafo executivo somente
   depois de existir uma fronteira completa e versionada; prioridade nunca
   cria interesse nem autoriza publicação.
5. **Persistência e runtime regional:** introduzir leitura privada e instalação
   validada antes de mailboxes, handoff ou atualização concorrente de entidades.

A externalização de NPCs permanece suspensa e não é pré-requisito de nenhuma
etapa acima.

## Validação obrigatória

- [ ] Repetir Mantizi, Glitch, mundo salvo e mundo inédito com a mesma seed.
- [ ] Comparar 1/N workers, pouca memória, prefetch, unload/reload e dois jogadores.
- [ ] Comparar tiles, dungeons, entidades, líquidos, colisão, iluminação e saves.
- [ ] Medir p50/p95/p99, bytes, retries, descartes e trabalho útil por etapa.
- [ ] Correlacionar Tracy e JSONL; medir profiling ligado e desligado.
- [ ] Confirmar em Windows a escrita da categoria `storage` após a proteção
  contra seções opcionais ausentes no relatório (`BadVariantType` observado em
  2026-11-06).
