# Roadmap, dependências e critérios de conclusão

[Índice](README.md) · [Baseline](baseline.md) · [Validação](validation-and-telemetry.md)

O [painel de progressão](progress.md) mantém os percentuais, a ordem das 49
tarefas, a fila imediata e suas evidências. Sua escala de marcos não substitui
os critérios técnicos/dependências deste roadmap nem mede horas ou aceleração.

## Estado e regra de progresso

Este é o **plano principal de execução**, adotado em 08/11/2026. H0 é a frente
ativa; listas anteriores não estabelecem uma sequência concorrente.

Os itens abaixo estão **pendentes**, salvo quando o estado e um relatório
informarem implementação e validação. Os fundamentos atuais
estão no baseline; os previews existentes não equivalem ao executor proposto.
Este roadmap detalha H0–H5 do
[desenho de hardware](../sector-generation-hardware-design-2026-11-08.md).

Cada ID é definido no seu módulo. Não criar outro contrato para o mesmo ID em
um relatório. Registrar avanço com estado, commit, evidência e limitação.
Estados: pendente → implementação → revisão/testes → validado no escopo →
elegível para adoção. Um bloqueio explicita dependência e próximo passo.

## H0 — Baseline automatizado e invariantes

Propósito: reproduzir o problema rapidamente e impedir amplificação de uma
falha ao aumentar paralelismo.

- [ ] VAL-01: benchmark de streaming com rota/save/config fixos.
- [ ] VAL-02: comparador de outputs e controle de conclusão de produtores.
- [ ] DAG-01: IDs estáveis para colocações e cumprimento único de promises.
- [ ] DAG-04: teste e correção da interação continuação/avanço/encerramento.
- [ ] RAM-01: contrato de reserva/bytes reais antes de ampliar admissão.
- [ ] MOD-01: fronteira inicial de efeitos e inputs puros documentada no código.

### Andamento de H0

DAG-01: **implementado e validado no escopo de testes sem assets**, para a
identidade das colocações pendentes. Os caminhos síncrono e assíncrono usam o
mesmo registro; a aceitação completa continua aberta até validar geração com
assets/mods e integrar o comparador de outputs. Ver
[relatório e limites](../sector-runtime-h0-placement-identities-2026-11-08.md).
Não há nova API de cancelamento de colocações ou executor de grafo nesta entrega.

DAG-04, recorte de continuação/avanço: **implementado e validado sem assets**
em `2131ead3`. A continuação deixa de interromper um avanço visível permitido;
cada candidato externo é tentado no máximo uma vez por passagem com o preview
ativo, e a primeira continuação preserva seu follow-up. O caminho legado
continua sem a nova seleção. VAL-01/02 recebem um harness determinístico de
passagens com produtos sintéticos, relógio virtual e produtores controlados:
18 testes novos, incluindo 192 combinações; 26 testes de H0 aprovados no total.
Ver [relatório e limites](../sector-runtime-h0-streaming-pass-2026-11-09.md).
Isso não conclui VAL-01/02 nem a mailbox executiva de DAG-04: falta executar
o streaming real de `WorldStorage`, comparar outputs de mundo/mods e medir A/B.

RAM-01, recorte inicial em `5c3c4c8f`: reserva antes das cópias de BaseTiles/halo,
rollback de preparação e registro da pendência antes do dispatch implementados.
Os limites anteriores são mantidos; bytes de arrays não representam toda
a RAM do pipeline. A build CMake dos contratos passou 75 testes, além de sete
casos de hash selecionados e sete grupos CTest, com sanitizers estritos.
Uma sentinela nula inválida foi corrigida; o overflow do RNG automático foi
registrado para CPU-01, não corrigido. Ver
[relatório da etapa e limites](../sector-runtime-h0-memory-admission-2026-11-09.md).

RAM-01, segundo recorte em `65bae55c`: ledger processual observacional de arrays privados,
com reserva única por wrapper e retenção após fechar o mundo, integrado a
BaseTiles/halo. Inventário de produtos/caches e limites está em
[memória](memory-and-prefetch.md); o
[relatório da etapa](../sector-runtime-h0-process-memory-2026-11-09.md) delimita
validação e contadores. São 20 testes novos, 95 testes de contratos no total,
400 repetições adicionais aprovadas e oito grupos CTest, com sanitizers
estritos. O objeto real de geração também compilou. Isso ainda não impõe
teto global nem mede toda a RAM.

