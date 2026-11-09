# H0 / RAM-01: inputs de distribuição imutáveis e compartilhados

Data: 09/11/2026. Entrega: `Experimental`; revisão local:
`codex/experimental-regional-runtime`. Base: `cd9c259d18197951d1e67d01c69b22c87f673425`.
Implementação: `887149c0030d15a30b853841c2eb6e1bcc7c78c5`.

[Programa](sector-runtime/README.md) · [Roadmap](sector-runtime/roadmap.md)
· [Inventário](sector-runtime/memory-and-prefetch.md)
· [Recorte anterior](sector-runtime-h0-output-buffers-2026-11-09.md)

## Objetivo e resultado

Evitar que cada snapshot de planejamento copie novamente as distribuições
dos pedidos enfileirados, incluindo suas tabelas Perlin. Agora cada pedido
conserva o input original até a primeira captura bem-sucedida, produz uma
única cópia equivalente à captura antiga e compartilha esse bundle const
entre as capturas seguintes. A mesma rotina atende o caminho síncrono e o
assíncrono. Pedidos nunca capturados não pagam a cópia.

A cobrança acompanha o bundle compartilhado até o último consumidor físico,
mesmo após retirar o pedido da fila ou destruir o facade do ledger. Cobertura
básica e compartilhamento independem do profiling. Não há nova configuração,
worker, cota, aumento de admissão ou política de cancelamento.

É uma redução estrutural de cópias de inputs, **não uma medição de ganho de
setores/s**. Cobre somente distribuições de colocações enfileiradas, não todas
as distribuições ambientais do template. Não implementa materialização TMX,
grafo executivo, nova WorldThread, simulação regional ou GPU. RAM-01/H0
continuam abertos; a prioridade permanece acelerar setores comuns.

## Por que manter uma cópia inicial

`MicroDungeonNames` contém um hash set cuja cópia reinsere elementos numa
tabela redimensionada. Isso pode mudar a ordem de iteração usada por
`staticRandomFrom`. Mover o input original diretamente para um bundle e
eliminar também a primeira cópia alteraria escolhas para certos históricos
de capacidade. Copiar novamente o bundle normalizado também não é necessário.

O novo caminho preserva exatamente uma cópia snapshot-equivalente de cada
input original, na primeira captura bem-sucedida. Depois, somente referências
const. IDs monotônicos, ordem relativa de pedidos sobreviventes e promises
continuam no registro da dona. `prepareSnapshots` prepara payloads; seu visitor
não deve mutar a estrutura do registro.

O teste negativo conserva uma tabela com histórico de reserva diferente:
a ordem original realmente difere da cópia nativa. A ordem do bundle coincide
com a cópia nativa e suas escolhas coincidem para 64 seeds. Outros testes
exercitam o sampler real aleatório/periódico, não uma implementação simulada.

## Ownership e contabilidade

`WorldGenerationSnapshotCache` adota o container original por move, sem cópia
de elementos, e registra uma estimativa do backing conhecido e da capacidade
do container. Esse original **já foi construído pelo chamador**: sua adoção
não é uma reserva anterior à alocação original.

No primeiro `freeze`:

1. Calcula a estimativa do backing conhecido e tenta reservá-la antes da cópia.
2. Usa `allocate_shared` e `WorldGenerationBufferAllocator` para reservar antes
   de cada pedido de storage do objeto/control block e do vetor de elementos.
3. Pré-reserva o vetor e copia os elementos diretamente, sem moves adicionais.
   Reconcilia a estimativa com os dados copiados e publica o bundle const.
4. Destrói o container original antes de liberar sua estimativa. Futuras
   capturas retornam o mesmo bundle, sem copiar os elementos novamente.

Falha de cópia/reserva antes da publicação preserva o original e permite retry.
Reconciliação incompleta mantém payload/cobrança anterior e incrementa o
diagnóstico; não transforma estimativa em garantia hard. Fonte/destino podem
diferir por capacidade/rehash. A reconciliação posterior não mede o pico
transitório dos heaps internos durante a cópia.

