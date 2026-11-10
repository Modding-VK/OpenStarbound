# Envelope de memória da geração: cobertura e pressão

Estado: H0 / RAM-01, `Experimental`, 09/11/2026.
[Programa](sector-runtime/README.md) · [Inventário de ownership](sector-runtime/memory-and-prefetch.md)
· [Roadmap](sector-runtime/roadmap.md) · [Relatório da entrega](sector-runtime-h0-capacity-kernel-2026-11-09.md)

Continuidade de identidade/equivalência: [contrato BaseTiles](sector-runtime-base-tiles-contract.md)
e [último relatório](sector-runtime-h0-real-sector-sampling-2026-11-10.md).

Continuidade de amostragem real: construtor normal, layout/seletores simples e
LRU reais agora produzem arrays BaseTiles 32×32 completos em teste opt-in.
Não é admissão profunda ou ownership de contexto completo: layout controlado
montado por leitor, sem Perlin/caches mutáveis da árvore, jobs ou publicação.
Nenhum modelo/cota/fila foi ampliado; RAM-01 continua em 25%.

Continuidade de inputs estáticos: config Json imutável retida com medição v2
opcional/deduplicada, lifetime de tokens ligado às capturas rastreadas. Raiz
inteira pode reter campos não usados; cobertura limitada/recusa não filtra dados.
Wrapper/vetor de tokens/control blocks e polígonos seguem excluídos; não é
admissão do contexto completo ou validação de schema. Sem classe/cota nova,
ampliação de filas ou adoção nos jobs normais.

Continuidade de custom terrain: captura proprietária de geometria/polígonos/
parâmetros, sem reter a origem, testada fora dos jobs normais. Seu heap de lista
e vértices **ainda não é admitido/registrado no ledger**; integrar essa cobertura
e lifetime antes de adoção no executor. Sem nova classe, cota ou ampliação de
filas. O adapter normal usa o helper emprestado, sem captura por bloco.

Continuidade de blockInfo: kernel separado do cache, adapter não proprietário
e fontes controladas em teste; sem snapshot profundo, classe de memória nova,
fita residente por tile ou ampliação de cotas. Caches de seletores continuam
privados; a contabilidade do contexto real é pendente.

Continuidade de 10/11: leitores reais testados com metadata controlada não ampliam
a cobertura do ledger. Catálogo/binding seguem fora do produtor normal; headers
de tipos/fachadas e tags privadas de fixture não acrescentam campos às classes.
Não usar a memória de fixtures ou bytes ativos de catálogo como teto de bancos,
layout/biomas/assets ou como autorização para ampliar filas.

Assets real e seus workers também foram exercitados com arquivos de fixture.
Isso confirma retenção física por binding e encerramento, não contabiliza os
heaps globais ou amplia o envelope. A correção de ByteArray vazio não aloca,
e a de parada de workers não cria novos buffers/classes/limites.

Root real agora é exercitado no alvo opt-in com fontes mínimas carregadas. A
retenção física do binding foi confirmada, mas Root, bancos globais e seus heaps
não passam a integrar o ledger privado. A realocação de corpos de manutenção
não adiciona campos/cobranças e a correção da seed não altera o envelope.

Este contrato delimita o **subconjunto privado de geração contabilizado**.
Não redefine RAM-01 como orçamento de todo o motor e não transforma estimativa
em limite superior. O [schema executável](../source/game/StarWorldGenerationMemoryEnvelope.hpp)
e seus [testes](../source/test/world_generation_memory_envelope_test.cpp) acompanham
este documento; a coleção principal continua com os mesmos 49 contratos.

## Três quantidades distintas

| Quantidade | Significado | Não significa |
| --- | --- | --- |
| ArrayPayload | sizeof dos arrays privados BaseTiles/halo reservados | Pedido completo de malloc, todos os heaps dos tiles ou estado autoritativo publicado |
| AllocatorRequest | Bytes solicitados pelo allocator rastreado, incluindo header/padding e elementos inline | RSS, malloc usable size ou backing profundo de cada elemento |
| Estimate | Fórmula explícita para parcelas conhecidas, observadas ou previstas | Censo físico instantâneo, cobertura integral ou garantia hard de heap |

`trackedBytes` é a soma atual das três parcelas. Inclui previsões/sobreposição
quando retidas pelo ledger, não apenas bytes residentes que malloc entregou.
Os picos históricos não são somados ao uso atual nem usados como pressão atual.
Não somar picos independentes como se tivessem ocorrido simultaneamente.

