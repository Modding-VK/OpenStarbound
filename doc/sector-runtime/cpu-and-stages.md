# CPU, produtos de setor e kernels por etapa

[Índice](README.md) · [Grafo](dependency-scheduler.md) · [RAM](memory-and-prefetch.md)

## Propósito

Manter os núcleos disponíveis ocupados com preparação que será consumida e
reduzir recomputações. A preparação acompanha demanda do grafo e orçamento
global, em vez de esperar a publicação da primeira cadeia bloqueada.

## CPU-01 — Contexto privado de cálculo

Criar inputs explícitos para parâmetros, seletores, materiais necessários e
versões. A árvore de caches mutáveis de `WorldTemplate` não pode ser
compartilhada entre workers. Reutilizar contexto privado por mundo/revisão,
com afinidade e cache orçada, sem reconstruí-lo a cada troca breve de tarefa.

Auditar a inicialização: `WorldTemplate(Json)` hoje passa por construtor que
consulta `Root` e consome RNG antes de substituir a seed. A futura preparação
pura deve resolver essas dependências em uma fronteira autoritativa e
preservar a ordem/número de escolhas que a compatibilidade exigir. Apenas
copiar o ponteiro ou tornar um método `const` não comprova pureza.

Achado em 09/11/2026: a validação ampliada de RAM-01 observou overflow
assinado em `Random::produceRandomSeed` (`StarRandom.cpp:197`) ao inicializar
a fonte automática/global. O [relatório](../sector-runtime-h0-memory-admission-2026-11-09.md)
registra reprodução e escopo. A [validação posterior de Root](../sector-runtime-h0-root-lifecycle-2026-11-10.md)
reproduziu o mesmo defeito na manutenção e o corrigiu usando uint64_t na
multiplicação, com wrap definido. Sequências de seed/entropia explícitas e consumo
de uma seed na construção default foram verificados. Não reabre seleção de
conteúdo ou justifica mudar a ordem de escolhas; a auditoria do contexto/RNG de
WorldTemplate continua pendente, assim como Perlin e demais campos não inicializados.

Fronteira preparatória de H0 em `8e190742`: o
[subkernel escalar de líquido de BaseTiles](../sector-runtime-h0-capacity-kernel-2026-11-09.md)
recebe materiais/oceano/y já resolvidos e devolve decisão modelo 1; adapter atua
só em LiquidStore privado. Usado no loop real sem antecipar consultas, Root,
RNG, alocar buffers ou copiar ServerTile. 7.560 combinações e 1.024 decisões
particionadas coincidem com a referência anterior. Keep depende do líquido
anterior; futura identidade de output deve conservá-lo. Isto não extrai o
contexto completo de WorldTemplate, nem resolve CPU-01 ou concede novo marco.

Em `f7785963`, [ticket/comparador BaseTiles](../sector-runtime-h0-base-tiles-identity-2026-11-09.md)
inclui revisões/geometria/versões, conservando bindings vivos. Isso é identidade
por admissão, não contexto puro/content key ou PreparedSector concluído. O
[contrato](../sector-runtime-base-tiles-contract.md) cobre todos os campos
inicializados de tiles válidos para diagnóstico. Auditoria adicional: WorldTile
default/cópia não inicializa/copia biomeTransition; BaseTiles o sobrescreve nos
tiles válidos. Não corrigido, não causa de crash estabelecida. Auditar os demais
consumidores e compatibilidade antes de mudar snapshots ou eliminar cópias.

Continuidade: [inputs escalares BaseTiles](../sector-runtime-h0-base-tile-inputs-2026-11-09.md).
WorldGenerationBaseTileInput explicita biomas/material/mod/oceano/y por tile;
um adaptador injeta materialHue/supportsMod/modHue sem mudar a ordem ou antecipar
consultas. O resolver real ainda usa o template do caminho e MaterialDatabase;
const/injeção não fecham dependências.
CPU-01 e PreparedSector permanecem pendentes; sem novo marco ou ganho medido.

