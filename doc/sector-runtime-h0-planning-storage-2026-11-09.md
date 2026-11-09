# H0 / RAM-01: nomes de saída e temporários do planejador

Data: 09/11/2026. Entrega: `Experimental`; revisão local:
`codex/experimental-regional-runtime`. Base: `77d4ed7d7f82de0ec156eaaba3c4b879b7edf224`.
Implementação: `cad311d17bf4ef4f9a841457b808f890e52cba7b`.

[Programa](sector-runtime/README.md) · [Roadmap](sector-runtime/roadmap.md)
· [Inventário](sector-runtime/memory-and-prefetch.md)
· [Etapa anterior](sector-runtime-h0-distribution-bundles-2026-11-09.md)

## Objetivo e resultado

Avançar os nomes de saída e reduzir alocações temporárias sem alterar a seleção
de geração. Os descritores passam a compartilhar uma string imutável por nome
distinto dentro de cada planejamento, em vez de copiar uma String por plano.
A lista de itens válidos reutiliza capacidade entre consultas **somente quando
os inputs não contêm MicroDungeonNames**; a restrição é necessária para preservar
o histórico de cópias que influencia a seleção.

Os caminhos síncrono/assíncrono usam as mesmas rotinas. Não há nova config,
limite, worker, ampliação de admissão, thread regional ou escrita paralela no
mundo. A etapa não implementa TMX, o grafo executivo, GPU ou nova WorldThread.
RAM-01/H0 continuam abertos; o objetivo principal continua acelerar a entrega
de setores comuns, não materialização de estruturas como fim separado.

É uma redução estrutural de cópias/alocações em casos definidos, **não um ganho
de setores/s medido**. Nomes únicos/curtos podem pagar mais overhead que antes;
esta entrega não prova aceleração universal nem maior uso útil de CPU/GPU.

## Nomes: ownership e contabilidade

`WorldGenerationNamePool` é local ao planejamento. Seu índice compara conteúdo
exato, incluindo case, UTF-8 e NUL interno; não altera escolhas, ordenação de
candidatos ou seeds. O nome é internado somente depois da escolha estática
existente. As chaves são ponteiros para strings imutáveis possuídas pelas
entradas, não para a string do chamador. Rehash não invalida o objeto apontado.

O índice e sua referência aos nomes são destruídos ao sair do planejamento.
Os descritores conservam handles const até o último consumidor físico,
inclusive após destruir a fila, o facade ou o worker. Não há índice global,
cache de assets, lookup do mundo ou callback no destrutor. Prewarm, factory
e hash diagnóstico recebem a mesma String por const referência, sem nova
conversão/cópia de texto na WorldThread.

| Parcela | Contrato |
| --- | --- |
| `MicroDungeonNameBuffer` (classe 10) | Cada pedido de objeto/control block, nodes e buckets do índice é reservado pelo allocator rastreado antes do raw allocator, com header/padding |
| `MicroDungeonNameEstimate` (classe 11) | Estimativa de backing da string, única por nome possuído; começa em tamanho UTF-8 + 1 antes da cópia e reconcilia capacidade + 1 após construir |
| Descritores | Capacidade do vetor já cobrada na classe 6; agora contém handles, não uma String por plano |
| Resultado preparado | A estimativa diagnóstica cobre seu objeto, não repete os nomes já acompanhados por seus donos |

Reservas básicas independem do profiling, inclusive no caminho síncrono.
Falha/recusa durante construção/inserção libera o nome ainda não publicado e
preserva entradas existentes; o teste finito libera o bloqueio e valida retry.
Elementos são destruídos antes das estimativas. Weak ownership pode manter o
pedido do control block depois de destruir a string; sua estimativa termina
com o último strong owner, o pedido físico com o último weak owner.