RAM-01, terceiro recorte em `803d4cf7`: estimativas v1 de Json compartilhado,
metadados e resultados variáveis implementadas, com deduplicação por dono do
storage entre handles rastreados. A coleta adicional exige profiling regional;
não é RSS, cobertura integral nem reserva antes do crescimento do output.
Ver [relatório da etapa](../sector-runtime-h0-retained-estimates-2026-11-09.md).
São 20 testes novos, 115 no total, 400 execuções adicionais e nove grupos
CTest aprovados com ASan/UBSan; objeto real de geração compilado.
Nenhum limite, default, seed ou regra de publicação mudou.

RAM-01, quarto recorte em `739067b1`: cache privado por worker rastreado até
substituição/saída da thread, com reserva do objeto e atualização do storage
solicitado da LRU nas fronteiras de job, mesmo sem profiling. Inclui buckets,
blocos retidos e bookkeeping do pool, não todo o grafo do template nem picos
transitórios. São 21 casos novos, 136 no total, 420 execuções adicionais e
dez grupos CTest com ASan/UBSan; dois objetos reais compilados. Ver
[relatório da etapa](../sector-runtime-h0-worker-cache-2026-11-09.md).

RAM-01, falha do pool corrigida em `1bd348e6`: controle negativo com dois testes
reproduziu seleção fora dos blocos possuídos após exceção. A publicação de
`unfilledBlock` agora ocorre depois da inserção bem-sucedida. Oito casos novos,
144 no total, 800 execuções adicionais e onze grupos CTest passaram com
ASan/UBSan, incluindo recuperação do pool no próximo job. Ver
[relatório da etapa](../sector-runtime-h0-allocator-exception-2026-11-09.md).
Isso não é ensaio de OOM real, ganho de throughput ou aumento de admissão.

RAM-01, buffers variáveis em `34c76ea3`: pedidos dos vetores de candidatos e
planos reservados antes do raw allocator, com sobreposição antigo/novo e
retenção por alocação. Integra caminhos síncrono/assíncrono sem depender de
profiling. Pré-reserva dos descritores e handle da promise eliminam crescimento
durante construção e cópia no consumo pela dona; payload preparado é move-only.
São 23 casos novos, 167 no total, 460 execuções adicionais e doze grupos CTest
aprovados com ASan/UBSan. Ver
[relatório da etapa](../sector-runtime-h0-output-buffers-2026-11-09.md).
Modelo de estimativas 2 separa buffers reservados de heaps ainda aproximados.
Não há teto novo ou ganho ponta a ponta medido.

RAM-01, inputs de distribuição em `887149c0`: primeira captura copia uma vez
para preservar normalização/ordem de hash e seleção; capturas seguintes
compartilham o bundle const. Original adotado e backing conhecido rastreados
sem profiling; buffers do bundle reservados antes de alocar. Estimativas 3,
sem teto hard para heaps profundos. São 18 casos novos, 185 no total, 360
execuções adicionais e treze grupos CTest com ASan/UBSan, incluindo sampler
real aleatório/periódico e oito leitores concorrentes. Ver
[relatório da etapa](../sector-runtime-h0-distribution-bundles-2026-11-09.md).
MOD-01 recebe auditoria restrita dos leitores desse bundle, não aprovação de
todo o caminho com assets/mods. Sem ganho no jogo medido.

RAM-01, nomes de saída/temporários em `cad311d1`: nomes imutáveis compartilhados
por planejamento, com índice/objetos reservados e backing estimado independente
do profiling. Scratch reutilizado somente sem MicroDungeonNames; controle
negativo confirma que evitar cópias de realocação pode mudar escolhas em
clusters de hash com wrap. Modelo 4, 19 casos novos, 204 no total, 380 execuções
adicionais e catorze grupos CTest aprovados com ASan/UBSan. Ver
[relatório da etapa](../sector-runtime-h0-planning-storage-2026-11-09.md).
Não conclui temporários, MOD-01 ou benchmark: nomes curtos/únicos podem pagar
mais overhead, e inputs micro preservam o crescimento legado por compatibilidade.

RAM-01, backing conhecido dos candidatos em `25cecf95`: previsão agregada antes
da factory e sobreposição durante crescimento, rollback/retry e retenção até
destruir o vetor. Não muda crescimento, comparator ou cópias; comparação nativa
com clusters de hash com wrap preserva escolhas. Amplia campos públicos dos
seis variants sem percorrer Json/privados. São 19 casos novos, 223 no total,
380 execuções adicionais e quinze grupos CTest, ASan/UBSan. Ver
[relatório da etapa](../sector-runtime-h0-candidate-payloads-2026-11-09.md).
Modelos geral 5/distribuições 2/payloads 1; incompletude de reconciliação é
diagnóstico, não um teto hard profundo. RAM-01 segue em 25% no painel.

