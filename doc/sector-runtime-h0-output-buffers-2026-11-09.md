# H0 / RAM-01: reserva dos buffers variáveis e consumo sem cópia

Data: 09/11/2026. Entrega: `Experimental`; revisão local:
`codex/experimental-regional-runtime`. Base: `44168f8a175afa640ec9dd45f5d1c69ded9a1b24`.
Implementação: `34c76ea367da8e0eaee35337e70c83c723e5db1d`.

[Programa](sector-runtime/README.md) · [Roadmap](sector-runtime/roadmap.md)
· [Inventário](sector-runtime/memory-and-prefetch.md)
· [Recorte anterior](sector-runtime-h0-allocator-exception-2026-11-09.md)

## Objetivo e resultado

Avançar RAM-01 da estimativa posterior à construção para reserva anterior à
alocação dos buffers variáveis. O planejador agora usa um allocator rastreado
nos vetores de candidatos e descritores, tanto síncrono quanto assíncrono.
Reservas acompanham a alocação física, inclusive buffers antigo/novo durante
realocação, resultados prontos e consumidores retirados enquanto o job executa.

Também removida a cópia involuntária de planos/strings na WorldThread. Depois
de ordenar candidatos, a capacidade dos descritores é pré-reservada pelo número
elegível, evitando seu crescimento durante a construção. São reduções
estruturais de trabalho/cópia, **não um ganho de setores/s medido**.

Não há nova config, limite, worker ou aumento de admissão. O ledger runtime
continua observacional. Strings e outros heaps internos ainda não estão
inteiramente cobertos; RAM-01/H0 permanecem abertos.

## Reserva por pedido de storage

`WorldGenerationBufferAllocator<T>` exige um ledger explícito e compartilha
diretamente seu estado. Não cria ledger silencioso, contexto auxiliar no heap
por planejamento, mapa de alocações ou dependência do mundo. A view imutável
usada para reservar é local; o facade público continua sem poder esquecer
reservas antigas por reatribuição.

Antes de chamar `operator new`, verifica produto/soma e reserva
`prefixo + count × sizeof(T)`. O prefixo arredonda o header do token para o
alinhamento do elemento. O token fica no próprio bloco, não no registro lógico
do job. Cada pedido tem reserva fixa: `resizeEstimate` recusa essas classes.
O custo de tokens/control blocks externos não faz parte desse pedido.

Falha/recusa anterior à construção não deixa bytes cobrados. Ao realocar,
o buffer antigo só perde a reserva quando é realmente desalocado: os dois
pedidos coexistem durante cópia/move dos elementos. Falha da alocação física
ou do construtor/cópia de um elemento deixa o caminho padrão do vector liberar
o novo bloco, preservando o anterior quando sua garantia de exceção permitir.
O header é destruído e o storage liberado antes da última referência ao token
sair. Facades/mundos já fechados não são chamados na liberação.

Alinhamento normal usa `operator new/delete` ordinários, mantendo o roteamento
normal dos backends existentes. Tipos sobrealinhados usam o par alinhado C++17.
Há testes até alinhamento 128; backends alternativos não foram executados.
Copy/move/swap propagam o domínio/classe do allocator; containers movidos
continuam com allocator utilizável. Cópias físicas recebem reservas próprias.

## Integração e trabalho serial evitado

| Produto | Classe e lifetime |
| --- | --- |
| Vetor intermediário de candidatos | `MicroDungeonCandidateBuffer`; até fim do planejamento ou unwind, incluindo buffers de realocação |
| Vetor de descritores síncrono | `MicroDungeonPlanBuffer`; até fim da aplicação/descarte local |
| Vetor de descritores assíncrono | mesma classe; producer → payload na promise → último handle de consumo/descarte |

O `List` anterior já usava vector; foram mantidas enumeração, comparador de
sort, ordem dos candidatos e seleção. Uma passagem adicional só conta tags
`MicroDungeonNames`; não repete seleção, callbacks nem chamadas RNG. O vetor
de planos reserva esse número antes de construir descritores. Saída vazia
não aloca um buffer; candidatos continuam com crescimento nativo rastreado.