Os elementos são destruídos antes da estimativa de seus heaps. Uma `weak_ptr`
pode manter apenas a alocação do control block depois de destruir o payload;
esse pedido continua cobrado até o último weak owner. Tokens/bookkeeping
externos do ledger ainda não são cobertura integral do allocator.

| Parcela | Cobertura |
| --- | --- |
| `MicroDungeonDistributionBuffer` (classe 8) | Pedidos fixos do objeto/control block e vetor, com header/padding; reserva anterior ao raw allocator |
| `MicroDungeonDistributionEstimate` (classe 9) | Original adotado ou backing conhecido do bundle; único por dono, não repetido por snapshot |
| Tabelas Perlin | Quatro arrays possuídos, por presença dos ponteiros; exclui objeto/overhead do allocator |
| Vetores internos | Capacidade de itens aleatórios e pares item/Perlin ponderados |
| `MicroDungeonNames` | Representação das strings ocupadas e capacidades UTF-8; sem buckets vazios/metadados do hash |
| Outros variants | Conteúdo preservado, mas heaps internos opacos e contados como `opaqueItems` |

Um Perlin float inicializado solicita `1026 × (sizeof(int) + 6 × sizeof(float))`
= 28.728 bytes nas tabelas desta build. A consulta também suporta default e
moved-from e não lê metadata escalar de um Perlin não inicializado.

Modelo geral de estimativas: **3**, separando o bundle compartilhado dos
metadados por snapshot. Fórmulas Json não mudaram; modelos de cache de worker,
buffers e distribuições são **1**. Não comparar diretamente parcelas v2/v3 nem
somar novamente a capacidade do bundle por consumidor.

Não é RSS, tamanho profundo completo ou limite superior conservador. SSO pode
superestimar; buckets e variants opacos podem subestimar. `complete` significa
aritmética/travessia e reconciliação do modelo, não ausência de itens opacos.
Saldo zero deste recorte não prova liberação de todos os recursos do processo.

## Telemetria de cardinalidade fixa

Com o coletor existente:

- `generation.memory.distributions.freeze`: tempo da primeira captura de cada
  pedido, incluindo estimativa, cópia e descarte do original;
- `generation.memory.distributions.sharedSnapshots`: referências capturadas,
  incluindo a primeira; não representa setores entregues ou hits exclusivos;
- `generation.memory.distributions.frozenBundles`, `items`, `opaqueItems` e
  `incomplete`: somados somente quando o bundle é criado;
- `generation.memory.distributions.modelVersion`: máximo 1;
- `generation.memory.process.sampledDistributionBufferRequestBytesTotal` e
  `sampledDistributionEstimateTotal`: amostras das classes 8 e 9;
- `sampledBufferResourcesTotal` inclui agora a classe 8, além de 6/7.

Amostras processuais usam `generation.memory.process.samples` como denominador.
Coletores de vários mundos observam o mesmo processo; não somar como memórias
independentes. Json/metadados profundos anteriores continuam condicionados ao
profiling regional. Nenhum nome/payload de mod foi adicionado aos contadores.

## Compatibilidade e fronteira pura

O bundle contém valores; `mode`, `itemToPlace`, seleção estática e consultas
Perlin leem dados sem cache mutável. Workers não recebem fila, keeper de
promise, entidade, Lua ou mundo vivo. O template continua privado por worker.
Essa auditoria é um recorte de MOD-01, não aprovação de todos os variants/assets
nem de escrita paralela.