As [observações congeladas](../sector-runtime-h0-base-tile-observations-2026-11-09.md),
`abfc3834`, permitem replay diagnóstico sem resolver vivo, com preflight de todas
as consultas/input/estado relevante antes de escrever. Não são chamadas no
produtor normal; custo sintético/176 bytes por tile desaconselham fita residente.
O [catálogo compartilhado](../sector-runtime-h0-base-tile-lookup-context-2026-11-09.md),
`4e5cc6c1`, deduplica chaves em oito bytes, sela e confere scope/cobertura antes
de escrever. Não retém fita ou input por tile; não guarda o estado inicial de
um tile específico. Protótipo fora do produtor normal, sem heap interno; scope
de assets/material era declarado naquela entrega.
O [binding de fontes](../sector-runtime-h0-lookup-source-generations-2026-11-09.md),
`5413e13a`, liga clocks às fronteiras reais de Root/Assets e versões de definições
de materiais. Scope 2 inclui sourceEpoch, handles fortes e checagem pré/pós;
não é read lease ou autorização de commit.
Os [leitores reais](../sector-runtime-h0-real-lookup-readers-2026-11-10.md)
agora são comparados ao catálogo/fita/corpo independente, incluindo exceções,
NoMod e troca de layout, sem construir Root/assets ou sortear seed. Facades de
Biome/WorldTemplate mantêm includes antigos; tags privadas servem só a metadata
de consultas, não a blockInfo/contexto completo. Auditar também o default herdado
de SkyParameters::horizonClouds: a fixture o inicializa, produção não foi alterada.
O [lifecycle de Assets real](../sector-runtime-h0-assets-lifecycle-2026-11-10.md)
agora é exercitado com arquivos de fixture e host de configuração RootBase.
Root/listeners ainda estavam pendentes naquela etapa. clearCache pode atualizar
conteúdo sem mudar generationRevision; essa versão cobre reload explícito,
não congela leituras brutas. O [alvo opt-in de Root](../sector-runtime-h0-root-lifecycle-2026-11-10.md)
já testa construtor/factory/reload/loadMods/listeners reais com fontes mínimas.
O [kernel de blockInfo](../sector-runtime-h0-block-info-kernel-2026-11-10.md)
agora separa o cálculo do cache e é usado no caminho normal. A interface mantém
consultas lazy; o adapter ainda usa o WorldTemplate do worker e custom terrain
com Root/Assets. CacheSelector, IslandSurfaceSelector, WormCaveSelector e
KarstCaveSelector possuem caches mutáveis: const não autoriza compartilhamento.
O [contexto de custom terrain](../sector-runtime-h0-custom-terrain-context-2026-11-10.md)
agora captura geometria, polígonos/bounds, blend e revisão por valor, sem reter
WorldTemplate/Root/Assets. A matemática compartilhada com o caminho normal
passa em geometria/polígonos reais, inclusive leitura por três tarefas após
destruir a origem. **CPU-01 em 25%:** primeiro recorte proprietário funcional,
não contexto BaseTiles completo ou adoção nos jobs normais. O adapter legado
mantém Root/Assets e usa uma visão emprestada, sem capturar/copiar por bloco.
Os [inputs estáticos compostos](../sector-runtime-h0-block-info-static-inputs-2026-11-10.md)
agora juntam custom terrain e raiz Json imutável retida, com getFloat somente
quando o kernel consulta o campo. Modelo Json v2 opcional acompanha lifetime
e deduplica backing; não é validação de schema ou admissão do contexto inteiro.
Composição testada no kernel contra corpo herdado, com layout/seletores ainda
controlados. Captura fora dos jobs normais; CPU-01 continua em 25%.
O [ensaio de amostragem real](../sector-runtime-h0-real-sector-sampling-2026-11-10.md)
agora usa Root/construtor normal, WorldLayout/Constant/FlatSurface e LRU reais,
compara a captura e produz BaseTiles 32×32 completos com materiais parseados.
Layout montado com metadata controlada: não builder procedural, Perlin ou caches
mutáveis de seletores. Adapter candidato só de teste, fora dos jobs normais.
23 corpos herdados relocados sem alteração, sem remover Root/Assets do legado.
19 casos novos opt-in, 752 padrão +32 opcionais, 40 grupos; CPU-01 ainda 25%.
Próximo bloco funcional: reproduzir factory de CacheSelector e inicialização/
cópia de Perlin, auditar baseHeight/regionLiquids default, estender noises e
árvores/caches privados reais. Fechar ownership/identidade e revalidar antes de
sombra com admissão/bytes. Não criar outro accessor isolado como substituto.
Não otimizar consultas isoladas como fim em si mesmo.
CPU-01 completo, contexto profundo/RNG e ganho de WorldTemplate seguem pendentes.

Aceitação: execução concorrente não alcança template vivo, callbacks ou RNG
global; comparador de outputs cobre a mudança de contexto e de worker.

## CPU-02 — Produto `PreparedSector`

Representar resultados independentes num produto versionado:

