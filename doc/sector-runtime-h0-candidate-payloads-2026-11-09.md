# H0 / RAM-01: backing conhecido dos candidatos

Data: 09/11/2026. Entrega: `Experimental`; revisão local:
`codex/experimental-regional-runtime`. Base: `1c6fa74cd4ad5c66b4211ab145eb532e35b05390`.
Implementação: `25cecf95a439c01bb2563d30f03b184fbb41c9c9`.

[Programa](sector-runtime/README.md) · [Roadmap](sector-runtime/roadmap.md)
· [Progressão](sector-runtime/progress.md) · [Inventário](sector-runtime/memory-and-prefetch.md)
· [Etapa anterior](sector-runtime-h0-planning-storage-2026-11-09.md)

## Objetivo e resultado

Avançar RAM-01 sobre os heaps dos candidatos que não estavam na reserva do
buffer de representações inline. O planejador agora mantém uma estimativa
agregada do backing conhecido, ligada ao container físico, com previsão antes
de construir o candidato temporário e durante crescimento do vetor.

O mesmo helper atende planejamento síncrono/assíncrono. Preserva o tipo dos
candidatos, capacidade/crescimento, cópias/moves, inclusão dos itens não micro,
comparator e ordem de seleção. Não muda RNG, TTL, níveis, pré-requisitos,
admissão, número de workers, defaults, APIs Lua/save/rede ou publicação serial.

Não é otimização demonstrada de setores/s, orçamento hard completo ou novo
executor. O recorte permite observar/validar mais memória antes de ampliar
concorrência; adiciona trabalho de sizing e sincronização, ainda sem benchmark
do overhead. A prioridade continua entrega de setores comuns, não TMX.

## Reserva agregada e lifetime

`WorldGenerationPayloadBuffer<T, Sizer>` encapsula o vetor rastreado. Não oferece
mutação externa de capacidade nem faz pré-reserva que alteraria cópias durante
realocação. Uma reserva de estimativa é criada somente no primeiro append;
um planejamento sem candidatos não cria token nem buffer para este recurso.

Antes do append, usa o backing conhecido retido `B` e a previsão do novo
candidato `S` para reservar:

- sem crescimento: `B + 2 × S`;
- com crescimento: `2 × B + 2 × S`.

As parcelas representam sobreposição estimada de candidatos antigos/copias e
temporário/destino. A reserva anterior ainda retida nunca é reduzida antes
da operação. **Não são limites superiores do heap completo**: a previsão
vem da fonte e uma cópia pode ter capacidade diferente, além dos campos
opacos. O source original de `potential`/`validItems` não pertence a essa reserva.

Uma factory retorna `Candidate{...}` por prvalue. C++17 garante elisão entre
seu retorno e a variável local; há o mesmo move para `push_back` que antes.
A reserva vem antes da factory, não depois da primeira cópia. No caminho
enfileirado, a cópia do loop `for (auto placement : validItems)` continua
existindo e permanece fora dessa previsão anterior: não foi eliminada.

O temporário é destruído enquanto a previsão ainda está reservada. Depois:

1. Se houve crescimento, mede todo o vetor resultante, pois as cópias dos
   elementos existentes podem alterar capacidades/ordem dos hash sets.
2. Sem crescimento, acrescenta somente a medida do novo elemento.
3. Após o sort, reconcilia o conjunto uma vez. O sort usa o mesmo
   `std::less<Candidate>` do helper legado; não filtra antes da ordenação.

Não há varredura completa a cada append sem crescimento. O teste verifica o
número exato de consultas ao sizer pelas fronteiras de capacidade observadas,
sem presumir uma taxa fixa de crescimento de libstdc++/libc++.

Recusa anterior à factory não constrói nem modifica o vetor. Exceção da factory,
recusa do buffer ou falha de cópia durante crescimento fazem rollback para a
cobrança anterior; o tipo exige cópia disponível ou move nothrow para manter
a garantia do vetor. Um sizer pós-inserção deve ser não lançante.

Falha de reconciliação mantém o payload aceito e a reserva anterior, marcando
o flag local incompleto e `incompleteEstimateUpdates` no processo. O flag local
não é um novo campo do resultado preparado. Overflow/incompletude aritmética
impede previsões posteriores; opacidade de Json/privados não rejeita itens.
O teste finito com previsão deliberadamente insuficiente demonstra que isto
**não garante o respeito a um teto hard de memória profunda**.

