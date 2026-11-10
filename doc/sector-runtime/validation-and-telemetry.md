# Validação acelerada, telemetria e continuidade

[Índice](README.md) · [Baseline](baseline.md) · [Roadmap](roadmap.md)

## Propósito

Reduzir a dependência de testes manuais sucessivos e provar que maior uso de
hardware melhora a entrega real. Automatizar invariantes e reproduções
curtas, mantendo testes em jogo para as fronteiras que exigem assets/mods.

## Primeiro recorte executável de H0 — 09/11/2026

`WorldGenerationPassTest` usa a política real de `WorldStorage` em um harness
de passagens com relógio virtual, estágios/produtos sintéticos e disponibilidade
controlada de produtores. São 18 testes, incluindo 192 combinações de limites
e modos; junto aos oito testes de identidade, 26 passaram com ASan/UBSan.
Ver [reprodução, comandos e limites](../sector-runtime-h0-streaming-pass-2026-11-09.md).
Esse recorte de VAL-01/02 não é benchmark do jogo: não executa o gerador
recursivo 3×3, assets, TTL, IO, Lua ou rede. A comparação sintética não valida
equivalência de mundos/mods nem permite declarar fator de aceleração.

O [recorte de memória processual](../sector-runtime-h0-process-memory-2026-11-09.md)
expande o alvo para 95 testes, com 20 casos de ledger/lifecycle e testes
usando WorkerPool real, gates controlados e oito threads. Os 20 casos passaram
também em 20 repetições embaralhadas. São oito grupos CTest NoAssets, sem
confundir casos únicos com repetições nem ASan/UBSan com validação de TSan.

O [recorte de estimativas v1](../sector-runtime-h0-retained-estimates-2026-11-09.md)
leva o alvo a **115 testes** e nove grupos CTest. Os 20 novos de identidade
de storage, sizing limitado, concorrência e retenção do output passaram também
20 vezes embaralhados (400 execuções adicionais), com ASan/UBSan estritos.
O objeto real de geração compilou; sintaxe Tracy passou, sem linking completo.
As estimativas exigem profiling regional e não medem toda a RAM. Custos da
coleta e throughput ainda exigem A/B real; não há ganho presumido por teste verde.

O [recorte do cache privado](../sector-runtime-h0-worker-cache-2026-11-09.md)
amplia para **136 testes**, dez grupos CTest e 21 casos novos repetidos 20 vezes
(420 execuções adicionais). Inclui allocator/LRU reais, retenção thread-local
e oito workers simultâneos com gates controlados e saldo final zero. Objetos
reais de geração/template e sintaxe Tracy aprovados, sem linking completo.
Naquela etapa, teste de falha multi-elemento não validava a falha de inserção
do vetor do pool; a reprodução/correção foi a próxima entrega.

O [recorte de exceção do pool](../sector-runtime-h0-allocator-exception-2026-11-09.md)
confirma o estado inválido com dois casos negativos antes da correção e passa
**144 testes**, onze grupos CTest e oito casos novos repetidos 100 vezes
(800 execuções adicionais), com ASan/UBSan. Injeta falha na fronteira interna
de inserção, sem forçar OOM real ou dereferenciar o ponteiro inválido; após a
correção exercita retry, aliases, valores vivos e uso do cache no próximo job.

O [recorte de buffers](../sector-runtime-h0-output-buffers-2026-11-09.md) passa
**167 testes** e doze grupos CTest. São 23 casos novos repetidos 20 vezes
(460 execuções adicionais), com ASan/UBSan: reserva anterior ao raw allocator,
overalignment, realocação/rollback, propagação entre domínios/classes, abandono
da promise, consumo sem cópia e seleção sintética equivalente. Modelo de
estimativas 2 separa requests de buffers de heurísticas de heaps internos.
Sem benchmark do jogo; menos cópia não é por si só um fator de aceleração medido.

