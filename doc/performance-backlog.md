# Backlog de desempenho da exploração

## Prioridade de implementação — 09/11/2026

O trabalho está detalhado na [coleção do runtime de setores](sector-runtime/README.md),
com [roadmap H0–H5/R1–R2](sector-runtime/roadmap.md), IDs por subsistema,
propósito, melhoria esperada, dependências e aceitação. Por decisão do usuário,
esse é o **programa principal**, não uma proposta alternativa. Usar o roadmap
como fonte de estado e sequência; manter aqui a prioridade consolidada.

O [painel de progressão](sector-runtime/progress.md) registra os percentuais
auditáveis, ordem das 49 tarefas, fila imediata e evidências. Sua contagem é do
novo programa, não da conclusão do jogo ou de um ganho de desempenho medido.

Frente ativa: H0. A identidade das colocações já tem o primeiro recorte
implementado, com [relatório e testes](sector-runtime-h0-placement-identities-2026-11-08.md).
O [recorte seguinte](sector-runtime-h0-streaming-pass-2026-11-09.md) já
reproduz/corrige a interação de DAG-04 e inicia VAL-01/02 com um harness de
passagens sintéticas. São 26 testes de H0 aprovados; ganho real e equivalência
com mundo/mods continuam pendentes. O [recorte inicial de RAM-01](sector-runtime-h0-memory-admission-2026-11-09.md)
reserva arrays antes de copiar inputs, faz rollback pré-dispatch e preserva a
reserva após handoff. O [ledger processual seguinte](sector-runtime-h0-process-memory-2026-11-09.md)
já agrega reservas de arrays privados com lifetime independente do mundo.
O [recorte de estimativas v1](sector-runtime-h0-retained-estimates-2026-11-09.md)
mede templates compartilhados entre handles rastreados, metadados e resultados
variáveis, condicionado a profiling regional e separado das reservas de arrays.
São 115 testes de contratos e nove grupos CTest aprovados, sem ganho de runtime
alegado. Próximo: caches privados, containers excluídos, crescimento de outputs
e cobertura independente do profiling, antes de limites processuais
configuráveis; preservar MOD-01 e não ampliar admissão com números parciais.
O [recorte do cache privado](sector-runtime-h0-worker-cache-2026-11-09.md), `739067b1`,
acompanha o template privado/LRU sobrevivente ao job/mundo; 136 testes e dez
grupos CTest aprovados, sem ganho de runtime alegado. O
[recorte de exceção](sector-runtime-h0-allocator-exception-2026-11-09.md), `1bd348e6`,
reproduz/corrige a publicação antecipada do ponteiro do pool em exceção de
inserção: 144 testes, 800 execuções adicionais e onze grupos CTest aprovados.
Não demonstra OOM real nem vínculo com crashes anteriores. Próximo: reserva
antes de crescer outputs, caminhos síncronos e heaps de layout/biomas/seletores
ainda excluídos, antes de ampliar admissão. O
[recorte dos buffers](sector-runtime-h0-output-buffers-2026-11-09.md), `34c76ea3`,
implementa reserva dos buffers variáveis antes da alocação nos caminhos
síncrono/assíncrono e consumo dos planos sem cópia. São 167 testes, 460
execuções adicionais e doze grupos CTest aprovados. Não há ganho de jogo medido;
próximo: heaps aninhados/temporários, cobertura de caches e gates de
MOD-01/VAL-01/02 antes de maior admissão. O
[recorte dos inputs](sector-runtime-h0-distribution-bundles-2026-11-09.md), `887149c0`,
compartilha inputs de distribuição após uma cópia snapshot-equivalente e cobra
o backing conhecido uma vez. São 185 testes, 360 execuções adicionais e treze
grupos CTest com ASan/UBSan, incluindo sampler real e leitores concorrentes.
Modelo de estimativas 3, sem ganho real medido ou cota ampliada. Próximo:
nomes de saída/temporários, variants opacos e layout/biomas/seletores; benchmark
real e equivalência continuam gates para a ampliação do pipeline.
O [recorte de nomes/scratch](sector-runtime-h0-planning-storage-2026-11-09.md), `cad311d1`,
compartilha nomes de saída por planejamento e reutiliza scratch sem inputs
MicroDungeonNames. Preserva o histórico de cópias nos casos micro: controle
negativo de hash com wrap reproduz seleção diferente sem proteção. São 204
testes, 380 execuções adicionais e catorze grupos CTest com ASan/UBSan, modelo
4. Sem ganho real medido; nomes curtos/únicos podem custar mais. Próximo:
payloads de candidatos/temporários, variants opacos, comparador de candidatos/
IDs/prioridades e streaming real, antes de maior admissão.
O [recorte de payloads](sector-runtime-h0-candidate-payloads-2026-11-09.md), `25cecf95`,
acompanha backing conhecido de candidatos antes da factory/crescimento e até
destruição física; amplia campos públicos dos seis variants e preserva
cópias/capacidade/seleção. São 223 testes, 380 execuções adicionais e quinze
grupos CTest com ASan/UBSan; modelos geral 5/distribuições 2/payloads 1.
Sem teto profundo hard ou ganho real; RAM-01 segue em 25%. Próximo: listas
potenciais/scratch, Json/TileDamage privados e buckets/metadados do hash,
mantendo comparador e gates antes de ampliar admissão.
O [recorte dos temporários](sector-runtime-h0-temporary-storage-2026-11-09.md), `c4d7ddb3`,
acompanha listas potenciais, cópia por valor e scratch com quatro posições e
um token agregado por job. Adota originais após construir, reserva previsão
antes da cópia e preserva a transferência/growth. São 246 testes, 460 execuções
adicionais e dezesseis grupos CTest, ASan/UBSan; modelo geral 6/temporários 1.
Sem ganho real ou teto completo; RAM-01 segue em 25%. Próximo: Json/TileDamage
privados, buckets/metadados do hash e envelope de memória/pressão, mantendo
baseline integrado e comparador antes de ampliar concorrência.
O [recorte dos containers](sector-runtime-h0-container-storage-2026-11-09.md), `730e36ba`,
inclui buckets completos de nomes e pares ocupados dos fatores de dano,
sem mudar layout/allocators/cópias/ordem/geração. São 266 testes, 400 execuções
adicionais e dezessete grupos CTest, ASan/UBSan. Modelos geral 7/distribuições 3/
payloads 2/temporários 2. RAM-01/H0 seguem em 25%; próximo: Json compartilhado,
bookkeeping/allocator dos nós e envelope processual/pressão. Não é teto profundo
completo nem ganho de setores/s medido; preservar os gates de equivalência.
O [relatório vigente](sector-runtime-h0-json-storage-2026-11-09.md), `45db2402`,
inclui buckets completos/chaves de JsonObject por owner, sem alterar input,
serialização, representação ou cópias. Objeto truncado é omitido da cobrança,
não registrado com tamanho parcial conflitante. São 294 testes, 560 execuções
adicionais e dezoito grupos CTest, ASan/UBSan; modelos geral 8/Json 2.
Coleta Json segue diagnóstica, não cobre Json dos variants ou nós/allocator.
RAM-01/H0 em 25%; próximo: esses donos e envelope/pressão processuais antes
de maior admissão. Comparador e baseline integrado continuam necessários.
Achado de CPU-01: overflow assinado na seed automática de `StarRandom.cpp:197`,
observado em execução estrita de sanitizers; RNG de produção permanece
inalterado. A reprodução e a separação dos testes de hash com seeds privadas
fixas estão no relatório de admissão; não considerar essa pendência resolvida.
A auditoria anterior também registra metadata escalar não inicializada na cópia
herdada de Perlin `Uninitialized`; a consulta de bytes só lê ponteiros.
Auditar antes de novos consumidores dessa metadata em CPU-01.
Os demais componentes do novo executor continuam pendentes.

