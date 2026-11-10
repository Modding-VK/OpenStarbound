# BaseTiles: identidade do produto e comparação de campos

Estado: H0, `Experimental`, 10/11/2026.
[Programa](sector-runtime/README.md) · [Roadmap](sector-runtime/roadmap.md)
· [Memória](sector-runtime-memory-envelope.md)
· [Relatório](sector-runtime-h0-block-info-kernel-2026-11-10.md)

## Identidade não é autorização de commit nem chave de conteúdo

`WorldGenerationBaseTilesIdentity`, modelo/produto **1**, identifica uma admissão:

| Grupo | Campos | Regra |
| --- | --- | --- |
| Task | worldEpoch, sequence, sector, region, stage | Epoch/sequence não zero; BaseTiles; setor canônico e região lógica 2×2 correta |
| Fonte observada | tileRevision, templateRevision | Valores inteiros exatos, inclusive zero; não hash do input |
| Geometria | worldSize, tileRegion | Mundo positivo representável em RectI; bounds exatos do setor 32×32, recortados na borda |
| Algoritmo | productVersion, liquidVersion | Só versões suportadas; versão escalar de líquido não substitui versão do produtor |

PreparedBaseTiles devolve a mesma identidade capturada no dispatch. Pending
guarda a expectativa. `matches` exige expectativa válida e igualdade de todos
os campos; duas identidades inválidas iguais não são aceitas. Nova task no mesmo
setor tem outra sequence; nova instância de mundo tem outro epoch.

Commit ainda exige registro/task atuais e válidos, vínculo ao array físico
original, revisão de tiles, ponteiro de template e revisão de template atuais.
Unload continua anulando o vínculo ao array e invalidando a task, protegendo
contra reuso ABA. Soma-se a conferência do resultado/versões/geometria; não se
substitui essa autorização pelo ticket. Publicação e descarte continuam no owner.

Isto **não é contexto puro completo ou chave de cache reutilizável**: não contém
bytes iniciais, seed/template serializado, identidade/reload de todos os assets
ou bancos de materiais. Mesmo ticket em um teste não implica payload igual.
Sem cache de produtos, replay automático, promoção de demanda ou mudança de
limites/defaults. O ramo escalar Keep depende do líquido anterior: identidade
de conteúdo futura deve incluí-lo. Revisões dependem de rastrear mutações reais.

## Inputs escalares e consultas do adaptador, modelo 1

WorldGenerationBaseTileInput contém só o que a aplicação por tile consome de
blockInfo: block/environment biome, transition, material/mod de foreground e
background, oceanLiquid/nível/enclosure e y absoluto. É valor sem Json, ponteiros,
Root, seed ou dono de memória. O loop real ainda obtém blockInfo uma vez por tile.

applyWorldGenerationBaseTile recebe tile, input const e resolver const emprestado.
Não cria std::function/vtable ou buffers. O resolver expõe materialHue, supportsMod
e modHue; a produção vincula o WorldTemplate já usado em cada caminho (privado no
worker, autoritativo no síncrono) e o MaterialDatabase existente.
Injeção/const **não certifica pureza**: só um resolver de valores imutáveis com
dependências fechadas permitirá reexecução pura. Assets/contexto profundo seguem
fora do input, e ticket igual não comprova igualdade dos valores do resolver.

Sequência preservada, inclusive estado parcial antes de exceção:

1. Atualizar os três campos de bioma.
2. Só se foreground for NullMaterialId: atribuir material e cor default, consultar
   hue usando o **novo block biome**, atribuir hue, consultar suporte do mod e,
   se suportado, atribuir mod antes de consultar/atribuir mod hue.
3. Repetir a mesma sequência para background, depois de foreground completo.
4. Aplicar líquido usando os materiais finais, só depois das consultas terminarem.

EmptyMaterialId já existente não é camada ausente. Não corrigir automaticamente
mod antigo não suportado: os campos antigos são preservados. NoModId ainda passa
por supportsMod e modHue, mesmo em ar/meta; atalhar essa chamada pode mudar hue
persistido/observável. Environment biome não substitui block biome nessas consultas.

O caminho síncrono pode estar escrevendo no array vivo; resolver antecipadamente
todas as consultas mudaria o estado parcial deixado por uma exceção. O adaptador
mantém as escritas e propaga falhas, sem retry interno. Repetir sobre um tile
parcialmente escrito não equivale a recomeçar do snapshot original. Testes incluem
esses controles negativos e o estado observado antes de cada chamada.