O vetor é destruído antes do token; a reserva sobrevive à destruição do facade
original e ao fechamento do contexto chamador enquanto um worker conserva o
container. Ao retornar planos, os candidatos podem ser descartados normalmente:
os nomes de saída já têm ownership próprio e não retêm todo o lote de candidatos.
Não foi criada política de cancelamento ou interrupção a cada append.

## Modelo de backing dos valores

`estimateBiomeItemGenerationBacking` é somente leitura, sem Root/assets,
alocação auxiliar ou travessia Json. O modelo é usado tanto por candidatos
quanto por distribuições, sem repetir a representação inline de BiomeItem.

| Variant | Parcela conhecida | Parcela ainda fora |
| --- | --- | --- |
| MicroDungeonNames | Strings ocupadas e suas capacidades UTF-8 | Buckets vazios/metadados do hash; SSO pode repetir bytes inline |
| TreasureBoxSet | Capacidade UTF-8 + 1 da String | Overhead do allocator e distinção heap/SSO; não inclui assets do tesouro |
| GrassVariant | Nome, diretório, capacidade/lista de imagens e strings | Json descriptions e internals de TileDamageParameters |
| BushVariant | Nomes/diretório, capacidade das shapes, imagens e listas de mods | Json e internals de TileDamageParameters |
| TreePair | Nomes e diretórios públicos dos dois variants | Json/settings/drop configs e internals de TileDamageParameters |
| ObjectPool | Capacidade dos pares ponderados e nomes dos objetos | Backing Json dos parâmetros e assets externos |

As quatro últimas variantes continuam com `opaqueItems=1` por valor, mesmo
quando a parcela conhecida é contada; os dois primeiros retornam zero. Isso
explicita a cobertura do modelo, não prova ausência de memória omitida.
Json compartilhado é omitido aqui para não cobrá-lo por cópia sem contrato
de identidade/ownership. A coleta Json anterior permanece separada.

Treasure deixa de ser opaco neste recorte de valor. O caso de regressão de
opacidade nas distribuições passa a usar ObjectPool, com sampler real; não se
remove cobertura de preservação de conteúdo. O novo caso de candidato preserva
parâmetro Json não medido até depois da construção/sort.

O modelo geral de estimativas passa a **5**, e o de distribuições a **2**,
pois a fórmula/backing e a interpretação de opacidade mudaram. Modelo dos
payloads de candidatos: **1**. Buffers, nomes e cache de worker continuam em
seus modelos 1; fórmulas de Json e metadados preparados não mudaram.

### Telemetria

- Classe 12: `MicroDungeonCandidateEstimate`, separada dos buffers fixos.
- `generation.memory.process.sampledCandidatePayloadEstimateTotal`: amostra
  da estimativa atualmente reservada, incluindo previsão transitória quando
  a amostra coincidir com construção/crescimento.
- `generation.memory.candidatePayloads.modelVersion`: máximo 1.
- `generation.memory.distributions.modelVersion`: máximo 2.
- `generation.memory.estimates.modelVersion`: máximo 5.

`sampledBufferResourcesTotal` não inclui a classe 12: ela é estimativa, não
raw buffer. Totais/picos processuais incorporam a nova classe automaticamente.
Coleta usa o denominador `generation.memory.process.samples`; vários mundos
observam o mesmo processo, não memórias independentes. A cobrança básica é
independente de profiling; publicar amostras continua exigindo o coletor.
Não há novo contador de opacidade de todos os candidatos emitido por job:
o campo do helper e as fórmulas delimitam cobertura, não um censo global.

## Validação executada

Ambiente cloud conferido pela skill operacional; ferramentas/dependências
existentes, sem instalação/download de dependências. GCC 14.2.0/C++17 Debug (`-Og`), GUI/Tracy/
allocators alternativos OFF, `VCPKG_MANIFEST_INSTALL=OFF`. C++:
`-fsanitize=address,undefined -fno-omit-frame-pointer -DXXH_NO_INLINE_HINTS=1`.
C/dependências pré-compiladas não têm a mesma instrumentação C++. Identifier:
`1c6fa74c-candidate-payload-local-dirty`; fontes do commit/manifesto abaixo.