RAM-01, temporários em `c4d7ddb3`: listas potenciais originais adotadas após
construção/população, previsão da cópia/saída antes da cópia e scratch retido
até sua destruição. Um tracker local de quatro posições conserva os donos
ambiental/enfileirado e o lifetime na transferência. São 23 casos novos,
246 no total, 460 execuções adicionais e dezesseis grupos CTest, ASan/UBSan;
432 candidatos nativos comparados com o caminho anterior. Ver
[relatório da etapa](../sector-runtime-h0-temporary-storage-2026-11-09.md).
Modelos geral 6/temporários 1; demais fórmulas mantidas. RAM-01 segue em 25%:
não é reserva inicial de todos os originais, teto hard profundo ou benchmark.

RAM-01, containers em `730e36ba`: consulta const do vetor de buckets de nomes
inclui vazios/sentinela/padding e substitui a parcela de objetos ocupados,
sem duplicá-la. Fatores de dano acrescentam os pares ocupados, não o nó completo.
Sem mudar layout/allocators/cópias/ordem; 20 casos novos, 266 no total, 400
execuções adicionais e dezessete grupos CTest, ASan/UBSan. Ver
[relatório da etapa](../sector-runtime-h0-container-storage-2026-11-09.md).
Modelos geral 7/distribuições 3/payloads 2/temporários 2. Json, overhead de
nós/allocator e teto processual continuam pendentes; RAM-01/H0 seguem em 25%.

RAM-01, Json em `45db2402`: buckets completos e capacidades UTF-8 das chaves
por owner rastreado; objeto sem allowance para todas as chaves é omitido da
cobrança, sem conflitar com captura completa posterior. Preserva input e
representação; coleta continua diagnóstica. São 28 casos novos, 294 no total,
560 execuções adicionais e dezoito grupos CTest, ASan/UBSan. Ver
[relatório da etapa](../sector-runtime-h0-json-storage-2026-11-09.md).
Modelos geral 8/Json 2; versões próprias de distribuição/candidatos/temporários
mantidas. Json dos variants e nós/allocator continuam fora; RAM-01/H0 em 25%.

RAM-01, Json dos variants enfileirados em `49a12fbf`: coleta dos campos sem
copiar itens, floresta limitada/deduplicada com template e transferência do
mesmo handle original→bundle. Mantém lifetime em retirada da fila, falha/retry
e workers após fechamento do facade; caminho sem heap não aloca tracker vazio.
São 26 casos novos, 320 no total, 520 execuções adicionais e dezenove grupos
CTest, ASan/UBSan. Ver
[relatório da etapa](../sector-runtime-h0-variant-json-2026-11-09.md).
Modelos geral 9/Json 2/Json enfileirado 1; fórmulas próprias anteriores mantidas.
Coleta diagnóstica, não cobertura geral do Json ambiental/privado ou dos nós.
RAM-01/H0 seguem em 25%, sem ganho de setores/s demonstrado.

RAM-01, envelope em `ada755bf`/`0915af25`: schema das 14 classes, somas/coerência verificadas
e política isolada de pressão com histerese/headroom por demanda. Runtime só
amostra o envelope, sem usar a política para admitir/cancelar tarefas. São 34
casos novos, 354 no total, 680 execuções adicionais e vinte grupos CTest,
ASan/UBSan. Ver [relatório da etapa](../sector-runtime-h0-memory-envelope-2026-11-09.md)
e [contrato de cobertura/exclusões](../sector-runtime-memory-envelope.md).
Entradas inválidas preservam a última pressão válida após dois controles de
regressão. Envelope modelo 1, fórmulas anteriores mantidas. RAM-01/H0 seguem em 25%:
não é orçamento hard do motor, governador processual ativo ou ganho medido.

RAM-01, admissão atômica em `4b5aed18`: novo ledger opt-in verifica/cobra sob
o mesmo mutex em reserve, Json novo e crescimento de estimativas. Fachadas
compartilham orçamento/histerese; tokens/allocators preservam demanda. São 34
casos novos, 388 no total, 680 execuções adicionais e 21 grupos CTest, ASan/UBSan,
incluindo disputas soft/hard, conselho obsoleto, overlap e rollback. Ver
[relatório daquela etapa](../sector-runtime-h0-atomic-memory-2026-11-09.md).
Singleton do jogo continua com pressão desligada, sem config/defaults novos;
admissão modelo 1, RAM-01/H0 em 25%. Headroom testado não prova todo DAG.