O consumo anterior declarava `PreparedMicroDungeonPlan const* preparedResult`
e depois fazia `std::move(*preparedResult)`. O rvalue continuava const e
selecionava a cópia do resultado, incluindo o vetor e suas strings. A nova
implementação mantém um handle local da promise e usa referência const ao
payload. Nenhum buffer/string é clonado nesse handoff e o payload não é movido
destrutivamente antes do sucesso da aplicação. Se ela lançar exceção, a
pendência ainda pode conservar o resultado original, como no caminho antigo.

O payload preparado é move-only, bloqueando cópia acidental na compilação.
Task e flag de dependência são capturadas antes de chamar a aplicação viva,
evitando reler o ponteiro do mapa de pendências depois desses callbacks.
Isso não é auditoria completa de reentrância/publicação do mundo vivo.
Pré-aquecimento e commit continuam na dona; não há nova escrita paralela.

## Telemetria e modelo de tamanho

O snapshot do ledger separa `reservedArrayBytes`, `reservedBufferBytes` e
`estimatedBytes`. `reservedBytes` é a soma das três parcelas declaradas, não
RSS. BaseTiles/halo conservam os campos anteriores; buffers não passam a ser
arrays de tiles nem estimativas redimensionáveis.

Campos novos sob `generation.memory.process.`:

| Campo | Semântica |
| --- | --- |
| `sampledReservedBufferBytesTotal` | soma amostrada de pedidos de buffers retidos |
| `sampledPlanBufferRequestBytesTotal` | parcela dos descritores |
| `sampledCandidateBufferRequestBytesTotal` | parcela dos intermediários |
| `sampledBufferResourcesTotal` | soma amostrada de grupos de buffers |
| `lifetimePeakReservedBufferBytes` | pico das reservas de pedidos, incluindo realocação e tentativas que depois falharam |

`generation.memory.buffers.modelVersion` é 1. O modelo de estimativas passa a
**2** em `generation.memory.estimates.modelVersion`: a estimativa assíncrona de
planos deixa de incluir `capacity × sizeof(plan)`, já cobrado pelo allocator,
e mantém objeto preparado + capacidades UTF-8. Fórmulas Json não mudaram;
modelo do cache de worker continua 1. Não comparar cegamente a parcela
estimada v1/v2 nem somá-la novamente à capacidade já reservada dos vetores.

A reserva básica independe de profiling, inclusive no caminho síncrono.
As amostras exigem o coletor existente; Json/metadados/strings profundos
continuam vinculados a profiling regional. A frequência continua a anterior:
somas divididas por `generation.memory.process.samples` dão médias amostradas.
Coletores de vários mundos observam o mesmo processo; não somar como memórias
independentes. O pico vitalício não reseta por janela/mundo.

### Limites importantes

- Conta o pedido com header/padding e representações inline, não overhead do
  allocator, RSS ou tamanho profundo dos elementos. Pode haver reserva antes
  da alocação; pico de pedidos não prova pico físico de malloc.
- Strings de nomes e heaps de `BiomeItemPlacement`/distribuições, temporários
  produzidos por `potentialBiomeItemsAt`/`validBiomeItems`, layout/biomas/seletores
  e demais caches continuam parcialmente fora. Não existe teto global novo.
- Strings/metadados assíncronos ainda são estimados após construir. SSO pode
  sobrepor armazenamento inline nessa estimativa; ela não é limite superior
  conservador. O caminho síncrono não ganhou aquela estimativa profunda.
- Per-allocation há token, header, sincronização do ledger e bookkeeping de
  referências. Não há medição isolada desse overhead nem amostra de tempo nova
  para ele; o elapsed do worker inclui o custo. Benchmark real ainda é gate.
- Retirar um consumidor não interrompe o cálculo a cada append. O teste de
  abandono retém o buffer físico até o producer/resultado terminar; não foi
  criada nova API/política de cancelamento de geração.

