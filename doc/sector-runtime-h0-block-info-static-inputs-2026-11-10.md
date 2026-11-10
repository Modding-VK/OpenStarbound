# H0 / CPU-01: config lazy e inputs estáticos compostos

Data: 10/11/2026 UTC. Entrega: `Experimental`;
revisão local: `codex/experimental-regional-runtime`.
Base: `740dc6c1eefc2a09a4e0ad6f58ed238b4aa9abd8`.
Implementação: `e076d96a76d70aa83fdb58090381c02515f9ce74`.

[Programa](sector-runtime/README.md) · [Roadmap](sector-runtime/roadmap.md)
· [Progressão](sector-runtime/progress.md)
· [Contrato BaseTiles](sector-runtime-base-tiles-contract.md)
· [Envelope](sector-runtime-memory-envelope.md)
· [Etapa anterior](sector-runtime-h0-custom-terrain-context-2026-11-10.md)

## Entrega

WorldTemplate::captureBlockInfoStaticInputs compõe num único objeto move-only
a captura de custom terrain e a **raiz Json imutável inteira** de configuração.
Não converte valores ou valida schema ao capturar: getFloat ocorre somente
quando o kernel consulta a chave, mantendo fallbacks, erros e ordem de consultas.

WorldGenerationBlockInfoStaticInputs retém o Json por ownership compartilhado
imutável e custom terrain por valor, sem WorldTemplate/Root/Assets. Json é
imutável e implicitamente compartilhado; seus construtores de objetos/arrays/
strings criam backing const e set retorna um novo valor. Não se confunde isso
com compartilhar caches mutáveis de layout/seletores.

O objeto já é exercitado no kernel real de cálculo com as duas fontes
compostas, depois da destruição da origem e por três leitores. Layout,
weighting/noise, seletores e metadados de bioma ainda são fronteiras controladas.
**Não é geração de planeta/setor real nem o contexto completo.**

Captura preparatória fora dos jobs normais: nenhuma consulta/cópia por tile foi
adicionada. WorldTemplate.cpp e o corpo independente de referência do kernel
permanecem byte-idênticos à base no escopo conferido. O caminho normal conserva
Root/Assets e getFloat originais; não mudou default, flag, orçamento, TTL, RNG,
conteúdo intencional, formato de save ou autorização de commit.

## Semântica lazy e memória

| Fronteira | Comportamento mantido |
| --- | --- |
| Sem layout/pesos | Kernel pode retornar sem consultar config inválida |
| biomeTransitionThreshold | getFloat com fallback 0 no ponto legado |
| Atenuação | dist só em terreno positivo; factor só abaixo de dist, fallbacks 0/1 |
| Chave ausente/null | Mantém fallback do chamador, inclusive bits de zero |
| Tipo/root inválido | Não saneia/converte na captura; JsonException somente na consulta alcançada |
| Medição desligada | Retém os inputs sem cobrar Json; flags explicitam ausência de medição |
| Medição limitada/recusada | Conserva todos os dados; cobertura incompleta não filtra config |
| Medição completa | Completa apenas o modelo Json visitado; não valida schema ou admissão do contexto |

captureWorldGenerationJson é reutilizado sem alterar modelo 9/Json v2 ou as
14 classes do ledger. Modelo estrutural observa owners de strings/arrays/
objetos, buckets/chaves e capacidades; não chama getFloat. Mesmo backing é
cobrado uma vez entre capturas rastreadas. Tokens duram até o último handle
rastreado; a destruição do snapshot solta Json antes dos tokens.

Owners ordinários não rastreados podem conservar o Json depois de a cobrança
terminar. **Não é censo do heap global.** Wrapper, vetor de tokens, control
blocks, temporários de medição e polígonos/custom terrain continuam excluídos.
Não é reserva composta antes de toda cópia nem um teto de contexto completo.

Reter a raiz inteira conserva inclusive campos/subárvores não utilizados pelo
kernel. É uma decisão explícita para preservar consulta/schema lazy e ownership,
não uma afirmação de custo mínimo. Não ampliar filas ou adotar nos jobs reais
antes de medir/fechar a retenção, admissão e reação a recusa/pressão. O booleano
jsonStorageModelComplete não substitui esses gates.

templateRevision continua uma observação local herdada da captura de custom
terrain. Não identifica mundo, assets/materials/algoritmo nem oferece read lease
ou permissão de publicar. Captura é owner-thread-only; não mover/destruir o
objeto enquanto leitores estiverem ativos.

## Testes e limites da comparação

**29 casos novos** verificam fallbacks, null, inteiros/floats/zero assinado,
tipos e roots inválidos, schema versus medição, ramos que não consultam config,
exceções/tipos/mensagens/prefixos iguais ao corpo herdado, composição com custom
terrain, troca/destruição da origem, move, três leitores, release do Json e dos
tokens, deduplicação, facade de ledger destruída, medição desligada/limitada e
recusa sem perda de inputs.