O [recorte de distribuições compartilhadas](../sector-runtime-h0-distribution-bundles-2026-11-09.md)
passa **185 testes**, treze grupos CTest e 18 casos novos repetidos 20 vezes
(360 execuções adicionais), com ASan/UBSan. Usa sampler real sem Root, cópia
nativa para referência e controle negativo da ordem de hash; verifica 561
coordenadas por variante, oito leitores concorrentes e retenção com WorkerPool.
Treasure opaco é preservado/amostrado. Modelo de estimativas 3, sem cobertura
completa de variants ou benchmark do jogo. `sharedSnapshots` conta referências,
não setores/s. Sete objetos reais e sintaxe Tracy de oito fontes aprovados;
sem linking completo, TSan ou validação de mods/mundos.

O [recorte de nomes/scratch](../sector-runtime-h0-planning-storage-2026-11-09.md)
passa **204 testes**, catorze grupos CTest e 19 casos novos repetidos 20 vezes
(380 execuções adicionais), com ASan/UBSan. Inclui 4.096 combinações de
predicados contra referência herdada, nomes compartilhados/lifecycle/recusa,
oito leitores e WorkerPool real. Controle negativo de hash com wrap confirma
que capacidade aquecida pode mudar escolhas: inputs MicroDungeonNames usam
storage fresco para preservar cópias. Modelo 4; dois objetos reais e sintaxe
Tracy de três fontes aprovados. Sem benchmark, linking completo ou equivalência
integrada de streaming/mods; scratch/heaps profundos continuam parciais.

O [recorte de payloads](../sector-runtime-h0-candidate-payloads-2026-11-09.md)
passa **223 testes**, quinze grupos CTest e 19 novos repetidos 20 vezes
(380 execuções adicionais), com ASan/UBSan. Inclui previsão antes da factory,
overlap, rollback/retry, sizing sem varredura completa por append, retenção em
worker e oito jobs concorrentes. Compara 96 candidatos nativos com hashes com
wrap, caminhos ambiental/enfileirado e mesmas escolhas/capacidades/ordem.
Modelos geral 5/distribuições 2/payloads 1; backing parcial, sem teto hard.
Dois objetos reais e sintaxe Tracy de três fontes aprovados; sem streaming
integrado, linking completo ou ganho de jogo medido.

O [recorte dos temporários](../sector-runtime-h0-temporary-storage-2026-11-09.md)
passa **246 testes**, dezesseis grupos CTest e 23 novos repetidos 20 vezes
(460 execuções adicionais), com ASan/UBSan. Inclui quatro donos simultâneos,
pré-reserva antes da cópia, transferência/capacidade, exceções e recusas com
rollback, WorkerPool e oito jobs. Reprodução nativa de 432 candidatos mantém
capacidade/ordem/IDs/escolhas. Modelo geral 6/temporários 1; adoção inicial
não é reserva anterior aos originais. Quatro objetos reais e sintaxe Tracy
de três fontes aprovados; sem linking completo ou benchmark integrado.

O [recorte dos containers](../sector-runtime-h0-container-storage-2026-11-09.md)
passa **266 testes**, dezessete grupos CTest e 20 novos repetidos vinte vezes
(400 execuções adicionais), ASan/UBSan. O recorder de teste confronta pedidos
do std::allocator com payload físico do vetor, incluindo vazios/sentinela e
alinhamento 64; não altera allocators de produção. Cobre clear/cópia/move,
normalização, ordem/hash/alocação inalterados ao consultar, fórmulas nativas,
fatores/bytes do mapa e oito leitores. Modelos geral 7/distribuições 3/payloads 2/
temporários 2; Json e overhead dos nós continuam opacos. Quatro objetos reais
e sintaxe Tracy de quatro fontes aprovados; não é linking completo, TSan,
prova integrada de save/mods ou ganho de jogo medido.

O [recorte do Json](../sector-runtime-h0-json-storage-2026-11-09.md) passa
**294 testes**, dezoito grupos CTest e 28 novos repetidos vinte vezes
(560 execuções adicionais), ASan/UBSan. JsonObject inclui buckets e chaves
por owner; testa omissão limitada sem envenenar o modelo de outra captura,
limite finito/recusa, sharing, bytes serializados, WorkerPool e oito capturas
concorrentes. Seis casos confrontam map com recorder std::allocator de teste,
inclusive alinhamento 64. Modelos geral 8/Json 2; profiling continua necessário.
Quatro objetos reais e sintaxe Tracy de quatro fontes aprovados; não é
streaming/assets/mods, TSan, linking completo ou ganho de jogo medido.

