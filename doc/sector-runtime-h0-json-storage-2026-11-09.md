# H0 / RAM-01: buckets e chaves do Json compartilhado

Data: 09/11/2026. Entrega: `Experimental`; revisão local:
`codex/experimental-regional-runtime`. Base: `509217830bfacf67cb18ae3357bb338a6564cf1a`.
Implementação: `45db240274c37b9ed1b675c728b3bef1f3509d3e`.

[Programa](sector-runtime/README.md) · [Roadmap](sector-runtime/roadmap.md)
· [Progressão](sector-runtime/progress.md) · [Inventário](sector-runtime/memory-and-prefetch.md)
· [Etapa anterior](sector-runtime-h0-container-storage-2026-11-09.md)

## Objetivo e recorte implementado

Avançar a cobertura do Json imutável compartilhado dos templates serializados.
O modelo anterior contava apenas pares ocupados de JsonObject, excluindo buckets
vazios, metadados do hash e texto das chaves. Agora o modelo usa o payload do
vetor inteiro e capacidade UTF-8 das chaves, deduplicados pelo dono do objeto.

FlatHashMap ganha uma consulta const `bucketStorageBytes() noexcept` que encaminha
para a introspecção de FlatHashTable já existente. Não troca campos, allocators,
hash, iteração, cópias, capacidade, representação de Json ou serialização.
Também não muda seleção/RNG, WorldTemplate, filas/TTL, publicação, workers,
config/defaults ou orçamento de execução. Não implementa novo executor/GPU/
thread regional nem demonstra setores/s maiores.

## Modelo canônico por proprietário

O objeto completo usa:

```text
sizeof(JsonObject)
+ bucketStorageBytes()  // capacity × sizeof(Bucket), inclusive sentinela/padding
+ soma(utf8.capacity() + 1 de cada chave)
```

Os pares String/Json inline já estão nos buckets: não adicionar novamente
`size × sizeof(value_type)`. Strings e arrays Json conservam suas fórmulas
anteriores; filhos com heap são visitados separadamente, compartilhados pelo
control block, não por endereço/hash/conteúdo. Chaves têm texto próprio dentro
do objeto; dois objetos independentes iguais continuam cobrados separadamente.

O ledger/registro de ownership não mudou. Cada owner tem uma fórmula única,
independente do limite diagnóstico da captura. Referências repetidas no mesmo
grafo não expandem novamente o nó; capturas/raízes diferentes compartilham
o token até o último handle rastreado. Referências Json comuns/assets podem
reter o backing depois disso: saldo zero não prova desalocação global.

## Limite diagnóstico e omissão explícita

A captura continua iterativa, por padrão limitada a 32.768 valores incluindo
referências repetidas. Cada chave medida acompanha um filho enfileirado dentro
do mesmo limite; não há segundo passeio profundo nem cópia de chave/conteúdo.

Antes de percorrer as chaves de um objeto não vazio, conferir se todos os seus
filhos imediatos cabem no allowance restante, descontando valores já visitados
e pendentes. Se não couberem, marcar incompletude e omitir **a cobrança daquele
objeto inteiro**, sem registrar bytes parciais por seu owner. Continuar a
travessia do prefixo de filhos que cabe, preservando o Json completo.

Isso permite uma captura completa posterior sem conflitar com um tamanho
anterior dependente do limite. Também funciona na ordem inversa. Um pai cujas
chaves cabem mantém seu modelo canônico mesmo quando um descendente é omitido.
Objeto vazio com reserve conserva o payload de buckets, sem percorrer o vetor.

Em capturas truncadas, essa regra pode reportar **menos** bytes que o modelo
anterior, que ainda cobrava pares ocupados do objeto parcialmente percorrido.
É exclusão declarada, não liberação de RAM. Não usar uma captura incompleta
para inferir teto, consumo total ou mudança real de uso da memória.

O limite é de valores/chaves processados e fila, não de milissegundos. Iteração
de um hash não vazio pode examinar slots vazios; capacidade esparsa, operações
do set de visited, alocações e locks também têm custo. Não há garantia nova de
latência máxima da coleta. Contadores/SSO continuam estimativas, não RSS.

## Ativação, versões e telemetria

Mantida a ativação existente por profiling de exploração com regionalDetails.
Sem medição, não percorre nem cobra Json. O cache continua medindo uma vez por
fonte/revisão; não habilita coleta independente do profiling nesta etapa.