RAM-01, reserva composta em `dd59f3b4`: batch atômico sem prefixo admitido e
primeiro consumidor BaseTiles, com array in place + parcela inicial do template
antes do dispatch. Cache consome reserva, libera previsão no hit e mantém overlap
do antigo; ledger/demanda fazem parte de sua identidade. São 32 casos novos,
420 no total, 640 execuções adicionais e 22 grupos CTest com ASan/UBSan. Ver
[relatório daquela etapa](../sector-runtime-h0-composite-memory-2026-11-09.md).
Conclusão em hard provada somente para produtor controlado com demanda limitada;
controle negativo expõe crescimento não previsto. Singleton desligado, modelos
mantidos e RAM-01/H0 em 25%; não é garantia de progresso do BaseTiles real/DAG.

RAM-01, saída variável em `8b68ab6a`: admissão tipada Admitted/Deferred/Impossible/
Invalid e protocolo owner-thread limitado para produtores puros. Fences dos
tokens impedem nova tentativa com recursos anteriores vivos; revisões úteis
evitam reconsultar Deferred a cada poll, sem esperar por budget no worker.
Fixture de dois jobs com resultados retidos progride e preserva bytes de saída;
exceções continuam falhas. São 32 novos casos, 452 no total, 640 execuções
adicionais e 23 grupos CTest com ASan/UBSan. Ver
[relatório daquela etapa](../sector-runtime-h0-variable-memory-2026-11-09.md).
Retry ainda sem consumidor real, sem evento automático ou allocator de crédito;
modelo do protocolo 1, anteriores mantidos. Pressão padrão desligada; RAM-01/H0
em 25%. Progresso do produtor controlado não prova todo DAG ou kernel real.

RAM-01/MOD-01/VAL-02, capacidade/subkernel em `8e190742`: release/shrink sinalizam
revisão do ledger, capturada com a recusa e consumida por admit sem argumento
(retry modelo 2). Sem wakeup/mailbox ou consumidor real de retry. Subkernel
escalar de líquido modelo 1 é usado em BaseTiles e comparado à lógica anterior;
não versiona todo o produtor. São 32 novos casos, 484 no total, 1.280 execuções
adicionais e 25 grupos CTest com ASan/UBSan. Ver
[relatório daquela etapa](../sector-runtime-h0-capacity-kernel-2026-11-09.md).
Pressão desligada, RAM-01/H0 em 25%; os contratos amplos continuam parciais.

Identidade/comparação em `f7785963`: BaseTiles real devolve ticket com task,
revisões, geometria e versões, mantendo guards vivos antes de publicar. Comparador
diagnóstico cobre 25 grupos de campos dos tiles inicializados, inclusive dano
privado/derivado e modos explícitos de cache; limita a leitura às bordas válidas.
São 40 casos novos, 524 no total, 800 execuções adicionais e 26 grupos CTest com
ASan/UBSan. Ver [relatório da etapa](../sector-runtime-h0-base-tiles-identity-2026-11-09.md)
e [contrato](../sector-runtime-base-tiles-contract.md). Ticket não é conteúdo ou
contexto puro completo; não há scan completo no commit. biomeTransition herdado
não inicializado/copiado foi registrado para auditoria, não corrigido. H0 em 25%.

Inputs/consultas em `2ea41cf8`: WorldGenerationBaseTileInput modelo 1 e adaptador
real injetam consultas mantendo ordem/argumentos/writes foreground→background→líquido.
NoMod, mod não suportado e seis pontos de exceção preservam estado anterior/parcial.
São 24 casos novos, 548 no total, 1.280 execuções adicionais e 27 grupos CTest com
ASan/UBSan; 4.096 combinações de branches e 1.024 tiles com resolver imutável.
Ver [relatório da etapa](../sector-runtime-h0-base-tile-inputs-2026-11-09.md).
Não é produtor puro completo: resolver depende de template/assets, input igual
não garante output igual e tile parcial não é input seguro de retry. H0 em 25%.

Observações/replay diagnóstico em `abfc3834`: registro modelo 1, até seis consultas
retornadas, versões e guard de input/14 campos originais. Preflight completo antes
de escrever; falhas incompletas não podem ser replayadas. 27 testes novos, 575 no
total, 1.820 execuções adicionais e 28 grupos CTest com ASan/UBSan. Microexperimento
opt-in separado mostrou overhead com resolver trivial e 176 bytes/tile; manter
fora do produtor normal, não adotar como cache residente. Ver
[relatório da etapa](../sector-runtime-h0-base-tile-observations-2026-11-09.md).
Não fecha blockInfo/WorldTemplate/assets, demanda ou retry real; H0 em 25%.

