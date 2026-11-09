# H0 / RAM-01: retenção do template privado e storage da LRU

Data: 09/11/2026. Branch de entrega: `Experimental`; revisão local:
`codex/experimental-regional-runtime`. Base: `54312ffd31ff51dd9149dd069e4804a03d0ca274`.
Implementação: `739067b1b7f4981a5dbc0c418eab78687773b4a2`.

[Programa principal](sector-runtime/README.md) · [Roadmap](sector-runtime/roadmap.md)
· [Inventário](sector-runtime/memory-and-prefetch.md)
· [Recorte anterior](sector-runtime-h0-retained-estimates-2026-11-09.md)

## Objetivo e resultado

Continuar RAM-01 pelo cache privado de `workerWorldTemplate`: identificar sua
retenção após o job/mundo e contabilizar storage que contar entradas ocupadas
não revela. A classe nova `WorkerTemplateEstimate` acompanha o objeto privado
e as reservas declaradas de seu allocator de cache de blocos. A contagem
permanece observacional, **não é orçamento global nem toda a RAM**.

O modelo do cache é v1, separado do modelo v1 de Json/metadados/planos do
relatório anterior. A cobrança básica do cache funciona sem profiling;
publicar suas amostras exige o coletor existente. As estimativas profundas de
Json/metadados/planos continuam vinculadas ao profiling regional.

Não houve ampliação de workers, cotas, prefetch, defaults ou mudança de
publicação. Não há ganho de setores/s medido; esta entrega reduz uma lacuna
de contabilidade antes de aumentar admissão.

## Auditoria de ownership

`workerWorldTemplate` já conservava **um template por thread**, compartilhado
pelos jobs BaseTiles e de planejamento dessa thread. Uma mudança de fonte ou
revisão reconstruía o template; fechar o mundo não destruía o payload ocioso
porque a identidade da fonte era fraca e o cache mantinha uma referência forte
ao template reconstruído. A nova implementação preserva essa política.

| Parte | Retenção observada no código | Cobertura desta entrega |
| --- | --- | --- |
| `WorldTemplate` privado | cache thread-local, até substituição/saída da thread; uso em curso pode prolongar a entrada antiga | objeto `sizeof(WorldTemplate)` |
| `m_blockCache` | LRU privada; expulsa entradas, mas pode reter bloco livre, buckets e capacidade do vetor de blocos | storage solicitado às famílias do BlockAllocator, inclusive slots vazios/retidos |
| Fonte do mundo | identidade por weak_ptr; jobs ainda capturam a fonte enquanto executam | nenhuma nova referência forte no cache |
| `WorldLayout` | reconstruído do Json; possui listas, regiões, biomas e seletores privados | heaps ainda fora; não substituir esse tamanho pelo Json serializado |
| Parâmetros/céu/clima/terrenos personalizados | campos inline no template e listas/strings/Json/objetos apontados | somente campos inline; backing dinâmico ainda fora |
| Json serializado | caches de inputs/jobs; reconstrução pode compartilhar subnós internos | mantém o tracker anterior; cache privado não passa a reter artificialmente esse tracker |

`WorldLayout(Json)` cria biomas, carrega seletores e materializa regiões.
Logo bytes de serialização não são uma estimativa confiável desse grafo
decodificado. O construtor de template ainda usa Root/assets e a inicialização
RNG anterior: esta entrega não conclui a auditoria de CPU-01/MOD-01.

## Implementação e segurança do lifecycle

`WorldGenerationWorkerCache<Source, Payload>` mantém uma entrada privada,
fonte fraca e revisão. Não é copiável; mover o cache não copia o payload.
Uma reserva inicial antecede construir a entrada/template. Fonte e revisão
só são publicadas após construção bem-sucedida; produtor que lança exceção,
retorna nulo ou encontra recusa inicial não substitui o cache anterior.

O uso do template ocorre por um escopo `Use`, não por uma referência solta
que deva sobreviver ao tracker. Esse escopo conserva entrada/token e atualiza
o tamanho ao terminar, inclusive no unwinding da geração. Uma entrada antiga
ainda usada sobrevive à substituição; o token é destruído depois do payload.
Mover o escopo transfere a atualização, sem executá-la duas vezes.

O job recebe uma cópia do facade do ledger capturada na dona. O estado do
ledger permanece compartilhado, sem chamada a singleton/mundo/coletor no fim
do job. A entrada retém o token, não o facade nem a fonte. Fechar o mundo não
apaga a cobrança enquanto o cache ainda existe. Saída da thread libera a
entrada; não foi acrescentada expulsão de cache ocioso ou limpeza por mundo.

