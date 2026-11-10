# H0 / CPU-01: contexto proprietário de custom terrain

Continuidade: [config lazy e inputs estáticos compostos](sector-runtime-h0-block-info-static-inputs-2026-11-10.md).

Data: 10/11/2026 UTC. Branch de entrega: `Experimental`;
revisão local: `codex/experimental-regional-runtime`.
Base: `82ff948ec69779356500ad9e87c42c2eb80b3c31`.
Implementação: `bfbbf40340cdd205f1740c0ee430da1aed129333`.

[Programa](sector-runtime/README.md) · [Roadmap](sector-runtime/roadmap.md)
· [Progressão](sector-runtime/progress.md)
· [Contrato BaseTiles](sector-runtime-base-tiles-contract.md)
· [Envelope](sector-runtime-memory-envelope.md)
· [Etapa anterior](sector-runtime-h0-block-info-kernel-2026-11-10.md)

## Objetivo, entrega e benefício preparado

Entregar o primeiro recorte proprietário de inputs de CPU-01: custom terrain
calculável sem template vivo, Root/Assets, callbacks, RNG ou caches mutáveis.
O kernel herdado agora recebe geometria, regiões e parâmetros explícitos.
WorldTemplate::captureCustomTerrain cria WorldGenerationCustomTerrainContext
com cópias profundas dos polígonos/bounds, geometria/blend por valor e revisão
observada do template. A interface permite leitura e handoff por move, não cópia
acidental por job; não mover/atribuir/destruir a captura enquanto houver leitores.

A captura pode sobreviver à destruição da origem e ser lida por tarefas
independentes. Esse é o benefício funcional já testado, não ganho de setores/s.
Prepara a composição/reutilização do contexto por mundo/revisão sob demanda e
orçamento. Não cria cache, worker novo, limite por CPU ou executor global.

**Dois caminhos distintos:** geração normal usa o mesmo kernel com uma visão
emprestada dos dados atuais; não captura/copia por bloco. A captura proprietária
é executada pelos testes e não foi adotada nos jobs normais. No adapter legado,
Root::singleton().assets() continua antes da matemática, mantendo o handle no
mesmo escopo. Não se antecipou nem removeu essa chamada/falha observável.

## Dados e fronteiras

| Capturado por valor | Garantia e limitação |
| --- | --- |
| WorldGeometry | Largura/altura ou geometria nula preservadas; wrap horizontal real, sem novo clamp vertical |
| Polígonos e bounds | Vértices copiados, mesma ordem; bounds existentes copiados, não recomputados na captura |
| solid/space | Mesmo sinal e ordem de acumulação, sem reordenação/redução paralela |
| blendSize/blendWeight | Floats já resolvidos pelo template; sem nova leitura/validação antecipada de JSON |
| templateRevision | Observação do contador local; não worldEpoch, identidade entre mundos, lease ou autorização de commit |

A lista é reservada uma vez por captura e cada polígono tem seu próprio storage.
Não há ponteiro para WorldTemplate, layout, Root ou Assets. A classe não possui
API de mutação dos dados, mas move assignment continua sendo uma escrita:
publicação/lifetime do futuro executor precisa protegê-la como qualquer owner.

O heap de lista/vértices **ainda não é admitido/registrado no ledger**.
A API é preparatória e fora dos jobs; antes da adoção será necessário reservar
antes das cópias, contabilizar storage/retentores únicos e manter a reserva até
término físico de todos os leitores. Não houve nova classe, cota ou modelo de
memória. H0/RAM-01 continuam parciais; a existência da captura não amplia filas.

Não inclui config lazy de blockInfo, layout/biomas/seletores, materiais, assets,
RNG ou identidade profunda. Caches mutáveis de seletores continuam exclusivos.
Não corrige os achados de Perlin, WorldTile::biomeTransition ou
SkyParameters::horizonClouds. A seed automática já foi corrigida anteriormente.
Capturar só os dados necessários evita copiar esses outros campos, não prova
a inicialização do contexto completo.

## Equivalência e compatibilidade