Catálogo/scope modelo 1 em `4e5cc6c1`: respostas únicas de oito bytes sem heap,
capacidade explícita, deduplicação/conflitos e seal. Preflight de cobertura antes
de qualquer write; não confundir catálogo com guard de tile original. 26 casos
novos, 601 no total, 2.340 execuções adicionais e 29 grupos CTest com ASan/UBSan;
microexperimento separado de custo/storage. Ver
[relatório da etapa](../sector-runtime-h0-base-tile-lookup-context-2026-11-09.md).
Fora do produtor normal; scope declara gerações, não detecta reload real. Não
fecha CPU-01/produtor completo nem demonstra setores/s; H0 em 25%.

Clocks/binding modelo 1 e scope 2 em `5413e13a`: Root/Assets têm hooks reais de
reload/lifecycle, banco de materiais versão de definições e factory com handles
fortes. Wrapper distingue Refused/Applied/Stale, sem autorização de commit ou
fallback em buffer alterado. 28 testes novos, 629 no total, 2.900 execuções
adicionais e 31 grupos CTest com ASan/UBSan. Ver
[relatório da etapa](../sector-runtime-h0-lookup-source-generations-2026-11-09.md).
Root/Assets/MaterialDatabase compilados; testes com fontes controladas, sem
reload/assets reais. Catálogo continua fora do produtor normal, H0 em 25%.

Os [leitores reais](../sector-runtime-h0-real-lookup-readers-2026-11-10.md), `061ce68c`,
de WorldTemplate/MaterialDatabase agora são ligados no NoAssets com os mesmos
corpos de produção e metadata controlada; catálogo/fita/referência independente
comparados, inclusive misses e exceções. 20 novos casos, 649 no total, 3.300
execuções adicionais e 32 grupos CTest, ASan/UBSan. Troca real de layout e fontes
materiais retidas testadas; lifecycle parent/Assets ainda controlado, sem Root
ou assets carregados. Catálogo fora do produtor normal; nenhum marco completo
adicional ou ganho de setores/s. H0/RAM-01 continuam 25%.

O [lifecycle de Assets real](../sector-runtime-h0-assets-lifecycle-2026-11-10.md), `571bb01e`,
agora carrega arquivos locais e executa hotReload/cache/retenção com host RootBase
de configuração. Corrige fill vazio de ByteArray e parada de workers Assets;
666 testes, 3.640 execuções adicionais e 34 grupos CTest, ASan/UBSan. Root real,
listeners e bancos completos não foram executados. clearCache pode renovar
conteúdo sem mudar versão: congelar os inputs lidos, não assumir que binding
é snapshot bruto. Sem novo marco completo ou ganho de setores/s; H0 em 25%.

O [alvo opt-in de Root real](../sector-runtime-h0-root-lifecycle-2026-11-10.md), `7001ae6d`,
agora valida construção/manutenção/factory, materiais carregados, reload,
hotReload, loadMods(false/true), listeners em falha/recuperação e fontes antigas
retidas/retire. 669 testes padrão +13 Root opcionais, 3.960 execuções adicionais,
35+1 grupos CTest, ASan/UBSan. Corrige a multiplicação da seed automática com
wrap unsigned, preservando as sequências explícitas verificadas. Cinco corpos
de manutenção relocados sem mudança. Opção de teste padrão OFF, sem alterar
Actions ou executar Root::fullyLoad, assets completos ou streaming. H0 em 25%.

O [kernel de blockInfo](../sector-runtime-h0-block-info-kernel-2026-11-10.md)
(`0871d13c`) separa cálculo e cache no caminho normal; compara campos e consultas
contra o corpo herdado, com fontes controladas e falhas lazy. Não transforma o
adapter em snapshot: custom terrain ainda alcança Root/Assets e os seletores
possuem caches mutáveis. 697 casos padrão +13 Root opt-in, 4.325 execuções
adicionais e 36+1 grupos, ASan/UBSan. Sem novo marco completo ou throughput medido.

