# H0 / RAM-01: listas potenciais, cópia de validação e scratch

Data: 09/11/2026. Entrega: `Experimental`; revisão local:
`codex/experimental-regional-runtime`. Base: `956ef66e4cab17e998c2796c595c59671b57f9a9`.
Implementação: `c4d7ddb3e1738ad281c7f798c7fcc32bb37eb9e5`.

[Programa](sector-runtime/README.md) · [Roadmap](sector-runtime/roadmap.md)
· [Progressão](sector-runtime/progress.md) · [Inventário](sector-runtime/memory-and-prefetch.md)
· [Etapa anterior](sector-runtime-h0-candidate-payloads-2026-11-09.md)

## Objetivo e resultado

Avançar RAM-01 sobre os originais de listas potenciais, a cópia por valor da
validação e a saída reutilizável do planejador. Esses dados agora têm escopos
de ownership e estimativa agregada ligados ao lifetime físico; a previsão da
saída e da cópia é reservada antes de construir a cópia de validação.

Os dois caminhos de planejamento usam o mesmo helper. A criação inicial das
entradas continua observada **após** construção/população: adoção de dados
retidos, não reserva anterior à primeira alocação. Capacidades dos buffers e
backing conhecido entram no modelo, não todos os internals do allocator/Json.

Não altera APIs públicas de WorldTemplate, representação de PotentialBiomeItems,
predicados de filtragem, RNG, ordem de consultas ao template, cópias de elementos,
crescimento protegido, sort, IDs, TTL, níveis ou publicação serial. Não há
nova config, worker, cota, aumento de admissão, GPU, executor ou thread regional.
Não é uma aceleração de setores/s demonstrada; o trabalho adicional de sizing
e ledger ainda precisa de benchmark integrado.

## Quatro escopos, um registro por planejamento

`WorldGenerationTemporaryMemory` é local ao job e possui quatro posições fixas:

| Posição | Dono e término | Cobertura |
| --- | --- | --- |
| Ambient | ScopedTemporary do resultado de potentialBiomeItemsAt, até o fim do ponto x/y | Cinco capacidades de listas e backing conhecido dos valores, adotados após retorno |
| Queued | ScopedTemporary de uma colocação enfileirada, até o fim dessa iteração | Listas populadas pelos mesmos dois chamados surface/underground, observadas após população |
| Validation | Cópia privada da entrada, até terminar a validação/transferência | Previsão antes da cópia; reserva persiste através do parâmetro movido |
| Output | ScopedTemporary da lista de itens válidos, até sair do planejamento | Capacidade e payload retidos; clear libera elementos, não a capacidade |

Ambient permanece vivo enquanto os pedidos enfileirados são processados,
como antes. Não reutilizar a mesma posição para ambos nem esquecer um input
ao observar outro. A soma é por posições, com overflow verificado.

O tracker é não copiável/movível e deve sobreviver aos escopos que o referenciam;
o código declara-o antes dos valores. Tickets são locais, não handles que
podem escapar para callbacks/jobs posteriores. Cada valor é destruído antes
de seu ticket liberar a parcela. O facade do ledger é copiado e retém seu
estado independentemente do facade original ou do mundo chamador.

Um token processual é criado somente quando surge cobrança não zero ou um
diagnóstico incompleto. Com o escopo Output ativo, pode ser conservado em zero
entre pontos para evitar novos tokens por tile. Sem dados/capacidades, o caminho
vazio não cria token rastreado; charge inalterado evita locks do ledger.
Isso não significa custo de CPU zero nem ausência de todas as alocações do jogo.

## Cópia preservada e transferência de ownership

`validateWorldGenerationTemporaryCopy` mantém exatamente uma cópia de valor
dos inputs. Não move o original nem elimina a cópia que normaliza hash sets.
Uma factory prvalue inicializa o valor diretamente, por elisão garantida C++17.
A cópia é então movida como container para o parâmetro por valor existente.
PotentialBiomeItems contém cinco listas de move nothrow, sem nova cópia/move
de seus elementos nessa transferência; há assert de tipo no helper.

