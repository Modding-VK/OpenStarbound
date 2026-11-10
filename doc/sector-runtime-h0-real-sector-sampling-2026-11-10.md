# H0 / CPU-01: layout/cache reais e BaseTiles 32×32

Data: 10/11/2026 UTC. Entrega: `Experimental`;
revisão local: `codex/experimental-regional-runtime`.
Base: `55b2d3691aad116f5421b5206faf170287b471fc`.
Implementação: `b0caadcea499afc15880768318b8297eb5b84969`.

[Programa](sector-runtime/README.md) · [Roadmap](sector-runtime/roadmap.md)
· [Progressão](sector-runtime/progress.md)
· [Contrato BaseTiles](sector-runtime-base-tiles-contract.md)
· [Envelope](sector-runtime-memory-envelope.md)
· [Etapa anterior](sector-runtime-h0-block-info-static-inputs-2026-11-10.md)

## Objetivo e entrega

Executar os inputs estáticos já compostos contra a amostragem normal, agora
com construtor normal de WorldTemplate, Root, WorldLayout e seletores reais,
em vez de substituir pesos/consultas por respostas programadas. Produzir arrays
BaseTiles completos de 32×32 tiles válidos e comparar seus 25 campos sem
publicação no mundo. Não adicionar outro accessor ou alterar o scheduler.

Configuração da fixture: mundo 64×64, blend de regiões 8, superfícies em y=16/40,
seed explícita dos seletores 1234. world_template.config usa customTerrainBlendSize=4,
customTerrainBlendWeight=32, blockCacheSize=16, biomeTransitionThreshold=0.75,
surfaceCaveAttenuationDist=0 e surfaceCaveAttenuationFactor=1. Casos negativos
alteram as chaves indicadas no teste; teste de eviction usa cache 2. Assets tem
zero workers. Não houve config ou ambiente de jogo do usuário nesta entrega.

O alvo opt-in existente recebe 19 casos novos e uma segunda entrada CTest.
Usa Root normal, Assets e parser de MaterialDatabase com arquivos mínimos de
primeira parte. WorldTemplate(Vec2U) chama o construtor normal, lê a configuração
real de Assets e consome sua seed como antes; não usa tag de fixture.

O grafo do layout é montado com metadata controlada por um friend de teste.
WorldLayout::getWeighting, navegação das células, wrap e blend horizontal/vertical
são implementações reais. ConstantSelector e FlatSurfaceSelector executam
construtores/get reais. Config/custom terrain capturados são compostos com uma
visão privada desse grafo num adapter **somente de teste**, não novo contexto
profundo ou produtor de jobs. Cada leitor recebe seu próprio layout/seletores.

Para permitir ligação isolada, 23 definições herdadas foram relocadas, sem
alterar seus corpos, em três unidades usadas também pela geração normal:

| Unidade nova | Definições | Fronteira |
| --- | ---: | --- |
| StarWorldTemplateSampling.cpp | 7 | construtores, size/biome, blockInfo, custom terrain e cache |
| StarWorldLayoutSampling.cpp | 9 | default de região/camada, pesos, navegação e consultas de noise |
| StarTerrainSelector.cpp | 7 | parâmetros e base dos seletores |

As 23 definições coincidem byte a byte com a base; retirando-as dos arquivos
originais, só diferem linhas em branco. SHA-256 da concatenação das definições
(7/9/7, separadas por newline):
`6b34a1944c7dcfd8f3e10bb5e80d3d50547dbb2642fb61a0484cbfc0c54e0acb`.
Kernel e teste/referência independente de blockInfo não foram modificados.
Sem símbolos substitutos, sanitizer desativado ou nova dependência externa.

## Cobertura e limites da comparação

- Grades de 1.435 coordenadas por cenário comparam os 16 campos de BlockInfo,
  incluindo bits da densidade de líquido: superfície, wrap, transições entre
  biomas/camadas, cavernas de foreground/background e primeiro match de
  subblocos/minérios. Coordenadas não são contadas como casos GTest únicos.
- A LRU real de WorldTemplate exercita hit/miss, limite/eviction, invalidação
  por custom terrain e troca de layout. Hit após destruir Root continua válido;
  miss mantém a falha herdada de acesso a Root. Exceção não insere resultado
  parcial. Config inválida não consultada continua lazy; hotReload não substitui
  o Json já retido pelo template antigo.
