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

O [programa principal de setores](sector-runtime/README.md), adotado em
08/11/2026, organiza
os contratos por subsistema. O [roadmap detalhado](sector-runtime/roadmap.md)
relaciona tarefas, dependências, propósito e critérios de conclusão; ele é a
entrada operacional para a implementação; documentos antigos são apoio e
histórico, sem outra fila de prioridades. O
[painel de progressão](sector-runtime/progress.md) consolida percentuais por
marcos e ordem das 49 tarefas; atualizar junto com roadmap/relatório a cada
entrega material, mesmo quando o percentual continuar igual.
Frente ativa: H0. O relatório atual é
[catálogo compartilhado de consultas BaseTiles](sector-runtime-h0-base-tile-lookup-context-2026-11-09.md), com
[contrato de cobertura/exclusões](sector-runtime-memory-envelope.md).
O primeiro recorte de DAG-01 está implementado/testado sem assets; a aceitação
integrada permanece pendente. DAG-04 tem o recorte de continuação/avanço
corrigido opt-in e VAL-01/02 têm harness sintético inicial, com 26 testes H0
aprovados naquele recorte. RAM-01 já reserva arrays antes das cópias, com
rollback de preparação e handoff. O ledger processual observacional acompanha
agora arrays privados sobreviventes ao mundo, sem mudar cotas ou impor novo
teto. `803d4cf7` acrescenta estimativas v1 de templates compartilhados,
metadados e planos variáveis, somente com profiling regional, separadas dos
arrays. Validação: 115 testes, 400 execuções adicionais e nove grupos CTest,
ASan/UBSan estritos; não é medição integral de RAM ou de throughput.
`739067b1` acompanha o objeto/cache privado por worker, mesmo sem profiling,
até saída da thread; não cobre todo o grafo privado nem picos entre jobs.
Validação daquela etapa: 136 testes, 420 execuções adicionais e dez grupos CTest,
incluindo oito workers concorrentes, com ASan/UBSan estritos.
`1bd348e6` reproduz/corrige a publicação antecipada da seleção do pool após
exceção; 144 testes, 800 execuções adicionais e onze grupos CTest aprovados
com ASan/UBSan, sem ensaio de OOM real. `34c76ea3` reserva buffers de candidatos
e planos antes de alocar, inclusive reallocation e caminhos síncronos; elimina
cópia dos planos no consumo. Validação daquela etapa: 167 testes, 460 execuções
adicionais e doze grupos CTest, com ASan/UBSan. Modelo de estimativas 2, sem
novo teto ou ganho de throughput alegado. `887149c0` compartilha distribuições
enfileiradas após uma única cópia equivalente, preservando seleção e lifetime;
conta o backing conhecido uma vez, sem depender de profiling. Validação da etapa:
185 testes, 360 execuções adicionais e treze grupos CTest, com ASan/UBSan.
Modelo de estimativas 3, sem ganho de jogo medido. `cad311d1` compartilha nomes
de saída e reutiliza scratch somente sem inputs MicroDungeonNames, preservando
cópias que afetam seleção. Validação daquela etapa: 204 testes, 380 execuções adicionais
e catorze grupos CTest, ASan/UBSan; modelo 4. Controle negativo de hash com wrap
reproduz escolha diferente sem proteção. Próximo: payloads de candidatos e
temporários, variants opacos, comparador completo e streaming real; sem cotas
ampliadas ou ganho de jogo alegado.
`25cecf95` acompanha backing conhecido antes da factory/crescimento dos
candidatos e até destruir o vetor; amplia campos públicos dos seis variants
sem trocar cópias/seleção. Validação daquela etapa: 223 testes, 380 execuções adicionais
e quinze grupos CTest, ASan/UBSan; modelos geral 5/distribuições 2/payloads 1.
RAM-01 permanece em 25%; próximo: originais de listas potenciais/scratch,
Json/privados e buckets do hash. Não é teto hard ou ganho de jogo medido.
`c4d7ddb3` acompanha originais de listas potenciais após construção, reserva
previsão antes da cópia de validação e conserva scratch/capacidade até destruir.
Validação daquela etapa: 246 testes, 460 execuções adicionais e dezesseis grupos CTest,
ASan/UBSan; modelo geral 6/temporários 1. RAM-01 segue em 25%; próximo:
Json/TileDamage privados, buckets/metadados e envelope processual explícito.
`730e36ba` acrescenta payload completo do vetor de buckets de nomes e pares
ocupados dos fatores de dano sem mudar layout/allocators/cópias/geração.
Validação daquela etapa: 266 testes, 400 execuções adicionais e dezessete grupos CTest,
ASan/UBSan; modelos geral 7/distribuições 3/payloads 2/temporários 2.
RAM-01/H0 seguem em 25%; próximo: Json compartilhado, bookkeeping/allocator
dos nós e envelope de memória/pressão. Não é teto profundo ou ganho medido.
`45db2402` inclui buckets completos e texto de chaves JsonObject, com owner
único e omissão explícita de objetos que não cabem no limite da captura.
Mantém input/serialização/cópias; modelos geral 8/Json 2, sem mudar os demais
modelos próprios ou exigir profiling para classes que já eram independentes.
Validação daquela etapa: 294 testes, 560 execuções adicionais e dezoito grupos CTest,
ASan/UBSan. RAM-01/H0 continuam em 25%; próximo: Json nos variants, nós/allocator
e envelope de memória/pressão. Coleta Json ainda depende de profiling.
`49a12fbf` acompanha Json dos variants enfileirados numa floresta limitada,
deduplicada com templates. O mesmo handle segue original/bundle/worker, sem
novas cópias de itens ou rescan no freeze; input sem heap não aloca tracker vazio.
Validação daquela etapa: 320 testes, 520 execuções adicionais e dezenove grupos CTest,
ASan/UBSan; modelos geral 9/Json 2/Json enfileirado 1. RAM-01/H0 em 25%.
Próximo: consolidar envelope/exclusões e pressão antes de limites por hardware;
Json ambiental/privado e nós seguem parciais. Sem ganho de setores/s medido.
`ada755bf`/`0915af25` consolida schema das 14 classes e envelope verificado. Há modelo
isolado de pressão/histerese, mas runtime só amostra métricas; não há limite
finito novo ou consumidor de admissão. Validação daquela etapa: 354 testes, 680 execuções
adicionais e vinte grupos CTest, ASan/UBSan. Envelope modelo 1, fórmulas mantidas.
RAM-01/H0 em 25%; próximo: reserva atômica do escopo, overlap/rollback e
headroom/progresso de dependências, depois controlador/fairness e limites externos.
`4b5aed18` integra pressão ao ledger opt-in sob o mesmo mutex da cobrança, com
demanda preservada em fachadas/tokens/allocators. Testes verificam disputas,
headroom, overlap e rollback; o singleton do jogo continua desligado, sem config
nova. Validação daquela etapa: 388 testes, 680 execuções adicionais e 21 grupos
CTest, ASan/UBSan; admissão modelo 1. A reserva composta era o próximo recorte.
`dd59f3b4` admite array in place e template inicial de BaseTiles antes de copiar/
despachar, com tokens consumidos pelo cache sem recobrança e isolamento ledger/
demanda. Validação daquela etapa: 420 testes, 640 execuções adicionais e 22 grupos CTest,
ASan/UBSan. Pressão padrão desligada; conclusão em hard só para produtor limitado,
não todo crescimento real/DAG. RAM-01/H0 em 25%; próximo: saída variável com
retry/drenagem e dois jobs/resultados retidos sem espera indefinida por bytes,
demanda ligada ao interesse real; depois provider/config/fairness processuais.
`8b68ab6a` entrega status atômicos e retry limitado para produtores puros,
sem espera por budget no worker; duas produções/resultados retidos progridem
na fixture com fences físicas e bytes equivalentes. Validação daquela etapa: 452 testes,
640 execuções adicionais e 23 grupos CTest com ASan/UBSan. Retry sem consumidor
real/evento automático; pressão padrão desligada. RAM-01/H0 em 25%; próximo:
eventos úteis no owner, adaptador puro/versionado, demanda real e comparação
MOD-01/VAL-02 antes de replay real; depois provider/config/fairness processuais.
`8e190742` substitui revisão manual por sinal release/shrink do ledger, capturado
atomicamente na recusa e consumido no próximo poll (retry 2). Não é wakeup/mailbox
nem retry real. Subkernel escalar de líquido modelo 1 usado em BaseTiles e
comparado à lógica anterior. Validação daquela etapa: 484 testes, 1.280 execuções
adicionais e 25 grupos CTest com ASan/UBSan. Pressão desligada; RAM-01/H0 em 25%.
Próximo: identidade/contexto do produto BaseTiles e comparador dos campos completos,
incluindo estado anterior em Keep e revisões, antes de ampliar pureza/demanda/replay.
`f7785963` entrega ticket versionado devolvido pelo worker, sem substituir guards
vivos, e comparador diagnóstico dos campos completos inicializados de tiles reais.
[Contrato](sector-runtime-base-tiles-contract.md) separa identidade/conteúdo,
cache normalizado e bordas válidas. Validação daquela etapa: 524 testes, 800 execuções
adicionais e 26 grupos CTest com ASan/UBSan. Comparador fora do hot path; retry/
pressão real desligados. H0 em 25%.
`2ea41cf8` explicita materiais/mods/biomas/oceano no input escalar e conserva
consultas/escritas e estado após exceções no adaptador real, incluindo NoMod.
Validação daquela etapa: 548 testes, 1.280 execuções adicionais e 27 grupos CTest,
ASan/UBSan. Resolver ainda não fecha template/assets.
`abfc3834` entrega captura versionada e replay diagnóstico com preflight completo
de consultas/input/estado inicial, sem chamar resolver vivo. 575 testes, 1.820
execuções adicionais e 28 grupos CTest aprovados, ASan/UBSan; microexperimento
separado executado. API fora do produtor normal, fita com overhead/176 bytes por
tile: não é cache/retry integrado ou ganho de entrega.
`4e5cc6c1` entrega catálogo selado de respostas únicas, capacidade explícita e
scope/checagem de cobertura antes dos writes. 601 testes, 2.340 execuções adicionais
e 29 grupos CTest, ASan/UBSan. Oito bytes/chave, sem heap ou fita por tile; escopo
de assets/material ainda declarado, sem produtor de gerações/reload real. Não
ativado no jogo. Próximo: gerações/lifetime e leitores reais antes de sombra por
setor com admissão/bytes. Não antecipar consultas/reaproveitar tile parcial;
auditar biomeTransition/default/cópias antes de normalizar. RAM-01/H0 em 25%,
programa 3,1% inalterados. Streaming real com assets/mods e executor de conclusões
ainda pendentes.