O [recorte de Json enfileirado](../sector-runtime-h0-variant-json-2026-11-09.md)
passa **320 testes**, dezenove grupos CTest e 26 novos repetidos vinte vezes
(520 execuções adicionais), ASan/UBSan. Floresta com limite único, sharing com
template, todos os variants, transferência do mesmo handle sem rescan, retirada
da fila, recusa/falha/retry, WorkerPool e oito bundles privados. Sampler misto
confrontado em 561 posições e até 64 seeds; ObjectPool em 64 seeds. Modelos
geral 9/Json 2/Json enfileirado 1. Quatro objetos reais e sintaxe Tracy de cinco
fontes aprovados; coleta diagnóstica, não cobertura geral, TSan ou streaming real.

O [recorte do envelope](../sector-runtime-h0-memory-envelope-2026-11-09.md)
passa **354 testes**, vinte grupos CTest e 34 novos repetidos vinte vezes
(680 execuções adicionais), ASan/UBSan. Schema das 14 classes, somas/recursos,
overflow, retenção real e oito threads; política isolada com headroom, histerese,
inválidos e grade de 19.521 combinações em um caso. Envelope modelo 1, sem mudar
fórmulas anteriores. Dois objetos reais e sintaxe Tracy de três fontes aprovados.
Runtime só amostra o envelope; não executa decisões de admissão, não houve
emissão em jogo ou benchmark integrado. Ver [escopo/exclusões](../sector-runtime-memory-envelope.md).

O [recorte de reserva atômica](../sector-runtime-h0-atomic-memory-2026-11-09.md)
passa **388 testes**, 21 grupos CTest e 34 novos repetidos vinte vezes
(680 execuções adicionais), ASan/UBSan. Inclui oito disputantes soft/hard,
conselhos obsoletos recusados atomicamente, sharing Json, demanda em tokens/
allocators, overlap e rollback real. Cinco objetos de produção e sintaxe Tracy
de cinco fontes aprovados, sem linking completo. Admissão modelo 1; singleton
continua desligado. Não é prova de progresso de todo DAG, TSan ou throughput.

O [recorte composto](../sector-runtime-h0-composite-memory-2026-11-09.md) passa
**420 testes**, 22 grupos CTest e 32 novos repetidos vinte vezes (**640 execuções
adicionais**), ASan/UBSan. Batch sem prefixo cobrado, oito disputantes, cache
hit/miss/overlap/rollback, isolamento ledger/demanda, lifetime após cancelamento
e conclusão em hard com parcelas limitadas. Controle negativo demonstra que
crescimento não previsto continua incompleto; não é prova de progresso real
do jogo/DAG. Cinco objetos de produção e sintaxe Tracy de cinco fontes aprovados,
sem linking completo. Não há novas séries; a reserva antecipada de templates
enfileirados muda o saldo observado, não comprova RSS/throughput maiores.

O [recorte variável](../sector-runtime-h0-variable-memory-2026-11-09.md) passa
**452 testes**, 23 grupos CTest e 32 novos repetidos vinte vezes (**640 execuções
adicionais**), ASan/UBSan. Status atômicos, fences de input/output, estados stale/
ativos/terminais, eventos sem capacidade, concorrência de oito descritores,
dois workers/resultados retidos, equivalência de bytes e exceções. Cinco objetos
reais e sintaxe Tracy de cinco fontes aprovados, sem linking completo. Retry
sem consumidor real; revisão útil do owner e cobertura física são pré-condições,
não eventos/heap completo inferidos. Não é TSan, benchmark ou prova de todo DAG.

