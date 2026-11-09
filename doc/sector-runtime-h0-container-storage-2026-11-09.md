# H0 / RAM-01: buckets de nomes e payload dos fatores de dano

Data: 09/11/2026. Entrega: `Experimental`; revisão local:
`codex/experimental-regional-runtime`. Base: `82a12abda9d2371bd141e9d338927ee8674bed18`.
Implementação: `730e36ba390ebd75b41680776126be01d2e6e15b`.

[Programa](sector-runtime/README.md) · [Roadmap](sector-runtime/roadmap.md)
· [Progressão](sector-runtime/progress.md) · [Inventário](sector-runtime/memory-and-prefetch.md)
· [Etapa anterior](sector-runtime-h0-temporary-storage-2026-11-09.md)

## Objetivo e resultado

Reduzir duas exclusões do modelo de RAM-01 sem alterar representação,
alocação, cópias ou seleção. O backing conhecido de BiomeItem agora inclui o
payload completo do vetor de buckets de MicroDungeonNames e os pares ocupados
dos mapas de fatores de dano de GrassVariant, BushVariant e ambos os TreeVariant
de TreePair. Distribuições, candidatos e temporários usam esse mesmo sizing.

Não implementa novo executor, thread regional, GPU, prefetch ou maior admissão.
Não muda config/defaults, workers, orçamento de tempo, TTL, níveis, RNG,
WorldTemplate, filtros/sort ou publicação serial. É cobertura de memória
parcial, não ganho de setores/s demonstrado nem conclusão de RAM-01.

## Buckets: payload solicitado, não apenas elementos ocupados

`FlatHashTable::bucketStorageBytes()` retorna
`m_buckets.capacity() × sizeof(Bucket)`; FlatHashSet expõe a consulta const.
Inclui objetos de chave inline, slots vazios, sentinela, hash e padding da
representação privada. Um set vazio com reserve pode ter cobrança não zero;
erase/clear não significam que seu buffer foi liberado. Não altera campos,
constructors, allocators, capacidade, hash, rehash, iteração ou normalização.

A consulta é `noexcept`, O(1), sem alocar, percorrer buckets ou recalcular hash.
Não inclui backing das strings, metadados de malloc ou blocos retidos por um
allocator de pool. `allocatorStorageBytes()` continua separado e inalterado:
o diagnóstico de família do BlockAllocator/LRU não foi substituído pelo novo
diagnóstico de payload de um único vetor.

Antes, MicroDungeonNames somava somente `size × sizeof(String)` e capacidades
UTF-8. Agora **substitui** a primeira parcela pelo vetor completo; não soma os
objetos String novamente. A mesma varredura das strings preserva o modelo
`capacity + 1`; SSO pode superestimar porque parte do texto já é inline.

Cada cópia tem capacidade própria. A cópia nativa de um set com grande reserva
pode normalizar para um vetor menor; a reconciliação posterior usa a capacidade
do destino. Não contornar essa diferença eliminando cópias: colisões com wrap
podem mudar a ordem/seleção mesmo em cópias de mesma capacidade, como mostram
os controles negativos anteriores. Aumento dos bytes reportados não implica
nova alocação, leak ou aumento do consumo real por esta mudança.

## TileDamage: valores conhecidos, nós ainda parciais

`TileDamageParameters::damageFactorPayloadBytes()` retorna
`m_damages.size() × sizeof(Map<TileDamageType, float>::value_type)`.
É consulta const O(1), `noexcept`, sem iterar ou alocar. O campo continua o
mesmo Map/std::map com allocator padrão, mesma ordem e layout. Não se presume
que o tamanho do nó red-black seja igual ao de seu par chave/valor.

O estimador adiciona essa parcela em grass, bush e nas duas árvores do par.
Ponteiros, cor/padding dos nós, bookkeeping de malloc e Json associado seguem
fora. Não é uma medição de requests completos de std::map nem do heap total
de TileDamage. Os variants continuam marcados opacos; MicroDungeonNames não
passa a marcar Json inexistente como opaco. O friend de teste permite fixtures
tipadas sem Root/assets, sem criar novo estado no objeto.

Consultas e fatores de dano, sum, Json e operadores de serialização de produção
não foram editados. O teste compara bytes da serialização do mapa de fatores
usando DataStream real; não é uma prova integrada de todos os formatos de save.

## Modelos e telemetria

| Modelo | Anterior → atual | Motivo |
| --- | --- | --- |
| `generation.memory.estimates.modelVersion` | 6 → **7** | Nova cobertura de buckets e valores dos fatores |
| `generation.memory.distributions.modelVersion` | 2 → **3** | Backing do bundle usa o estimador ampliado |
| `generation.memory.candidatePayloads.modelVersion` | 1 → **2** | Backing dos candidatos/factory usa o estimador ampliado |
| `generation.memory.temporaries.modelVersion` | 1 → **2** | Listas potenciais, cópia e scratch usam o estimador ampliado |