- Três tarefas executam 72 amostras cada, com layouts privados, depois de
  destruir template e Root de origem, compartilhando apenas inputs estáticos.
  Não é prova TSan, captura de todo o grafo ou uso do pool de geração real.
- Dois casos produzem arrays reais de 1.024 tiles, inicializados explicitamente,
  usando a aplicação BaseTiles e o resolver de WorldTemplate/MaterialDatabase
  reais. Comparador valida todos os 25 campos sob ticket sintético válido.
  Há expectativas independentes para sólido/ar/hue e líquido: cenário seco
  permanece vazio; cenário oceânico cria fonte ID 3, nível 1 e pressão 4 em
  (8,20), mantendo líquido vazio no sólido (8,4).

As duas rotas usam o **mesmo** kernel e o mesmo adaptador downstream de
BaseTiles: esse ensaio isola equivalência de bindings/amostragem, não constitui
segunda implementação independente de toda a geração. As referências
independentes anteriores continuam cobrindo matemática/ordem. Sem memcmp de
classes/padding, cópia de tile que normaliza cache ou leitura de borda inválida.

O layout não vem do builder procedural/asset terrestre; não há planeta real
completo. BlendNoise/BlockNoise estão ausentes nos cenários: Perlin não foi
exercitado. O cache validado é a LRU de WorldTemplate, **não CacheSelector**
nem caches mutáveis de Island/Worm/Karst. Banco mínimo só tem dois materiais;
não representa catálogo/mods completos. Oceano aqui é metadata e LiquidStore,
não simulação de líquidos, clima ou solução do backlog de planetas tóxicos.

## Auditoria para a próxima etapa

Achados de inspeção, sem atribuição aos crashes/coletas anteriores:

1. TerrainSelectorParameters default não inicializa baseHeight; WorldRegion
   default não inicializa regionLiquids. Os corpos herdados foram preservados.
   Testes usam parâmetros Json completos e atribuem todos os campos de líquidos
   antes de ler/copiar. Não generalizar isso para inicialização segura do motor.
2. Em StarCacheSelector.cpp, o construtor lê sourceType, mas passa o membro type
   ("cache") a createSelectorType. **Suspeita ainda não reproduzida nesse alvo**:
   executar factory real com source mínimo, confirmar seleção/falha e corrigir
   com controle negativo antes de usar o wrapper no contexto privado. Não
   corrigido silenciosamente nesta relocação.
3. Permanecem abertas a metadata de Perlin, biomeTransition default/cópia e
   SkyParameters::horizonClouds. O teste não copia/serializa WorldTemplate/Sky
   nem lê esse flag. ASan/UBSan não substituem MSan para inicialização.

## Build e validações executadas

Skill cloud usada para conferir ambiente/rede e reutilizar cache/dependências,
sem instalar/baixar ferramentas. GCC 14.2.0/C++17, CMake 3.29.2/Ninja
1.13.0.git.kitware.jobserver-pipe-1, Debug -Og; GUI/Tracy/allocators alternativos
OFF, BUILD_TESTING=ON, VCPKG_MANIFEST_INSTALL=OFF. Mesmo sysroot/vcpkg/hook SDL3
real do [setup anterior](sector-runtime-h0-block-info-kernel-2026-11-10.md).
Cache build/block-info-h0, identifier `55b2d369-real-sampling-local-dirty`.
C++: -fsanitize=address,undefined -fno-omit-frame-pointer e XXH_NO_INLINE_HINTS=1;
C/dependências pré-compiladas não têm a mesma cobertura.

Com UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 e
ASAN_OPTIONS=halt_on_error=1, sem diagnósticos dos sanitizers nas aprovações:

1. **752/752 casos padrão**, 38 suites; sem adicionar casos ao alvo padrão.
2. **32/32 casos opt-in**, duas suites: 13 anteriores de Root e 19 novos.
   **784 únicos com opção ON**; opção continua padrão OFF, não no Actions padrão.
3. **268×20 =5.360** execuções adicionais padrão, 13 suites; mais **32×5 =160**
   opcionais, shuffle/seed inicial 1236: **5.520 adicionais**, não casos únicos.
   Três microexperimentos DISABLED não executados.
4. **38+2 =40 grupos CTest**, -j2, NoAssets; 100% aprovados. Repetem os mesmos
   casos, não toda a bateria core_tests/game_tests.
