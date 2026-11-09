# BaseTiles: identidade do produto e comparação de campos

Estado: H0, `Experimental`, 09/11/2026.
[Programa](sector-runtime/README.md) · [Roadmap](sector-runtime/roadmap.md)
· [Memória](sector-runtime-memory-envelope.md)
· [Relatório](sector-runtime-h0-base-tiles-identity-2026-11-09.md)

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

Próximo: fronteira explícita para materiais/mods/biomas e comparador de outputs
do produtor, sem antecipar consultas ou mudar cópias/RNG. Auditar biomeTransition
antes de normalizar input; versionar assets/contexto profundo e ligar demanda
ao interesse real antes de retry/cache de produtos. Preservar gates MOD-01/VAL-02,
baseline integrado, revisão independente e fallback CPU.