## Validação executada

Ambiente cloud conferido pela skill operacional; ferramentas/dependências
existentes, sem instalação. Build headless GCC 14.2.0/C++17 Debug (`-Og`),
ASan/UBSan, GUI/Tracy/allocators alternativos OFF. C++ usa
`-fsanitize=address,undefined -fno-omit-frame-pointer -DXXH_NO_INLINE_HINTS=1`;
C mantém o workaround de xxHash. Dependências pré-compiladas/C não têm a
mesma instrumentação C++. Identifier: `44168f8a-output-buffers-local-dirty`;
fontes validadas correspondem ao commit/manifesto abaixo.

Com `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1` e
`ASAN_OPTIONS=halt_on_error=1`, sem diagnóstico dos sanitizers:

1. Contratos: **167/167 testes** em doze suites, incluindo **23 novos** em
   `WorldGenerationBufferTest`.
2. Os 23 novos, embaralhados com seed GTest `1213`, repetidos 20 vezes:
   **460 execuções adicionais**, não 460 casos únicos.
3. CTest `^world_generation_contract_`: **12/12 grupos NoAssets** aprovados;
   repetem casos do binário, não outros 167 casos distintos.
4. Build dos contratos e alvos dos objetos reais de geração/template concluída;
   gerador recompilado. Não houve linking completo de cliente/servidor.
5. Sintaxe/tipos de geração, template e teste novo com definições
   `STAR_ENABLE_TRACY`/`TRACY_ENABLE` e headers Tracy 0.11.2 existentes:
   aprovada, sem build/linking Tracy completa.
6. `git diff --check` aprovado; 245 links locais em 22 documentos sem destino
   ausente, 13 módulos alcançáveis, 49 IDs únicos referenciados no roadmap e
   dez fontes do manifesto conferidas em tamanho e SHA-256.

Casos: reserva antes do raw allocator; recusa finita; falha física injetada;
overflow; tamanho zero; sobrealinhamento; dois buffers durante cópia; exceção
de elemento/retry; clear com capacidade retida; cópia, move, assignment e swap
entre domínios/classes; facade destruído; reserva fixa imutável; consumo de
resultado move-only por handle; falha de apply preservando resultado;
abandono da promise durante producer; oito threads/800 ciclos por repetição;
pré-reserva sem cópias/realocação; sort/filtro sintético equivalente e parcelas
de arrays/buffers/estimativas disjuntas. TSan não foi executado.

Reprodução em build equivalente:

```bash
cmake --build <build-dir> --target world_generation_contract_tests \
  game/CMakeFiles/star_game.dir/StarWorldGeneration.cpp.o \
  game/CMakeFiles/star_game.dir/StarWorldTemplate.cpp.o -j3
UBSAN_OPTIONS=halt_on_error=1 ASAN_OPTIONS=halt_on_error=1 \
  ./dist/world_generation_contract_tests --gtest_brief=1
UBSAN_OPTIONS=halt_on_error=1 ASAN_OPTIONS=halt_on_error=1 \
  ./dist/world_generation_contract_tests --gtest_filter='WorldGenerationBufferTest.*' \
  --gtest_repeat=20 --gtest_shuffle --gtest_random_seed=1213 --gtest_brief=1
UBSAN_OPTIONS=halt_on_error=1 ASAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir <build-dir> -R '^world_generation_contract_' --output-on-failure
```

## Configuração, compatibilidade e limites de evidência

Sem config/log/Tracy novo fornecido ou coletado; nenhuma captura anterior
reanalisada, arquivo do usuário alterado ou dado bruto adicionado ao Git.
O cenário é NoAssets; não gera um planeta real. Mantidos contratos públicos
de Lua/save/rede, RNG/seeds, ordem de aplicação, TTL, níveis e pré-requisitos.
Containers alterados são privados; compatibilidade com mods/saves/multiplayer
continua crítica e não foi provada somente por comparação sintética.