- Modelo geral: **8**, antes 7. Submodelo Json: **2**, antes fórmula v1.
- `generation.memory.estimates.jsonModelVersion`: máximo 2, emitido em medição.
- `generation.memory.estimates.templateObjectKeys`: soma das chaves medidas;
  mede esforço por captura, não quantidade processual única de chaves.
- `generation.memory.estimates.templateOmittedObjects`: soma dos objetos não
  cobrados porque suas chaves não cabiam; não inclui valores não alcançados.
- `templateMeasurements` é o denominador das capturas; `templateIncomplete`
  também cobre limites/overflow/recusa. Chaves medidas não significam reserva
  aceita: uma recusa do ledger preserva esse contador e marca incompletude.

Não cria classe de memória. Os bytes continuam na classe 2 de Json compartilhado.
Distribuições 3, candidatos/temporários 2 e nomes/buffers/cache de worker 1
mantêm fórmulas/versões próprias. Json interno dos BiomeItem continua fora
dessas classes aninhadas; não presumir que este tracker de template o cobre.

## Validação

Ambiente cloud conferido pela skill operacional, sem instalar/baixar
dependências. GCC 14.2.0/C++17, Debug `-Og`, GUI/Tracy/allocators alternativos
OFF, `VCPKG_MANIFEST_INSTALL=OFF`. C++ com
`-fsanitize=address,undefined -fno-omit-frame-pointer -DXXH_NO_INLINE_HINTS=1`;
C/dependências pré-compiladas não têm a mesma instrumentação. Identifier:
`50921783-json-storage-local-dirty`; fontes do commit/manifesto abaixo.

Versão final com `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1` e
`ASAN_OPTIONS=halt_on_error=1`, sem diagnóstico dos sanitizers:

1. **294/294 testes**, dezoito suites. **28 novos**: 22 de Json e seis de map.
2. Esses 28 repetidos vinte vezes, embaralhados com seed GTest 1219:
   **560 execuções adicionais**, não 560 casos únicos.
3. **18/18 grupos CTest** `^world_generation_contract_`, NoAssets; repetem os
   casos do binário, não outros 294 testes distintos.
4. Build dos contratos e quatro objetos reais: geração, template, sampling
   e estimador Json. A dependência do header core recompilou core/base/testes;
   não foi feito linking completo do jogo.
5. Sintaxe/tipos de geração, estimador e dois testes modificados/novo aprovados
   com `STAR_ENABLE_TRACY`/`TRACY_ENABLE` e headers Tracy 0.11.2 existentes.
   Não é build/linking Tracy completa. Aviso herdado de fmt permanece.
6. `git diff --check` aprovado; **407 links locais em 31 documentos** sem
   destino ausente, 14 módulos alcançáveis, 49 contratos únicos cobertos pelo
   roadmap e ordem 1–49 conferida. Oito totais de fase e 6/196 marcos = 3,1%
   recalculados; RAM-01/H0 mantidos em 25%. Oito fontes do manifesto conferidas
   em tamanho/SHA-256. Nenhum teste manual do jogo foi presumido.

Casos: map confrontado com pedidos de um recorder std::allocator apenas no
teste, reserve vazio, crescimento/clear, cópia/move, ordem/hash/alocação
inalterados na consulta, heap aninhado excluído, alinhamento 64; Json com
buckets retidos, chaves longas/UTF-8, identidade e bytes serializados, subárvores
repetidas e independentes, dois owners compartilhando folha, limite exato/
insuficiente/zero, medição desligada, capturas parcial→completa e inversa,
filhos de objeto omitido, teto finito/recusa, WorkerPool após fechar facade,
oito capturas concorrentes e cadeia de 512 objetos. ASan/UBSan não é TSan.

As suites anteriores, incluindo replays de 96/432 candidatos e controles
negativos de hash/wrap, também passaram. São testes sem assets, não streaming
real de WorldStorage nem validação completa de mods/saves/multiplayer.

Reprodução, após configurar build equivalente, a partir da raiz:

```bash
cmake --build <build-dir> --target world_generation_contract_tests \
  game/CMakeFiles/star_game.dir/StarWorldGeneration.cpp.o \
  game/CMakeFiles/star_game.dir/StarWorldTemplate.cpp.o \
  game/CMakeFiles/star_game.dir/StarBiomePlacementSampling.cpp.o \
  game/CMakeFiles/star_game.dir/StarWorldGenerationMemoryEstimate.cpp.o -j3
UBSAN_OPTIONS=halt_on_error=1 ASAN_OPTIONS=halt_on_error=1 \
  ./dist/world_generation_contract_tests --gtest_brief=1
UBSAN_OPTIONS=halt_on_error=1 ASAN_OPTIONS=halt_on_error=1 \
  ./dist/world_generation_contract_tests \
  --gtest_filter='WorldGenerationJsonStorageTest.*:WorldGenerationContainerStorageTest.Map*' \
  --gtest_repeat=20 --gtest_shuffle --gtest_random_seed=1219 --gtest_brief=1
UBSAN_OPTIONS=halt_on_error=1 ASAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir <build-dir> -R '^world_generation_contract_' --output-on-failure
```

## Limites, progresso e continuação

JsonObject agora cobre seu vetor e texto de chaves; não os control blocks,
malloc/allocator, estruturas temporárias de diagnóstico ou todo heap do motor.
SSO pode repetir bytes inline. Json interno de variants, bookkeeping dos nós
TileDamage, layouts/biomas/seletores, decode e picos pré-adoção continuam parciais.
Não há teto processual profundo, reação à pressão ou admissão maior.

RAM-01/H0 permanecem em **25%**, programa **3,1%**: recorte funcional adicional,
não novo marco cumulativo integral. Compatibilidade com mods/saves/multiplayer
e fallback CPU são essenciais. NPC externo suspenso; Linux Clang adiado.
Os achados CPU-01 de seed automática/Perlin Uninitialized não foram corrigidos.

Próximos passos: delimitar ownership de Json nos variants sem duplicar cobrança
por referências ou copiar conteúdo; tratar nós/allocator e consolidar envelope
coberto/exclusões/pressão antes de limites por hardware/configuração. Prosseguir
com VAL-02/MOD-01 e baseline de streaming de setores comuns com rota/save/config/
assets/mods fixos antes de alegar ganho ou ampliar o pipeline. Não exigir cada
heap do motor para um H1 em sombra, mas declarar o escopo antes de sua ampliação.

Nenhum config/log/Tracy novo fornecido/coletado, nem captura antiga reanalisada.
Nenhum arquivo do usuário alterado ou dado bruto adicionado ao Git. Sem teste
de jogo/assets/mods, save/reload, multiplayer, TSan, OOM real, throughput,
Windows/macOS/ARM ou allocators alternativos. Sem Actions, release, merge em
main ou revisão independente.

## Manifesto das fontes

Fontes locais coletadas/conferidas em 09/11/2026, nomes relativos ao checkout,
bytes e SHA-256. Sem novo artefato de execução
do usuário nesta etapa.

| Arquivo | Bytes | SHA-256 |
| --- | ---: | --- |
| `source/core/StarFlatHashMap.hpp` | 23206 | `9977b5474e38cc6c9f654a00f9647cfa4e973e24e2b8f4782d8e92316e966558` |
| `source/game/StarWorldGeneration.cpp` | 126165 | `b74ef41502d72a61c2fccbfb99a88feacb0661a5ae0ddf835d346b73bccd423b` |
| `source/game/StarWorldGenerationMemoryEstimate.cpp` | 3442 | `245fc1273cd277f0daae6f038eb9e751797f5173d8a30cb9cfae9c0b8df3f7e6` |
| `source/game/StarWorldGenerationMemoryEstimate.hpp` | 1728 | `5c6639b856afd75cec1208772e0450a09912446f997ed601d85d77e88906cf13` |
| `source/test/CMakeLists.txt` | 5531 | `3e9bb753b6a33b688f9a2b6da39e1052ea080391963b7f4833c7018647981457` |
| `source/test/world_generation_container_storage_test.cpp` | 18977 | `d17b280869c0ae13cd4cfbcfd3c1749f680f460972bfb87d865a365ef95eb62c` |
| `source/test/world_generation_estimate_test.cpp` | 11802 | `df5f5a6bbbf60384a55de661defbf3f7dfdb132b8719c35878c0bab34836c181` |
| `source/test/world_generation_json_storage_test.cpp` | 13240 | `ec7aabebb875c396617ea63d55e6dbeb16566a35b4c7d8bec62522c858a7816f` |