5. Build/link dos dois executáveis e nove objetos normais: Template/Layout/
   TerrainDatabase originais, três unidades novas, WorldGeneration e seletores
   Constant/FlatSurface. Não link/execução do jogo completo. Root OFF: 38 grupos,
   zero de Root/amostragem, alvo padrão sem trabalho pendente. Restaurado ON;
   dois executáveis/nove objetos sem trabalho pendente.
6. Corpos relocados, kernel/referência, manifesto, links, IDs, percentuais e
   git diff --check conferidos. Sem novo marco completo por contagem de testes.

A primeira ligação revelou dependência ausente de MemoryEnvelope no alvo
opt-in, incluída com a implementação real. Um warning novo de indentação no
teste foi corrigido; avisos herdados de fmt permanecem. As primeiras execuções
aprovadas precederam expectativas explícitas de líquido; o alvo final foi
recompilado e toda a bateria, Root repetido e CTest foram reexecutados.
As repetições padrão usam fontes inalteradas. Configs inválidas das fixtures
são erros intencionais, não novos crashes de jogo.

Conferência documental: 712 destinos locais de arquivo em 53 documentos,
14 módulos alcançáveis, 49 contratos únicos/cobertos no roadmap, ordem 1–49,
oito totais de fase e 7/196 marcos =3,6%. Fragmentos/âncoras não auditados nessa
contagem. Vinte entradas do manifesto verificadas por bytes/hash;
git diff --check aprovado. Nenhum marco adicional concedido por estes testes.

Reprodução após o setup indicado:

```bash
cmake -S source -B build/block-info-h0 -DSTAR_SOURCE_IDENTIFIER=55b2d369-real-sampling-local-dirty -DSTAR_BUILD_ROOT_LIFECYCLE_TESTS=ON
cmake --build build/block-info-h0 --target world_generation_contract_tests world_generation_root_lifecycle_tests -j3
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ASAN_OPTIONS=halt_on_error=1 \
  ./dist/world_generation_contract_tests --gtest_brief=1
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ASAN_OPTIONS=halt_on_error=1 \
  ./dist/world_generation_root_lifecycle_tests --gtest_brief=1
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ASAN_OPTIONS=halt_on_error=1 \
  ./dist/world_generation_root_lifecycle_tests --gtest_repeat=5 --gtest_shuffle --gtest_random_seed=1236 --gtest_brief=1
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ASAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir build/block-info-h0 -R '^world_generation_(contract_|root_lifecycle$|real_block_info$)' --output-on-failure -j2
```