Falha de sizing/resize conserva a cobrança anterior, marca o estado da entrada
como incompleto e incrementa um contador processual saturado. Não substitui
a exceção original do kernel nem rejeita seu output apenas por diagnóstico.
A recusa da reserva inicial continua explícita; no runtime o teto é ilimitado
salvo overflow, como nos recortes anteriores.

## Storage do allocator e custo

`BlockAllocator::requestedStorageBytes()` soma cada tipo de sua família de
rebind uma vez: `sizeof(Data)`, capacidade do vetor de ponteiros de blocos,
quantidade de blocos × `sizeof(Block)` e bytes de requests multi-elemento
ainda vivos. Essas últimas requests são registradas depois de alocação
bem-sucedida e retiradas na deallocation. Falha de alocação não acrescenta
bytes. Cópias/rebinds observam o mesmo estado; famílias independentes não se
misturam. Não há mutex novo dentro do allocator: continua restrito à dona.

Os accessors do hash/ordered map e LRU encaminham essa informação nos tipos
com BlockAllocator. As famílias independentes do índice e da lista ordenada
são somadas uma vez. `generationCacheStorageBytes()` acrescenta
`sizeof(WorldTemplate)`; não duplica o objeto inline da LRU.

O getter de allocator da lista pode criar bookkeeping de um rebind na primeira
consulta; consultas subsequentes não percorrem entradas. A soma percorre os
tipos de rebind, não tiles, nós de lista nem blocos individuais. Há contador
adicional no caminho de requests multi-elemento e um ponteiro de sizing por
tipo no registro da família, inclusive em outros usos de BlockAllocator.
A política de alocação, block size, eviction e hashing não mudou, mas o custo
de bookkeeping existe e **ainda precisa de benchmark**.

### Limites da medição

- Não inclui hash buckets/entradas do registro da própria família, control
  blocks, overhead de malloc/alinhamento, wrapper do cache ou heaps apontados.
  Não equivale a RSS, malloc usable size ou limite superior de memória.
- O ledger atualiza o cache após reconstrução e no fim do job, não a cada
  crescimento. Durante o kernel pode mostrar o valor anterior; não captura
  picos transitórios de rehash/cópia. O teste integrado verifica essa fronteira.
- `estimateComplete` significa atualização bem-sucedida **desse modelo**, não
  cobertura do grafo privado completo. Ausência de falhas não autoriza teto
  hard baseado nesta contagem parcial.
- A LRU vazia pode continuar cobrando storage: `clear()` não implica liberar
  os buckets/blocos. Zero no ledger privado não prova que o heap global acabou.
- A implementação não coleta amostras de tempo próprias para o sizing do
  cache. O elapsed do worker inclui esse custo; não interpretá-lo isoladamente
  como mudança de custo do kernel ou ganho de streaming.

## Telemetria

Sob `generation.memory.process.`:

| Campo novo | Semântica |
| --- | --- |
| `sampledWorkerTemplateEstimateTotal` | soma amostrada de objeto + allocator do cache privado |
| `sampledWorkerTemplatesTotal` | soma amostrada das entradas retidas, inclusive antigas ainda em uso |
| `lifetimeIncompleteEstimateUpdates` | máximo do contador cumulativo de atualizações incompletas do processo; não número de entradas atualmente incompletas |

`generation.memory.estimates.workerCacheModelVersion` é máximo 1. O
`modelVersion` anterior continua 1 para suas fórmulas. `sampledEstimatedBytesTotal`
e `lifetimePeakEstimatedBytes` passam a incluir a nova classe; parcelas de
arrays conservam sua semântica. Não somar o total às parcelas novamente.

Amostras seguem a frequência anterior por coletor; para média, dividir as somas
por `generation.memory.process.samples`. Coletores diferentes veem o mesmo
processo, não memórias independentes. Contagem básica existe sem profiling;
ausência de relatório não demonstra zero. Todas as chaves são fixas, sem nomes,
endereços ou conteúdo de mundo/mod.

## Validação executada

Ambiente cloud conferido pela skill operacional, sem instalar pacotes.
Reutilizada a build headless GCC 14.2.0/C++17 Debug, `-Og`, ASan/UBSan,
`STAR_BUILD_GUI=OFF`, `STAR_ENABLE_TRACY=OFF`, allocators alternativos OFF.
C++ usa `-fsanitize=address,undefined -fno-omit-frame-pointer` e o workaround
`-DXXH_NO_INLINE_HINTS=1`; C mantém somente esse workaround. Dependências
pré-compiladas/C não recebem a mesma cobertura dos sanitizers C++.
Identifier: `54312ffd-worker-cache-local-dirty`; manifesto abaixo identifica
o código validado e commitado em `739067b1`.