Modelo do input/adaptador 1; produto/ticket e kernel líquido permanecem 1 porque
esta extração preserva comportamento. Não há mudança de save, contexto completo,
cache de produto, orçamento, número de workers ou publicação serial.

## Observações congeladas e replay diagnóstico, modelo 1

WorldGenerationBaseTileObservation guarda input, versões observação/input/líquido,
14 campos iniciais relevantes e até seis consultas tipo/chaves/valor. Captura é
explícita: envolve o adaptador e registra cada resultado depois que a consulta
original retorna, sem antecipar chamadas. Reset inicial impede reutilizar um
sucesso anterior após falha; exceção mantém a fita incompleta e o tile parcial.

Replay valida todo o registro, inclusive a última chave, antes de qualquer write:
complete/versões/contagem, sequência condicional, argumentos/faixas, input esperado
e dez campos de camadas/quatro de líquido originais. Floats usam bits. Só então
aplica com resolver congelado/cursor local, sem consultar template/assets. Uma
observação const pode servir tiles privados em paralelo, sem cursor compartilhado.

Guard não inclui dano/root/dungeon/colisão/cache, que passam intactos; nem lê
biomas antigos, sobrescritos pelo adaptador. Não é identidade de tile inteiro,
hash/selo de integridade, geração de assets ou autorização de commit/cache/retry.
Requer campos relevantes inicializados, input/observação estáveis e tile exclusivo.
O domínio aritmético do líquido continua o herdado. Modelo não é codec persistido.

A API não é chamada pelo produtor normal. Custa 176 bytes por tile nesta ABI;
o microcusto Debug/ASan com resolver trivial mostrou overhead, não aceleração.
Fica como referência de equivalência, não fita residente ou custo novo no hot path.
Produto/ticket/input/líquido e demais modelos anteriores permanecem inalterados.

## Catálogo compartilhado de consultas, protótipo modelo 1

WorldGenerationBaseTileLookupContext<Capacity> guarda respostas únicas em palavras
de 8 bytes ordenadas por chave injetiva tipo/biome/material/mod, sem hash ou heap
interno. Capacity é explícita por dono, não default do jogo. Amostras completas
são deduplicadas antes de inserir; recusa por escopo/capacidade/amostra inválida
não insere parcialmente. Conflito de valor para a mesma chave invalida o builder.
Após seal há apenas leitura compartilhada; lifetime/publicação são do chamador.

WorldGenerationBaseTileLookupScope **modelo 2** exige epoch, gerações de assets/
material e sourceEpoch de Root não zero, revisão de template (zero permitido)
e versões scope/input/líquido. Oito campos conferidos em add/apply. A factory
Root::worldGenerationLookupSources fornece gerações reais; scopes literais ainda
são interface de baixo nível, não prova de frescor ou pureza por amostragem.

apply resolve uma vez todas as chaves condicionais antes de qualquer write e
usa seis bytes locais no adaptador existente. Miss, inclusive no último hue,
recusa com tile intacto para escolha do caminho legado. Não há callback/resolver
vivo, fallback interno ou tape por tile. Diferente do replay diagnóstico, aplica
ao tile privado **atual**: não inclui guard de 14 campos originais ou autoriza
retry de estado parcial. Ownership/revisões/guards vivos continuam necessários.

Fora do produtor normal. No teste com scope 2, Context<32> ocupa 328 bytes,
256 reservados em entries e 48 ativos para seis chaves; não é dimensão garantida
para todo mundo.
Microcusto sintético não demonstra aceleração de setores ou de WorldTemplate.

## Fontes reais, clock e binding, modelo 1

Root invalida seu clock em reload/hotReload/loadMods e o retira no encerramento.
Assets tem geração própria para hotReload explícito; MaterialDatabase identifica
as definições carregadas. IDs são processuais monotônicos sem RNG/wrap; zero é
indisponível. Nested/overlap só publicam após todos completarem; falha mantém zero.
Não é hash de conteúdo ou monitor de disco/cache; sincronização dos dados continua
pelos locks existentes. Banco de materiais não é reconstruído por hotReload.

Binding retém Assets/material e relógio, não Root/mundo; confere versões antes
de produzir scope e antes/depois de aplicar. Refused garante nenhum write;
Stale após aplicação exige descartar output privado, sem fallback nesse buffer.
Applied é checagem local, não lease/autoridade: fonte pode mudar em seguida, logo
o dono revalida fontes/task/template/tiles no commit. Strong handles retêm fontes
mesmo após invalidação; não cancelam/drainam jobs ou recuperam RAM automaticamente.