O backing de String continua **estimado**, não um pedido interceptado do seu
allocator interno. Capacidade da cópia é dependente da biblioteca; reconciliação
pode falhar e registrar `incompleteEstimateUpdates`, sem descartar o payload.
`estimateComplete` do nome não torna a estimativa um limite superior do heap.
SSO pode repetir bytes inline já contados; overhead geral de tokens/malloc e
demais heaps seguem fora. Não existe teto processual hard novo.

O modelo geral de estimativas passa a **4**: a parcela diagnóstica de planos
não soma mais capacidades das strings por descritor. Modelos próprios de
buffers, distribuições, nomes e cache de worker continuam **1**. Fórmulas Json
não mudaram; não comparar as parcelas v3/v4 sem essa mudança de classificação.

Telemetria nova, com cardinalidade fixa:

- `generation.memory.process.sampledNameBufferRequestBytesTotal` (classe 10);
- `generation.memory.process.sampledNameEstimateTotal` (classe 11);
- `generation.memory.names.modelVersion` (máximo 1).

`sampledBufferResourcesTotal` passa a somar classes 6/7/8/10. Amostras processuais
mantêm o denominador `generation.memory.process.samples`; coletores de vários
mundos observam o mesmo processo, não memórias independentes. Não foram
adicionados nomes/payloads de mods aos contadores.

## Temporários: descoberta e proteção de equivalência

`WorldTemplate::validBiomeItems` conserva assinatura e comportamento por valor.
A nova entrada, de nome distinto `validBiomeItemsInto`, permite saída não
aliasing reutilizável. Geometria, wrap, ordem de consultas do block cache,
testes de terreno e ordem surface/cave floor/cave ceiling/background/ocean
continuam os anteriores. A política de filtragem foi extraída para um helper
de valores, testável sem Root. A API anterior não fica ambígua para obter seu
endereço; `PotentialBiomeItems` não mudou de representação.

Reutilizar indiscriminadamente um vetor evita cópias durante seu crescimento.
`BiomeItemPlacement` pode ser copiado em realocação; uma cópia de hash set pode
reordenar um cluster que atravessa a última posição da tabela. Essa normalização
**não é necessariamente idempotente**, mesmo para duas tabelas de igual tamanho.
Logo, menos cópias pode mudar o nome escolhido, não apenas o custo.

O controle negativo usa três nomes cujo hash termina no bucket 7 da tabela
mínima e cinco colocações. Confirma que uma cópia muda a ordem, reproduz ordem
e escolha diferentes ao aquecer a capacidade sem proteção e verifica a
equivalência do caminho protegido em 64 repetições/seeds. O candidato sem
proteção não foi entregue como solução.

`resetBiomeItemSelectionOutput` limpa elementos e, se há MicroDungeonNames em
qualquer lista potencial, descarta a capacidade anterior para preservar o
crescimento/cópias legados. Mesmo nomes de uma categoria que depois seria
filtrada provocam esse fallback conservador. Sem microdungeon, pode reter
capacidade: esses candidatos são descartados pelo planejador depois do sort,
cuja comparação usa prioridade/ID, não o conteúdo do variant.

O scratch é local à invocação, não thread-local nem cache global. Os elementos
são limpos após copiar candidatos; a capacidade termina ao sair da função.
Não se removeu a cópia por valor dos inputs nem as cópias de elementos no
caminho ambiental/enfileirado, e o sort ainda inclui os candidatos não micro.
Este recorte **não reserva/contabiliza o scratch padrão nem todos os heaps
internos dos candidatos**. Listas potenciais ainda são construídas por consulta.

Há custo de examinar tipos quando existe capacidade para reutilizar; o benefício
depende do conteúdo e precisa de benchmark. Não marcar temporários ou MOD-01
inteiros como concluídos com base nesse recorte.

## Validação executada

Ambiente cloud e política conferidos pela skill operacional. Ferramentas e
dependências existentes, sem instalação/download. GCC 14.2.0/C++17, Debug
`-Og`, GUI/Tracy/allocators alternativos OFF, `VCPKG_MANIFEST_INSTALL=OFF`.
C++: `-fsanitize=address,undefined -fno-omit-frame-pointer -DXXH_NO_INLINE_HINTS=1`.
C/dependências pré-compiladas não têm a mesma instrumentação C++. Identifier:
`77d4ed7d-planning-storage-local-dirty`; fontes do commit/manifesto abaixo.