Com `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1` e
`ASAN_OPTIONS=halt_on_error=1`, sem diagnóstico dos sanitizers:

1. Build CMake do binário de contratos: **136/136 testes** em dez suites,
   incluindo **21 novos casos** em `WorldGenerationWorkerCacheTest`.
2. Os 21 novos, embaralhados com seed GTest `1211`, repetidos 20 vezes:
   **420 execuções adicionais** aprovadas, não 420 casos únicos.
3. CTest `^world_generation_contract_`: **10/10 grupos NoAssets** aprovados;
   repetem os casos do binário, não acrescentam outros 136 casos.
4. Compilação dos objetos reais de `StarWorldGeneration.cpp` e
   `StarWorldTemplate.cpp`, além das dependências core/base: concluída.
   Isso não é linking completo de cliente/servidor.
5. Sintaxe/tipos de geração/template com `STAR_ENABLE_TRACY`/`TRACY_ENABLE`
   e headers Tracy 0.11.2 existentes: aprovada, sem build/linking Tracy completo.
6. `git diff --check` aprovado; 223 links locais em 20 documentos sem destino
   ausente, 13 módulos alcançáveis, 49 IDs únicos referenciados no roadmap e
   14 fontes do manifesto conferidas em tamanho e SHA-256.

Casos incluem reserva antes da construção, cache hit sem reconstrução,
substituição de revisão, fonte fraca/retida, falha/null/refusal sem perder cache,
destruição payload→token, unwinding, scope movido, sizing incompleto e retorno
do payload preservado. Allocator: cópia/rebind, famílias distintas, request
multi-elemento, falha de request, pools liberados/retidos, LRU e hash map com
16.384 entradas. O teste integrado LRU/cache verifica cobrança só nas fronteiras.

WorkerPool real: cache thread-local sobrevive ao facade; oito produtores
concomitantes em oito threads mantêm oito entradas distintas (8 × 64 bytes
**sintéticos declarados**), e `finish()` deixa saldo zero. Gates controlados,
sem sleeps; testes não instanciam um WorldTemplate real com assets.
ASan/UBSan/stress não substituem TSan, não executado.

Reprodução após configurar uma build equivalente:

```bash
cmake --build <build-dir> --target world_generation_contract_tests \
  game/CMakeFiles/star_game.dir/StarWorldGeneration.cpp.o \
  game/CMakeFiles/star_game.dir/StarWorldTemplate.cpp.o -j2
UBSAN_OPTIONS=halt_on_error=1 ASAN_OPTIONS=halt_on_error=1 \
  ./dist/world_generation_contract_tests --gtest_brief=1
UBSAN_OPTIONS=halt_on_error=1 ASAN_OPTIONS=halt_on_error=1 \
  ./dist/world_generation_contract_tests --gtest_filter='WorldGenerationWorkerCacheTest.*' \
  --gtest_repeat=20 --gtest_shuffle --gtest_random_seed=1211 --gtest_brief=1
UBSAN_OPTIONS=halt_on_error=1 ASAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir <build-dir> -R '^world_generation_contract_' --output-on-failure
```

Warnings herdados de fmt e de move redundante na aplicação do plano não foram
declarados corrigidos. Sem jogo/assets/mods, Windows/macOS/ARM, rota de streaming,
save/reload, multiplayer, A/B de performance, build completa ou Actions.

## Achado de exceção ainda pendente

Inspeção de `BlockAllocator::allocate(1)` revelou uma fronteira antiga a
cobrir antes de testar pressão de memória: ao criar um bloco, atribui
`unfilledBlock` antes de inserir o unique_ptr no vetor de blocos. Se essa
inserção precisar crescer o vetor e lançar exceção, o bloco local pode ser
destruído com o ponteiro de seleção ainda publicado. Isso é um risco de
ponteiro pendente inferido do código, **não um crash reproduzido nesta run**.

O teste novo de falha cobre requests multi-elemento, não fault injection
dessa inserção interna. A ordem antiga não foi alterada neste recorte.
Próximo trabalho deve oferecer reprodução controlada e publicar o ponteiro
somente depois da inserção bem-sucedida, com garantia de recuperação do pool.
O overflow de seed automática em `StarRandom.cpp:197` também continua aberto
em CPU-01; não houve mudança do RNG.

## Configuração, compatibilidade e manifesto

Nenhum config, log ou Tracy novo foi fornecido/coletado; não houve reanálise de
capturas anteriores ou alteração de arquivo do usuário. Dados brutos/caches
continuam fora do Git. Não há nova chave de `starbound.config`.