Hooks de metadata no runtime real; catálogo ainda não chamado pelo produtor normal.
NoAssets agora liga os mesmos corpos de leitores usados no jogo, com objetos
reais e metadata controlada. O recorte posterior também constrói Assets e executa
hotReload com arquivos locais de fixture; parent clock e definições continuam
controlados naquele recorte. O alvo opt-in posterior executa Root real e sua
factory com arquivos mínimos; não constrói todo o contexto de geração. Validação
integrada com mundo/assets/mods completos permanece gate
antes de uso/sombra por setor.

## Leitores reais e equivalência sem assets

O [recorte de leitores](sector-runtime-h0-real-lookup-readers-2026-11-10.md)
extrai o resolver emprestado de produção e reloca seus leitores/construtores
de valores sem mudar os corpos. Biome/WorldTemplate têm headers de tipos e
fachadas antigas que preservam includes transitivos; campos/layout não mudam.
Tags privadas de fixture não constroem mundos/bancos completos e não são usadas
nos construtores normais ou para blockInfo/geração.

Consultas reais confirmam que NoMod pode ter hue não zero, supportsMod aceita
NoMod antes de procurar material e não consulta pertencimento de mods reais
ao banco. Bioma inválido/material ausente conservam suas exceções e prefixos
de escrita, inclusive na referência independente. Não adicionar validação ou
normalização que mude esses comportamentos em nome de uma otimização.

4.096 combinações comparam trace/campos entre corpo anterior, captura, fita e
catálogo; 1.024 tiles comparam os leitores reais e catálogo em quatro workers
a uma referência serial. Troca real de layout avança revision e recusa catálogo
antigo; replacement de lifecycle usa clocks controlados, não Root::reload.
Catálogo continua diagnóstico, sem uso no produtor normal ou ganho de setores/s.

## Lifecycle de Assets real com arquivos de fixture

O [recorte de lifecycle](sector-runtime-h0-assets-lifecycle-2026-11-10.md)
exercita construtor, leitura/cache de JSON/bytes, hotReload, versões e retenção
de Assets real. Um host RootBase fornece somente Configuration; não é Root,
seus listeners ou parsing completo dos bancos do jogo. O catálogo antigo é
recusado sem writes após hotReload direto; um catálogo novo volta a reproduzir
os leitores sobre as mesmas definições de fixture.

Editar disco não invalida a versão; clearCache pode atualizar conteúdo sem
avançar essa versão; hotReload não reescaneia arquivos novos. Portanto scope/
binding não certifica conteúdo bruto de assets imutável. Fechar/capturar os
inputs efetivamente lidos antes de ampliar CPU-01; não mudar reload legado para
simular essa garantia. Invalidar binding ainda conserva fontes até o último handle.

Testes reais revelaram/corrigiram fill de ByteArray vazio com memset(nullptr,0)
e janela de lost-wakeup na parada de workers Assets. Não mudam conteúdo dos
tiles, formatos, filas ou autorização de commit; não são causa atribuída aos
crashes antigos nem prova de velocidade maior.

## Root real e factory com arquivos mínimos

O [recorte de Root](sector-runtime-h0-root-lifecycle-2026-11-10.md) executa o
construtor normal, manutenção, reload/hotReload/loadMods, listeners e destruição,
incluindo Root::worldGenerationLookupSources e MaterialDatabase carregado pelo
parser real. Treze casos opt-in confirmam versões, falhas/recuperação, retenção
e override de arquivos, não scripts de mods ou setores gerados.

HotReload mantém o banco de materiais; reload o reconstrói. LoadMods(false)
invalida o epoch sem aplicar fontes novas até reload. Listener em execução/falha
não emite binding válido; destruição retira o clock sem manter Root vivo.
Seed automática passou a multiplicar em uint64_t para definir o wrap, corrigindo
overflow reproduzido na manutenção; gerador com seed fixa e consumo de uma seed
na construção default preservam sequências verificadas. Nenhum scope vira lease.

## Cálculo de blockInfo e fronteira do contexto

O [recorte de cálculo](sector-runtime-h0-block-info-kernel-2026-11-10.md) extrai
calculateWorldGenerationBlockInfo e o conecta ao miss do cache normal de
WorldTemplate. O cache, wrap de x, conversões de coordenadas, ordem de consultas
e primeiro match continuam legados. O kernel não possui Root ou cache, mas o
adapter normal **não é um snapshot puro**: usa o template do worker, seletores
com caches mutáveis e customTerrainWeighting, cujo acesso a Root/Assets foi
preservado no mesmo ponto. Não compartilhar esse adapter entre jobs.

