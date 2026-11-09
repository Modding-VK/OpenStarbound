# BaseTiles: identidade do produto e comparação de campos

Estado: H0, `Experimental`, 09/11/2026.
[Programa](sector-runtime/README.md) · [Roadmap](sector-runtime/roadmap.md)
· [Memória](sector-runtime-memory-envelope.md)
· [Relatório](sector-runtime-h0-base-tile-observations-2026-11-09.md)

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

Próximo: contexto compartilhado e compacto de consultas por chaves/versão,
usando a fita como referência. Vincular revisão/assets e recusar cobertura ausente
antes da aplicação; comparar outputs/ordem/exceções e custo/bytes antes de sombra
por setor ou cache/retry. Não antecipar consultas nem reaproveitar tile parcial.
Auditar biomeTransition antes de normalizar snapshots; versionar assets/contexto
profundo e ligar demanda ao interesse real. Preservar gates MOD-01/VAL-02,
baseline integrado, revisão independente e fallback CPU.