O [recorte capacidade/subkernel](../sector-runtime-h0-capacity-kernel-2026-11-09.md)
passa **484 testes**, 25 grupos CTest e 64 casos afetados repetidos vinte vezes
(**1.280 execuções adicionais**), ASan/UBSan. São 32 casos novos. Capacidade cobre
marker da recusa, lost-signal negativo, release por worker, aliases, shrink,
escopo e saturação; retry modelo 2 usa o sinal do ledger. Kernel escalar real
cobre 7.560 combinações de campos/float bits e 1.024 decisões particionadas,
não setores completos. Cinco objetos e sintaxe Tracy de seis fontes aprovados,
sem linking completo; sem novas séries no profiling ou throughput demonstrado.

O [recorte identidade/comparação](../sector-runtime-h0-base-tiles-identity-2026-11-09.md)
passa **524 testes**, 26 grupos CTest e 40 novos repetidos vinte vezes (**800
execuções adicionais**), ASan/UBSan. Tipos reais ServerTile/TileDamageStatus em
NoAssets; 25 grupos de campos, dano privado/derivado, geometria ativa e políticas
de cache, bits float, bordas válidas, tickets/versões/resultados tardios e lifetime
físico. [Contrato](../sector-runtime-base-tiles-contract.md) exige campos inicializados:
não lê biomeTransition indefinido nem padding. Sete objetos reais e sintaxe Tracy
de seis fontes aprovados, sem linking completo do jogo. Comparator não roda no
hot path. Novo contador de código: generation.baseTiles.staleProductIdentity;
não há nova run que o tenha observado, throughput ou equivalência integrada.

O [recorte inputs/consultas](../sector-runtime-h0-base-tile-inputs-2026-11-09.md)
passa **548 testes**, 27 grupos CTest e os 64 casos afetados repetidos vinte vezes
(**1.280 execuções adicionais**), ASan/UBSan. São 24 novos: campos completos e
traces em 4.096 combinações, seis pontos de exceção com estado parcial equivalente,
NoMod, camadas existentes, input intacto, resolver emprestado e 1.024 tiles em quatro
workers com valores imutáveis. Controles negativos demonstram que input igual não
fecha o resolver e tile parcial não é input seguro de retry. Sete objetos reais
e sintaxe Tracy de três fontes aprovados, sem linking completo do jogo/Tracy.
Não executa WorldTemplate/assets reais nem mede throughput; H0 continua parcial.

O [recorte observações/replay](../sector-runtime-h0-base-tile-observations-2026-11-09.md)
passa **575 testes**, 28 grupos CTest e 91 casos afetados repetidos vinte vezes
(**1.820 execuções adicionais**), ASan/UBSan; 27 novos normativos. Matriz de 4.096
branches compara captura/replay/corpo anterior e trace; preflight inclui todas
as chaves/input/campos relevantes/versões/faixas antes da primeira escrita. Seis
falhas preservam tile parcial e invalidam captura. 1.024 tiles usam a mesma
observação em quatro workers com cursores locais. Sem execução integrada do
produtor/template/mods ou novo marco. Microexperimento DISABLED separado executado
explicitamente: não integra os 575 casos/CTest nem mede entrega; revela overhead
com resolver trivial, logo fita diagnóstica fora do hot path. Três fontes com
sintaxe Tracy aprovadas, sete objetos reais existentes conferidos sem reconstrução
forçada e link NoAssets aprovado; não é linking completo do jogo/Tracy.

O [catálogo compacto](../sector-runtime-h0-base-tile-lookup-context-2026-11-09.md)
passa **601 testes**, 29 grupos CTest e 117 casos afetados repetidos vinte vezes
(**2.340 execuções adicionais**), ASan/UBSan; 26 novos normativos. 4.096 branches
comparam catálogo/fita/corpo anterior e trace de captura/referência. Scope, chaves
limítrofes, capacidade sem inserção parcial, deduplicação/conflitos e recusa da
última chave antes de writes são exercitados; fallback escolhido pelo chamador
mantém a falha/estado parcial legados. Um catálogo serve 1.024 tiles distintos em
quatro workers. Microcusto novo DISABLED executado à parte, não dentro dos 601
ou CTest; dois experimentos ficam desativados por padrão. Sem jogo/assets/reload
ou throughput integrado; sintaxe Tracy de três fontes e objetos reais existentes
conferidos como na etapa anterior, não link completo. H0 continua parcial.