Com `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1` e
`ASAN_OPTIONS=halt_on_error=1`, sem diagnóstico dos sanitizers:

1. **204/204 testes**, catorze suites; **19 novos** de planning storage.
2. Novos casos repetidos 20 vezes, embaralhados com seed GTest 1215:
   **380 execuções adicionais**, não 380 casos únicos.
3. **14/14 grupos CTest** `^world_generation_contract_`, NoAssets, aprovados;
   repetem casos do binário, não outros 204 testes distintos.
4. Build dos contratos e dos objetos reais `StarWorldGeneration.cpp.o` e
   `StarWorldTemplate.cpp.o` aprovada. Sem linking completo de cliente/servidor.
5. Sintaxe/tipos dos dois arquivos reais e teste novo com
   `STAR_ENABLE_TRACY`/`TRACY_ENABLE` e headers Tracy 0.11.2 existentes aprovados;
   não é build/linking completa de Tracy. Aviso herdado de fmt permanece.
6. `git diff --check` aprovado; 269 links locais em 24 documentos sem destino
   ausente, 13 módulos alcançáveis, 49 IDs únicos referenciados no roadmap e
   12 fontes do manifesto conferidas em tamanho/SHA-256.

Casos incluem mil consultas repetidas sem crescer as reservas, nomes distintos,
UTF-8/NUL/case/vazio, origem destruída, índice vazio, retenção de strong/weak,
facade fechado, recusa total/parcial com rollback/retry, rehash de 256 entradas,
parcelas disjuntas e ausência de cache global. O teste de orçamento parcial
obtém o pedido do shared object na implementação corrente, sem presumir tamanho
de control block/node de libstdc++/libc++. Não é teste de OOM real.

Também: oito leitores simultâneos, WorkerPool real retendo nome após fechar
pool de nomes/facade, **4.096 combinações** dos predicados geométricos contra
referência herdada, capacidade reutilizada/resultado limpo em 256 consultas,
ordem/seleção de microdungeons e sequência de 256 escolhas após interning.
O helper é real, mas os BlockInfo de teste são valores tipados: não executam
terrain/cache/WorldTemplate real ou streaming completo. ASan/UBSan não é TSan.

Reprodução em build equivalente, a partir da raiz do checkout:

```bash
cmake --build <build-dir> --target world_generation_contract_tests \
  game/CMakeFiles/star_game.dir/StarWorldGeneration.cpp.o \
  game/CMakeFiles/star_game.dir/StarWorldTemplate.cpp.o -j3
UBSAN_OPTIONS=halt_on_error=1 ASAN_OPTIONS=halt_on_error=1 \
  ./dist/world_generation_contract_tests --gtest_brief=1
UBSAN_OPTIONS=halt_on_error=1 ASAN_OPTIONS=halt_on_error=1 \
  ./dist/world_generation_contract_tests --gtest_filter='WorldGenerationPlanningStorageTest.*' \
  --gtest_repeat=20 --gtest_shuffle --gtest_random_seed=1215 --gtest_brief=1
UBSAN_OPTIONS=halt_on_error=1 ASAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir <build-dir> -R '^world_generation_contract_' --output-on-failure
```

## Limites e próximos passos

Nenhum config/log/Tracy novo fornecido/coletado ou captura anterior reanalisada;
nenhum arquivo do usuário alterado ou dado bruto adicionado ao Git. NoAssets
não prova compatibilidade com mods/saves/multiplayer. Não executados jogo,
assets/mods, save/reload, multiplayer, TSan, OOM real, A/B de throughput/overhead,
Windows/macOS/ARM ou allocators alternativos. Sem Actions, release, merge em
main ou revisão independente. Build normal e diagnóstica continuam distintas.

