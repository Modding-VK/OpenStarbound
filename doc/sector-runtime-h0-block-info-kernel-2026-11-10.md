# H0: kernel de blockInfo e fronteira do contexto privado

Data: 10/11/2026 UTC. Entrega: `Experimental`; revisão local:
`codex/experimental-regional-runtime`.
Base: `05b63092c0cc387ba9c72b65cea43cfe2cfc3865`.
Implementação: `0871d13ce0171f66d126a7b419deff42b6e2fde0`.

[Programa](sector-runtime/README.md) · [Roadmap](sector-runtime/roadmap.md)
· [Progressão](sector-runtime/progress.md)
· [Contrato BaseTiles](sector-runtime-base-tiles-contract.md)
· [Envelope](sector-runtime-memory-envelope.md)
· [Etapa anterior](sector-runtime-h0-root-lifecycle-2026-11-10.md)

## Objetivo e entrega

Separar a matemática de blockInfo de seu cache/host, preservando a geração
existente, para fechar o read set do futuro contexto privado. Não é aumentar
orçamento ou ligar mais previews. O algoritmo herdado de WorldTemplate foi
extraído para calculateWorldGenerationBlockInfo, com adapter estático no miss
do cache normal; BaseTiles e os demais consumidores de blockInfo passam por ele.
Não há segundo algoritmo de terreno em produção.

O kernel recebe oito operações de fonte, conserva output local e só o retorna
ao concluir. Não possui cache, Root, registro global ou escrita em mundo.
O adapter é não proprietário, usa o WorldTemplate atual do worker e NÃO é
um contexto congelado, lease ou autorização de commit. Uma fonte arbitrária
pode produzir efeitos: o template genérico não prova pureza por si só.

Sem nova flag/default, tarefa, orçamento, classe do ledger, fita residente por
tile, std::function ou despacho virtual adicional. As consultas aos seletores
já virtuais continuam existindo. Não foi medido custo por tile, memória total
ou ganho de setores/s; não confundir extração testável com aceleração entregue.

## Read set e ordem que devem ser preservados

| Entrada/consulta | Momento e efeito legado |
| --- | --- |
| Presença do layout | Layout ausente retorna BlockInfo default sem outras consultas |
| Pesos planos | Primeira consulta; bioma ambiental e metadata de líquidos vêm do primeiro peso plano |
| Blend noise | Depois dos pesos planos; conversão para int mantém truncamento, não arredondamento |
| Block noise e geometria | Opcional; pesos do bloco usam x deslocado pelo blend, transição usa posição sem esse deslocamento |
| Pesos vazios | Checados depois das consultas de noise/transição; não antecipar o retorno |
| biomeTransitionThreshold | Float lazy, fallback 0, comparação estrita; não prevalidar config inteira |
| Seletores de terreno | Ordem dos pesos planos, coordenada xValue e y original, soma/multiplicações sem reassociação |
| Custom terrain | Depois da seleção de terreno, antes das cavernas; quintic2 mantém ordem e precisão existentes |
| Cavernas | Só em terreno positivo; foreground antes de background por peso |
| surfaceCaveAttenuationDist/Factor | dist só em terreno positivo; factor só quando terreno < dist; fallbacks 0/1 |
| Líquidos/enclosure | Metadata do peso plano, limite y < oceanLiquidLevel; não mover para o bioma deslocado |
| Bioma do bloco | Só em terreno; ausência mantém materiais vazios e evita consultas seguintes |
| Subblocos | Ordem e primeiro match; usa xValue menos blend e y do bloco; ainda consulta mesmo nas duas cavernas |
| Minérios | Foreground depois background, somente camada não cavada, primeiro match; coordenadas originais x/y |

O wrapper de blockInfo, wrap horizontal, chave/limite/evicção do cache e seu
comportamento de retorno não foram alterados. Conversões unsigned/signed
continuam as da implementação anterior, não uma nova normalização portátil.
Arrays incorretos/config inválida só falham se o ramo realmente os consulta.
Não transformar uma falha privada em retry de efeitos ou fallback sobre tile
já modificado. Nenhuma nova recuperação foi adicionada.

## Dependências ainda não congeladas