A direção de arquitetura vigente é o
[pipeline orientado a CPU, RAM e GPU](sector-generation-hardware-design-2026-11-08.md),
revisado para o cenário de hardware com folga declarado pelo usuário. A
política de alto throughput deve ampliar preparação antecipada, residência de
resultados e uso dos backends compatíveis conforme o ganho de entrega.

A [auditoria completa do pipeline de setores](sector-generation-architecture-audit-2026-11-08.md)
fundamenta o rework. Priorizar entrega de setores padrão: na run 29,
houve orçamento médio de 27,982 ms para passagens que usaram só 1,935 ms,
e BaseTiles pronto aguardou em média 66,660 ms até commit. A frente TMX trata
outliers de estruturas e não deve bloquear esta prioridade.

Sequência do programa principal, ainda sem implementação do novo executor:

1. IDs estáveis para colocações pendentes, benchmark de streaming e cobertura
   da interação entre continuação e o encerramento antecipado do último preview.
2. Planejamento de candidatos por template/revisão sem cópias do halo que a
   seleção atual não lê; depois compartilhar cálculo privado com BaseTiles.
3. Grafo persistente de dependências, primeiro em sombra, antecipando preparação
   e deduplicando auxiliares entre pedidos; manter ordem autoritativa de efeitos.