| Produto | Inputs | Consumo autoritativo |
| --- | --- | --- |
| BaseTiles privado | template, seed e estado inicial relevante | instalar se ticket/revisões forem atuais |
| Campos terreno/bioma | template e coordenadas | reutilizar nas etapas que dependem desses campos |
| Candidatos ordenados | campos, distribuições e IDs de colocações | validar etapa/estado vivo antes de aplicar |
| Resultado vazio | prova de ausência e mesmos inputs | concluir a parte vazia quando os pré-requisitos existirem |
| Delta de etapa | snapshot semântico completo e versão | revalidar read set e aplicar no dono |

O produto pode ser consumido em marcos distintos; preparar não altera o nível
persistido. Referências compartilhadas evitam copiar campos para cada
consumidor. Não reutilizar uma simulação de tiles como campo puro se ela já
contiver edições específicas de uma encarnação.

BaseTiles e seleção de candidatos percorrem dados semelhantes; compartilhar
cálculo e cache local antes de criar mais produtores independentes. Finalize
continua filtrando pelo `dungeonId` e pelos efeitos reais da geração anterior.

## CPU-03 — Executor processual e granularidade

Coordenar a capacidade CPU usada pelos mundos, decode, compressão e tarefas
de tiles. A existência de uma pool por subsistema não cria capacidade física
adicional. Considerar afinidade/quota utilizável e topologia quando disponível;
dimensionamento é agnóstico ao modelo de CPU e ajustado pelo custo observado.

Filas CPU intensiva, IO bloqueante e submissão de GPU possuem classes próprias.
Usar afinidade de template e roubo de trabalho para kernels puros. O futuro
executor não tem o teto fixo de 32 como premissa; sua substituição exige
controle de memória, drain e comparação antes de mudar o limite atual.

Lotes de vários setores amortizam setup e favorecem localidade/SIMD. Cada
setor mantém identidade, seed, resultados e cancelamento individuais. Reduzir
lote que atrase o setor urgente. Uma janela grande de jobs pode ocultar
latência; uma janela excessiva pode apenas produzir resultados stale.

## CPU-04 — Extração por etapa

**BaseTiles:** preservar materiais preexistentes e regras de oceanos/bioma.
O cálculo já é privado; reduzir cópias e publicação/invalidação de colisão
sem sobrescrever alterações reais feitas após o snapshot.

**MicroDungeons:** seleção atual usa template e distribuições, sem ler halo.
Remover essa cópia da seleção após DAG-01 e validar a versão dos inputs.
Aplicação de estrutura permanece no dono até o contrato MD de comandos.

**CaveLiquid:** extrair flood-fill sobre snapshot completo do alcance lido e
delta ordenado de pressão/líquidos/background. A operação pode ultrapassar o
setor e depende de estruturas anteriores. Não aplicar um delta parcialmente
válido nem supor que cada célula de líquido seja independente.

**Finalize:** separar normalização, cálculo de grama/cores/colisão e seleção
de flora dos efeitos de criar plantas, objetos e tesouros. Capturar as
leituras de vizinhos e revalidar antes de publicar. Inicialização de entidades
e ativação do motor de líquidos continuam operações autoritativas.

**Terraform:** tem revisões e semântica próprias; não usar o cache da geração
inicial depois de editar biomas/layout. Equivalência deve incluir reload.

A investigação de oceanos/clima continua no backlog e depende de amostra
específica. Esta extração não declara resolvido o custo dos líquidos em geral.

## CPU-05 — Governador de recursos e perfis

Observar demanda executável, custo por classe, ocupação das pools, resultados
retidos, tempo de publicação e resposta do render/simulação. Crescer
concorrência e antecipação enquanto isso melhorar entrega; estacionar/drainar
workers ao reduzir capacidade, preservando tickets.

Perfis futuros: equilibrado, alto throughput, dinâmico com viés configurável
e manual. Nomes são conceitos de produto, não chaves atuais. Alto throughput
aceita consumo elevado conforme alvos escolhidos, sem exigir mudança do
`dt`. GPU integrada e render também disputam banda/memória; medir a carga
combinada é parte do controle.

## Melhorias esperadas e aceitação

Esperam-se mais tarefas úteis simultâneas, menor custo por tile e menor tempo
entre fases. Medir CPU por setor, bytes copiados, contexto reconstruído,
reutilização de campos, taxa de cancelamento e entrega ponta a ponta.

Comparar um/N workers, vários mundos, SIMD ligado/desligado quando existir,
mod stack de referência e pouca/muita memória. SIMD e fusão de jobs não
podem alterar arredondamento, sequência RNG ou ordenação de candidatos.
Dependências: [compatibilidade](compatibility.md), [validação](validation-and-telemetry.md)
e contratos específicos de [estruturas](microdungeons-and-tmx.md).