WorldTemplate::customTerrainWeighting ainda chama Root::singleton().assets()
antes da matemática, embora não use a variável nessa matemática. A chamada
pode construir/carregar fontes e falhar: removê-la apenas como variável não
utilizada mudaria efeitos/lifecycle. O adapter preserva o ponto da chamada.
O kernel de teste controla essa fronteira; não executa Root nesse caminho.

O layout e biomas permanecem ligados ao template atual do worker. Seletores
const não são todos imutáveis: CacheSelector tem m_cache, IslandSurfaceSelector
columnCache, WormCaveSelector m_cache e KarstCaveSelector m_layerPerlinsCache/
m_sectorCache, todos mutáveis. Não compartilhar esses objetos entre jobs.
Construtores/reconstrução, RNG e retenção profunda continuam parte de CPU-01.

Config de template é consultada nos ramos, não pré-decodificada na captura.
Uma representação futura deve preservar essas falhas lazy e reter os valores
efetivamente usados, não confiar apenas no clock de Assets: clearCache pode
renovar conteúdo bruto sem avançar a versão explícita. Binding anterior não
se tornou snapshot por causa desta extração.

Sem correção adicional de inicialização: Perlin, WorldTile::biomeTransition e
SkyParameters::horizonClouds continuam abertos; seed automática já foi corrigida
na etapa anterior. ASan/UBSan não provam ausência de leitura não inicializada.

## Teste acelerado e referência independente

A suíte nova usa entradas e fronteiras de consulta controladas, sem implementar
WorldLayout/Perlin nem substituir símbolos de produção. Biome/BlockInfo são
tipos reais. A referência é o corpo herdado da base, independente do kernel:
apenas indentação e tipo dos pesos foram adaptados à fixture; os bindings
locais fornecem layout/config/geometry/custom terrain/bioma controlados.
Verificação textual contra git show da base aprovada. SHA-256 desse corpo
normalizado: `48a94037424d8ac2eb60ffeb078e18c45088ba54111b18f52fb2bb5a15f302ad`.

São comparados todos os **16 campos** de BlockInfo (density float por bits),
trace completa de consultas e, nas falhas, tipo/mensagem de exceção e prefixo
da trace. Cenários dirigidos incluem entradas vazias, noises, coordenadas,
biomas independentes, pesos/nulos, custom terrain/space, cavernas/atenuação,
líquidos, primeiro match, arrays curtos, config inválida e falha injetada em
cada fronteira de consulta. Uma matriz de **4.096 combinações** é UM caso GTest,
não 4.096 testes únicos ou setores reais. Não mede throughput do jogo.

## Build e validações executadas

O executor reiniciou nesta continuidade; o cache e headers Tracy em /tmp da
etapa anterior não estavam disponíveis. A skill de ambiente cloud orientou a
conferência do executor restaurado e reutilização de ferramentas/dependências
já existentes em workspace, sem instalação/download. Cache novo e ignorado:
`build/block-info-h0`, fora de /tmp; identifier
`05b63092-block-info-local-dirty`, correspondente ao código acima.

GCC 14.2.0/C++17, CMake 3.29.2 e Ninja 1.13.0.git.kitware.jobserver-pipe-1,
Debug -Og; GUI/Tracy/allocators alternativos OFF, BUILD_TESTING=ON,
VCPKG_MANIFEST_INSTALL=OFF. Reutiliza o vcpkg/sysroot do checkout base e hook
SDL3 real externo ao Git, como no [setup anterior](sector-runtime-h0-base-tile-inputs-2026-11-09.md).
C++: -fsanitize=address,undefined -fno-omit-frame-pointer e
-DXXH_NO_INLINE_HINTS=1. C/dependências pré-compiladas não têm a mesma cobertura.

Uma invocação redundante/concorrente de build e sua interrupção deixaram o cache
de dependências inconsistente. O primeiro link falhou por objeto ausente; uma
repetição expôs o cache truncado. As invocações foram encerradas, a ferramenta
`ninja -t recompact` reparou seus metadados e os alvos foram reconstruídos.
Não foi uma correção de fonte do jogo. Tentativas intermediárias não contam
como aprovações; o build final repetido terminou **sem trabalho pendente**,
sem warning de recuperação. Não repetir duas invocações Ninja no mesmo cache.