Uma matriz de **1.024 combinações em UM caso GTest** compara os 16 campos de
BlockInfo, consultas e exceções. A referência do kernel continua independente e
inalterada; recebe a mesma ponderação custom como input para isolar config/fluxo.
A matemática custom é comparada de forma independente nos 26 casos anteriores,
também reexecutados. Não anunciar cada coordenada/matriz/leitor como teste único.

A factory é método real de WorldTemplate, sobre origem de metadata criada pela
tag privada já existente. O novo friend só prepara Json/geometria/blend sem
substituir factory/Json/ledger/matemática. Não executa construtor normal de
WorldTemplate, Root ou WorldLayout nesse caminho. Leituras concorrentes usam
fixtures de layout/seletores separadas, compartilhando só os inputs capturados.
Não é prova TSan ou comparação do streaming real.

## Build e validações executadas

Skill de ambiente cloud usada para conferir executor/política e reutilizar
cache/dependências existentes, sem instalação/download. GCC 14.2.0/C++17,
CMake 3.29.2/Ninja 1.13.0.git.kitware.jobserver-pipe-1, Debug -Og;
GUI/Tracy/allocators alternativos OFF, BUILD_TESTING=ON, VCPKG_MANIFEST_INSTALL=OFF.
Cache `build/block-info-h0`, identifier `740dc6c1-static-inputs-local-dirty`;
mesmo vcpkg/sysroot/hook SDL3 real do [setup anterior](sector-runtime-h0-block-info-kernel-2026-11-10.md).
C++: -fsanitize=address,undefined -fno-omit-frame-pointer e XXH_NO_INLINE_HINTS=1.
C/dependências pré-compiladas não têm a mesma cobertura.

Com UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 e ASAN_OPTIONS=halt_on_error=1,
sem diagnóstico dos sanitizers nas aprovações:

1. **752/752 casos padrão**, 38 suites; **29 novos** nesta entrega.
2. **13/13 Root opt-in**, regressão: **765 únicos com a opção ON**,
   não no alvo/Actions padrão; opção continua padrão OFF.
3. **268×20 =5.360 execuções adicionais**, 13 suites, mais **13×5 =65** Root:
   **5.425 adicionais**, shuffle/seed inicial 1236; não casos únicos.
   Três microexperimentos DISABLED não executados.
4. **38+1 grupos CTest**, contratos + Root, NoAssets, -j2, aprovados; repetem
   os mesmos casos, não toda a bateria core_tests/game_tests.
5. Build/link dos dois executáveis e quatro objetos normais: WorldTemplate,
   BlockInfoStaticInputs, CustomTerrain e WorldGeneration. Não link/execução do
   jogo completo. Root OFF: 38 grupos, zero de Root, alvo padrão sem trabalho
   pendente. Restaurado ON, ambos executáveis/quatro objetos sem trabalho pendente.
6. Referência independente e WorldTemplate.cpp conferidos contra a base;
   manifesto/links/IDs/percentuais e diff conferidos na revisão documental.

A execução diagnóstica inicial teve uma asserção de teste comparando bits
inteiros com float, corrigida para bits nos dois lados; não defeito do runtime.
Braces corrigem também warning novo de dangling else no teste. Essas execuções
intermediárias não são a bateria aprovada acima. Avisos herdados de fmt/Assets
não corrigidos. Tracy/GUI/outras plataformas não revalidados. Erros intencionais
de Json/material/listeners nas fixtures não são crashes novos de jogo.

Conferência documental: **694 destinos locais de arquivo em 52 documentos**,
14 módulos alcançáveis, 49 contratos únicos/cobertos no roadmap, ordem 1–49,
oito totais de fase e 7/196 marcos =3,6% conferidos. Fragmentos/âncoras não
auditados nessa contagem. Doze fontes do manifesto verificadas por bytes/hash;
git diff --check aprovado. Nenhum marco adicional concedido por estes testes.

Reprodução após o setup acima:

```bash
cmake -S source -B build/block-info-h0 -DSTAR_SOURCE_IDENTIFIER=740dc6c1-static-inputs-local-dirty -DSTAR_BUILD_ROOT_LIFECYCLE_TESTS=ON
cmake --build build/block-info-h0 --target world_generation_contract_tests world_generation_root_lifecycle_tests -j3
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ASAN_OPTIONS=halt_on_error=1 \
  ./dist/world_generation_contract_tests --gtest_brief=1
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ASAN_OPTIONS=halt_on_error=1 \
  ./dist/world_generation_root_lifecycle_tests --gtest_brief=1
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ASAN_OPTIONS=halt_on_error=1 \
  ./dist/world_generation_contract_tests \
  --gtest_filter='AutomaticRandomSeedTest.*:ByteArrayTest.*:WorldGenerationAssetsLifecycleTest.*:WorldGenerationLookupReadersTest.*:WorldGenerationSourceVersionTest.*:WorldGenerationLookupSourcesTest.*:WorldGenerationBaseTilesTest.*:WorldGenerationBaseTileInputsTest.*:WorldGenerationBaseTileObservationTest.*:WorldGenerationBaseTileLookupContextTest.*:WorldGenerationBlockInfoTest.*:WorldGenerationCustomTerrainTest.*:WorldGenerationBlockInfoStaticInputsTest.*' \
  --gtest_repeat=20 --gtest_shuffle --gtest_random_seed=1236 --gtest_brief=1
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ASAN_OPTIONS=halt_on_error=1 \
  ./dist/world_generation_root_lifecycle_tests --gtest_repeat=5 --gtest_shuffle --gtest_random_seed=1236 --gtest_brief=1
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ASAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir build/block-info-h0 -R '^world_generation_(contract_|root_lifecycle$)' --output-on-failure -j2
```