calculateWorldGenerationCustomTerrainWeighting conserva a matemática herdada:
filtro pelo bounds padded, distância real ao polígono, comparação distance >=
blendSize, normalização, sinal de space e influência pela menor distância.
Sem novos valores default, validação de parâmetros, normalização de bounds,
RNG, formatos persistidos, chamadas de mods ou publicação no mundo.

Os três corpos de addCustomTerrainRegion/addCustomSpaceRegion/clearCustomTerrains
foram relocados para StarWorldGenerationCustomTerrain.cpp sem alteração.
Mesmas assinaturas, append/clear, invalidação do block cache e incremento de
generationRevision; mesma implementação no jogo e testes, sem stubs.

Conferência textual contra a base:

- Bloco contíguo dos três mutadores, sem whitespace final: 476 bytes,
  SHA-256 `be187054e58b9222b82b52ceb0e2207c6c533f0a734f667765b700552e042be6`.
- Corpo matemático anterior, excluindo aquisição de Assets e newline final:
  863 bytes, SHA-256
  `c32e9dca5d17d3d09fdbc2291efd355375688c37f08083a0e15d4af7a4280da6`.
  A referência independente no teste contém esse corpo inalterado.
- Adapter normal verificado mantendo Assets antes do helper. A chamada real
  desse adapter com Root/layout completo ainda não foi executada nesta suíte.

Sem promessa de equivalência integrada de todos os mods/saves/multiplayer.
WorldGeometry mantém suas restrições para objetos maiores que meia largura do
mundo; os testes de wrap usam formas pequenas. Não foi auditado todo parâmetro
não finito, polígono degenerado ou coordenada extrema, nem feito saneamento
silencioso de comportamento legado.

## Teste acelerado com geometria real

**26 casos novos** com WorldGeometry/PolyF reais e comparação por bits dos dois
floats contra a matemática herdada independente. Há expectativas dirigidas e
grades de coordenadas por caso; cada ponto da grade não é outro caso GTest.

Cobrem solid/space, ausência de regiões, interior/exterior/borda, filtro por
bounds versus distância, cancelamento e pesos normalizados, ordem de 24 regiões,
triângulo/concavidade, wrap, y negativo, geometria nula, blend zero/negativo e
amplitude zero/negativa. Bounds intencionalmente inconsistentes numa fixture
confirmam que a captura não os recomputa; não é política de construtor normal.

A factory e os três mutadores são métodos reais de WorldTemplate. A origem é
criada pela tag privada já existente, com somente geometria/blend/metadata
controlados e SkyParameters::horizonClouds inicializado pela fixture. **Não**
é construção normal de WorldTemplate/WorldLayout nem teste de seletores/Root
nesse caminho. O novo friend só dá acesso ao teste, não substitui métodos.

Também confirmados:

- captura não altera revisão/cache; mutadores reais invalidam uma entrada
  sintética de cache e avançam revisão — não teste real de blockInfo hit/miss;
- captura antiga resiste a clear e 128 novas regiões, alteração in-place de
  vértices, geometria/parâmetros novos e destruição da origem;
- weak_ptr confirma que a captura não retém o template; move preserva os dados;
- três leitores via std::async obtêm os mesmos bits após destruição da origem;
  são leitores de teste, não três workers de setor no runtime nem prova TSan;
- dois mundos com a mesma revisão podem produzir saídas diferentes. Nunca
  aceitar resultado usando apenas esse contador.

## Build e validações executadas

Skill de ambiente cloud usada para conferir executor/política e reutilizar o
cache/dependências existentes, sem instalação/download. GCC 14.2.0/C++17,
CMake 3.29.2/Ninja 1.13.0.git.kitware.jobserver-pipe-1, Debug -Og;
GUI/Tracy/allocators alternativos OFF, BUILD_TESTING=ON, VCPKG_MANIFEST_INSTALL=OFF.
Cache `build/block-info-h0`, identifier `82ff948e-custom-terrain-local-dirty`;
mesmo vcpkg/sysroot/hook SDL3 real do [relatório anterior](sector-runtime-h0-block-info-kernel-2026-11-10.md).
C++ usa -fsanitize=address,undefined -fno-omit-frame-pointer e
-DXXH_NO_INLINE_HINTS=1; C/dependências pré-compiladas não têm a mesma cobertura.

