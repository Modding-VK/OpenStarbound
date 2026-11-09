# H0 / RAM-01: envelope contabilizado e aconselhamento de pressão

Data: 09/11/2026. Entrega: `Experimental`; revisão local:
`codex/experimental-regional-runtime`. Base: `4c85e5287f1f3c706f360fbc6e7754982aa3d864`.
Implementação inicial: `ada755bfb803eccdb60a9f3d53b08880da889094`.
Correção de histerese / estado final: `0915af25174ffc0f79aa5b7e2df01464e55f1c1b`.

[Programa](sector-runtime/README.md) · [Roadmap](sector-runtime/roadmap.md)
· [Progressão](sector-runtime/progress.md) · [Contrato do envelope](sector-runtime-memory-envelope.md)
· [Inventário](sector-runtime/memory-and-prefetch.md)
· [Etapa anterior](sector-runtime-h0-variant-json-2026-11-09.md)

## Objetivo e resultado

Consolidar o subconjunto de RAM já contabilizado antes de impor orçamento
processual ou ampliar admissão. Implementados schema das 14 classes, envelope
com somas/coerência verificadas e modelo isolado de pressão com histerese e
distinção entre trabalho visível, dependência e especulação.

**No runtime, só a consolidação observacional está integrada ao profiling.**
A política não tem consumidor de admissão, não bloqueia/cancela tarefas nem
libera buffers/caches. Não há nova config, limite finito padrão, detecção de RAM,
mudança de workers, filas/TTL/níveis, RNG/seleção, publicação, saves ou mods.
Não implementa executor/WorldThread regional/GPU nem demonstra setores/s maiores.

## Schema e envelope

`StarWorldGenerationMemoryEnvelope` classifica cada enum exatamente uma vez,
com validação em compile time de cardinalidade/ordem. Distingue:

- dois payloads de arrays: reserva de sizeof(Array), não request inteiro;
- quatro classes de requests do allocator: incluem header/padding, não heaps
  dos elementos ou malloc overhead;
- oito classes estimadas, três condicionadas a profiling na criação e cinco
  com cobrança básica independente dele.

A fronteira é declarada como anterior à alocação, observação/previsão ou
fronteira de job. Fórmulas, tokens, ledger e lifetimes existentes não mudaram.
Os campos básicos do envelope são arrayPayloadBytes, allocatorRequestBytes,
estimatedBytes, alwaysEstimatedBytes, profilingEstimatedBytes e trackedBytes.

O builder soma bytes/recursos por classe com proteção contra overflow e
confronta os totais com o snapshot. Rejeita bytes sem recurso, divergência de
categorias/totais e inconsistência do registro compartilhado Json. Não soma
picos históricos ao uso atual. O contador histórico de atualizações incompletas
é preservado como histórico, não tratado como qualidade atual de cada owner.

Conversão usa o mesmo snapshot já obtido sob mutex, trabalho fixo em 14 classes,
sem novos locks/alocações ou travessia de payloads. Criar entradas de métricas
continua tendo o custo do coletor existente; não se declara custo zero de profiling.
Coerência aritmética não certifica precisão/cobertura: CompleteHeap é sempre false.

O [contrato consolidado](sector-runtime-memory-envelope.md) registra todas as
classes, exclusões e condições para uma integração futura. Não é exigido medir
cada heap do motor para H1 em sombra; é exigido declarar o escopo governado.

## Política isolada, não reserva ou governador ativo

Por padrão, o modelo está desligado e não escolhe limites. Quando ativado por
seu chamador de teste, exige resume < soft < hard. Usa pressão atual, não bytes
de um pedido ainda não admitido, e retorna motivo/estado/recomendação:

1. Normal: especulação pode usar headroom soft; visível/dependência, headroom hard.
2. Soft: adiar crescimento especulativo, manter headroom urgente.
3. Hard: não crescer; reuso realmente sem novos bytes e drenagem continuam possíveis.
4. Invalid: não recomendar pedido diante de limite/envelope/demanda inválidos;
   a drenagem existente continua preservada.

Hard regride a Soft ao ficar abaixo de hard; pressão só volta a Normal em uso
<= resume. Pedidos podem chegar exatamente ao limite correspondente, mas não
ultrapassá-lo. Comparação por headroom evita overflow/underflow. Zero bytes não
autoriza um kernel que na prática aloca; a API não decide autoridade no mundo.