O [recorte de fontes/gerações](../sector-runtime-h0-lookup-source-generations-2026-11-09.md)
passa **629 testes**, 31 grupos CTest e 145 casos afetados repetidos vinte vezes
(**2.900 execuções adicionais**), ASan/UBSan; 28 novos. Testa clock/binding reais
com fontes controladas: nested/overlap/falhas/retire, 512 IDs concorrentes, saturação
local, troca/lifetime de fontes e Refused/Applied/Stale. Scope 2 exige sourceEpoch
e oito campos; a recusa não escreve e stale posterior exige descarte privado.
Root/Assets/MaterialDatabase compilados e dez objetos reais conferidos; seis
fontes com sintaxe Tracy aprovadas, não linking do jogo/Tracy. Não executou
reload/assets/mods reais, TSan ou benchmark. Dois microexperimentos seguem
DISABLED e não foram reexecutados. Catálogo continua fora do produtor normal.

O [recorte de leitores reais](../sector-runtime-h0-real-lookup-readers-2026-11-10.md)
passa **649 testes**, 32 grupos CTest e **165 × 20 = 3.300 execuções adicionais**
com ASan/UBSan; 20 novos casos normativos. Os corpos reais de WorldTemplate/
MaterialDatabase e a referência independente comparam trace, estado parcial nas
exceções, 4.096 combinações e arrays de 1.024 tiles com quatro workers. Layout
real substituído/revisionado; clocks parent/Assets controlados e definições de
materiais reais só com metadata de fixture. Não executou Root::reload/Assets,
terreno, mods ou streaming. Três microexperimentos DISABLED por padrão; somente
o novo de custo dos leitores foi executado à parte, conforme relatório, sem
asserção de timing ou inferência de setores/s. CPU-01 e gates integrados abertos.

O [recorte de lifecycle Assets](../sector-runtime-h0-assets-lifecycle-2026-11-10.md)
passa **666 testes**, **34 grupos CTest** e **182 × 20 = 3.640 execuções adicionais**,
ASan/UBSan. São 12 casos novos de Assets, quatro novos de ByteArray e um caso
ByteArray anterior agora também no alvo. Arquivos locais de fixture e Assets
real; RootBase só fornece Configuration, não substitui Root completo/listeners.
Reproduziu/corrigiu memset de fill vazio; repetição anterior parou com um worker
em futex, motivando correção de stop/wakeup e estresse de 32 ciclos por execução.
Não é prova TSan de todas as races. Os três microexperimentos ficaram desligados.
Raw assets precisam de input capturado: clearCache não avança a versão explícita.

## VAL-01 — Benchmark de streaming

Construir um harness que execute trajetória/interesse e ticks reprodutíveis,
com save inicial copiado, seed, mods e configuração fixos. Os benchmarks
existentes de `generateRegion` síncrono continuam úteis, mas não reproduzem
fila, TTL, fairness, prefetch e entrega durante exploração.

Separar medições de startup, setor inédito, setor salvo e revisita quente.
Classificar origem/nível inicial de cada setor para não contar pedido já
satisfeito como terreno novo. Incluir completions controladas para reproduzir
as esperas artificiais sem depender de uma CPU específica.

Executar três repetições ou mais por variante, mesma duração/distância e
estado inicial, registrando mediana e dispersão. Usar uma execução sem
profiling detalhado para throughput e outra instrumentada para diagnóstico.

## VAL-02 — Comparador de outputs e testes sem assets

Testes puros: identidade, exclusão de chave, 3×3, wrap, nós compartilhados,
resultado vazio, limite de memória, retorno invertido, justiça, deadline e
cancelamento. Cobrir explicitamente a interação continuação/`break`.

Colocações: múltiplas promises, compactação, ID ausente, nova geração, retry
e cumprimento único. Caches: alteração de template/assets, output antigo,
read set incompleto e bytes compartilhados. GPU: referência CPU, perda de
contexto e resultado tardio. IO: bytes antigos após write e promoção de carga.