Com UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 e ASAN_OPTIONS=halt_on_error=1,
sem diagnóstico dos sanitizers:

1. **723/723 casos padrão**, 37 suites; **26 novos** nesta entrega.
2. **13/13 Root opt-in**, regressão, não casos novos: **736 únicos com a opção
   ON**, não no alvo/Actions padrão. Opção continua padrão OFF.
3. **239×20 =4.780 execuções adicionais** em 12 suites, mais **13×5 =65** Root:
   **4.845 adicionais**, shuffle/seed inicial 1235, não casos únicos.
   Três microexperimentos DISABLED não foram executados.
4. **37+1 grupos CTest**, contratos + Root, NoAssets, -j2, todos aprovados.
   Repetem os mesmos casos; não toda a bateria core_tests/game_tests.
5. Build/link dos dois executáveis; três objetos normais do jogo compilados:
   WorldTemplate, WorldGenerationCustomTerrain e WorldGeneration. Não link ou
   execução do jogo completo. Nenhum stub ou sanitizer removido.
6. Root OFF: 37 grupos de contratos, nenhum de Root, alvo padrão sem trabalho
   pendente. Restaurado ON: ambos executáveis e três objetos sem trabalho
   pendente. A nova suite não exige Root opt-in.
7. Referência/mutadores e ordem de Assets conferidos contra a base; manifesto
   e links/IDs/percentuais conferidos na revisão documental. Avisos herdados
   de fmt/Assets não corrigidos; Tracy/GUI não revalidados nesta entrega.

JSON/material/listeners inválidos nos testes de Root/leitores são cenários
esperados, não crashes novos. Nenhuma captura/log/config de usuário foi recebida
ou reanalisada. Arquivos de runtime do usuário não foram alterados.

Conferência documental: **676 destinos locais de arquivo em 51 documentos**,
14 módulos alcançáveis, 49 contratos únicos/cobertos no roadmap, ordem 1–49 e
oito totais de fase conferidos; fragmentos/âncoras não auditados nessa contagem.
7/196 marcos =3,6% e dez fontes do manifesto verificados; diff sem whitespace
inválido. O marco novo é de CPU-01, não de H0/RAM-01 ou throughput medido.

Reprodução após o setup acima:

```bash
cmake -S source -B build/block-info-h0 -DSTAR_SOURCE_IDENTIFIER=82ff948e-custom-terrain-local-dirty -DSTAR_BUILD_ROOT_LIFECYCLE_TESTS=ON
cmake --build build/block-info-h0 --target world_generation_contract_tests world_generation_root_lifecycle_tests -j3
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ASAN_OPTIONS=halt_on_error=1 \
  ./dist/world_generation_contract_tests --gtest_brief=1
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ASAN_OPTIONS=halt_on_error=1 \
  ./dist/world_generation_root_lifecycle_tests --gtest_brief=1
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ASAN_OPTIONS=halt_on_error=1 \
  ./dist/world_generation_contract_tests \
  --gtest_filter='AutomaticRandomSeedTest.*:ByteArrayTest.*:WorldGenerationAssetsLifecycleTest.*:WorldGenerationLookupReadersTest.*:WorldGenerationSourceVersionTest.*:WorldGenerationLookupSourcesTest.*:WorldGenerationBaseTilesTest.*:WorldGenerationBaseTileInputsTest.*:WorldGenerationBaseTileObservationTest.*:WorldGenerationBaseTileLookupContextTest.*:WorldGenerationBlockInfoTest.*:WorldGenerationCustomTerrainTest.*' \
  --gtest_repeat=20 --gtest_shuffle --gtest_random_seed=1235 --gtest_brief=1
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ASAN_OPTIONS=halt_on_error=1 \
  ./dist/world_generation_root_lifecycle_tests --gtest_repeat=5 --gtest_shuffle --gtest_random_seed=1235 --gtest_brief=1
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ASAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir build/block-info-h0 -R '^world_generation_(contract_|root_lifecycle$)' --output-on-failure -j2
```

## Progresso e próximos passos