Dados foram extraídos para `StarBiomePlacementTypes.hpp`; o header anterior
continua como facade, preservando inclusive o include transitivo de Plant.
Ordem/tipos dos campos de distribuição mantidos. Seis definições de amostragem/
defaults e quatro construtores default de valores foram extraídos das fontes
herdadas, sem mudança de corpo/inicializadores ignorando whitespace. Construção
por config/Root e serialização ficam nos arquivos originais. Isso permite
linking NoAssets do sampler real sem stubs de entidades nem desativação de
checks de vptr dos sanitizers.

A API anterior de `WorldTemplate::addPotentialBiomeItems` continua disponível,
sem ambiguidade para obter seu endereço. Nova entrada:
`addPotentialBiomeItemsFromArray`, mantendo ordem/seleção e tratando array vazio
sem aritmética sobre ponteiro nulo. API pública de enqueue, seeds, RNG, TTL,
níveis, pré-requisitos e publicação serial permanecem os anteriores.

## Validação executada

Ambiente cloud conferido pela skill operacional; ferramentas/dependências
existentes, sem instalação. GCC 14.2.0/C++17 Debug (`-Og`), GUI/Tracy/allocators
alternativos OFF, `VCPKG_MANIFEST_INSTALL=OFF`. C++:
`-fsanitize=address,undefined -fno-omit-frame-pointer -DXXH_NO_INLINE_HINTS=1`.
C mantém o workaround de xxHash; dependências pré-compiladas/C não têm a mesma
instrumentação C++. Identifier: `cd9c259d-distribution-bundles-local-dirty`;
fontes validadas correspondem ao commit/manifesto deste relatório.

Com `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1` e
`ASAN_OPTIONS=halt_on_error=1`, sem diagnóstico dos sanitizers:

1. **185/185 testes** em treze suites, incluindo **18 novos** de distribuição.
2. Os 18 novos repetidos 20 vezes, embaralhados com seed GTest 1214:
   **360 execuções adicionais**, não 360 casos únicos.
3. CTest `^world_generation_contract_`: **13/13 grupos NoAssets** aprovados;
   repetem casos do binário, não outros 185 testes distintos.
4. Build dos contratos e sete objetos reais de geração/template/distribuição/
   defaults/plant database/tile damage concluída. Sem linking completo do jogo.
5. Sintaxe/tipos dos sete arquivos reais e teste novo com
   `STAR_ENABLE_TRACY`/`TRACY_ENABLE` e headers existentes Tracy 0.11.2 aprovados.
   Não é build/linking completa de Tracy; aviso herdado de fmt permanece.
6. Comparação das dez definições extraídas com a base: idênticas ignorando
   whitespace. `git diff --check` aprovado; 257 links locais em 23 documentos
   sem destino ausente, 13 módulos alcançáveis, 49 IDs únicos referenciados
   no roadmap e 20 fontes do manifesto conferidas em tamanho/SHA-256.

Casos: cópia lazy única, 64 reutilizações sem nova cópia, falha/retry, recusa
finita, overflow, input não usado, ordem de IDs, retirada da fila com bundle
vivo, facade fechado, weak owner, array vazio e diagnóstico incompleto.
Samplers reais: 561 coordenadas por variante aleatória/periódica, nomes/ordem
equivalentes à cópia antiga; oito leitores simultâneos em 128 coordenadas,
e WorkerPool real retendo input após fechar o facade. Variant opaco Treasure
efetivamente amostrado/preservado, não descartado pela contabilidade.
ASan/UBSan e leitores concorrentes não substituem TSan.

Reprodução após configurar build equivalente, a partir da raiz do checkout:

```bash
cmake --build <build-dir> --target world_generation_contract_tests \
  game/CMakeFiles/star_game.dir/StarWorldGeneration.cpp.o \
  game/CMakeFiles/star_game.dir/StarWorldTemplate.cpp.o \
  game/CMakeFiles/star_game.dir/StarBiomePlacement.cpp.o \
  game/CMakeFiles/star_game.dir/StarBiomePlacementSampling.cpp.o \
  game/CMakeFiles/star_game.dir/StarBiomePlacementDefaults.cpp.o \
  game/CMakeFiles/star_game.dir/StarPlantDatabase.cpp.o \
  game/CMakeFiles/star_game.dir/StarTileDamage.cpp.o -j3
UBSAN_OPTIONS=halt_on_error=1 ASAN_OPTIONS=halt_on_error=1 \
  ./dist/world_generation_contract_tests --gtest_brief=1
UBSAN_OPTIONS=halt_on_error=1 ASAN_OPTIONS=halt_on_error=1 \
  ./dist/world_generation_contract_tests --gtest_filter='WorldGenerationDistributionTest.*' \
  --gtest_repeat=20 --gtest_shuffle --gtest_random_seed=1214 --gtest_brief=1
UBSAN_OPTIONS=halt_on_error=1 ASAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir <build-dir> -R '^world_generation_contract_' --output-on-failure
```

## Evidência ausente e riscos restantes

Não foi fornecido/coletado config/log/Tracy novo, nem reanalisada captura
anterior. Nenhum arquivo do usuário alterado ou dado bruto adicionado ao Git.
Cenário NoAssets, não um planeta real. Não executados assets/mods, save/reload,
multiplayer, TSan, OOM real, Windows/macOS/ARM, allocators alternativos,
build completa ou A/B de throughput/overhead. Actions manuais não disparadas;
sem release, merge em main ou revisão independente. Compatibilidade com
mods/saves/multiplayer continua gate crítico.

Primeiro freeze ainda paga uma cópia mais sizing/descarte do original; pode
custar mais que a primeira cópia antiga. Há custo de referências, tokens e
sincronização do ledger. Benefício depende de reutilização e volume de pedidos
enfileirados; não é aceleração universal de BaseTiles.

Continuam fora: heaps de layout/biomas/seletores, internals opacos, temporários
de `potentialBiomeItemsAt`/`validBiomeItems`, strings do output antes da
construção e overhead global. Não impor teto hard ou ampliar filas com esse
modelo parcial. CPU-01 conserva o overflow da seed automática em
`StarRandom.cpp:197`. A cópia herdada de Perlin `Uninitialized` também deixa
metadata escalar não inicializada; esta etapa só consulta ponteiros nesse
estado, sem mudar RNG/copy/move. Auditar antes de novos consumidores da
metadata. NPC externo suspenso e Linux Clang adiado.

## Manifesto das fontes

Fontes locais coletadas/conferidas em 09/11/2026; nomes relativos ao checkout,
bytes e SHA-256. Não há novos artefatos de execução do usuário nesta etapa.