4. Mailbox de conclusões e entrega por tarefas elegíveis dentro do orçamento,
   com limites de memória/justiça e prioridade correta no wrap do planeta.
5. Integrar IO/decode salvo e prefetch previsto; extrair kernels puros de
   CaveLiquid e Finalize depois de seus contratos de validação.

Essa sequência agora compõe H0–H3 da revisão de hardware. Acrescentar H4/H5:
backend GPU opcional com fila assíncrona, buffers/fences, kernel CPU de
referência, comparação em sombra e seleção CPU/GPU por custo completo.
O protótipo GPU pode avançar em paralelo ao grafo e às caches; a publicação
autoritativa depende de equivalência aprovada. Hardware sem compute e servidor
dedicado preservam execução CPU completa.

RAM passa a ter papel explícito: campos de terreno/bioma, planos vazios,
produtos `PreparedSector`, tiles salvos decodificados e staging versionado,
com orçamento processual e contagem de bytes únicos. Workers e lotes deixam de
ser metas fixas: o controlador futuro usa capacidade utilizável, fila
executável, demanda prevista e pressão observada. Os novos nomes do desenho
não são chaves de configuração já disponíveis no runtime.

Mods e saves seguem como critérios de bloqueio. NPC externo permanece suspenso
e Linux Clang continua adiado. Nenhum ganho multiplicativo é alegado sem A/B
reprodutível; não ativar prewarm como padrão.

## Prioridade histórica — 03/11/2026 (subordinada ao programa acima)

Por decisão do usuário, trabalhar primeiro no threading por setores, conforme
o [plano de threads dedicadas](world-region-rework-advanced-concept.md).
A externalização de NPCs está suspensa: não iniciar novas implementações,
investigações específicas ou ativações dessa frente até retomada explícita.
Os planos e protótipos existentes ficam preservados como histórico.
Interações e transferência de entidades continuam sendo requisitos de
correção do threading por setores, sem reabrir um serviço externo de IA.

As [evidências dos 41 logs](log-audit-2026-11-03-deep.md) orientam o trabalho
de progresso das etapas de geração, dependências e finalização dos setores.
O [recorte de progresso e contratos por etapa](sector-scheduler-progress-plan.md)
reconstrói os setores presos e define a sequência: identidade de pedido,
seleção com oportunidade para pedidos antigos e pré-requisitos, validação de
conflitos e execução pelas threads donas. Identidade independente do HUD,
comparação observacional e promoção experimental limitada do pedido mais antigo
estão implementadas. A próxima camada também pode promover uma etapa
executável da árvore de dependências que já esteja na fila; a validação em jogo
continua pendente.