Sequência:

1. Reserva previsão da saída `2 × outputConhecido + 2 × inputConhecido`, somada
   aos demais escopos vivos. Trata sobreposição durante reconstrução/crescimento.
2. Reserva a parcela da cópia, usando o backing conhecido do original, **antes**
   da factory que o copia. Reconciliada depois de construir a cópia.
3. Chama o validador anterior, movendo os containers privados para seu parâmetro.
   Source original continua intacto; consultas e filtros são os mesmos.
4. Destrói o parâmetro/cópia enquanto a previsão da saída ainda está mantida;
   essa previsão cobre a ponte até observar os elementos transferidos à saída.
5. Reconcilia saída/capacidade e, depois da captura dos candidatos, clear é
   observado sem fingir que o buffer retido foi desalocado.

Essas fórmulas **não são limites superiores completos**. Capacidades de cópia/
crescimento dependem da biblioteca e parte do backing continua opaca. Durante
a operação, previsões podem superestimar ou repetir storage já transferido;
contadores representam reserva estimada, não um censo instantâneo de malloc.
O modelo não intercepta cada realocação dos buffers std::allocator originais.

Recusa da previsão não altera o valor; recusa posterior da parcela da cópia
não executa a cópia. Exceção de construção/validação libera os escopos em ordem
e reconcilia a saída parcial. A população privada enfileirada também observa
um resultado parcial antes de propagar exceção. Mutação/transferência não pode
reter referências ou valores movidos além do escopo previsto.

Falha finita de adoção/reconciliação não filtra um valor já construído: mantém
o valor, o charge anterior e marca incompletude. Predições aritmeticamente
incompletas impedem novas cópias. Release pode reduzir a cobrança, mas não
aloca/admite mais para reparar uma observação incompleta durante destruição.
O teste com limite zero e input já construído demonstra por que ainda não há
teto hard de memória profunda. Falha física de bookkeeping não foi simulada
como OOM real; pode propagar erro com destruição dos valores já construídos.

## Fórmulas, telemetria e fronteiras

`BiomeItemListBackingSizer` soma `capacity × sizeof(BiomeItemPlacement)` e o
backing conhecido de cada valor ativo. `PotentialBiomeItemBackingSizer` soma
as cinco listas, sem cobrar o objeto de stack novamente. Usam o modelo de
campos públicos dos seis variants da etapa anterior, sem Root ou travessia Json.
Json, TileDamage privados, buckets vazios/metadados do hash e overhead geral
continuam fora. SSO pode repetir bytes já presentes na representação inline.

- Classe 13: `MicroDungeonTemporaryEstimate`, estimativa, não raw buffer.
- `generation.memory.process.sampledTemporaryEstimateTotal`: amostra dessa
  classe, incluindo previsões transitórias quando presentes.
- `generation.memory.temporaries.modelVersion`: máximo **1**.
- Modelo geral de estimativas: **6**, pela nova cobertura de temporários.
- Modelos de distribuições **2**, payloads/nomes/buffers/cache de worker **1**
  e fórmulas anteriores de Json não mudaram.

`sampledBufferResourcesTotal` não inclui a classe 13. Totais/picos processuais
somam a classe estimada separadamente dos buffers fixos. Cobrança básica não
depende do profiling; amostras exigem o coletor existente e usam
`generation.memory.process.samples` como denominador. Vários mundos observam
o mesmo processo: não somar coletores como memórias independentes.

O flag de incompletude é local ao tracker, com diagnóstico processual quando
uma observação não puder ser reconciliada; não foi adicionado ao payload de
planos preparados. Também não foi criado censo global dos itens opacos.

## Validação executada