## Schema das 14 classes

Os IDs são os de WorldGenerationMemoryClass; uma validação em compile time exige
classificar cada enum exatamente uma vez e na ordem. Always significa que a
cobrança básica existente não exige profiling; não certifica todo heap daquela
categoria. RegionalProfiling descreve a condição de criar a estimativa: tokens
já criados continuam retidos até o fim de seu lifetime.

| IDs / classes | Quantidade | Coleta | Fronteira e exclusões principais |
| --- | --- | --- | --- |
| 0 BaseTilesArray / 1 MicroDungeonHalo | ArrayPayload | Always | Reserva antes de construir/copiar; fora: wrappers/control blocks e heaps não incluídos no array |
| 2 TemplateJsonEstimate | Estimate | RegionalProfiling | Observação pós-construção de owners compartilhados; template e Json enfileirado deduplicam no mesmo registro; scan/SSO/overhead parciais |
| 3 SnapshotMetadataEstimate / 4 MicroDungeonPlanEstimate | Estimate | RegionalProfiling | Observações/fórmulas de objetos e containers conhecidos; não todos os índices/ponteiros externos ou picos de construção |
| 5 WorkerTemplateEstimate | Estimate | Always | Objeto/cache e storage solicitado da LRU observado na fronteira do job; não todo layout/biomas/seletores nem picos internos durante o job |
| 6 MicroDungeonPlanBuffer / 7 MicroDungeonCandidateBuffer / 8 MicroDungeonDistributionBuffer / 10 MicroDungeonNameBuffer | AllocatorRequest | Always | Reserva antes do raw allocator, incluindo overlap de realocação; não backing dos elementos nem malloc overhead |
| 9 MicroDungeonDistributionEstimate | Estimate | Always | Original adotado após construção; previsões/reconciliação do bundle; Json opcional separado na classe 2, sem cobrança duplicada |
| 11 MicroDungeonNameEstimate / 12 MicroDungeonCandidateEstimate | Estimate | Always | Previsão de parcelas conhecidas e reconciliação; SSO/privados e previsões intermediárias não são heap completo |
| 13 MicroDungeonTemporaryEstimate | Estimate | Always | Listas originais observadas e cópia/saída previstas; capacidade retida após clear; não picos anteriores à adoção |

Os lifetimes físicos e fórmulas detalhadas continuam no inventário e relatórios
dos recortes. Fechar mundo/facade ou cancelar logicamente um pedido não apaga
reservas ainda retidas por workers, promises, resultados ou bundles. Publicar
um array transfere-o para o mundo vivo, fora do ledger privado, sem afirmar que
a RAM foi fisicamente liberada.

## Coerência não é completude

O envelope verifica, com soma protegida contra overflow:

- soma de bytes/recursos das classes contra totais do snapshot;
- totais de arrays, buffers e estimativas contra suas categorias;
- bytes sem recurso proprietário e consistência dos recursos Json/registro
  compartilhado.

O snapshot é obtido sob o mutex já existente; a conversão usa somente esse valor,
sem novos locks, alocações ou travessia de payloads. Ela não confere se cada
fórmula estima bem um heap nem se todos os produtores foram observados.
`coherent=true` não concede `CompleteHeap`: este permanece **false**, até vazio.
Em caso incoerente/overflow, não usar os totais parciais para recomendar admissão.

`historicalIncompleteUpdates` é diagnóstico acumulado do ledger, não o número
de owners atualmente incompletos. Não bloquear para sempre porque houve uma
falha antiga, nem interpretar zero como prova de cobertura. Falta um censo
processual da qualidade atual dos owners; flags locais continuam separados.

## Exclusões assumidas neste escopo

Não cobertos integralmente: estado autoritativo de WorldStorage, entidades/Lua,
assets/caches globais, IO/decode salvo, Json ambiental e de templates privados,
layout/biomas/seletores, nós/allocator de TileDamage, malloc/control blocks não
rastreados, bookkeeping do próprio diagnóstico e picos anteriores à adoção.
RAM/VRAM de GPU não entram neste ledger. Memória unificada exigirá regra própria
para não contar o mesmo buffer duas vezes.