O modelo é owner-local e independente de CPU/SO. Não consulta memória disponível,
impõe cota entre mundos, evicta ou reserva atomicamente. Snapshot pode ficar
desatualizado antes de uma alocação concorrente. Nenhum teste desta etapa prova
ausência de deadlock em toda cadeia de dependências ou teto de RSS.

## Telemetria integrada

Usa a amostragem existente de WorldGenerator, no máximo uma vez por segundo
por coletor quando houver profiling, sem nova coleta profunda. Prefixo
`generation.memory.envelope.`:

- modelVersion: máximo **1**, separado dos modelos de tamanho existentes.
- samples, coherentSamples e incoherentSamples: qualidade e denominadores.
- partialHeapSamples: declara a cobertura parcial de cada amostra, inclusive vazia.
- sampledArrayPayloadBytesTotal, sampledAllocatorRequestBytesTotal,
  sampledEstimatedBytesTotal, sampledAlwaysEstimatedBytesTotal,
  sampledProfilingEstimatedBytesTotal e sampledTrackedBytesTotal: somas das
  amostras coerentes; usar coherentSamples como denominador.

Amostra incoerente não publica seus totais parciais nessas novas séries.
Contadores processuais legados permanecem inalterados. Vários mundos observam
o mesmo processo: não somar suas séries como memórias independentes. Não há
telemetria de decisões da política no jogo, pois ela ainda não é chamada ali.

Modelos geral **9**, Json **2**, Json enfileirado **1**, distribuições **3**,
candidatos/temporários **2**, nomes/buffers/cache de worker **1** mantidos.
Não cria classe de memória nem muda fórmulas/política de cobrança.

## Validação executada

Ambiente cloud conferido pela skill operacional; sem instalar/baixar dependências.
GCC 14.2.0/C++17, Debug `-Og`, GUI/Tracy/allocators alternativos OFF,
`VCPKG_MANIFEST_INSTALL=OFF`. C++ com
`-fsanitize=address,undefined -fno-omit-frame-pointer -DXXH_NO_INLINE_HINTS=1`;
C/dependências pré-compiladas não têm a mesma instrumentação. Identifier:
`4c85e528-memory-envelope-local-dirty`; fontes do commit/manifesto abaixo.

Na versão final, com `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1` e
`ASAN_OPTIONS=halt_on_error=1`, sem diagnóstico dos sanitizers:

1. **354/354 testes**, vinte suites; **34 novos** de schema/envelope/pressão.
2. Novos casos repetidos vinte vezes com shuffle e seed GTest 1221:
   **680 execuções adicionais**, não 680 casos únicos.
3. **20/20 grupos CTest** `^world_generation_contract_`, NoAssets; repetem
   os casos do binário, não são outros 354 testes distintos.
4. Build dos contratos e dois objetos reais, geração e envelope, aprovada.
   Sem linking completo do jogo.
5. Sintaxe/tipos de geração, envelope e teste novo aprovados com
   `STAR_ENABLE_TRACY`/`TRACY_ENABLE` e headers Tracy 0.11.2 existentes; não é
   build/linking Tracy completa. Aviso herdado de fmt permanece.
6. `git diff --check` aprovado; 452 links locais em 34 documentos sem destino
   ausente, 14 módulos alcançáveis, 49 contratos únicos cobertos pelo roadmap,
   ordem 1–49 e oito totais de fase conferidos. Recalculados 6/196 marcos = 3,1%;
   RAM-01/H0 mantidos em 25%. Seis fontes conferidas em bytes/SHA-256.

Revisão do modelo inicial reproduziu uma falha em dois testes reforçados antes
da correção: após pressão Soft, uma observação/demanda inválida apagava o estado
de recuperação; a próxima amostra abaixo de soft mas acima de resume permitia
especulação cedo demais. Entradas inválidas agora recusam o conselho atual sem
apagar a última pressão válida, em `0915af25`. Os dois casos continuam dentro dos 34 testes,
não são casos únicos adicionais. Não afetava admissão do jogo, pois a política
permanece isolada; a bateria final inclui esses controles aprovados.

Casos novos: schema completo/IDs inválidos, 14 classes nativas, header real do
allocator, sharing/facade, clear/destruição física, picos históricos, totais e
recursos inválidos, overflow, histórico de incompletude, oito threads atualizando
ledger/Json e snapshots, limites inválidos, demanda inválida, pressão/histerese,
headroom exato, reuso, ausência de mutação/reserva pelo conselho e size_t máximo.
Uma grade em um dos casos verifica **19.521 combinações** de bytes/demanda;
não são novos casos únicos nem benchmark de throughput.