Com UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 e ASAN_OPTIONS=halt_on_error=1,
sem diagnóstico dos sanitizers nas execuções aprovadas:

1. **697/697 casos padrão**, 36 suites, **28 novos** de blockInfo.
2. **13/13 casos opcionais de Root**, regressão da etapa anterior, não 13 casos
   novos: **710 únicos com a opção ON**, não 710 no alvo/Actions padrão.
3. **213×20 =4.260 execuções adicionais**, 11 suites, mais **13×5 =65** de
   Root: **4.325 adicionais**, shuffle/seed inicial 1234. Não casos únicos.
   Três microexperimentos DISABLED não foram executados.
4. **36+1 grupos CTest**, filtro de contratos + Root, NoAssets, -j2, todos
   aprovados; repetem os mesmos casos. Não toda a bateria core_tests/game_tests.
5. Build/link dos dois executáveis com core/base e serviços reais; objetos
   normais de WorldTemplate e WorldGeneration compilados com o kernel/adapter.
   **Não** link ou execução do jogo completo. Nenhum stub ou sanitizer removido.
6. Opção Root OFF: 36 grupos, zero de Root e build padrão sem trabalho pendente;
   restaurada para ON, os dois executáveis e dois objetos seguem sem trabalho
   pendente. A nova suite padrão não depende da opção de Root.
7. Referência herdada normalizada conferida contra a base e manifesto de onze
   fontes conferido por bytes/SHA-256; links/IDs/percentuais e diff conferidos
   na revisão documental. Avisos herdados de fmt/Assets/ABI não corrigidos aqui.

Erros de JSON/material/listeners e falhas de consulta são cenários intencionais,
não crashes novos ou diagnósticos de sanitizer. Uma execução diagnóstica inicial
com 27 casos, antes do último teste, não é outra contagem de casos únicos.
Tracy não foi rechecado nesta entrega; nenhum header novo foi baixado ou pin
alterado. Não inferir validação de GUI, CI ou plataformas pela compilação GCC.

Conferência documental: **657 destinos locais de arquivo em 50 documentos**,
14 módulos alcançáveis, 49 contratos únicos/cobertos no roadmap e ordem 1–49.
Fragmentos/âncoras não foram auditados por essa contagem. Oito totais de fase,
6/196 marcos e 3,1% recalculados; onze fontes do manifesto verificadas.
`git diff --check` aprovado. Nenhum percentual foi aumentado por estes testes.

Reprodução após configurar dependências/hook/flags acima:

```bash
cmake -S source -B build/block-info-h0 -DBUILD_TESTING=ON -DSTAR_BUILD_ROOT_LIFECYCLE_TESTS=ON
cmake --build build/block-info-h0 --target world_generation_contract_tests world_generation_root_lifecycle_tests -j3
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ASAN_OPTIONS=halt_on_error=1 \
  ./dist/world_generation_contract_tests --gtest_brief=1
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ASAN_OPTIONS=halt_on_error=1 \
  ./dist/world_generation_root_lifecycle_tests --gtest_brief=1
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ASAN_OPTIONS=halt_on_error=1 \
  ./dist/world_generation_contract_tests \
  --gtest_filter='AutomaticRandomSeedTest.*:ByteArrayTest.*:WorldGenerationAssetsLifecycleTest.*:WorldGenerationLookupReadersTest.*:WorldGenerationSourceVersionTest.*:WorldGenerationLookupSourcesTest.*:WorldGenerationBaseTilesTest.*:WorldGenerationBaseTileInputsTest.*:WorldGenerationBaseTileObservationTest.*:WorldGenerationBaseTileLookupContextTest.*:WorldGenerationBlockInfoTest.*' \
  --gtest_repeat=20 --gtest_shuffle --gtest_random_seed=1234 --gtest_brief=1
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ASAN_OPTIONS=halt_on_error=1 \
  ./dist/world_generation_root_lifecycle_tests --gtest_repeat=5 --gtest_shuffle --gtest_random_seed=1234 --gtest_brief=1
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ASAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir build/block-info-h0 -R '^world_generation_(contract_|root_lifecycle$)' --output-on-failure -j2
```