Json comum/assets podem reter backing após o último tracker. Capacidade/SSO
podem superestimar e exclusões podem subestimar. Um saldo zero do ledger não
prova RSS zero nem desalocação global. Nenhuma margem ou teto total é inferido
automaticamente dessas parcelas.

## Modelo de pressão e admissão opt-in do ledger

WorldGenerationMemoryPressurePolicy continua um modelo sem callbacks, locks,
alocação, relógio, SO ou modelo de CPU. O ledger agora pode usá-lo de forma
opt-in, sob seu mutex, conforme o [relatório de reserva atômica](sector-runtime-h0-atomic-memory-2026-11-09.md).
O singleton do jogo continua usando o construtor anterior, com pressão desligada:
não há chaves novas de config, orçamento finito padrão ou ajuste de workers.

O chamador futuro fornece limites explícitos de um escopo declarado:
`resumeBytes < softBytes < hardBytes`. Por padrão o modelo está desligado,
retorna Unrestricted e não escolhe valores conforme a máquina. Limites ativados
inválidos, envelope incoerente ou demanda desconhecida recusam a recomendação
de pedido, mas nunca a drenagem existente.
Entradas inválidas também preservam a última pressão válida para a histerese:
uma falha transitória não pode autorizar recuperação precoce na próxima amostra.

| Pressão observada | Crescimento especulativo | Visível / dependência | Trabalho existente |
| --- | --- | --- | --- |
| Normal | Somente até o limite soft | Até o headroom hard | Drenagem preservada |
| Soft | Adiar novos bytes | Até o headroom hard | Drenagem preservada |
| Hard | Não crescer | Não crescer | Reuso sem bytes novos e drenagem preservados |
| Invalid | Não recomendar pedido | Não recomendar pedido | Drenagem preservada; corrigir a observação/contrato |
| Unrestricted | Política desligada, sem garantia de limite | Política desligada, sem garantia de limite | Nenhuma decisão de runtime é alterada |

Com limites/observação/demanda válidos, pedido de zero bytes pode ser aconselhado
em Normal/Soft/Hard. Isso pressupõe ausência real de crescimento; não autoriza
um kernel que aloca a declarar zero nem altera as cotas de CPU/publicação.

Hard entra em uso atual >= hard. Ao cair abaixo de hard, vira Soft enquanto
uso >= soft ou a recuperação ainda estiver acima de resume. Só volta a Normal
em uso <= resume; assim não oscila junto ao soft. Pedido recusado não vira
consumo nem aumenta pressão por si só. Pedidos aceitos podem chegar exatamente
ao limite correspondente; o próximo snapshot refletirá a fronteira atingida.
Comparações usam headroom por subtração verificada, sem soma que possa dar wrap.

## Reserva atômica implementada no escopo

O construtor tipado do ledger aceita maximumBytes e limites explícitos; quando
ativados, exige resume < soft < hard <= maximumBytes. A sobrecarga antiga conserva
seu teto finito de teste, independente da pressão. Limites desligados não ativam
scan do envelope nem checks da política em cada reserva.

`withDemand(Visible/Dependency/Speculative)` produz uma fachada do mesmo estado,
não outro orçamento. Checagem do envelope, decisão e cobrança ocorrem sob o
mesmo mutex em reserve, nova reserva Json e crescimento positivo de estimativa.
Governa a soma contabilizada, não heap omitido/RSS. Dois ledgers independentes
ainda são dois escopos: a integração futura precisa usar uma raiz processual.

O token conserva a demanda de criação para crescimentos posteriores. Não há API
de promoção de token ou ajuste dinâmico de limites nesta etapa. Json com owner
já rastreado é reuso sem novos bytes; modelo conflitante continua recusado.
Shrink, destruição e liberação não precisam de nova admissão.

Allocators conservam demanda em copy/move/rebind e propagação de containers.
Demanda diferente participa de sua igualdade. No runtime legado, todos seguem
a demanda padrão anterior, com pressão desligada; não se altera a sequência
de cópias/crescimento dos elementos. O request integral continua reservado antes
do raw allocator, com buffers velho/novo cobrados juntos na realocação.

Recusa não cobra bytes nem constrói payload; reserva retorna nulo, resize retorna
false e allocator mantém sua exceção bad_alloc. Falha física/cópia lançante libera
o novo request, sem liberar o buffer antigo ainda vivo. Isso não implementa retry
de scheduler nem autoriza tratar a exceção como resultado de geração vazio.