| Arquivo | Bytes | SHA-256 |
| --- | ---: | --- |
| `source/core/StarPerlin.hpp` | 17022 | `c8b53a42f93ed358637189bfdce46fbfb1fb4bec41edd849b5c4ddc08b128a15` |
| `source/game/CMakeLists.txt` | 16314 | `d47b27255527b9b07fba9ed95cdaea57a744f984a87e9977400db7fa6cc6240f` |
| `source/game/StarBiomePlacement.cpp` | 10389 | `59bda9334bee7c42eaf24d2c4a28f36180e9ad5da91aca5c20142cd3bd566c2e` |
| `source/game/StarBiomePlacement.hpp` | 156 | `7c4811d89fe4aaf2dc6ace285b4d5afaca77bfbf0490954189cb6b61e3ed9849` |
| `source/game/StarBiomePlacementDefaults.cpp` | 617 | `b5960f3c570ad3f8f8a6ab278b77809f1bafb7ebd5295c6114e5cd98c1985d64` |
| `source/game/StarBiomePlacementSampling.cpp` | 3274 | `18a529472adcb7b0d25c7c3c3db98b0c5f4403c04b46b440f24010f28b481626` |
| `source/game/StarBiomePlacementTypes.hpp` | 2785 | `51683a87c3d023f1bebf47e214774fd78fa9c39bc8dd14f20412c42e829f93ca` |
| `source/game/StarPlantDatabase.cpp` | 13740 | `51b58cf7e9b056cd047d3d0a6bae0c1c176bbe207eecdce487a20ccf579db4d7` |
| `source/game/StarTileDamage.cpp` | 10712 | `13a1540bb69f5d9f89ba697709f460c43b0cf2fa6d21156f7664c62f4cd366d3` |
| `source/game/StarWorldGeneration.cpp` | 123443 | `3fbe101807fcfec1a7b948c87ffbfc6b2ab2c3cb14ae02b340e34693062c2cfc` |
| `source/game/StarWorldGeneration.hpp` | 14040 | `cd19cf9fc68ce59e38e2119850e11fbaf438410d5115bbe900fddd2a0d942e9d` |
| `source/game/StarWorldGenerationImmutableArray.hpp` | 4567 | `73fa54c6892b87bdb7ab3a2c28c9c4bed0d220ec076a10f0e7e9abedb6c641e9` |
| `source/game/StarWorldGenerationMemory.hpp` | 4807 | `6a08b5b426f6b04c9ac27f9de69d6cd59f746e828c0c262d9640d405612f0695` |
| `source/game/StarWorldGenerationMemoryEstimate.hpp` | 1460 | `8177e4740d7a893286fe3d3135ea7eea9d0825270e1d50bcc7c5ee06552c230a` |
| `source/game/StarWorldGenerationPlacementQueue.hpp` | 2034 | `b6123afe607f98006a03a73f834892affc1ec7abca7cec59326001665f99bcb5` |
| `source/game/StarWorldTemplate.cpp` | 37220 | `cf004c75933bc6444ef182e8550d127c42e405914bd0e4373b32efe15158bef2` |
| `source/game/StarWorldTemplate.hpp` | 8010 | `c3040985f78ae86971f66460e4e0fd7d58ddfe34a32341f60d43f06872759d8d` |
| `source/test/CMakeLists.txt` | 5147 | `cfdead82b88abe4043506c57b89c60516efaf64c2fb7173b686324444a9f9c58` |
| `source/test/world_generation_distribution_test.cpp` | 16430 | `b1d442f8a742d1c2d44d5bcc135601a24e7a741c3574f1cc7da2ad7d584f8a73` |
| `source/test/world_generation_estimate_test.cpp` | 11802 | `8e9b3662f4355f689d77ba5d88aac943f501b4f6b1206d0a330f7807f5cdd6cd` |

## Próximo trabalho executável

1. Nomes de saída e heaps temporários de candidatos: estabelecer ownership e
   cobertura anterior à construção sem mudar ordem/seleção. Auditar variants
   opacos separadamente; estimativa reconciliada não é reserva hard.
2. Layout/biomas/seletores e demais caches compartilhados: identidade de backing,
   lifetime e cobertura básica antes de cotas processuais/reação à pressão.
3. MOD-01 e VAL-01/02: ampliar dependências reais no harness e o comparador,
   executar streaming com rota/save/config/assets/mods fixos e medir setores
   inéditos entregues/s e interesse→entrega, com/sem profiling detalhado.

Manter fallback síncrono, limites existentes e publicação serial. Não adiar
todo H1 até medir cada heap do motor, mas explicitar lacunas e seu limite antes
de ampliar admissão. Gates de equivalência e benchmark continuam abertos.

## Continuação em 09/11/2026

O [recorte seguinte](sector-runtime-h0-planning-storage-2026-11-09.md),
`cad311d1`, compartilha nomes de saída e protege a reutilização de scratch
contra mudanças de cópias/ordem de hash. Modelo 4, 204 testes e catorze grupos
CTest aprovados. Este relatório mantém os resultados/manifesto históricos;
retomar pelo seguinte e pelo roadmap, sem presumir cobertura integral de
temporários ou ganho de jogo.