Não foram executados jogo/assets/mods, save/reload, multiplayer, OOM real,
A/B de throughput/overhead, backends alternativos, Windows/macOS/ARM, build
completa ou Actions. Sem fator de aceleração medido. O overflow de seed
automática em `StarRandom.cpp:197` continua pendente em CPU-01.
NPC externo suspenso e Linux Clang adiado; sem release, merge em `main`
ou revisão independente nesta atividade.

## Manifesto e próximos passos

Fontes conferidas em 09/11/2026, bytes e SHA-256:

| Arquivo | Bytes | SHA-256 |
| --- | ---: | --- |
| `source/game/CMakeLists.txt` | 16170 | `eeff1b011bab479a265a50183cf810a8230528fb1be16a694a35d7c2eec89280` |
| `source/game/StarWorldGeneration.cpp` | 121534 | `586c5b343baeea05fd3424168d87af0ea818511442b29b5e943ccfdc7d5f2922` |
| `source/game/StarWorldGeneration.hpp` | 13824 | `aaff64e8b56f287565fe1b9527133e7c2810895a195a707e1b0655f81c4315ec` |
| `source/game/StarWorldGenerationMemory.cpp` | 7540 | `373e50cb78907620cd7988a9339cfbeec150fbbb732305e162233b1ab90929fb` |
| `source/game/StarWorldGenerationMemory.hpp` | 4661 | `61bfb23df79bd8b7e233369bf7e0f87d077731822c8d8a85387f0a9cfe5e29c7` |
| `source/game/StarWorldGenerationMemoryEstimate.hpp` | 1034 | `7a29d39c9e0ad07b6ea0258c6cdc5c979a19e6e238865a2a565fca77df7b4c7e` |
| `source/test/CMakeLists.txt` | 4988 | `98ec18458e28bbad756c4f293ca8617dedc8488f04b2df4ab1cf52eeaa03706f` |
| `source/test/world_generation_estimate_test.cpp` | 11802 | `549a2782be79a501e32ab44972857a2a43056997677aa35c249fc40a99abba65` |
| `source/game/StarWorldGenerationBufferAllocator.hpp` | 4836 | `0805df18197e17d656213a479baf072e16eb925674505746ac6bffaacfeafaef` |
| `source/test/world_generation_buffer_test.cpp` | 17171 | `1e571f631dfa1535069341d07012394e9acb61c7f69a85b2ad718be834eee90c` |

Retomar RAM-01/H0 por:

1. Heaps aninhados/temporários de nomes e distribuições e metadados ainda
   excluídos: definir ownership e reserva antes da construção sem transformar
   uma estimativa tardia em garantia hard. Preservar fallback síncrono.
2. Cobertura de layout/biomas/seletores/caches e storage compartilhado, com
   orçamento processual configurável somente após lacunas e reações à pressão
   explícitas. Não ampliar filas com a contabilidade parcial.
3. MOD-01 e VAL-01/02: auditar efeitos/inicializadores e ampliar streaming real,
   comparador de outputs e benchmark com assets/mods para medir overhead e
   ganho ponta a ponta, mantendo a prioridade de setores comuns.

Os buffers do planejador são uma etapa do runtime comum, não um desvio para
materialização TMX. H0 completo, executor H1, nova WorldThread e GPU continuam
pendentes; não dependem de declarar toda a memória do motor já contabilizada.

## Continuação em 09/11/2026

O [recorte seguinte](sector-runtime-h0-distribution-bundles-2026-11-09.md),
`887149c0`, compartilha distribuições enfileiradas após uma única cópia
snapshot-equivalente e rastreia backing conhecido único, inclusive no caminho
síncrono. Modelo de estimativas 3, 185 testes e treze grupos CTest aprovados.
Este relatório preserva o manifesto e os resultados da etapa anterior;
retomar pelo relatório seguinte e pelo roadmap, não pelas pendências históricas
sem considerar sua cobertura parcial posterior.