Filtro/reprodução das 268×20 execuções padrão:
[comando da etapa anterior](sector-runtime-h0-block-info-static-inputs-2026-11-10.md#build-e-validações-executadas).
O código do filtro e a seed não mudaram.

## Continuidade e progresso

**CPU-01/H0/RAM-01 continuam em 25%; H1 em 2,5%; programa 7/196 =3,6%.**
A amostragem real avançou, mas não fecha requisitos completos de CPU-01:
contexto proprietário profundo, inicialização, caches/reutilização, identidade
de assets/algoritmo e admissão antes de cópias ainda faltam. O adapter capturado
está fora dos jobs; não há publicação/PreparedSector completo, nova WorldThread,
GPU, orçamento/default alterado ou ganho de setores/s medido.

Próxima entrega executável, sem exigir nova coleta do jogador:

1. Reproduzir a factory de CacheSelector e a inicialização/cópia de Perlin com
   fontes mínimas reais; corrigir defeitos confirmados sem mudar RNG/conteúdo.
2. Estender este ensaio com noises/árvore de seletores e caches privados reais,
   primeiro com seeds fixas e controles independentes, depois reconstrução/
   troca de worker. Não anunciar a LRU de WorldTemplate como cache da árvore.
3. Compor ownership/identidade/admissão e materiais congelados, comparar
   BaseTiles privados e revalidar no dono antes de sombra por setor. Só então
   benchmark integrado com rota/save/config/assets/mods fixos, setores inéditos/s,
   interesse→entrega e update/frame. Não otimizar consultas isoladas como fim.

Mods/save/multiplayer e fallback CPU continuam essenciais. NPC externo
suspenso, Linux Clang adiado. Não houve nova captura/log/config do usuário,
reanálise de Tracy, teste de save/reload/multiplayer, mods completos, MSan/TSan,
OOM/pressão, GUI/Tracy/GPU/outras plataformas, revisão independente, Actions,
release ou merge em main. Artefatos brutos permanecem fora do Git.

## Manifesto das fontes

Vinte fontes locais de 10/11/2026 UTC: onze alteradas/adicionadas e nove
fronteiras de auditoria. Caminhos relativos, bytes e SHA-256, sem dados do usuário.

| Arquivo | Bytes | SHA-256 |
| --- | ---: | --- |
| `source/game/CMakeLists.txt` | 17474 | `aebe52ea2cae778be20cd39140381159ff83a509d28c48bfaa78c653ba4553d5` |
| `source/game/StarWorldTemplate.cpp` | 25879 | `6effe3141a1d879dff77d879246292b39488c7b6094045e22cc1ace522ae93ff` |
| `source/game/StarWorldTemplateSampling.cpp` | 3161 | `886cd4312f13f6e8f715262101bf7719dc9bffb4227a2ada3a7b674ccc8ed41e` |
| `source/game/StarWorldTemplateTypes.hpp` | 9441 | `c2751ba1179fd399561cb025142be7cef3fb990c9abdfc1f38fdfb20a957eb8c` |
| `source/game/StarWorldLayout.cpp` | 34093 | `9abab77423bbb157a4fa3d50a4563287d2903dc092d2555dcea2bd8428e1890e` |
| `source/game/StarWorldLayoutSampling.cpp` | 5397 | `cf29b6e1b1c7eec8eae51199d452ff601467fa04bf013bd868b4e65d61d94c1c` |
| `source/game/StarWorldLayout.hpp` | 5896 | `75c22a4ff47c857a0bbc1bd6e148d4f2d47307af09dc6e7950ef1f094d1cc41f` |
| `source/game/StarTerrainDatabase.cpp` | 4603 | `bb7f1087599534561eb6276244685b04d051dc6e7f5f5810526acc61f944f4d7` |
| `source/game/StarTerrainSelector.cpp` | 1359 | `02752295c5e4b481d28d4df1fef80c88ad0228ffbf5f4c395c5d1f86ee053807` |
| `source/test/CMakeLists.txt` | 10699 | `3a0391f348322910801c4da9fb2e3fbdaa8a6eac10d85099917ae244a187282d` |
| `source/test/world_generation_real_block_info_test.cpp` | 18171 | `7a9f3bb07d2aad3e010860937731e33dd3221334b0c2c76ec1e026363a762946` |
| `source/game/StarWorldGenerationBlockInfo.hpp` | 7292 | `37d6f0af1a9c49e7e987ce5654c2ea1bfc763de6fdb13508472bff0235e4649c` |
| `source/game/StarWorldGenerationBlockInfoStaticInputs.hpp` | 2267 | `6f3fdfa7ebff5fe3ef0e89b0e87630a98a13bb33ab2ddc1fcd48b7962d28c604` |
| `source/game/StarWorldGenerationBaseTileInput.hpp` | 2522 | `632312f5530b2ba8d2897e88ed60241e34c78572331b23a078db92239b39970a` |
| `source/game/StarWorldGenerationBaseTiles.cpp` | 6876 | `f2c8ea7e61a402798a74a0d5cb86928b6eb0e2d183bed5306eb25ac11db76c32` |
| `source/game/StarWorldGenerationLookupResolver.hpp` | 714 | `f7adc16443e078efbfddb106c40317f02c0d8ae9b4c6f4b6a15fe4cce76477a0` |
| `source/game/terrain/StarConstantSelector.cpp` | 369 | `185e7e40fde517f38d4516a95f50a05290b5a77fdf1f8a6a96d40860e5e5cf3a` |
| `source/game/terrain/StarFlatSurfaceSelector.cpp` | 522 | `ddc780b2c25ac841b952da4fd6e1badf26fd72390db570513d99fe53fefcdcaa` |
| `source/game/terrain/StarCacheSelector.cpp` | 846 | `22361b3f1858745ace3ad273312761f9dc373fc37b6d0832d3f75a056a45e3ba` |
| `source/game/StarTerrainDatabase.hpp` | 1622 | `5ee2a587b8364aeaf9d57a1d0838aab0a4df21c9bc57c0b8979b28bf377fece5` |
