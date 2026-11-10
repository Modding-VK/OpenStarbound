# H0: leitores reais, equivalência e custo das consultas BaseTiles

Data de implementação/coleta: 10/11/2026 UTC. Entrega: `Experimental`;
revisão local `codex/experimental-regional-runtime`.
Base: `ee94c3c46a552236863277a539a56ccee41f5993`.
Implementação: `061ce68c6db2dfe0353bb295385ff00b4ec77e50`.

[Programa](sector-runtime/README.md) · [Roadmap](sector-runtime/roadmap.md)
· [Progressão](sector-runtime/progress.md)
· [Contrato BaseTiles](sector-runtime-base-tiles-contract.md)
· [Envelope](sector-runtime-memory-envelope.md)
· [Etapa anterior](sector-runtime-h0-lookup-source-generations-2026-11-09.md)

## Objetivo, resultado e limite

Comparar as três consultas do resolver **real** de BaseTiles com o corpo
anterior independente, captura/fita e catálogo compacto. Antes, esse contrato
usava somente resolvers controlados. Agora o alvo NoAssets liga os mesmos
métodos de WorldTemplate/MaterialDatabase usados pelo jogo, sobre objetos reais
com metadata explicitamente preparada. Não há implementação alternativa desses
leitores no teste nem simulação de funções de entidades para satisfazer o link.

Isso verifica a fronteira de consultas; **não gera terreno ou setores completos,
não constrói Root/Assets nem carrega os assets do jogo**. O lifecycle parent/Assets
continua representado por clocks controlados. MaterialDatabase tem definições
reais de fixture e sua versão real; não passou pelo construtor de parsing. A
troca de layout usa o método real, incluindo revisão e limpeza do block cache.

Catálogo/fita continuam fora do produtor normal. Nenhum preview, fila, budget,
worker, TTL, nível/pré-requisito, RNG, formato persistido ou commit serial mudou.
Sem ganho de setores/s demonstrado. A finalidade é viabilizar um contexto seguro
para a arquitetura de maior entrega, não promover consultas baratas a gargalo.

## Extração e compatibilidade

- WorldGenerationLookupResolver é o mesmo pequeno resolver emprestado que
  antes era local ao loop BaseTiles: duas referências const e três métodos
  inline. Sem callback virtual, alocação, lookup antecipado ou cache novo.
- StarWorldGenerationLookupReaders.cpp recebe cinco definições existentes:
  revision/setWorldLayout, os dois leitores de hue e supportsMod. getMaterialInfo
  passa ao header de MaterialDatabase com `inline`: seu corpo continua visível
  aos acessores de materiais, evitando retirar oportunidades de incorporação
  pelo compilador só para isolar o alvo de testes. Não é `always_inline` nem
  demonstra ganho em Release; não trocar isso por getter out-of-line inadvertidamente.
  StarWorldGenerationLookupDefaults.cpp recebe seis construtores de valores
  existentes: WorldLayout, BiomePlaceables, Biome, SpawnProfile, SkyParameters e
  MaterialInfo. Os construtores normais com Root/assets ficam nas unidades antigas.
- A tentativa de link isolado revelou que os headers de Biome e WorldTemplate
  puxavam Plant/Spawner e vtables de Entity. A solução foi separar Types e manter
  os headers antigos como fachadas com os includes transitivos históricos.
  Não foram criados stubs de Entity, removidas checagens de sanitizer ou alteradas
  as interfaces de gameplay para contornar o problema.
- Duas tags/construtores privados e um friend de WorldLayout fornecem metadata
  para o teste. **Não são mundos/bancos completos, APIs públicas ou produtores
  de blockInfo/store/terreno**. Os construtores de produção não delegam a eles.
  Tag de WorldTemplate fixa geometria/seed sem Root ou RNG automático; a de
  MaterialDatabase não carrega materiais/mods. Fixture não deve vazar ao runtime.

Conferência contra a base via extração de corpos, respeitando strings/comentários:
**17 trechos assinatura/corpo byte a byte iguais** — 12 relocados, dois construtores
normais inalterados e três métodos do resolver extraído. A extração começa no
tipo de retorno/assinatura; o prefixo `inline` acrescentado ao getter não integra
o hash, e é a única mudança declarativa desse corpo. SHA-256 agregado da sequência
dos hashes individuais separados por newline:
`e1a88968f0ed022bf8f4134041c7d1572475dee0a10ee951b97c58dd0aa890f0`.
Declarações de Biome/BiomePlaceables idênticas; a classe WorldTemplate preserva
campos/ordem/tipos, salvo as três declarações privadas da seam. MaterialDatabase
e WorldLayout também não ganham campos. Não é prova de ABI para outras plataformas.

## Comportamentos legados confirmados