Suites anteriores, incluindo replays de candidatos/seleção, bytes serializados,
lifetime e controles negativos de hash, também passaram. Não foi executado
WorldStorage com assets nem a emissão real dos novos contadores em jogo.

Reprodução após configurar build equivalente, a partir da raiz:

```bash
cmake --build <build-dir> --target world_generation_contract_tests \
  game/CMakeFiles/star_game.dir/StarWorldGeneration.cpp.o \
  game/CMakeFiles/star_game.dir/StarWorldGenerationMemoryEnvelope.cpp.o -j3
UBSAN_OPTIONS=halt_on_error=1 ASAN_OPTIONS=halt_on_error=1 \
  ./dist/world_generation_contract_tests --gtest_brief=1
UBSAN_OPTIONS=halt_on_error=1 ASAN_OPTIONS=halt_on_error=1 \
  ./dist/world_generation_contract_tests --gtest_filter='WorldGenerationMemoryEnvelopeTest.*' \
  --gtest_repeat=20 --gtest_shuffle --gtest_random_seed=1221 --gtest_brief=1
UBSAN_OPTIONS=halt_on_error=1 ASAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir <build-dir> -R '^world_generation_contract_' --output-on-failure
```

## Limites, progresso e próximos passos

Sem novos config/log/Tracy do usuário ou captura antiga reanalisada; nenhum
arquivo do usuário alterado ou dado bruto adicionado ao Git. Não executados
jogo/assets/mods, save/reload, multiplayer, TSan, OOM/pressão externa reais,
benchmark de overhead/throughput, Windows/macOS/ARM ou allocators alternativos.
Sem Actions, release, merge em main ou revisão independente.

RAM-01/H0 continuam em **25%**, programa em **3,1%**. O envelope agora tem
schema/limites explícitos e política executável isolada, não admissão global
completa ou outro marco cumulativo integral. Não conceder crédito retroativo
a RAM-03/CPU-05 pela existência desse modelo sem retenção/governador ativos.

Próxima ação executável: contrato e reserva atômica do escopo processual,
incluindo realocação/rollback e headroom/progresso de dependências. Depois,
controlador único/fairness entre mundos, política de pressão externa e limites
de usuário/hardware. Não multiplicar orçamentos por mundo nem habilitar o modelo
isolado como se fosse garantia hard de heap. Comparador MOD-01/VAL-02 e baseline
integrado continuam gates antes de ampliar admissão/publicação, priorizando
setores comuns e aproveitamento útil de hardware sem modelo de CPU fixo.

Mods/saves/multiplayer e fallback CPU são essenciais. NPC externo suspenso e
Linux Clang adiado; achados CPU-01 de seed automática/Perlin Uninitialized
continuam abertos, sem mudança de RNG nesta etapa.

Continuação em `4b5aed18`: o
[recorte de reserva atômica](sector-runtime-h0-atomic-memory-2026-11-09.md)
integra pressão a ledgers opt-in, com check/charge sob o mesmo mutex e demanda
preservada nos tokens/allocators. O singleton do jogo continua desligado. Ver o
novo relatório para estado/validações e próximos passos; este arquivo preserva
o resultado e manifesto da etapa histórica do modelo isolado.

## Manifesto das fontes

Fontes locais de 09/11/2026, nomes relativos ao checkout, bytes e SHA-256.
Nenhum novo artefato de execução do usuário nesta etapa.

| Arquivo | Bytes | SHA-256 |
| --- | ---: | --- |
| `source/game/CMakeLists.txt` | 16549 | `316c058a1d7003efe9ce0db811a09c5d91470edee0d218b4518c195b0fc15996` |
| `source/game/StarWorldGeneration.cpp` | 128539 | `31797dedb797ac7300600feda78a09c1e25abb85931062aeab9e64e86c01d256` |
| `source/game/StarWorldGenerationMemoryEnvelope.cpp` | 5988 | `ac2c2bd00da503c38147e3f083668b3e8e7f497a7e5719ea9ef6935b93221af9` |
| `source/game/StarWorldGenerationMemoryEnvelope.hpp` | 2985 | `f863187f475c8ba6628bdfa40a286354a998381e77001bb98bacdff6e2cc0aa0` |
| `source/test/CMakeLists.txt` | 5696 | `2d43d1b6163754097092ed809228d6f384866a1a63e83d3d80da589fc6dec01f` |
| `source/test/world_generation_memory_envelope_test.cpp` | 17637 | `800b3958668d60b4a4765f8be61011e46dab63f58a7d171b07fce19f36a42a87` |