O [contexto proprietário de custom terrain](../sector-runtime-h0-custom-terrain-context-2026-11-10.md)
(`bfbbf403`) captura inputs e executa a matemática com geometria/polígonos reais, inclusive
após destruir a origem e trocar leitores. Primeiro recorte funcional de CPU-01
em 25%, H0/RAM-01 em 25%, programa 7/196 marcos =3,6%; não é contexto completo,
benchmark ou nova frente de executor. Captura fora dos jobs normais; a consulta
legada a Root permanece no mesmo ponto. Não adicionar cópia por bloco.
723 casos padrão +13 Root opt-in, 4.845 execuções adicionais e 37+1 grupos,
ASan/UBSan, no escopo declarado pelo relatório.

Os [inputs estáticos compostos](../sector-runtime-h0-block-info-static-inputs-2026-11-10.md)
retêm config Json imutável e custom terrain no mesmo objeto, com consultas lazy
e contabilidade Json v2 opcional. Composição exercitada no kernel; layout/
seletores ainda controlados, captura fora dos jobs e sem admissão do contexto
inteiro. CPU-01 continua em 25%, programa em 3,6%, sem ganho de setores/s medido.

O [ensaio de amostragem real](../sector-runtime-h0-real-sector-sampling-2026-11-10.md)
(`b0caadce`) executa Root/construtor normal, WorldLayout/Constant/FlatSurface e
LRU reais, compara a captura estática e produz BaseTiles 32×32 completos.
19 novos opt-in; 752 padrão +32 opcionais, 5.520 execuções adicionais e 40 grupos
CTest, ASan/UBSan. Grafo montado com metadata controlada, sem builder de planeta,
noise/Perlin ou cache mutável da árvore. 23 corpos relocados sem alteração;
nenhum novo marco, contexto em jobs ou ganho de setores/s medido.

O [recorte de Perlin e CacheSelector](../sector-runtime-h0-perlin-cache-2026-11-10.md)
reproduz e corrige falhas de cópia de metadata e seleção de fonte, e executa
noises e caches privados em fixture. Construir a árvore avança o RNG global;
essa dependência precisa ser isolada antes da integração em workers.

O [ensaio de fronteiras de RNG](../sector-runtime-h0-rng-boundary-2026-11-10.md)
restringiu o consumo à montagem da fixture de layout. Cache de constante, árvore
de seletores e noises, isoladamente, não consumiram RNG na amostra; a operação
específica dentro da montagem ainda está pendente.

Correção da conclusão: o [ensaio com Root encerrado](../sector-runtime-h0-rng-root-entropy-2026-11-10.md)
remove interferência de sua thread de manutenção, que adiciona entropia global.
A fixture preserva a sequência nesse cenário. `buildRegion` real, porém, usa
parâmetros default que consomem uma seed antes do override; preservar esse
consumo ao projetar o contexto privado e testar equivalência com assets/mods.

O [snapshot de parâmetros](../sector-runtime-h0-selector-snapshot-2026-11-10.md)
captura campos inicializados por valor e os materializa sem RNG adicional.
Ainda está fora do builder e dos jobs, e não é contexto/lease completo.

O [recorte integrado de região](../sector-runtime-h0-region-selector-capture-2026-11-10.md)
prepara os três parâmetros de buildRegion via snapshot, preservando uma chamada
global e as seeds derivadas. Testes compararam parâmetros e próximo RNG; falta
executar builder real completo com assets e validar equivalência integrada.

O [ensaio da factory real](../sector-runtime-h0-region-selector-factory-2026-11-10.md)
compara parâmetros capturados em seletores de terreno, caches de cavernas após
eviction/reconstrução e falhas com sequência de RNG. O builder de planeta completo
e a equivalência com mods/saves/multiplayer continuam pendentes.

O [builder real de região](../sector-runtime-h0-real-region-builder-2026-11-10.md)
agora é exercitado com bancos reais, assets mínimos, bioma, patch, cache e
BaseTiles. Dez cenários cobrem um recorte de validação, não dez contratos.
O builder procedural completo, streaming e equivalência integrada seguem abertos.

O [ensaio do builder terrestre](../sector-runtime-h0-terrestrial-builder-2026-11-10.md)
executa o builder procedural com cinco camadas e assets mínimos, cobrindo Json,
oceano, patch, falha, RNG e layouts privados. Está em validação: referência
independente, mods/saves/multiplayer e benchmark de streaming permanecem gates.