A proposta vigente foi retrabalhada para aproveitar CPU, RAM e GPU:
[sector-generation-hardware-design-2026-11-08.md](sector-generation-hardware-design-2026-11-08.md).
Ela define executor e orçamento globais, resultados residentes versionados,
antecipação pela rota, backend GPU assíncrono com referência CPU e entregas
H0–H5. Estes componentes são entregas planejadas, ainda não implementadas; os modos e
chaves de configuração existentes não mudam com o documento.

A auditoria atual da arquitetura e a sequência proposta para acelerar setores
padrão estão em
[sector-generation-architecture-audit-2026-11-08.md](sector-generation-architecture-audit-2026-11-08.md).
Ela identifica orçamento ocioso, espera entre etapas, cópia de halo não usado
no planejador e necessidade de IDs estáveis para colocações pendentes. O grafo
executivo e o planejador menor são propostas, ainda não implementadas.

- [ ] H0/H1: estabilizar IDs, baseline de streaming e grafo executivo com
  preparação CPU/limites processuais; verificar entrega e ordem dos efeitos.
- [ ] H2: produtos `PreparedSector`, caches versionadas e prefetch por rota,
  com orçamento de RAM e contabilização de memória compartilhada.
- [ ] H3: ampliar kernels CPU e preparar batches de publicação validáveis.
- [ ] H4/H5: GPU compute opcional, comparação CPU/GPU em sombra, seleção por
  custo e orçamento de VRAM/latência; manter execução CPU independente.