| Caso | Comportamento que a implementação futura precisa preservar |
| --- | --- |
| Sem layout ou NullBiomeIndex | Hue zero, sem consultar o bioma inválido |
| Material diferente de mainBlock | Hue zero, usando blockBiome, não environmentBiome |
| Qualquer dos quatro slots de grass | Hue do bioma; slots default NoMod podem dar hue **não zero** para NoMod |
| supportsMod(material, NoMod) | Retorna true antes de procurar material, inclusive material real ausente/meta |
| Meta material/mod com mod não NoMod | Retorna false antes de lookup inválido |
| Mod real fora do banco de mods | supportsMod considera o material; não verifica membership do mod |
| Material real ausente/sparse com mod real | MaterialException real, conservando prefixo de escrita da aplicação |
| Bioma não nulo fora do layout | Exceção herdada de WorldLayout; não normalizar para hue zero |
| Catálogo sem cobertura/scope atual | Refused antes de escrever; fallback explícito conserva a falha legada |

O trace observa argumentos/ordem e estado anterior a cada chamada (14 campos
fonte mais os três campos de bioma). A referência independente contém a lógica
legada de líquido literalmente, sem chamar o helper novo de líquido/aplicação.
Falhas em foreground/background comparam os mesmos writes anteriores à exceção,
sem transformar tile parcial em origem segura de retry.

4.096 combinações de branches comparam corpo anterior, observação, replay e
catálogo por campos; a captura também compara trace ao corpo anterior. Um teste
com 1.024 tiles e quatro workers usa simultaneamente o resolver real e o catálogo
compartilhado em arrays privados distintos, comparados à referência serial.
As fontes/layout ficam estáveis nesse teste; não é teste de races de hotReload.

Substituir o layout real avança a revisão, invalida o catálogo antigo e conserva
o layout antigo pelo handle. Trocar o banco de definições com clock parent
controlado conserva o banco antigo até soltar o binding e não reconstrói o
template. Isso **não equivale a executar Root::reload/hotReload ou listeners**.

## Validação executada

Skill de ambiente cloud usada para conferir executor/política e reutilizar os
tools/caches existentes, sem instalação/download novo. GCC 14.2.0/C++17,
CMake 3.29.2/Ninja 1.13, Debug -Og; GUI/Tracy/allocators alternativos OFF,
BUILD_TESTING=ON, VCPKG_MANIFEST_INSTALL=OFF. Cache
`/tmp/sector-input-h0.RX3XLi`, identifier
`ee94c3c4-real-lookup-readers-local-dirty`. Mesmo sysroot/hook SDL3 externo ao Git
da etapa anterior; não é instalação limpa das dependências.

C++ com `-fsanitize=address,undefined -fno-omit-frame-pointer` e
`XXH_NO_INLINE_HINTS=1`; C/dependências pré-compiladas não têm a mesma cobertura.
Runtime com `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1` e
`ASAN_OPTIONS=halt_on_error=1`, sem diagnóstico de sanitizer nas execuções:

1. **649/649 testes**, 32 suites: **20 normativos novos**. Três microexperimentos
   DISABLED excluídos dessa contagem. A suite nova também passou isolada (20).
2. **165 casos × 20**, shuffle GTest seed inicial **1231**: **3.300 execuções
   adicionais**, não casos únicos. São as sete suites de leitores/fontes/clock/
   BaseTiles/inputs/observações/catálogo.
3. **32/32 grupos CTest** `^world_generation_contract_`, label NoAssets, repetindo
   os mesmos casos normativos; não benchmarks ou streaming.
4. Build/link NoAssets; quinze targets de objetos reais do jogo conferidos:
   Root, MaterialDatabase, WorldGeneration, BaseTiles, TileDamage, TileDamageState,
   WorldTiles, WorldTemplate, WorldStorage, Biome, WorldLayout, SkyParameters,
   SpawnTypeDatabase, novos Readers e Defaults. Assets é ligado pelo alvo base.
   Objetos inalterados reaproveitados, não todos forçados a recompilar. Não é
   link do jogo completo ou build Windows/macOS.

5. Sintaxe/tipos com STAR_ENABLE_TRACY/TRACY_ENABLE de **sete fontes**: Root,
   WorldGeneration, WorldTemplate, MaterialDatabase, Readers, Defaults e o teste
   novo. Headers oficiais locais de Tracy 0.11.2 no commit verificado
   `b8c2e25c3d02009059e09f623072921677dd6bb4`; não link do Tracy.

6. Conferência documental: **634 links locais em 47 documentos**, 14 módulos
   alcançáveis, 49 contratos únicos/cobertos no roadmap e ordem 1–49. Oito totais
   de fase e 6/196 marcos = 3,1% recalculados; manifesto de 19 fontes conferido
   por bytes/SHA-256. `git diff --check` aprovado; build final sem trabalho pendente.