`snapshot().admission` expõe enabled, pressureChecks, pressureRefusals e lastAdvice,
modelo próprio 1. Checks contam avaliações da política, inclusive reservas zero,
não todos os motivos possíveis de recusa. Reuso Json, shrink e resize sem mudança
não contam novo crescimento. LastAdvice é a observação anterior à cobrança, não
pressão atual recalculada após toda liberação. Counters saturam em size_t máximo;
não há nova série desses dados no profiling do jogo nesta entrega.

## Reserva composta antes do dispatch

`reserveBatch(array<Request, N>)` admite todas as parcelas não compartilhadas sob
um único lock/check da política, ou nenhuma. Soma e classes são validadas antes
de criar os tokens inativos; todas as alocações dos tokens precedem a cobrança.
O resultado é optional de array fixo, sem vetor intermediário. Recusa retorna
optional vazio e não altera bytes/picos; falha de alocação pode lançar. Batch
vazio é sucesso sem check; parcelas zero ainda são recursos distintos. Classes
repetidas representam recursos diferentes; Json compartilhado é recusado aqui
e continua exigindo seu registro por owner. Cada token transfere/libera sua
parcela independentemente depois da admissão conjunta. Não é uma transação
entre o ledger processual e o registro owner-thread de tasks.

O primeiro consumidor real é BaseTiles assíncrono: demanda explícita Visible,
igual à classificação padrão anterior, e duas parcelas antes de copySector:
um sizeof(Array) e uma estimativa inicial sizeof(WorldTemplate). O array privado
é modificado e devolvido **in place**: input e output são o mesmo backing,
não duas cobranças. Estado autoritativo antigo permanece fora desse ledger.
O template antigo do worker continua cobrado enquanto vivo; sua possível
substituição já tem a parcela inicial reservada. O cache consome o token sem
nova admissão, ou libera a previsão se houver hit. Tokens de ledger/demanda/
classe/tamanho incorretos são recusados. A identidade do cache também considera
ledger/demanda para não reutilizar uma entrada cobrada em outro escopo.

Guarda de task e owners locais preservam rollback antes de aceitar o produtor;
após handoff, invalidação/retirada lógica não liberam owners físicos ainda vivos.
O wrapper destrói o payload antes do token. O caminho do planejador, metadados
e Json não passaram a ter reserva composta. Não há classificação dinâmica de
interesse/dependência/especulação, promoção ou alteração de prioridades/TTL.

A reserva inicial do template **não é previsão completa de crescimento** de
LRU/layout/biomas. Refresh continua observacional e pode marcar incompletude;
o teste negativo em hard demonstra isso. A prova de conclusão em hard usa um
produtor limitado ao contrato declarado, não demonstra progresso do jogo/DAG
inteiro. Não ativar orçamento finito real com base nesse teste. Contadores
atuais passam a incluir previsões de templates enfileirados; não interpretar
essa antecipação como aumento medido de RSS ou somar input/output in place.
Modelos de envelope/admissão e fórmulas profundas permanecem inalterados.

## Saída variável e protocolo limitado de retry

`tryReserveBatch` acrescenta status e requestedBytes aos tokens da mesma admissão:
Admitted, Deferred (cabe no escopo vazio, não agora), Impossible (não cabe no teto
fixo daquela demanda nem vazio) e Invalid (contrato/envelope inválido). Só Admitted
devolve tokens; tokens inativos não escapam da recusa. Soft é o teto da demanda
especulativa, hard o de visível/dependência; desligado vale o máximo legado.
`reserveBatch` delega e conserva seu optional anterior. Não inferir o motivo
lendo lastAdvice após liberar o mutex: outra thread já pode tê-lo atualizado.

`WorldGenerationMemoryRetry<N>` é protocolo owner-thread, modelo 2, ainda **sem
consumidor na geração real**. Exige produtor puro/reexecutável e parcelas físicas
com tokens. A tentativa aceita um descritor de crescimento estrito, abandona os
dados parciais antes de alocar a saída maior e só pode readmitir depois que todas
as weak refs dos tokens anteriores expirarem. Fence não libera payload nem cobre
backing que escapou sem token. O ledger e a demanda ficam vinculados ao descritor.