A [auditoria de 07/11](performance-audit-2026-11-07.md) permanece como
referência para o crash anterior e seus limites conhecidos.

A fronteira de IO de setores e a fila explícita de resultados prontos foram
auditadas em
[sector-io-ready-queue-audit-2026-11-07.md](sector-io-ready-queue-audit-2026-11-07.md).

| Componente | Estado |
| --- | --- |
| BaseTiles em pool | Implementado, com identidade e validação de revisão |
| Admissão de tarefas | Estágios declarados validados; chaves por `(setor, etapa)` podem coexistir |
| Reserva dos snapshots | Antes das cópias; rollback pré-dispatch e handoff; cotas lógicas por mundo preservadas |
| Reservas físicas de arrays privados | Ledger processual único por wrapper, inclusive após fechar o mundo; teto global/deep accounting pendentes |
| Regiões lógicas em sombra | Implementado para observação e métricas |
| Planejador de microdungeon por snapshot | Seleção extraída; caminho síncrono e worker opt-in disponíveis |
| Identidade de colocações pendentes | IDs monotônicos no plano e registro pela dona; testes sem assets aprovados, validação integrada pendente |
| Continuação/avanço de setores visíveis | Correção opt-in e visita única por passagem; harness sintético aprovado, A/B real pendente |
| `MicroDungeonPlan` task | Planejador em worker opt-in; publicação ainda serial |
| Ativação inicial com workers | Preview opt-in; aguarda conclusão e preserva contrato síncrono |
| Entrega orientada à CPU | Modos `legacy`/`adaptive`/`aggressive`/`dynamic`: pool automática inicial nos modos não legados, orçamento por backlog e recuo conforme a política; sem redimensionamento em execução ainda |
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
   As colocações pendentes são copiadas com seu ID monotônico estável.
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
- [x] Preview de capacidade de entrega de setores padrão: ampliar apenas a
  janela pendente de BaseTiles até `4 × workers`, mantendo resultados prontos
  e commit autoritativo existentes. Requer validação de memória e expirações.