Com `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1` e
`ASAN_OPTIONS=halt_on_error=1`, sem diagnóstico dos sanitizers:

1. **223/223 testes**, quinze suites; **19 novos** em WorldGenerationPayloadTest.
2. Novos casos embaralhados com seed GTest 1216, repetidos 20 vezes:
   **380 execuções adicionais**, não 380 casos únicos.
3. **15/15 grupos CTest** `^world_generation_contract_`, NoAssets; repetem
   casos do binário, não outros 223 testes distintos.
4. Build dos contratos e objetos reais `StarWorldGeneration.cpp.o` e
   `StarBiomePlacementSampling.cpp.o` aprovada; sem linking completo do jogo.
5. Sintaxe/tipos dos dois arquivos reais e teste novo com
   `STAR_ENABLE_TRACY`/`TRACY_ENABLE` e headers Tracy 0.11.2 existentes aprovados.
   Não é build/linking completa de Tracy; aviso herdado de fmt permanece.
6. Seis definições herdadas de sampler/defaults comparadas com a base:
   idênticas ignorando whitespace. `git diff --check` aprovado; 368 links locais
   em 28 documentos sem destino ausente, 14 documentos da coleção alcançáveis,
   49 contratos únicos presentes no roadmap, ordem 1–49 e oito totais de fase
   do painel conferidos, H0 25%/programa 3,1% recalculados e onze fontes do
   manifesto conferidas em tamanho/SHA-256.

Casos: reserva antes da factory, sobreposição durante cópia, crescimento e
contagem de copies/moves iguais ao vetor anterior, recusa/rollback/retry,
factory/cópia lançantes, raw buffer recusado, overflow, reconciliação parcial,
token presente no destrutor, facade fechado, WorkerPool real e oito jobs
concorrentes com saldo final zero. ASan/UBSan e concorrência não são TSan.

Comparação nativa: três nomes com colisões e cluster com wrap, itens micro e
não micro, caminhos ambiental/enfileirado, 96 candidatos com empates de
prioridade/IDs, mesmas capacidades, ordenação e escolhas em 64 seeds por
candidato micro. O controle que demonstra a cópia não idempotente também
passa. Não é execução completa de WorldTemplate/WorldStorage com assets.

Reprodução, a partir da raiz, após configurar build equivalente:

```bash
cmake --build <build-dir> --target world_generation_contract_tests \
  game/CMakeFiles/star_game.dir/StarWorldGeneration.cpp.o \
  game/CMakeFiles/star_game.dir/StarBiomePlacementSampling.cpp.o -j3
UBSAN_OPTIONS=halt_on_error=1 ASAN_OPTIONS=halt_on_error=1 \
  ./dist/world_generation_contract_tests --gtest_brief=1
UBSAN_OPTIONS=halt_on_error=1 ASAN_OPTIONS=halt_on_error=1 \
  ./dist/world_generation_contract_tests --gtest_filter='WorldGenerationPayloadTest.*' \
  --gtest_repeat=20 --gtest_shuffle --gtest_random_seed=1216 --gtest_brief=1
UBSAN_OPTIONS=halt_on_error=1 ASAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir <build-dir> -R '^world_generation_contract_' --output-on-failure
```

## Limites, progresso e próxima ação

Não foi fornecido/coletado config/log/Tracy novo nem reanalisada captura
anterior. Nenhum arquivo do usuário alterado ou dado bruto adicionado ao Git.
Sem execução de jogo/assets/mods, save/reload, multiplayer, TSan, OOM real,
A/B de throughput/overhead, builds Windows/macOS/ARM/allocators alternativos
ou linking completo. Sem Actions, release, merge em main ou revisão independente.
Compatibilidade com mods/saves/multiplayer e fallback CPU permanecem essenciais.

A estimativa pode superestimar por SSO/previsão e subestimar por internals
omitidos/capacidade da cópia. Há sizing adicional e até duas atualizações do
ledger por append, além de varreduras nas fronteiras de crescimento/sort.
Não há ganho de desempenho alegado nem autorização de aumentar filas.