## Próximos passos executáveis

1. Separar a matemática de custom terrain de sua aquisição legada de Root,
   mantendo o adapter legado e testando geometria/polígonos reais. Capturar
   geometria, regiões e parâmetros com ownership/lifetime explícitos, sem
   antecipar silenciosamente a chamada/falha de Assets no caminho legado.
2. Exercitar adapter/layout/noise/seletores reais, cache hit/miss/invalidação
   e mudanças de template. Preservar caches exclusivos e história de
   construção/cópias/RNG; fechar a auditoria de inicialização antes de snapshot.
3. Construir o contexto privado completo e sua identidade profunda; comparar
   outputs de BaseTiles, revalidar no dono e só então sombra por setor com
   admissão/bytes/descarte stale. Não ampliar filas pela existência do kernel.
4. Streaming com rota/save/config/assets/mods fixos: setores inéditos entregues/s,
   interesse→entrega e update/frame. Depois mailbox/DAG e executor produtivo
   conforme o roadmap. Não exigir nova captura do usuário para o item 1.

**H0/RAM-01 seguem em 25%, programa em 3,1% (6/196 marcos, 49 contratos).**
CPU-01 permanece em 0%: o contexto completo não foi entregue. Nenhum novo marco
completo concedido por contagem de testes. Mods/save/multiplayer, fallback CPU,
commit serial, NPC externo suspenso e Linux Clang adiado permanecem decisões
críticas. Sem jogo/assets/mods completos, streaming, save/reload, multiplayer,
MSan/TSan, OOM/pressão, benchmark integrado, GPU, outras plataformas/allocators,
revisão independente, Actions, release ou merge em main. Sem dados novos do
usuário ou reanálise de capturas; artefatos brutos permanecem fora do Git.

## Manifesto das fontes

Onze fontes locais de 10/11/2026; caminhos relativos, bytes e SHA-256.
Cinco arquivos de implementação/testes alterados; demais são fronteiras
auditadas, não mudanças nesta entrega. Sem dependência externa nova.

| Arquivo | Bytes | SHA-256 |
| --- | ---: | --- |
| `source/game/CMakeLists.txt` | 17200 | `8544d2c062f4b5b43e5fa836480bd9f12b5ea03547b28d1afe61418b16508775` |
| `source/game/StarWorldGenerationBlockInfo.hpp` | 7292 | `37d6f0af1a9c49e7e987ce5654c2ea1bfc763de6fdb13508472bff0235e4649c` |
| `source/game/StarWorldTemplate.cpp` | 29851 | `ab90b360441da690aa560378c2918acc946a52efd7e8de33b004f9f1f9ebfb53` |
| `source/game/StarWorldTemplateTypes.hpp` | 8527 | `47d5bd8945578765cc402dab6078c72eedd59cfb7cd4af17d4a606e1f78c789d` |
| `source/game/StarWorldLayout.hpp` | 5840 | `400827cb19bbaca99a1d798a6f508b02b879b1ae4132ab9c82343bb5355e2448` |
| `source/game/terrain/StarCacheSelector.hpp` | 444 | `8b54443914595b54714c15ef3e4cf6239a3c0066ac031ce254adcdad393b6f45` |
| `source/game/terrain/StarIslandSurfaceSelector.hpp` | 680 | `1996b78d44bc54b21c59f5fc39c544d58228446929d0278b25269aee957a3ba2` |
| `source/game/terrain/StarWormCave.hpp` | 758 | `3acb0f418ccf0be30b38439c1d8077a7ac8c580687e28665076b9b5494dfd2e8` |
| `source/game/terrain/StarKarstCave.hpp` | 1316 | `95e803600c8e052923881c1d6facb6a1d0d897ab37edcaab81eb6d11bf690765` |
| `source/test/CMakeLists.txt` | 9532 | `6d96e205f1436d18dfb99659e92bb5b36ca145619c231c1da89a4efb7b5f17b1` |
| `source/test/world_generation_block_info_test.cpp` | 21854 | `5b67d72d039fba50db203e84f4fb388c4a994029a6d6983f924514de710bb5fa` |