As hipóteses de rework discutidas para setores de 64 × 64, dungeons compostas,
pré-geração e cálculo paralelo de microdungeons estão em
[world-generation-rework-hypotheses.md](world-generation-rework-hypotheses.md).
O [plano conceitual avançado de regiões](world-region-rework-advanced-concept.md)
organiza reconstruir a execução do mundo com propriedade por regiões de setores.
A [fundação inicial](world-region-foundation.md), na branch `Experimental`,
integra identidade e registro de tarefas ao pipeline BaseTiles e adiciona a
partição lógica 2×2. O [coordenador em sombra](world-region-coordinator.md)
agora captura metadados limitados, reserva turnos exclusivos e revalida revisões
no update seguinte. Grafo de etapas, orçamento global, propriedade/migração de
entidades e runtime regional autoritativo continuam pendentes.
O plano de instrumentação por linha do tempo está em
[tracy-integration-plan.md](tracy-integration-plan.md).
O painel local do HUD e suas métricas estão em
[performance-overlay.md](performance-overlay.md).
O painel sob demanda de threads e pools está em
[thread-panel.md](thread-panel.md).
As pesquisas de operações estão separadas entre
[IA dos NPCs](npc-ai-parallelism-hypotheses.md) e
[objetos e roteadores](object-operations.md).
O [plano de ação de rework dos NPCs](npc-ai-rework-action-plan.md) organiza
as fases e seus critérios de aprovação. O primeiro marco é medir, isolar
leituras e comparar cálculos paralelos sem aplicar efeitos (fases 0–2);
a fase 0 recebeu [instrumentação detalhada de NPCs](npc-profiling.md) e
[auditoria estática da fronteira Lua](npc-lua-boundary-audit.md). As fases 1–2
agora possuem um [protótipo de snapshot e busca sombra](npc-path-shadow.md)
desligado por padrão; baseline, validação e aprovação continuam pendentes.
As fases 3–7 ainda não foram iniciadas.
A [ponte diagnóstica de atores](npc-actor-bridge.md) já permite observar
sinais da rede em uma thread de NPCs por mundo, sem esperar o tick para
recebê-los. Ela ainda não move Lua ou IA e continua desligada por padrão.
O [modelo conceitual de externalização](npc-externalization-concept.md)
define perfis legado, busca de rota em worker e Actor v1 opt-in, incluindo a
API para NPCs customizados que reutilizam ações vanilla.
O plano de workers e API para modders desses objetos está em
[object-worker-architecture-plan.md](object-worker-architecture-plan.md).
Novas dependências e ferramentas seguem a política de
[procedência de código externo](dependency-sources.md); suas pendências de CI
estão listadas nesse documento.

## Estado em 24/10/2026

Branch de trabalho: `perf/threading-audit`. A build Windows #21 compilou o
commit `70a4977f` e produziu o instalador. Ainda falta testar no jogo a fila de
prefetch de tiles básicos, introduzida em `3f848041`. `sectorWorkers` precisa
ser maior que zero para que o prefetch seja usado; começar com `2`.

O log anterior à fila de prefetch mostrou `generation.timeBudgetExhausted` em
algumas janelas, mas não mediu o tempo de espera de cada setor. Portanto, não
atribuir a demora do vilarejo Mantizi ao orçamento sem a próxima coleta.

## Atualização de evidência — 26/10/2026 (build anterior 6bc48199)