- [x] Preview limitado de planejamento privado de MicroDungeons para a
  fronteira já elegível de uma cadeia visível; aplicação, entidades e promises
  continuam seriais. Tracy 16 encontrou stales de halo antes da correção de
  admissão integral; requer repetição. Ver
  [runtime-validation-2026-11-08-tracy16-microdungeon-preparation.md](runtime-validation-2026-11-08-tracy16-microdungeon-preparation.md).
- [x] Implementar preview de seleção por TTL restante: somente o pedido
  visível mais próximo de expirar dentro de uma janela configurável recebe uma
  tentativa reservada; o TTL e os contratos de geração permanecem inalterados.
  Requer A/B focado em expirações e `sectorWait`.
- [ ] Validar o preview de risco de expiração contra a mesma rota, duração,
  save e mods, comparando expirações, `sectorWait`, justiça e tempo do update.
  Tracy 15 confirma ativação e resultado favorável sem crash, mas é sessão curta
  sem controle `false`; ver
  [runtime-validation-2026-11-07-tracy15-expiry-risk.md](runtime-validation-2026-11-07-tracy15-expiry-risk.md).
- [x] Implementar preview de continuidade de cadeia para setores visíveis que
  avançaram, preservando uma única tentativa `Complete` subsequente sem renovar
  TTL nem aumentar orçamento. Requer A/B de expirações e `sectorWait`.