Próximo trabalho executável: validar builder/assets e preservar a sequência
observável e fechar ownership e admissão do contexto de produção.
Não confundir a LRU validada com CacheSelector. Auditar também defaults
baseHeight/regionLiquids. Manter caches exclusivos; fechar o contexto de BaseTiles,
inicialização e inputs efetivamente capturados; o lifecycle de Root mínimo já foi
testado, não repetir o mesmo recorte. Revalidar preparação e commit antes de
sombra por setor, com admissão/
bytes e outputs equivalentes. Não antecipar consultas,
assumir resolver arbitrário puro ou alterar cópias/RNG. Auditar biomeTransition e
SkyParameters::horizonClouds default (achado registrado, não corrigido)
antes de normalizar snapshots; versionar contexto profundo/assets e ligar demanda
ao interesse real antes de replay/cache de produtos.
Não repetir callbacks/Lua/mundo vivo por uma exceção.
Depois, provider processual único configurável, promoção de demanda,
fairness entre mundos e pressão externa/limites de hardware. Manter exclusões;
não exigir cada heap para H1 em sombra nem ativar orçamento de jogo só pela
existência do backend. Preservar fallback síncrono,
ownership único e história de cópias. Estender
o comparador ao fluxo de candidatos/IDs/prioridades e ao
streaming real, usando o controle de colisões como gate antes de eliminar cópias.
Auditar metadata do Perlin não inicializado antes de novos consumidores em
CPU-01; o gerador com seeds explícitas não mudou, a seed automática teve overflow
corrigido no recorte acima. Ampliar a cobertura dos
heaps de layout/biomas/seletores e caches com ownership/storage únicos.
Definir cobertura sem profiling antes da admissão obrigatória, limites
processuais por hardware/configuração e reação à pressão.
O inventário é parcial, não autorização para ampliar filas.
Manter MOD-01 explícito antes de mudar admissão ou ownership.
Estender o harness para dependências reais e o baseline com rota/save/config
fixos. Não ampliar admissão nem ligar previews por padrão antes desses gates.

Saída: baseline executável, pendências de identidade cobertas e causas de
espera reproduzíveis. Não depende de uma nova amostra TMX para começar.

## H1 — Grafo executivo e preparação CPU

Propósito: transformar orçamento e núcleos livres em progresso de setores.
Depende de H0 para identidade e comparação.

- [ ] DAG-02: nós compartilhados, demanda por consumidor e grafo em sombra.
- [ ] DAG-03: prioridade herdada, justiça e distância com wrap.
- [ ] DAG-04/05: mailbox, revalidação, retry e cancelamento com drain.
- [ ] CPU-01: primeiro recorte proprietário de custom terrain em 25%; completar
  contexto privado com dependências/RNG de inicialização auditados e admissão.
- [ ] CPU-02 e MD-01: candidatos leves e resultado vazio versionado.
- [ ] CPU-03: capacidade processual e granularidade inicial.
- [ ] WT-01/02: drenagem produtiva, orçamento e motivos de saída explícitos.
- [ ] CPU-05: primeira política baseada em fila executável e custo observado.

Saída: mais tarefas independentes em voo, menor pronto→commit e equivalência
no streaming. Publicação mantém a ordem autoritativa aprovada.

## H2 — RAM residente e antecipação

Propósito: preparar a rota antes da demanda urgente e evitar recomputação.
RAM-01 começa em H0; as caches dependem das versões/produtos de H1.

- [ ] CPU-02: compartilhar terreno/campos/candidatos entre consumidores.
- [ ] RAM-02/03: caches versionadas, retenção por demanda e pressão/histerese.
- [ ] RAM-04/05: rota prevista, cancelamento e consumo compartilhado.
- [ ] WT-03: entrada síncrona usando admissão da região inteira.
- [ ] IO-01: promover tiles decodificados sem refazer leitura/decode.

Saída: ganho em antecipação e revisita, bytes únicos observáveis e ausência
de crescimento persistente após mudança de rota ou fechamento do mundo.

## H3 — Kernels adicionais, estruturas e publicação

Propósito: reduzir trabalho serial após o desbloqueio do pipeline.

- [ ] CPU-04: extrair parte pura de CaveLiquid e Finalize por contrato.
- [ ] MD-02/03: formato estático TMX separado da ligação que consome RNG.
- [ ] MD-04: mapa completo de efeitos, footprints e comandos de estruturas.
- [ ] MD-05 e WT-04: aplicação em batches e staging quando necessário.
- [ ] IO-02/03/04: banco, checkpoint e unload coerentes com novos produtos.
- [ ] IO-05: pacote confirmado e medição até aplicação no cliente.

Saída: custo serial menor por setor/estrutura, com save, scripts e rede
equivalentes. Cada kernel pode avançar separadamente; uma dungeon composta
complexa não bloqueia o kernel simples já validado de setor padrão.

## H4 — Backend GPU e comparação em sombra