O log da build `6bc48199` reproduziu a fila travada no mesmo planeta, agora
com as métricas detalhadas. Houve espera concluída de 40,6 s e pedido ainda
pendente com 60,0 s. Entre 1.295 resultados de tiles básicos coletados, 1.115
foram invalidados por mudança na revisão do setor; 2.272 resultados prontos
foram descartados para abrir vagas no limite de tarefas pendentes. O pool
tinha dois workers e aparecia sem trabalho nas amostras, enquanto a fila de
setores continuava longa. Ver [dados e limites](performance-baseline.md).

**Prioridade naquela etapa:** localizar os caminhos que incrementam a revisão dos
setores pendentes e medir coordenadas de resultados invalidados/descartados.
Depois, avaliar uma política que preserve resultados prontos ainda solicitados
até sua aplicação ou invalidação, com limite de memória explícito. Repetir o
mesmo snapshot e rota para comparar espera, tentativas refeitas e integridade
dos tiles. Aumentar o número de workers ou o orçamento por tick não é a
primeira mudança indicada por este registro.
A [investigação de código](sector-generation-stall-investigation.md) encontrou
um caminho de consulta de colisão que eleva a revisão mesmo sem alteração
persistente; a participação desse caminho nos eventos do log ainda precisa
ser medida antes de modificar a regra de invalidação.
A instrumentação de atribuição e coordenadas foi adicionada à branch;
**naquela etapa pendiam build e nova coleta** com o mesmo snapshot.
A coleta da build #27 abaixo atualiza esse estado. Só usar os dados dessa
coleta para decidir a correção da revisão e do descarte.

## Próxima coleta: vilarejo Mantizi

1. Guardar cópia do jogador e universo antes de entrar na região inédita.
2. Usar a build #21 com os mesmos mods, resolução, zoom e configuração de
   `sectorWorkers` da coleta anterior. Ativar `explorationProfiling` conforme
   [exploration-profiling.md](exploration-profiling.md).
3. Registrar vídeo e horário aproximado de entrada no vilarejo, primeira área
   incompleta e momento em que a estrutura fica inteira.
4. Enviar o `starbound.log` completo e informar se o setor visitado já tinha
   sido gerado antes. Para comparar duas builds, restaurar a mesma cópia de
   universo antes de cada execução.

Ler `generation.sectorWait` e `generation.oldestQueuedAge` junto com
`generation.queueDepthTotal / generation.queueDepthSamples`. Conferir
`generation.prefetchCompleted`, `generation.baseTiles.worker`,
`generation.microDungeons`, `generation.finalize` e
`generation.timeBudgetExhausted`. Um contador de prefetch alto sozinho não
prova que a estrutura ficou pronta antes: ele cobre só os tiles básicos.

## Decisões após a coleta

| Evidência | Próxima mudança | Critério de aceitação |
| --- | --- | --- |
| Espera alta com tiles básicos ainda pendentes | Limitar e ordenar o prefetch por distância, medir pedidos, descartes e tempo de conclusão | Reduzir `generation.sectorWait` na mesma rota sem elevar atrasos dos setores visíveis |
| Tiles básicos prontos, espera concentrada em `microDungeons` ou `finalize` | Priorizar a conclusão de setores visíveis já iniciados; preservar dependências entre vizinhos | Estruturas aparecem antes, sem tiles faltantes, alterações de dungeon ou divergência ao recarregar |
| Limite de tempo atingido com fila visível longa | Avaliar orçamento adaptativo com teto por fidelidade e folga do frame | Reduzir espera sem aumentar picos perceptíveis no cliente e na thread do mundo |
| `painter.environment` domina mesmo em área já gerada | Instrumentar suas subetapas antes de alterar iluminação | Identificar custo dominante e preservar aparência em mundos e mods |

## Trabalho posterior

- Investigar pathfinding de NPC com snapshot imutável, conforme
  [world-threading.md](world-threading.md). Lua, movimento, dano e escrita do
  mundo continuam na thread do mundo até existir uma fronteira segura.