## Continuidade e progresso

**CPU-01/H0/RAM-01 permanecem em 25%; programa 7/196 marcos =3,6%.**
Mais um recorte parcial não fecha o segundo marco de CPU-01. Faltam layout/
seletores/materiais/versões reais, inicialização/cópias/RNG, reutilização/
afinidade e admissão completa; não se concede percentual por número de testes.

Próximo bloco executável: executar adapter/layout/noise/seletores e cache reais
usando a composição já existente, cobrindo setor comum e saída válida completa.
Não criar outro accessor isolado ou repetir o mesmo lifecycle de Root como
substituto. Resolver as dependências necessárias para esse cenário integrado,
preservando caches exclusivos e decisões/ordem de RNG.

Depois: compor identidade profunda e admissão antes das cópias; comparar
BaseTiles privados, revalidar no dono e só então sombra por setor. Streaming
com rota/save/config/assets/mods fixos para medir setores inéditos/s,
interesse→entrega e update/frame, antes de concluir ganho ou adotar o executor.

Sem ganho de setores/s medido, nova WorldThread, uso de GPU ou escrita paralela
no mundo. Nenhuma captura/log/config de usuário recebida/reanalisada ou alterada;
artefatos brutos fora do Git. Mods/save/multiplayer e fallback CPU continuam
críticos; NPC externo suspenso, Linux Clang adiado. Sem jogo/assets completos,
save/reload, multiplayer, MSan/TSan, OOM/pressão, GPU, outros allocators/plataformas,
revisão independente, Actions, release ou merge em main.

### Continuidade posterior

O [relatório seguinte](sector-runtime-h0-real-sector-sampling-2026-11-10.md),
`b0caadce`, executa construtor normal/Root, layout/Constant/FlatSurface e LRU
reais e compara arrays BaseTiles 32×32 completos com metadata controlada.
Não altera as evidências históricas acima: Perlin/caches mutáveis, contexto
profundo/admissão/jobs e streaming continuam pendentes. É o novo ponto canônico
para continuar, sem novo marco ou ganho de desempenho demonstrado.

## Manifesto das fontes

Doze fontes locais de 10/11/2026; seis alteradas e seis fronteiras auditadas.
Caminhos relativos, bytes e SHA-256; nenhuma dependência externa nova.

| Arquivo | Bytes | SHA-256 |
| --- | ---: | --- |
| `source/game/CMakeLists.txt` | 17380 | `4e09f4b5582d36e7a9073e2b9ed379d384fb57d4106b95024a56f15d87e19e8a` |
| `source/game/StarWorldTemplateTypes.hpp` | 9385 | `904ca8bc292b40e54bfa203e134e18ef82a9d3fc49a7aff7a2ea8bc0d5445732` |
| `source/game/StarWorldGenerationBlockInfoStaticInputs.hpp` | 2267 | `6f3fdfa7ebff5fe3ef0e89b0e87630a98a13bb33ab2ddc1fcd48b7962d28c604` |
| `source/game/StarWorldGenerationBlockInfoStaticInputs.cpp` | 562 | `ad765ccd1ae4b5d2c44a8d5ad18323afcfd8833d7a1256355923f168b78b09e0` |
| `source/test/CMakeLists.txt` | 9759 | `b35e0085e037df6f898d38cf3dd01c3d87b25bff5c31f90ba4fbc54d8d394fb3` |
| `source/test/world_generation_block_info_test.cpp` | 39445 | `e0095dc3e9fb47699815bd5cc7eecdbf6c0ee6794ba37979637916e8d90862d0` |
| `source/core/StarJson.hpp` | 13745 | `f18dbef8fe6fe505b82a98561a9202f2707dfeb353f63c67b20bb8292be687c0` |
| `source/core/StarJson.cpp` | 27217 | `9e1830a6195e06a5c5b8ef474032d2c84c9c076657c2441beea7ee1d855552de` |
| `source/game/StarWorldGenerationMemoryEstimate.hpp` | 2827 | `697a65cd0fd7abe9d9487ee94f8bc3c917b305fe0698cc4e42f0338eb7cb8368` |
| `source/game/StarWorldGenerationMemoryEstimate.cpp` | 4468 | `752b59ce83e1992230990797a10a27ba4c9957ff52f80d084a5f1d51814da90e` |
| `source/game/StarWorldGenerationMemory.cpp` | 13158 | `c39bb4ae6cd8c8efa0dfd7818ca91c642ada76c3463385ff1445b610981b7094` |
| `source/game/StarWorldTemplate.cpp` | 28668 | `f6e17ad793e38a4b302c207d20d64aa085b3cfffee31493d342588edab43104e` |