Propósito: criar o caminho concreto de uso da GPU. O protótipo de interface e
capacidade pode avançar em paralelo a H1/H2; publicação exige contrato estável
do kernel CPU e do resultado versionado.

- [ ] GPU-01: interface CPU/GPU, capacidades e build CPU independente.
- [ ] GPU-02: thread de contexto, buffers, fences e recuperação.
- [ ] GPU-03: primeiro kernel exato e lotes apropriados.
- [ ] GPU-04: comparação em sombra e corpus de bordas/precisão/drivers.

Saída: equivalência e custo completo demonstrados, sem publicar divergências.
A existência de shaders/uso de GPU no gráfico não conclui esta entrega.

## H5 — Seleção heterogênea e escala

Propósito: escolher recursos pelo menor tempo de entrega e usar hardware com
folga de forma sustentada. Depende de H1/H2 e H4 para backends autoritativos.

- [ ] GPU-05: roteamento por custo de transporte/execução e prazo.
- [ ] CPU-05: política completa de CPU/RAM/VRAM, perfis e limites manuais.
- [ ] VAL-03/04/05: métricas de recursos e campanhas de escala equivalentes.
- [ ] MOD-02/03/04: adoção, fallback e migração de configuração documentados.

Saída: ganho de entrega ponta a ponta, resposta do jogo dentro dos alvos e
CPU completa em hardware sem compute. Nenhuma meta fixa de uso de hardware
substitui o resultado do benchmark.

## Evolução regional — R1 e R2

Esta frente mantém a decisão de uma thread dedicada por região 2×2 carregada.
Pode aproveitar produtos de H1–H3, mas depende de auditoria completa de
propriedade e efeitos; não é liberada por um kernel GPU aprovado.

- [ ] R1: REG-01/02, MOD-01 e IO-03 — lifecycle autoritativo, estado local,
  snapshots de borda e checkpoint, inicialmente com passo lógico comum.
- [ ] R1: WT-05 — WorldThread como coordenadora dos donos regionais.
- [ ] R2: REG-03/04/05 — intenções multirregião, handoff, falhas e custo real.
- [ ] R2 posterior: avaliar independência de passos após relógio/rede/save
  específicos; registrar decisão antes de remover barreira comum.

Saída: updates locais realmente sobrepostos, sem dupla autoridade e com
compatibilidade declarada por perfil. Não abrir NPC externo ou trocar a
unidade de thread para setor físico durante essa migração.

## Mapa de IDs e vínculos anteriores

| Família | Fonte do contrato | Relação histórica |
| --- | --- | --- |
| WT | [WorldThread](worldthread.md) | reestruturação do update e coordenação |
| DAG | [Grafo](dependency-scheduler.md) | WRG-03/04/05 e schedulers em preview |
| CPU | [Etapas](cpu-and-stages.md) | BaseTiles e planejamento privado existentes |
| RAM | [Memória](memory-and-prefetch.md) | WRG-04/06 e prefetch |
| GPU | [Compute](gpu-compute.md) | novo backend H4/H5 |
| IO | [Persistência/rede](io-persistence-network.md) | WRG-08 e auditoria de IO |
| MD | [Estruturas](microdungeons-and-tmx.md) | WRG-07 e plano de dungeons retomáveis |
| REG | [Regiões](regional-threads.md) | WRG-09/10/12, unidade 2×2 vigente |
| MOD | [Compatibilidade](compatibility.md) | requisito transversal, não fase opcional |
| VAL | [Validação](validation-and-telemetry.md) | evidência, benchmark e retomada |

WRG-11 e referências antigas a NPCs não retomam a externalização suspensa.
Essa correspondência permite localizar história; não declara um item WRG
inteiro concluído ao concluir apenas uma tarefa H.

## Procedimento para a próxima sessão

1. Ler este índice/roadmap e o último relatório associado ao item ativo.
2. Confirmar branch, commit e alterações locais; localizar os símbolos do
   módulo antes de editar.
3. Executar o primeiro item pendente cujas dependências estejam satisfeitas.
4. Rodar verificações apropriadas ao risco e registrar o escopo real.
5. Atualizar contrato se necessário, estado do item, relatório, checklist e
   painel de progressão; recalcular notas somente com evidência de novo marco.
6. Informar commit e limitações; manter Actions manuais e revisão antes de main.

Retomar pelo andamento de H0 e pelo relatório vinculado acima. A prioridade
de entrega permanece setores comuns; uma coleta TMX inconclusiva não impede
avançar essa sequência.