**CPU-01: 0% →25%**, primeiro recorte proprietário funcional e testado, não só
interface de callbacks ou desenho. Diferente do kernel anterior, o cálculo
capturado independe efetivamente da origem após destruição e troca de leitores.
É aplicação do primeiro marco do painel, não crédito por quantidade de testes.

Não recebe 50%: faltam config lazy, layout/seletores e materiais/versões, auditoria
de inicialização/RNG, reutilização/afinidade e orçamento do contexto completo.
Integração nos jobs, equivalência com assets/mods/saves/multiplayer, benchmark e
revisão independente continuam gates posteriores. **H0/RAM-01 em 25%;
H1 em 2,5%; programa 7/196 marcos =3,6%; zero contratos integralmente aceitos.**
H0 continua ativo; este recorte de apoio não inicia grafo/executor ou muda a
ordem de dependências do roadmap.

Próximas entregas executáveis, sem exigir outra captura de usuário:

1. Capturar config de blockInfo preservando getFloat lazy/falhas somente no ramo
   alcançado; reter valores/nós JSON usados com lifetime/contabilidade explícitos.
2. Exercitar adapter/layout/noise/seletores reais e cache hit/miss/invalidação,
   mantendo caches exclusivos; fechar os achados de inicialização/cópias/RNG.
3. Compor o contexto privado completo, identidade profunda e reserva antes das
   cópias; comparar BaseTiles, revalidar no dono e só então sombra por setor.
4. Streaming com rota/save/config/assets/mods fixos para medir setores inéditos
   entregues/s, interesse→entrega e update/frame; depois mailbox/DAG/executor.

Sem nova flag/default, orçamento, TTL, paralelismo de mundo vivo ou ganho de
setores/s medido. Mods/save/multiplayer e fallback CPU continuam críticos;
NPC externo suspenso, Linux Clang adiado. Sem streaming/jogo/assets completos,
save/reload, multiplayer, MSan/TSan, OOM/pressão, GPU, outras plataformas/
allocators, revisão independente, Actions, release ou merge em main.
Artefatos brutos permanecem fora do Git.

## Manifesto das fontes

Dez fontes locais de 10/11/2026; sete alteradas e três fronteiras geométricas
auditadas. Caminhos relativos, bytes e SHA-256; nenhuma dependência nova.

| Arquivo | Bytes | SHA-256 |
| --- | ---: | --- |
| `source/game/CMakeLists.txt` | 17282 | `a24390ae9cea9af83d610b62ed47ae0f2761bae49c10cc2cbd721a8d158a7264` |
| `source/game/StarWorldTemplate.cpp` | 28668 | `f6e17ad793e38a4b302c207d20d64aa085b3cfffee31493d342588edab43104e` |
| `source/game/StarWorldTemplateTypes.hpp` | 8858 | `6a38335fef0043e2cb019614eebecfb58f3f8738b21f7f0492c83125acdb3a26` |
| `source/game/StarWorldGenerationCustomTerrain.hpp` | 3146 | `6778878d26fbcc812e87e96cc996ae65ea56da4db1098b01b0c0b45e428a0112` |
| `source/game/StarWorldGenerationCustomTerrain.cpp` | 1130 | `ee268715bd631b207273e5bc88b9e99878da1274af5ab57985f7eeed0cc0fe80` |
| `source/test/CMakeLists.txt` | 9659 | `8e284dea13d981c0a12be3818d46bdf48fa1238dc2a0cc801dfcd3dc9774c976` |
| `source/test/world_generation_custom_terrain_test.cpp` | 14419 | `62dfb57f7c4c54fc16aed970bbe5a126b3382e3fe59753de743da39f8559ec4e` |
| `source/base/StarWorldGeometry.cpp` | 11274 | `1b70898835f38ee67b1cb64a16c3de58d0a0881a19ea51bbaa875819b558c8ce` |
| `source/base/StarWorldGeometry.hpp` | 8074 | `59dda6cb9ffc5d507574af44c89e543bc3704d50d0dbfa87998319f1aaaa37b4` |
| `source/core/StarPoly.hpp` | 21667 | `9960f9de744332ea0e6cd9db071b6b0a5d20fe3d6d65aa2c2435d7ad6047a9dd` |