O índice acrescenta hash, nodes/control blocks e operações de referência.
Nomes curtos/únicos podem custar mais que String inline; o ganho depende de
repetição. Não há medição isolada desse overhead ou fator de aceleração real.
O scratch protegido não acelera casos com MicroDungeonNames; preservar seleção
é um gate crítico para mods/saves. TTL, níveis, pré-requisitos e publicação
serial continuam inalterados.

Retomar H0 por:

1. Heaps de payloads de candidatos/temporários e variants opacos: ownership,
   cobertura e reserva sem trocar a sequência de cópias, usando o controle de
   colisões como gate; não otimizar por move/filtragem antecipada sem comparador.
2. MOD-01/VAL-01/02: estender comparação ao fluxo completo de candidatos/IDs,
   prioridades e outputs, e ao streaming real com rota/save/config/assets/mods
   fixos. Medir setores comuns inéditos entregues/s e interesse→entrega.
3. Layout/biomas/seletores/caches e limites por hardware/pressão ainda parciais.
   Explicitar lacunas e envelope antes de ampliar admissão, sem exigir medir
   cada heap do motor para começar um grafo H1 em sombra.

CPU-01 mantém overflow da seed automática em `StarRandom.cpp:197` e auditoria
da metadata de Perlin `Uninitialized`, sem alterar RNG nesta entrega.
Externalização de NPCs suspensa e Linux Clang adiado.

## Manifesto das fontes e conferência documental

Fontes locais coletadas/conferidas em 09/11/2026; nomes relativos ao checkout,
bytes e SHA-256. Nenhum novo artefato de execução do usuário.

| Arquivo | Bytes | SHA-256 |
| --- | ---: | --- |
| `source/game/CMakeLists.txt` | 16381 | `0b84849de7c8bbfd08ae42cae5c4a65068f119e099aa6ae5f83dd64544d1ce6b` |
| `source/game/StarBiomeItemSelection.hpp` | 2283 | `5dd21d2f4de82854bee4f145da91ced165476034e7554ac098eb42183195a0ab` |
| `source/game/StarWorldGeneration.cpp` | 124039 | `edf4fa0470aa4630367c552bcf87bbab7ae9923b47cd26c096a0292c70579619` |
| `source/game/StarWorldGeneration.hpp` | 14139 | `2530514dadc4a7d620c4a32ded4e7e3e27372f0fe21d17f3d086fa7eb9fb3371` |
| `source/game/StarWorldGenerationMemory.hpp` | 4929 | `46ebac37585f67a9928a9852bd016e58e25aad04529c60c1f393f4c5f3b29119` |
| `source/game/StarWorldGenerationMemoryEstimate.hpp` | 1465 | `94be5b934e2d5e03cac5cf3ac5638922bee3cf22ccaaa64cfb2b805beb324ba0` |
| `source/game/StarWorldGenerationNamePool.hpp` | 3266 | `91b58f69a384f9513e79071e3c49a88d2e6239926c1d7788a4fe5ff4d10af5f7` |
| `source/game/StarWorldTemplate.cpp` | 36713 | `212b83b214b3598f62699e5bedb211494d9a425ba9ca1f6cc375fd946ff64abc` |
| `source/game/StarWorldTemplate.hpp` | 8366 | `3dd6ad6d4aec4bbae01de561ec2ee0d32ebd141a505add13fe8d07c03682176a` |
| `source/test/CMakeLists.txt` | 5231 | `f5d28093e166ec352201be2bf60a5e1c87d2cb7828cbd96406cb37e60dab28a9` |
| `source/test/world_generation_estimate_test.cpp` | 11802 | `8e644269fda394d0df9fafb8653f0e5c29631beea4b00518f2312af289b9d42c` |
| `source/test/world_generation_planning_storage_test.cpp` | 16912 | `3017a62cc4a27ebda77f1d5236013ac10f27b81a0fac30bd1cfe78b39ee7f5a1` |