- **Pesquisa futura de alto risco e alta complexidade:** avaliar a externalização
  completa do update dos NPCs para workers. O acesso direto e simultâneo ao
  estado vivo do mundo não garante leituras consistentes; um protótipo exigiria
  uma visão coerente por tick, comandos de efeito aplicados em ordem pela thread
  do mundo e regras para resultados obsoletos. Antes de considerar a migração,
  mapear todas as chamadas de IA e Lua que leem ou modificam terreno, objetos e
  outras entidades; medir contenção, custo de snapshots e descartes; demonstrar
  equivalência de comportamento para mods, multiplayer, persistência e ordem de
  eventos. Não tratar este item como extensão automática da fila de pathfinding.
  Histórico, evidências e desenho hipotético em
  [npc-ai-parallelism-hypotheses.md](npc-ai-parallelism-hypotheses.md).
- Medir separadamente inicialização e callbacks Lua de entidades; o contador
  atual `luaTotalMilliseconds` cobre apenas o `update` periódico.
- Considerar instalador comum sem PDBs e símbolos em artefato separado para
  reduzir tempo de empacotamento e tamanho do download. Medir o ganho real
  antes de alterar a distribuição.

Em qualquer mudança de geração, conferir recarregamento do mesmo mundo,
estruturas e iluminação, NPCs, Frackin' Universe e multiplayer antes de
considerar a otimização concluída.

## Instrumentação preparatória regional

Implementada a [coleta limitada de dependências e custo espacial](regional-dependency-profiling.md),
desligada por padrão. A build #27 já compilou e coletou dados; próximo passo:
comparar alvos compartilhados/separados, medir overhead e melhorar a agregação
diante dos descartes registrados. O mapa completo de
recursos/topologia e os medidores do futuro runtime regional continuam pendentes.


## Plano avançado: geração de setores e mundo regional