Pelo diff, preservados identidade da fonte/revisão, seleção/construção do
template, cache LRU, seeds/RNG, TTL, pré-requisitos, níveis e commit serial.
Nenhuma chamada a Lua, save, rede ou mundo vivo foi acrescentada aos workers.
Equivalência integrada com assets/mods ainda não foi demonstrada pelos testes
sintéticos. Compatibilidade continua um gate obrigatório.
NPC externo suspenso, Linux Clang adiado; sem release, merge em `main` ou
revisão independente nesta atividade.

Fontes conferidas em 09/11/2026, bytes e SHA-256:

| Arquivo | Bytes | SHA-256 |
| --- | ---: | --- |
| `source/core/StarBlockAllocator.hpp` | 9020 | `88774ea66637b1e974d67f6fd32e04ded2909e2fa68022fadf5ae8eeac6c8319` |
| `source/core/StarFlatHashMap.hpp` | 22849 | `f01107900d85cc744c4a092f3082dff8821aaf647885201028b82a6103d252c5` |
| `source/core/StarFlatHashTable.hpp` | 20725 | `35b7d095bd41a03f623c92cf42fabf68edf72b6eef55088579d777d3c986241f` |
| `source/core/StarLruCache.hpp` | 4161 | `4f805155660d54c886d7a9bf9bbb458cb809c3a4c6e81332a1776b47a2c446b9` |
| `source/core/StarOrderedMap.hpp` | 26404 | `ec8b19c95bcb9dfdcb9b6d98471a9962276e755a44c101459311221531bca6d3` |
| `source/game/CMakeLists.txt` | 16127 | `fd6278302299ca7be227c71b81897c4a23a264111a5c61e28a097b65e89738e0` |
| `source/game/StarWorldGeneration.cpp` | 119975 | `53bf4d2ab7e5dffcddd704b3b09ebcb337a0d8846764ebcab33e79c00e0d5a2e` |
| `source/game/StarWorldGenerationMemory.cpp` | 7238 | `f3117372d4408156c9a90df7c24c9682c360c8a7b84c5114e17d9939a7912d5a` |
| `source/game/StarWorldGenerationMemory.hpp` | 4116 | `760e214560c1dff0519cc444008289dfafae16005fc293766a7804ec34d28629` |
| `source/game/StarWorldTemplate.cpp` | 36791 | `d3a05edf044ecbe68583cbbd36a5dc19e92e18f6dd6afad49a25ac5cf5d4d5b0` |
| `source/game/StarWorldTemplate.hpp` | 7790 | `0b097fc451073b47b07ec29f1a04937d75f2290b7734aee5ef4577e983eb64d1` |
| `source/test/CMakeLists.txt` | 4854 | `2f4a110fa2a486c3f83a4a2707eef0ecbcb6c4b0e7025516e97bb39a0a18ce6a` |
| `source/game/StarWorldGenerationWorkerCache.hpp` | 2906 | `0448b69dd292c6a4765f49e0877afbb05475821ff56fc03141884e23c492b17b` |
| `source/test/world_generation_worker_cache_test.cpp` | 14016 | `d841bdf5f4254f0824ec9a540e0848caa4591251a92debfd300eb98816eba5a7` |

## Próximos passos executáveis

1. Reproduzir/corrigir a publicação antecipada de `unfilledBlock` e testar
   falhas na criação/inserção do bloco antes de ensaios de pressão.
2. RAM-01: cobrir crescimento/construção de outputs variáveis e caminho
   síncrono; ampliar o inventário para layout/biomas/seletores e metadados
   ainda fora, com deduplicação correta. Coleta obrigatória não pode depender
   de profiling ou de estimativas parciais silenciosas.
3. Manter MOD-01 e ampliar VAL-01/02 para streaming real e outputs com
   assets/mods. Medir overhead e ganho ponta a ponta antes de ampliar admissão.

RAM-01 completo/H0 continuam abertos; a nova WorldThread, executor H1 e GPU
permanecem futuras entregas, não efeitos desta contabilidade.

### Continuação posterior — `1bd348e6`

O [relatório seguinte](sector-runtime-h0-allocator-exception-2026-11-09.md)
reproduz o estado inválido inferido acima e corrige a publicação da seleção
do pool. São 144 testes, onze grupos CTest e oito casos novos repetidos 100
vezes (800 execuções adicionais). A injeção não é OOM real; não houve vínculo
demonstrado com capturas/crashes anteriores. Este documento preserva o estado
histórico de `739067b1`; o relatório seguinte governa a retomada de RAM-01.