Reprodução no build equivalente dos relatórios anteriores:

```bash
cmake --build <build-dir> --target world_generation_contract_tests -j3
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ASAN_OPTIONS=halt_on_error=1 \
  ./dist/world_generation_contract_tests --gtest_brief=1
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ASAN_OPTIONS=halt_on_error=1 \
  ./dist/world_generation_contract_tests \
  --gtest_filter='WorldGenerationLookupReadersTest.*:WorldGenerationSourceVersionTest.*:WorldGenerationLookupSourcesTest.*:WorldGenerationBaseTilesTest.*:WorldGenerationBaseTileInputsTest.*:WorldGenerationBaseTileObservationTest.*:WorldGenerationBaseTileLookupContextTest.*' \
  --gtest_repeat=20 --gtest_shuffle --gtest_random_seed=1231 --gtest_brief=1
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ASAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir <build-dir> -R '^world_generation_contract_' --output-on-failure
```

## Microexperimento de custo, separado dos testes normativos

Teste opt-in DISABLED_SyntheticRealReaderCosts: **8.192 tiles × nove rodadas**
por modo, ordem rotativa. Mede o loop de aplicação, não só uma chamada de lookup.
Preparação das fixtures/catálogo e reset dos tiles/cache ficam fora do intervalo;
os modos de catálogo incluem o retorno boolean/status no checksum. Leitura do
checksum de campos fica depois do intervalo em todos. A tabela é da execução
final após manter getMaterialInfo inline; uma execução exploratória anterior
não foi combinada com essa amostra. Os outros dois experimentos não foram
reexecutados. Uma invocação, Debug/sanitizers no executor cloud, não Release
ou hardware do usuário, e sem compilação ativa durante essa execução final.

| Modo | Mediana ns/tile | Mínimo | Máximo |
| --- | ---: | ---: | ---: |
| Aplicação com leitores reais emprestados | 368,79 | 315,25 | 694,92 |
| Catálogo com preflight/aplicação | 397,61 | 380,81 | 431,08 |
| Catálogo com binding e checagens pré/pós | 607,23 | 506,17 | 850,29 |

O checksum passou. Nesta ABI, Context<32> mantém **328 bytes** totais, **256**
reservados de entries; cinco chaves distintas usam **40 bytes ativos**. Não há
heap interno, mas fontes/objetos retidos e construção não entram nessa medida.
Esses cinco valores são desta fixture, não cobertura universal de um planeta.

**Não há evidência de ganho de velocidade aqui.** O catálogo é candidato a fechar
dependências/versionamento, não uma otimização necessária para consultas tão
pequenas. O ruído/escopo não sustentam estimativa de regressão ou ganho no jogo.
Não ativar catálogo/fita por causa de storage menor, nem gastar mais ciclos de
otimização nesse microcusto sem hipótese ligada à entrega de setores comuns.

```bash
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ASAN_OPTIONS=halt_on_error=1 \
  ./dist/world_generation_contract_tests --gtest_also_run_disabled_tests \
  --gtest_filter='WorldGenerationLookupReadersTest.DISABLED_SyntheticRealReaderCosts'
```

## Inicialização: achado aberto, não correção de produção

O default herdado de SkyParameters **não inicializa horizonClouds**. Seu corpo
foi relocado sem mudar esse comportamento. A fixture define false explicitamente
antes de ler/copiar o objeto; não altera o construtor normal ou prova segurança
do grafo completo. Registrar em CPU-01 junto de biomeTransition default/cópia,
seed automática e Perlin. Sem atribuir causa aos crashes anteriores ou considerar
esses itens corrigidos. ASan/UBSan não substituem MSan para inicialização.
MaterialInfo de fixture só lê os campos preenchidos; não copiar/usar os demais
campos escalares como se o parsing de definições tivesse sido executado.

## Continuidade, decisão e progresso

1. Completar a validação integrada de lifecycle: Root/Assets reais, reload/
   hotReload, listener com falha e fontes antigas retidas. Preservar o fato de
   que hotReload legado não reconstrói MaterialDatabase/layouts existentes.
2. Fechar o contexto do próximo kernel de setores comuns: blockInfo, inputs
   inicializados, read set/assets/identidade e fontes esperadas. Não declarar
   CPU-01 completo só porque os três leitores const são determinísticos.
3. Revalidar após preparar e pelo dono no commit; então sombra por setor com
   admissão/bytes, comparador e descarte stale. Não aplicar catálogo em tiles
   vivos nem fallback/retry em output já modificado. Metadata não é read lease.
4. Integrar a validação de streaming com save/config/assets/mods/rota fixos,
   setores inéditos entregues/s, interesse→entrega e resposta de update/frame.
   Depois avançar mailbox/DAG, executor/produtos, demanda/provider/fairness e
   escala de hardware conforme o roadmap, sem mais rodadas sintéticas sem hipótese.