Ambiente cloud conferido pela skill operacional; ferramentas/dependências
existentes, sem instalação/download de dependências. GCC 14.2.0/C++17 Debug
(`-Og`), GUI/Tracy/allocators alternativos OFF, `VCPKG_MANIFEST_INSTALL=OFF`.
C++: `-fsanitize=address,undefined -fno-omit-frame-pointer -DXXH_NO_INLINE_HINTS=1`.
C/dependências pré-compiladas não têm a mesma instrumentação C++. Identifier:
`956ef66e-temporary-storage-local-dirty`; fontes do commit/manifesto abaixo.

Com `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1` e
`ASAN_OPTIONS=halt_on_error=1`, sem diagnóstico dos sanitizers:

1. **246/246 testes**, dezesseis suites; **23 novos** de temporários.
2. Novos casos repetidos 20 vezes, embaralhados com seed GTest 1217:
   **460 execuções adicionais**, não 460 casos únicos.
3. **16/16 grupos CTest** `^world_generation_contract_`, NoAssets; repetem
   casos do binário, não outros 246 testes distintos.
4. Build dos contratos e quatro objetos reais de geração, template, ledger e
   sampling aprovada. WorldTemplate/sampler não tiveram seu código editado,
   mas os objetos foram recompilados pelas dependências. Sem linking do jogo.
5. Sintaxe/tipos de geração, template e teste novo com
   `STAR_ENABLE_TRACY`/`TRACY_ENABLE` e headers Tracy 0.11.2 existentes aprovada;
   não é build/linking Tracy completa. Aviso herdado de fmt permanece.
6. `git diff --check` aprovado; 381 links locais em 29 documentos sem destino
   ausente, 14 documentos da coleção alcançáveis, 49 contratos únicos no
   roadmap/catálogo, ordem 1–49 e oito totais de fase conferidos. H0 25% e
   programa 3,1% recalculados; nove fontes do manifesto conferidas em
   tamanho/SHA-256. Nenhuma validação manual do jogo foi presumida.

Casos: fontes vazias sem token, previsão antes da factory/cópia, quatro posições
coexistentes, posição inválida/ocupada, reutilização do token, recusa nas duas
fronteiras de admissão, rollback/retry, factory/cópia/validação/população lançantes,
overflow, diagnóstico de adoção insuficiente, destructor com reserva presente,
facade fechado, capacidade de scratch após clear, Json opaco preservado,
WorkerPool real e oito jobs concorrentes com saldo final zero. Não é TSan.

Comparação nativa: 24 pontos com surface/ocean/cave floor/ceiling/background,
entradas ambientais/enfileiradas, **432 candidatos** micro/não micro, empates,
IDs, mesmas capacidades/ordem e escolhas em 64 seeds por candidato micro.
Hash sets com colisões/wrap exercitam cópias não idempotentes; controles
negativos anteriores continuam na suite. Usa valores dos BlockInfo de teste
e helpers reais, não WorldStorage/WorldTemplate com assets ou streaming real.

Reprodução, após configurar build equivalente, a partir da raiz:

```bash
cmake --build <build-dir> --target world_generation_contract_tests \
  game/CMakeFiles/star_game.dir/StarWorldGeneration.cpp.o \
  game/CMakeFiles/star_game.dir/StarWorldTemplate.cpp.o \
  game/CMakeFiles/star_game.dir/StarWorldGenerationMemory.cpp.o \
  game/CMakeFiles/star_game.dir/StarBiomePlacementSampling.cpp.o -j3
UBSAN_OPTIONS=halt_on_error=1 ASAN_OPTIONS=halt_on_error=1 \
  ./dist/world_generation_contract_tests --gtest_brief=1
UBSAN_OPTIONS=halt_on_error=1 ASAN_OPTIONS=halt_on_error=1 \
  ./dist/world_generation_contract_tests --gtest_filter='WorldGenerationTemporaryTest.*' \
  --gtest_repeat=20 --gtest_shuffle --gtest_random_seed=1217 --gtest_brief=1
UBSAN_OPTIONS=halt_on_error=1 ASAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir <build-dir> -R '^world_generation_contract_' --output-on-failure
```