- [x] Instrumentar marcos por pedido visível (Tiles e cada etapa de geração),
  mantendo vizinhos recursivos fora da telemetria de interesse do jogador.
- [x] Instrumentar o primeiro bloqueador de cada tentativa `Complete` visível,
  distinguindo etapa, vizinho, deadline e limite de níveis; requer nova coleta
  para decidir entre MicroDungeons, CaveLiquid e política de entrega.
  Tracy 17 identificou MicroDungeons como bloqueador dominante e revelou a
  precedência incorreta da continuidade, corrigida para a próxima coleta; ver
  [runtime-validation-2026-11-08-tracy17-visible-blockers.md](runtime-validation-2026-11-08-tracy17-visible-blockers.md).
- [x] Validar a continuidade após correção de precedência: Tracy 18 confirmou
  435 retomadas para 443 agendamentos, sem crash. MicroDungeons segue o
  bloqueador dominante e planos stale requerem fila de resultados prontos; ver
  [runtime-validation-2026-11-08-tracy18-continuation.md](runtime-validation-2026-11-08-tracy18-continuation.md).
- [x] Documentar protocolo A/B e critérios que condicionam prioridade futura
  de microdungeons/vizinhos; ver
  [regional-runtime-expiry-risk-validation-protocol.md](regional-runtime-expiry-risk-validation-protocol.md).
- [x] Separar relatórios de exploração entre lançamentos: arquivo primário
  existente é rotacionado antes da primeira escrita da nova sessão e cada
  relatório registra o início monotônico da sessão. Tracy 23 confirmou o
  isolamento do arquivo do mundo; o fallback recuperável de `storage` no
  perfil transitório `Nowhere` ainda requer correção de causa raiz; ver
  [runtime-validation-2026-11-08-tracy23-factory-phases.md](runtime-validation-2026-11-08-tracy23-factory-phases.md).
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
3. **Microdungeons:** validar a fila de planos prontos revalidada e ordenada
   por visibilidade antes de ampliar produtores; batches de mutação e a
   fachada gravadora vêm depois da equivalência do planejador.
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
- [ ] Para `generationMicroDungeonReadyResultSchedulerPreview`, comparar a
  mesma rota com/sem o preview e registrar pronto→aplicação, stale,
  completos/s, expirações e p99 da world thread.
- [ ] Correlacionar Tracy e JSONL; medir profiling ligado e desligado.
- [ ] Confirmar em Windows a escrita da categoria `storage`. Se uma seção
  opcional ainda causar `BadVariantType`, o writer deve preservar uma linha
  mínima com `reportFallback: "badVariantType"`; investigar a origem sem
  perder a janela de profiling.
- [ ] Validar `performanceMode: "dynamic"` em máquina com CPU distinta: medir
  workers automáticos, `dynamicPasses`, recuos pelo teto da atualização, p99,
  completos/s e expirações contra `aggressive` com a mesma rota.
- [ ] Correlacionar os estágios `generation.microDungeonPlan.apply.*` com Tracy
  para separar custo de planejamento, aplicação serial e publicação de mundo.
- [ ] Para cada entrada de `slowMicroDungeonFactories`, reproduzir a mesma seed
  e rota, comparar hash/footprint/duração e confirmar equivalência de tiles,
  entidades e saves antes de otimizar a factory correspondente.
- [ ] Usar a partição interna de factory para escolher uma única causa por vez:
  `definition`, âncora, colisão, `canPlace` ou `build`; comparar antes/depois
  com o mesmo hash, footprint e rota.