Detalhamento e critérios no [backlog WRG-01 a WRG-12](world-region-rework-advanced-concept.md#84-itens-executáveis-do-backlog).
A solicitação de planejamento mantém a reconstrução regional no backlog;
a solicitação posterior autoriza iniciar as correções de invalidação/retenção
no modelo atual e tratar containers/ITDs em uma frente separada.

A coleta da build #27 (`defb650f`) trouxe evidências separadas por planeta:
no mundo `383738425:-48324202:-255713414:2`, espera concluída de 36,11 s,
1.041 resultados rejeitados por revisão e 835 resultados prontos descartados.
As revisões rejeitadas acumulam principalmente consultas/cache de colisão.
No mundo `383737864:-48324405:61106789:8`, não houve rejeições por revisão,
mas objetos consumiram 79,75 s e um ITD demorou 685,63 ms num update.
Fonte, limitações e métricas completas estão na seção 8.1 do plano.

Ordem proposta:

1. WRG-01–04: separar métricas por mundo, revisões de conteúdo/cache,
   identidade de jobs e retenção limitada de resultados úteis.
2. WRG-05–08: dependências por etapas, progresso garantido sob condições
   explícitas, regiões em sombra, estruturas por footprint e carga de saves.
3. WRG-09–12: donos regionais paralelos, eliminação gradual da barreira global,
   APIs de objetos/NPCs e ajuste de escala por hardware.

O alvo de máximo paralelismo é execução simultânea de tarefas independentes
com validação local. A etapa inicial com barreira global não garante isolamento
de uma região lenta. Setores físicos continuam 32×32; 64×64 lógico é candidato
de agrupamento. O caminho legado preserva a API de mods não adaptados.

A coleta regional já foi compilada no Windows e utilizada neste log. A perda
de 1.371.739 chamadas de dependência por limite de cardinalidade exige melhorar
a agregação antes de inferir independência entre redes. Não ampliar o limite
sem medir memória, volume de log e custo da coleta.

### Recorte autorizado após o planejamento

Implementada a [correção inicial de setores](sector-generation-stall-investigation.md#correção-inicial-após-a-coleta-da-build-27):
acesso separado ao cache de colisão, drenagem de BaseTiles prontos dentro do
orçamento, admissão sem expulsar resultados úteis e chave pendente preservada
até término após unload. É um recorte de WRG-02–04 no modelo atual; orçamento
global em bytes, grafo de dependências e runtime regional continuam pendentes.
Os objetos Linux das três unidades centrais compilaram; falta validar a build
Windows e repetir a rota. Não considerar WRG-02–04 concluídos antes dessa validação.

A frente de containers recebeu temporizadores por API e fases de conversão,
mutação e retorno, ativados por `regionalDetails`. Compilação parcial Linux
concluída; coleta Windows pendente. Reescrita de inventários aguarda esse
[detalhamento](object-operations.md#frente-de-containers-após-a-coleta-windows-27).

### Atualização após a coleta de `c0da7a4b`

O detalhamento atribuiu 23,61 dos 25,25 s de `containerPutItemsAt` à criação
do item a partir do JSON. `ItemDatabase::fromJson` passou a reutilizar o cache
de protótipos existente e devolver um clone independente. As unidades de
produção afetadas compilam no Linux; a build e a medição Windows permanecem
pendentes.

No mundo `…:3`, 391 resultados foram aplicados e só um ficou obsoleto, mas a
espera chegou a 17,45 s com orçamento e vizinhos bloqueando a fila. Foram
adicionadas métricas por passagem visível para decidir entre grafo de
dependências, prioridade de etapas iniciadas e eventual orçamento adaptativo.
Nenhum valor de orçamento foi alterado nesta etapa.

### Coleta `ceb0017b`: dungeons compostas e espera da fila

No mundo `383738425:-48324202:-255713414:5`, 1.766 passagens da fila
visível foram medidas. Destas, 200 não avançaram nenhum nível e 1.108
avançaram níveis sem concluir setor; 487 setores foram concluídos. A espera
máxima concluída chegou a 9,4 s. Os 1.718 resultados BaseTiles prontos foram
aplicados, sem descarte por revisão. A fila média era de cerca de 58 setores
quando amostrada. Esta rota difere do planeta `…:3` da coleta anterior.

Sete gerações de dungeon composta somaram 5,88 s, com pico de 1,66 s;
`dungeon.flush` somou 3,70 s. Dentro dele, criação de entidades somou 1,55 s
e colocação de objetos 1,41 s. Essas dungeons são colocadas na criação inicial
do mundo por `WorldServer::init`; o custo não deve ser atribuído integralmente
à fila de setores explorados depois. A geração de microdungeons acumulou
10,88 s e ainda executa na thread do mundo.

O próximo passo seguro é medir os volumes de objetos, veículos, árvores, itens
de bioma, NPCs e stagehands colocados no `flush`, para separar custo por
entidade de custo fixo. Contadores agregados foram acrescentados sob
`explorationProfiling.enabled`, sem registrar payloads ou IDs. Depois da
coleta, investigar cache de definição e criação de variantes repetidas; só
propor divisão entre ticks quando houver plano para persistência de operação
incompleta e recuperação após interrupção.

Para uma divisão futura, manter o footprint inteiro reservado e indisponível
para geração concorrente até o commit. O plano deve conservar ordem das
partes, RNG, IDs, fios, líquidos e callbacks; aplicar efeitos na thread do
mundo; persistir identidade e estágio da operação; e retomar ou desfazer de
forma determinística após save/reload. Um orçamento entre operações completas
não resolve o pico interno de 1,66 s observado aqui.

O protocolo de estados, publicação e retomada está detalhado no
[plano de dungeons interrompíveis](dungeon-generation-resume-plan.md).
As consultas e construções repetidas no `flush` foram mapeadas na
[auditoria de criação](dungeon-flush-creation-audit.md); a coleta de volumes
deve preceder mudanças no cache de NPCs ou objetos.