Nomes de saída, buffers e cache do worker permanecem no modelo próprio **1**.
Não foram adicionadas classes ou métricas. Classes estimadas 9/12/13 absorvem
a cobertura nas previsões/reconciliações existentes; as mesmas limitações de
adoção após construção, picos intermediários e pressão processual permanecem.
Cobrança básica desses donos continua independente do profiling, mas amostras
exigem o coletor já existente. Json/template/metadata têm suas próprias
condições anteriores, não modificadas aqui.

Não comparar totais de modelos diferentes como uma variação real de RAM sem
separar a mudança de fórmula. `complete` significa completude aritmética/do
modelo percorrido, não cobertura integral do heap. Não ampliar filas ou cotas
com base nessa parcela parcial.

## Validação executada

Ambiente cloud conferido pela skill operacional; sem instalação/download de
dependências. GCC 14.2.0/C++17, Debug `-Og`, GUI/Tracy/allocators alternativos
OFF, `VCPKG_MANIFEST_INSTALL=OFF`. C++ com
`-fsanitize=address,undefined -fno-omit-frame-pointer -DXXH_NO_INLINE_HINTS=1`;
C/dependências pré-compiladas não têm a mesma instrumentação. Identifier:
`82a12abd-container-storage-local-dirty`; fontes do commit/manifesto abaixo.

Com `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1` e
`ASAN_OPTIONS=halt_on_error=1`, na versão final sem diagnóstico dos sanitizers:

1. **266/266 testes**, dezessete suites; **20 novos** neste recorte.
2. Novos casos repetidos vinte vezes, embaralhados com seed GTest 1218:
   **400 execuções adicionais**, não 400 casos únicos.
3. **17/17 grupos CTest** `^world_generation_contract_`, NoAssets; repetem
   os casos do binário, não são outros 266 casos distintos.
4. Build dos contratos e objetos reais de geração, template, sampling e
   TileDamage aprovada. A mudança no header core recompilou dependências;
   nenhum linking completo do jogo foi executado.
5. Sintaxe/tipos de geração, sampling, TileDamage e teste novo aprovados com
   `STAR_ENABLE_TRACY`/`TRACY_ENABLE` e headers Tracy 0.11.2 existentes.
   Não equivale a build/linking Tracy completa. Aviso herdado de fmt e nota
   de ABI GCC no fixture de alinhamento 64 não impediram a compilação.
6. `git diff --check` aprovado; **394 links locais em 30 documentos** sem
   destino ausente, 14 módulos alcançáveis, 49 contratos únicos cobertos pelo
   roadmap e ordem 1–49 conferida. Oito totais de fase e 6/196 marcos = 3,1%
   recalculados; RAM-01/H0 mantidos em 25%. Nove fontes do manifesto conferidas
   em tamanho/SHA-256. Nenhuma validação manual do jogo foi presumida.

O allocator de observação existe **só no teste**, delega ao std::allocator e
compara o payload reportado com os pedidos ativos, inclusive alinhamento 64.
Uma primeira tentativa do harness falhou porque o registro não era compartilhado
com o vetor temporário default-constructed usado no crescimento da tabela.
O fixture foi corrigido para usar uma família intercambiável no escopo do teste;
nenhum allocator ou algoritmo de produção foi modificado por essa falha.

Casos novos cobrem set padrão/reservado vazio, 256 inserções/crescimentos,
erase/clear, consulta sem hash/alocação/mudança de ordem, cópia e normalização
de capacidade, move, padding/sentinela, heap textual excluído da parcela de
buckets, fórmula nativa sem cobrança duplicada, reconciliação em snapshot,
scratch com nomes vazios mas reservados, fatores padrão/copias/valores/bytes,
grass/bush/ambas as árvores e oito leitores simultâneos. Não é TSan.

As suites anteriores também passaram, incluindo os replays nativos de 96/432
candidatos e os controles negativos de colisões/wrap. Não é execução de
WorldStorage/WorldTemplate com assets ou streaming de um planeta real.

Reprodução, após configurar build equivalente, a partir da raiz:

```bash
cmake --build <build-dir> --target world_generation_contract_tests \
  game/CMakeFiles/star_game.dir/StarWorldGeneration.cpp.o \
  game/CMakeFiles/star_game.dir/StarWorldTemplate.cpp.o \
  game/CMakeFiles/star_game.dir/StarBiomePlacementSampling.cpp.o \
  game/CMakeFiles/star_game.dir/StarTileDamage.cpp.o -j3
UBSAN_OPTIONS=halt_on_error=1 ASAN_OPTIONS=halt_on_error=1 \
  ./dist/world_generation_contract_tests --gtest_brief=1
UBSAN_OPTIONS=halt_on_error=1 ASAN_OPTIONS=halt_on_error=1 \
  ./dist/world_generation_contract_tests --gtest_filter='WorldGenerationContainerStorageTest.*' \
  --gtest_repeat=20 --gtest_shuffle --gtest_random_seed=1218 --gtest_brief=1
UBSAN_OPTIONS=halt_on_error=1 ASAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir <build-dir> -R '^world_generation_contract_' --output-on-failure
```

## Limites, progresso e continuação

Nenhum config/log/Tracy novo fornecido/coletado, nem captura anterior reanalisada.
Nenhum arquivo do usuário alterado ou dado bruto adicionado ao Git. Não
executados jogo/assets/mods, save/reload, multiplayer, TSan, OOM real, benchmark
de overhead/throughput, Windows/macOS/ARM ou allocators alternativos. Sem
Actions, release, merge em main ou revisão independente.

RAM-01 e H0 continuam em **25%**, programa em **3,1%**. A consulta de bytes
ficou mais completa sem alterar o consumo que observa; não há outro marco
cumulativo concluído. O contrato ainda não oferece teto processual profundo
nem reação a pressão/hardware. Compatibilidade com mods/saves/multiplayer e
fallback CPU continuam essenciais; NPC externo suspenso e Linux Clang adiado.

Próximas ações executáveis:

1. Json compartilhado e bookkeeping/allocator dos nós de mapas: identificar
   storage/lifetime únicos antes de cobrar, sem multiplicar por referência ou
   trocar representação/cópias que possam afetar compatibilidade.
2. Consolidar envelope, exclusões de layout/biomas/seletores/decode, pressão e
   limites por hardware/configuração. Não exigir cada heap do motor para H1
   em sombra, mas delimitar o escopo antes de ampliar a admissão correspondente.
3. VAL-02/MOD-01 e baseline integrado: outputs/efeitos completos e streaming
   com rota/save/config/assets/mods fixos; medir throughput e custo do sizing
   com/sem profiling antes de alegar aceleração.

CPU-01 conserva overflow da seed automática e metadata Perlin Uninitialized
como pendências. Não foram corrigidos por consultas const de memória.

Continuação em `45db2402`: o
[recorte do Json](sector-runtime-h0-json-storage-2026-11-09.md) cobre buckets
completos/chaves de JsonObject por owner, com omissão explícita de objetos
que não cabem no limite. Ver esse relatório para modelos, testes e próximos
passos vigentes; os resultados e manifesto deste arquivo são históricos.

## Manifesto das fontes

Fontes locais coletadas/conferidas em 09/11/2026, nomes relativos ao checkout,
bytes e SHA-256. Nenhum novo artefato de execução do usuário nesta etapa.

| Arquivo | Bytes | SHA-256 |
| --- | ---: | --- |
| `source/core/StarFlatHashSet.hpp` | 19334 | `5cf7ebdd20353e18073db76f98abd417815505504dce8948cb0e874d73ff83c7` |
| `source/core/StarFlatHashTable.hpp` | 21193 | `fd4d470dd12540f58c5b410abb9927ff9805ea333d6aea425b0f6835981a924e` |
| `source/game/StarBiomePlacementSampling.cpp` | 5395 | `cafa3d6750d33ec6b25356f700d62351eeb7cc0e097afcac8947dbb2b1e4d4ad` |
| `source/game/StarTileDamage.hpp` | 4555 | `e8fbcb36016552e9a08f4f187fc4f3e10b236e9323f8f132998a4518149b87d9` |
| `source/game/StarWorldGeneration.cpp` | 125845 | `090d17f1d18bde4351c10e5032d3a6e0fa296fc9bc3d5e0e09bb2fc1f7feb409` |
| `source/game/StarWorldGenerationMemoryEstimate.hpp` | 1468 | `ab5b9d8a95b6e1080dbc3d51d01f189f9e006437fef5db29011616dedf92e256` |
| `source/test/CMakeLists.txt` | 5455 | `b446c1e28953e5cc08f1cc68efc2ec565f665dfbc4ad48e6efd92babe8759a83` |
| `source/test/world_generation_container_storage_test.cpp` | 15171 | `e5d3c07aeeac3d44f669c9e3feff43151ac2faecc96760a929955cb2469cd70d` |
| `source/test/world_generation_estimate_test.cpp` | 11802 | `b339c560ccd11b2aa3b89a5de2065645121cde75db59adfce0638af1d68421d5` |