RAM-01 permanece em **25%**, H0 em **25%**, programa em **3,1%**: houve avanço
dentro de um recorte, não conclusão do contrato abrangente para o próximo
marco. O painel atualiza evidência/próximo passo sem inflar a nota por testes.

Próximos trabalhos executáveis:

1. Original de listas potenciais/scratch e cópia por valor de validação:
   lifetime e reserva sem trocar capacidade/cópias legadas. O temporário da
   factory está coberto apenas na parcela conhecida; aqueles originais não.
2. Json compartilhado, internals TileDamage e buckets/metadados do hash:
   definir identidade/storage únicos antes de reserva profunda, evitando
   dupla cobrança de backing compartilhado. Layout/biomas/seletores e decode
   salvo continuam fora do envelope processual completo.
3. VAL-02/MOD-01: ampliar o comparador e a fronteira de efeitos; depois
   streaming com rota/save/config/assets/mods fixos e A/B com/sem profiling
   detalhado. Não exigir cada heap do motor para protótipo H1 em sombra, mas
   explicitar o envelope antes de qualquer ampliação correspondente.

CPU-01 conserva o overflow de seed em `StarRandom.cpp:197` e a auditoria de
metadata do Perlin Uninitialized; RNG não alterado. NPC externo suspenso e
Linux Clang adiado. Não é necessário novo teste manual para validar este
recorte local; equivalência/desempenho integrados continuam gates posteriores.

## Continuação em 09/11/2026

O [recorte seguinte](sector-runtime-h0-temporary-storage-2026-11-09.md),
`c4d7ddb3`, acompanha listas potenciais/cópia/scratch com escopos físicos e
previsão antes da cópia, sem suprimir cópias/seleção. Modelo geral 6, 246 testes
e dezesseis grupos CTest. Este relatório mantém resultados/manifesto históricos;
retomar pelo seguinte e pelo roadmap. Json/privados e teto completo seguem abertos.

## Manifesto das fontes

Fontes locais coletadas/conferidas em 09/11/2026, nomes relativos ao checkout,
bytes e SHA-256. Nenhum novo artefato de execução do usuário nesta etapa.

| Arquivo | Bytes | SHA-256 |
| --- | ---: | --- |
| `source/game/CMakeLists.txt` | 16422 | `144f387d4f964b1b1d9906353185e0fde59a5cbd5b8c6d125a398f0a57d01eb8` |
| `source/game/StarBiomePlacementSampling.cpp` | 5082 | `e8cbc106104bb2f6f64a595b4a6070dd36844bd0c43d0927ae95af082cb0eb61` |
| `source/game/StarBiomePlacementTypes.hpp` | 3046 | `9f7ed8a38cb61ba4e900e3109307b51c905d970658c196da3dcf01bfbfcf33b1` |
| `source/game/StarWorldGeneration.cpp` | 124590 | `5dd1c480a9ab49ae208b6e0a18b892971df9dfbd6328f00bf3ba14b4d467fbcd` |
| `source/game/StarWorldGenerationMemory.hpp` | 4962 | `8aad3553951e8e6430433d307379021a77a620da80731337615c9a23fbbd9e85` |
| `source/game/StarWorldGenerationMemoryEstimate.hpp` | 1464 | `fd1780565e3f31bf07de238183f5de05a15fce7a99587621f56cf2bf55708839` |
| `source/game/StarWorldGenerationPayloadBuffer.hpp` | 4251 | `0429b7caea39b0653b8f3b070ecbffb906672140be1d0f1cf69120f305b4798b` |
| `source/test/CMakeLists.txt` | 5298 | `07a5ea87e98aa8fd4f2711b60c57237aa3daeff76e2084e9f1d85f099c9e999b` |
| `source/test/world_generation_distribution_test.cpp` | 16302 | `4e700f3e8ad1ed5d8efaf38c7ae371da5ac7a99a8e2c87b8f6c4fe38a96744bb` |
| `source/test/world_generation_estimate_test.cpp` | 11802 | `24f4d9dd9cd001e081662fe9a978c525c113d5912f770ebae0b27cc921a0c40d` |
| `source/test/world_generation_payload_test.cpp` | 18433 | `3eb819485281696efddb9f716dee9705ff6ad34316521f6e7af5f878812685bd` |