**RAM-01/H0 permanecem 25%; programa 3,1%, 6/196 marcos dos 49 contratos.**
Nenhum contrato novo ou marco completo foi concedido por quantidade de testes.
Scope 2, clock/binding 1, input/líquido/observação/catálogo/ticket/comparador 1,
retry 2, estimativa 9 e envelope/admissão 1 seguem; 14 classes, pressão padrão
desligada, sem campos/config/defaults de geração novos.

Sem novos logs/config/Tracy do usuário, reanálise das runs antigas ou jogo nesta
entrega. Sem mods/save/reload/multiplayer reais, TSan/MSan, OOM/pressão externa,
outros allocators/arquiteturas, GPU, revisão independente, Actions, release ou
merge em main. Compatibilidade mods/saves/multiplayer e fallback CPU são gates
essenciais. NPC externo suspenso e Linux Clang adiado. Dados brutos fora do Git.

## Manifesto das fontes

Fontes locais de 10/11/2026, nomes relativos ao checkout, bytes/SHA-256 de
**19 fontes** alteradas. Nenhum artefato novo do usuário nesta entrega.

| Arquivo | Bytes | SHA-256 |
| --- | ---: | --- |
| `source/game/CMakeLists.txt` | 17131 | `e298e73d0501e5cbcd1df34ab47bd7ed5fcd9dec43e4708db54cce4dd5a69e5d` |
| `source/game/StarBiome.cpp` | 3026 | `dd779b122eeb6059015bc254f897cac30cd4e442eb15feed7389cf72499cdb59` |
| `source/game/StarBiome.hpp` | 176 | `149d14e7106bff5282fda02c405762eb3bf7a81e1c208ee4d269fc7dc6557d17` |
| `source/game/StarBiomeTypes.hpp` | 1335 | `3743995d08ed97fbf9cbe3878541dd13c195555c4fe82dfa0ef8b2303a1b9993` |
| `source/game/StarMaterialDatabase.cpp` | 21594 | `56fc8c501ef7c182aebaed9871bd9b3e4d0125baeda0d35c7261038f49408a2a` |
| `source/game/StarMaterialDatabase.hpp` | 9983 | `6c3f2f88e574e7eed1591b85208a6da67f884eaf25bc5f6767446dc01b746ccb` |
| `source/game/StarSkyParameters.cpp` | 8155 | `518416cd636c54ae885f97f3e0298aabfd845ec910cc63dfd85bf78e0aa9202f` |
| `source/game/StarSpawnTypeDatabase.cpp` | 5369 | `17ac4a875b53bd6fa92a6e66ec1f592689977351dade0fb7fb9fc172955552f9` |
| `source/game/StarWorldGeneration.cpp` | 128743 | `516f7fc4a80780513080502904de26f3d33d292f1db01829f3dd5486d077925e` |
| `source/game/StarWorldGenerationLookupDefaults.cpp` | 1528 | `a3a13d4c135ac965e5816f787efea9475fa6c623bd12523f0afdce3dd383abac` |
| `source/game/StarWorldGenerationLookupReaders.cpp` | 1561 | `c12f385a7e9c202cfd6c5d29744d4173075c61960b7c405e91de340910084d65` |
| `source/game/StarWorldGenerationLookupResolver.hpp` | 714 | `f7adc16443e078efbfddb106c40317f02c0d8ae9b4c6f4b6a15fe4cce76477a0` |
| `source/game/StarWorldLayout.cpp` | 39247 | `47df111a15c1208ef26f2c8c25c2182870c62ee4e92d54b3ffbd1cb0f1cc7ded` |
| `source/game/StarWorldLayout.hpp` | 5840 | `400827cb19bbaca99a1d798a6f508b02b879b1ae4132ab9c82343bb5355e2448` |
| `source/game/StarWorldTemplate.cpp` | 35684 | `c29b96df78304d61f80d369ba6334a4968677c6ba8a6fc3a2065868cbe69e266` |
| `source/game/StarWorldTemplate.hpp` | 231 | `fa79c9ecfdf248cdd458dca71f63a75becdb7594ac7845918efd5651713c4a3e` |
| `source/game/StarWorldTemplateTypes.hpp` | 8527 | `47d5bd8945578765cc402dab6078c72eedd59cfb7cd4af17d4a606e1f78c789d` |
| `source/test/CMakeLists.txt` | 6723 | `078de624462434325dbd22b6978819977fa9c54acbabe8dee06c3c2e72d91e86` |
| `source/test/world_generation_lookup_readers_test.cpp` | 23805 | `903e9bc7c4e71f2800026b6158afec217748c9f66c945062b6a4dd8e755b8967` |