- [x] Para factories lentas dominadas por `definition`, separar busca na cache,
  resolução interna e leitura/construção da definição, sem registrar nome ou
  path do asset. Tracy 23 identificou esta necessidade; a coleta seguinte deve
  decidir se há miss de cache ou custo recorrente de lookup. Tracy 24 confirmou
  misses de cache e custo dominante de leitura/construção; o próximo recorte é
  taxa global de hits/misses/evicções e ocupação antes de alterar capacidade;
  ver [runtime-validation-2026-11-08-tracy24-definition-cache.md](runtime-validation-2026-11-08-tracy24-definition-cache.md).
- [x] Contar hits, misses, evicções, ocupação e capacidade da cache de
  definições de MicroDungeon, com cardinalidade fixa. Corrigir a LRU para
  evictar somente ao inserir um miss, nunca antes de confirmar um hit. Não
  estimar bytes sem modelo de memória transitiva verificável. Tracy 25 confirmou
  84 hits, 7 misses, zero evicções e máximo de 7/20 no mundo principal; o
  outlier remanescente é primeiro carregamento frio, não churn; ver
  [runtime-validation-2026-11-08-tracy25-lru-hit-fix.md](runtime-validation-2026-11-08-tracy25-lru-hit-fix.md).
- [x] Adicionar preview serial e limitado de pré-aquecimento de definições para
  planos privados de MicroDungeon já prontos e revalidados. O orçamento limita
  admissões, mas não preempta leitura fria; manter desligado até comparar p99,
  expirações, save e mods contra a mesma rota. Tracy 26 confirmou que misses
  deixam a factory, mas revelou defer redundante de planos vazios e um miss
  individual de 134,765 ms ainda serial; corrigir o plano vazio antes de nova
  validação; ver [runtime-validation-2026-11-08-tracy26-definition-prewarm.md](runtime-validation-2026-11-08-tracy26-definition-prewarm.md).
- [x] Corrigir o preview para que plano pronto sem MicroDungeon não seja
  adiado: registrar `skippedEmptyPlans` e aplicar na mesma passagem. Revalidar
  expirações e espera de setor com o preview habilitado antes de qualquer
  decisão de adoção. Tracy 27 confirmou 814 bypasses e redução de 760 para 40
  passagens, mas o primeiro miss ainda causa 140,102 ms na WorldThread; manter
  o preview desligado fora de testes; ver
  [runtime-validation-2026-11-08-tracy27-empty-prewarm-fix.md](runtime-validation-2026-11-08-tracy27-empty-prewarm-fix.md).
- [x] Particionar o primeiro miss frio de `DungeonDefinition` entre JSON,
  metadata/rules, tileset e construção inclusiva de partes `image`/`tmx`, sem
  caminhos ou nomes de assets. A próxima coleta deve escolher uma única etapa
  para otimização; não mover leitura de assets para workers sem nova auditoria.
  Tracy 28 identificou readers TMX (129,831 ms em duas partes) como a causa
  dominante do miss frio; ver
  [runtime-validation-2026-11-08-tracy28-tmx-definition.md](runtime-validation-2026-11-08-tracy28-tmx-definition.md).
- [x] Particionar reader TMX em JSON do mapa, `TMXMap`, tilesets, camadas/
  descompressão e grupos de objetos, com contagens fixas. A próxima coleta deve
  decidir entre cache de mapas e outra otimização serial de baixo risco; não
  mover assets para workers nesta fase. Tracy 29 não acionou factory, portanto
  não é evidência para essa decisão; repetir uma rota com MicroDungeon TMX
  nova; ver
  [runtime-validation-2026-11-08-tracy29-tmx-inconclusive.md](runtime-validation-2026-11-08-tracy29-tmx-inconclusive.md).
- [x] Adicionar preview limitado de avanço de setores visíveis após o primeiro
  bloqueio: examina cadeias posteriores já prontas antes do fallback legado de
  BaseTiles-only, sem escrita concorrente, TTL novo ou interesse implícito.
  Requer comparação de entrega, expirações, justiça e saves/mods.