Deferred carrega deferredCapacity da própria decisão sob lock. O ledger avança
capacityRevision em shrink positivo/release ativo, inclusive recurso zero; reuso,
carga/crescimento/recusa inativa não sinalizam capacidade. admit não recebe mais
revisão manual: compara o marker com a revisão do ledger vinculado e retorna
AwaitProgress sem criar tokens/reavaliar pressão enquanto puder suprimir o poll.
Uma liberação depois da recusa não se perde em snapshot posterior. Não há callback
ou wakeup: o owner ainda deve chamar admit, e um sinal não garante que o pedido
inteiro caiba. É capacidade declarada, não RSS. Saturação uint64 deixa de suprimir
consultas, sem wrap. Modelo do sinal 1; consulta pequena usa o mutex existente.
Tentativa ativa não duplica dispatch; respostas stale não mudam a nova tentativa.
Planos crescem conservadoramente, mantendo classes/nenhuma parcela menor, sem
buscar footprint mínimo. Limite explícito de tentativas admitidas e Impossible/
Invalid encerram o ciclo; Deferred não consome tentativa de produção. stop e
conclusão não liberam owners ainda vivos. Exceções de alocação/produção propagam,
nunca viram automaticamente previsão insuficiente ou output vazio.

A fixture com dois workers e resultados retidos comprova progresso **somente no
produtor controlado**: input 60 + output previsto 20, output descoberto 60, hard
200 e resultado antigo 40. Duas tentativas liberam seus inputs; A conclui, B adia
sem capturar input e retoma depois do drain útil. A saída é igual à produção com
previsão suficiente. Não prova replay seguro do kernel real, fairness, OOM ou
ausência de bloqueio por dependências externas. Não há espera por budget no
helper. Pool parado/drenado na fixture é mecanismo de observação, não estratégia
do jogo. Allocators de containers existentes ainda não consomem esse crédito.

## Fronteira para a próxima integração de tarefas

Uma recomendação de snapshot **não é reserva atômica**. O caminho opt-in acima
resolve essa janela no ledger, não em um conselho consumido fora dele. Antes
de ativar um orçamento para tarefas reais:

1. O sinal já alimenta o protocolo; falta mailbox/retry real. Ticket versionado e
   comparador de campos BaseTiles foram entregues, mas não são conteúdo/contexto
   puro completo. Inputs escalares de materiais/mods/biomas já têm adaptador sem
   heap; observações diagnósticas congeladas não são contexto profundo ou replay
   integrado. Fita fora do produtor normal, 176 bytes/tile nesta ABI; não está
   implicitamente no ledger. Catálogo compacto de capacidade fixa já foi testado,
   não integrado ao produtor: scope 2/binding já usam clocks reais de Root/Assets
   e versões de materiais. Strong handles retêm fontes inteiras mesmo inválidos;
   não contar só 72 bytes de binding/64 de clock desta ABI ou declarar globals
   cobertos pelo ledger. Comparar leitores/lifecycle/assets/cobertura e demanda,
   MOD-01/VAL-02, antes de integrar replay. Contabilizar
   objeto/backing reservado/donos antes de alocar, não só entradas ativas nem por
   leitor. Não reutilizar tiles parciais após exceção.
   Auditar biomeTransition/default/cópias. Estender tokens
   sem recobrança; não reexecutar callbacks/Lua/mundo vivo por capturar exceção.
2. Preservar headroom e progresso de dependências/consumo: os testes preservam
   a margem soft→hard e o overlap real, não provam ausência de deadlock em todo
   grafo com resultados bloqueados e caches retidas.
3. Usar controlador processual único e fairness/cotas explícitas entre mundos;
   não multiplicar o orçamento por WorldGenerator ou instância de policy.
4. Definir pressão externa, limites do usuário/hardware e memória unificada
   com observações válidas. Esta API não detecta RAM livre nem oferece teto RSS.
5. Validar MOD-01/VAL-02 e streaming equivalente antes de ampliar admissão ou
   ligar comportamento por padrão. Um protótipo H1 em sombra pode usar escopo
   delimitado sem exigir contabilizar cada heap do motor.

Evicção, cancelamento e mudança de ownership continuam contratos separados.
Não liberar buffers ainda em uso nem mudar TTL, ordem/RNG, save, callbacks ou
publicação para satisfazer uma recomendação de pressão. A prioridade segue
entrega de setores comuns; utilização de hardware e budgets maiores não são
ganho demonstrado de throughput.