Os testes devem verificar comportamento e outputs; duplicar internamente o
mesmo algoritmo num teste não comprova equivalência. Usar comparações de
referência, invariantes e sequências adversariais de eventos.

## VAL-03 — Métricas e marcos de entrega

| Marco/custo | O que deve registrar |
| --- | --- |
| Interesse | origem, sequência, prioridade, tempo e nível inicial |
| Preparação | reserva, início/fim, backend, bytes e input revision |
| Elegibilidade | pré-requisito que faltava e instante de liberação |
| Publicação | validação, custo exclusivo, efeitos e nível confirmado |
| Entrega | enqueue/envio/recepção/aplicação de pacote inicial |
| Saída da passagem | vazio, worker, dependência, política, etapas, deadline ou erro |
| Recursos | CPU útil, RAM única/em voo, VRAM, transferência e espera GPU |

Os nomes são requisitos de instrumentação futura, não contadores já
implementados. Reutilizar os existentes quando sua semântica corresponder.
Separar conclusão física do worker de primeiro polling, e instante de pacote
enfileirado de apresentação no cliente.

Perfis agregados por etapa preservam tempos inclusivos/exclusivos declarados.
Somar p95 de etapas não produz o p95 da entrega. Janelas com perda de eventos,
fallback de relatório ou truncamento declaram cobertura incompleta. O problema
`storage`/`BadVariantType` transitório continua pendente de causa raiz.

## VAL-04 — Matriz de cenários

| Cenário | Falha ou ganho procurado |
| --- | --- |
| Céu e terreno sem estrutura | throughput padrão e custo de planos vazios |
| TMX frio e repetido | materialização versus cache; aplicação separada |
| Mundo salvo/editado | nenhuma regeneração ou sobrescrita indevida |
| Mudança de direção/teleporte | cancelamento, bytes retidos e prefetch desperdiçado |
| Borda horizontal/vertical | wrap, regiões parciais e prioridade |
| Um/N workers e vários mundos | escalabilidade e orçamento processual |
| Pouca/muita RAM, GPU integrada/dedicada | pressão real e contagem correta de memória |
| GPU ausente/lenta/perdida | fallback completo, sem frame bloqueado por fence |
| Dois jogadores próximos/distantes | justiça, demanda compartilhada e transferências |
| Mantizi/Glitch e base FU | estruturas, callbacks, fios e inventários íntegros |
| Unload, sync, crash/restart | tickets, checkpoint e recuperação |

O backlog de oceanos/clima exige comparar outros líquidos e separar geração,
simulação e render. Não deduzir uma causa geral de uma amostra tóxica isolada.

## VAL-05 — Gates de adoção e relatório

Uma variante só avança quando possui: invariantes aprovados, comparação de
outputs, benchmark equivalente, memória limitada, falhas/recovery cobertas e
limites conhecidos. Definir em H0 os alvos quantitativos de throughput,
latência e resposta antes de medir a candidata; não ajustar a meta depois
apenas para declarar sucesso.

Mais CPU/RAM/GPU deve resultar em mais setores inéditos/s ou menor espera,
sem regressão inaceitável de update/frame, justiça ou integridade. A magnitude
de aceleração permanece uma hipótese até o benchmark.

Criar relatório versionado por teste material: objetivo, commits/build,
config, artefatos com nome/tamanho/data/hash, cenário, resultados, limitações,
decisão e próximo passo. Dados brutos permanecem fora do Git por padrão.
Selecionar a sessão correta dos ZIPs acumulados; não misturar runs anteriores.
Manter os relatórios por sessão já isolados e as políticas de rotação.

## Operação e retomada

Atualizar estado do item no [roadmap](roadmap.md), relatório ligado ao item e
checklist regional. Informar exatamente o que passou: syntax-only não é build
completa, teste sem assets não valida mod, e push não valida Actions.
Workflows continuam manuais. Nenhuma release ou merge em `main` decorre
automaticamente de um teste aprovado.

Melhoria esperada: regressões reproduzidas rapidamente, evidência rastreável
entre agentes e menos rodadas de teste manual para erros de escalonamento.