## Limites, progresso e continuação

Nenhum config/log/Tracy novo coletado/fornecido ou captura anterior reanalisada;
nenhum arquivo do usuário alterado nem dado bruto adicionado ao Git. Não
executados jogo/assets/mods, save/reload, multiplayer, TSan, OOM real, benchmark
de overhead/throughput, Windows/macOS/ARM ou allocators alternativos. Sem
Actions, release, merge em main ou revisão independente. Compatibilidade com
mods/saves/multiplayer e CPU fallback continuam essenciais.

Há sizing adicional de listas/valores, fold constante de quatro parcelas e
sincronização do ledger quando o charge muda. Caminho vazio evita token, não
todo esse trabalho. Previsões podem superestimar; Json/privados/overhead e picos
anteriores à adoção podem subestimar. Não ampliar admissão/defaults com o
modelo parcial nem confundir testes locais com aumento de setores/s.

RAM-01 e H0 permanecem em **25%**, programa em **3,1%**: avanço dentro do marco
inicial, não conclusão do contrato de orçamento processual. Próximas ações:

1. Json compartilhado, internals de TileDamage e buckets/metadados do hash:
   identificar storage/lifetime únicos e evitar cobrança por cada referência.
2. Consolidar o envelope coberto, exclusões, pressão/reação e limites por
   hardware/configuração; layout/biomas/seletores/decode continuam parciais.
   Não exigir cada heap do motor para um protótipo H1 em sombra, mas limitar
   explicitamente o escopo antes da ampliação correspondente.
3. VAL-02/MOD-01 e baseline integrado: comparar outputs/efeitos completos,
   streaming com rota/save/config/assets/mods fixos e custo com/sem profiling.

CPU-01 conserva overflow da seed automática e auditoria de Perlin Uninitialized,
sem alterar RNG nesta etapa. NPC externo suspenso e Linux Clang adiado.

## Manifesto das fontes

Fontes locais coletadas/conferidas em 09/11/2026, nomes relativos ao checkout,
bytes e SHA-256. Nenhum novo artefato de execução do usuário nesta etapa.

| Arquivo | Bytes | SHA-256 |
| --- | ---: | --- |
| `source/game/CMakeLists.txt` | 16465 | `7c8a228f13f2f1d386d6cb590ab5d8e57fa8f3a3b2264ebae10560d669c37ffb` |
| `source/game/StarBiomeItemSelection.hpp` | 3162 | `2b179cdf724a512bed38d13716a30610f90c9d8399640c905bf6e6d0e045e06b` |
| `source/game/StarWorldGeneration.cpp` | 125845 | `5b198c14ff8b0b41f66eb36942291cfcda2c70fa42df807f67d6782f9117b2f6` |
| `source/game/StarWorldGenerationMemory.hpp` | 4995 | `69341f5af734fd7b740c85c93117bc357e0660e2acdc6dd458cc1fdcbd84fe8c` |
| `source/game/StarWorldGenerationMemoryEstimate.hpp` | 1460 | `e939f91504eada6a5872e6cf409b05faf162683b05714c8f93ba527d91fd169b` |
| `source/game/StarWorldGenerationTemporaryMemory.hpp` | 7870 | `257867aebf3675febedb89b025e20799bb09880a1ad51b19e225f362a20c2d7a` |
| `source/test/CMakeLists.txt` | 5369 | `153121cfae5a5295b4d886933959fa19dd9bf03442fb14ecded52459a34f407e` |
| `source/test/world_generation_estimate_test.cpp` | 11802 | `b2bb7dcf102308fb4d6ac4b5286c3aa92b2ef9ff93701036a6a00a1a6ada51b8` |
| `source/test/world_generation_temporary_test.cpp` | 22725 | `89987c275879b965688bcd7d64f6e94e2873813ef043a4dbcdbeadbdd875b1b2` |