Config continua lazy: biomeTransitionThreshold depois dos pesos, dist somente
para terreno positivo e factor somente abaixo de dist. Não antecipar leituras
ou validar todos os arrays antes do ramo: isso muda falhas observáveis. A saída
local só retorna quando completa; não há fallback/retry sobre tile parcial.
O kernel não autoriza commit, amplia fila ou estabelece identidade profunda.
Próximo: capturar custom terrain/config com lifecycle explícito e manter caches
de seletores exclusivos, testando o adapter/layout real antes de sombra por setor.

## Comparação de campos, modelo 1

O comparador recebe referências const de ServerTile reais. Não serializa, aloca
snapshots ou usa memcmp do objeto inteiro. São **25 bits de diferença**:

| Grupo | Campos comparados |
| --- | --- |
| Materiais | Foreground/background, hue, mod, mod hue e variante de cor: dez bits |
| Colisão | Collision, objectCollision, dirty e geometria ativa do cache: quatro bits |
| Biomas | Block/environment biome e biomeTransition: três bits |
| Dano | Foreground/background: dois bits, incluindo percentual, tempo de efeito privado, harvested, origem x/y, tipo e percentual derivado |
| Outros | DungeonId, rootSource e presença do Maybe: dois bits |
| Líquido | ID, level, pressure e source: quatro bits |

Floats são comparados por bits: sinais de zero e payloads NaN distinguem estado.
Cache compara quantidade/ordem de entries, kind/space, bounds e vértices/ordem.
Capacidade, padding, slots inativos e bookkeeping de allocator não são estado
semântico e ficam fora. Acesso privado de dano é somente de leitura e inclui
estado que o save de tile não persiste. Comparar saves não substitui este teste.

Modos explícitos:

- Exact: todos os campos acima, inclusive dirty/cache.
- IgnoreCollisionCache: ignora **somente** dirty e geometria, pois a cópia herdada
  normaliza esse cache; continua comparando ambas as collision kinds e todo
  material/dano/líquido. Não significa equivalência de estados autoritativos.

Resultado tem valid, comparedTiles, differentTiles, differentFields e primeira
posição diferente, em ordem x→y. Equal exige valid e nenhuma diferença. Modo
desconhecido/identidade inválida falha fechado. Array comparison exige identidade
válida e só visita tileRegion: nunca lê as células físicas fora de borda parcial.
Não há comparador/fingerprint profundo no hot path de publicação nesta entrega;
o runtime usa apenas o ticket. Custo do comparador é diagnóstico/NoAssets.

## Pré-condição de inicialização e cópia herdada

Comparar apenas campos inicializados e entries ativas válidas. O construtor de
WorldTile e seu operador de cópia **não inicializam/copiam biomeTransition**.
BaseTiles preenche esse campo nos tiles válidos antes de devolver o produto;
isso não prova que toda origem/snapshot do motor esteja inicializada. Cópia de
ServerTile também normaliza cache; não criar clones diagnósticos que alterem
o objeto que se pretende medir.

Essa semântica herdada não foi corrigida nesta etapa, nem atribuída a crashes
anteriores. Fixtures preenchem os campos explicitamente e não leem bool indefinido.
ASan/UBSan não substituem MSan para detectar inicialização. Auditar consumidores
e compatibilidade antes de alterar o construtor/cópias ou eliminar snapshots.

## Fronteira de continuidade

O ticket está no fluxo real e o comparador exercita tipos reais sem assets;
isso não é execução integrada de terreno, mods, saves ou multiplayer. As rotinas
puras de constructor/reset de TileDamageStatus foram movidas sem mudança de
corpo para permitir esses testes sem ligar Root/assets; dano/recovery/codec não
mudaram. CMake registra a nova unidade no jogo e no alvo NoAssets.

Próximo: integrar fontes/assets reais e lifecycle de Root: reload/hotReload,
listener que falha e fontes antigas retidas, sem reconstrução implícita de bancos.
Revalidar preparação e commit antes de sombra por setor com admissão/bytes e
comparação. Não antecipar consultas nem reaproveitar tile parcial.
Auditar biomeTransition e SkyParameters::horizonClouds default antes de normalizar snapshots; versionar assets/contexto
profundo e ligar demanda ao interesse real. Preservar gates MOD-01/VAL-02,
baseline integrado, revisão independente e fallback CPU.
