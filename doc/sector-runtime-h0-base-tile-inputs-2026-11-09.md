# H0: inputs escalares e consultas de BaseTiles

Data: 09/11/2026. Entrega: `Experimental`; revisão local:
`codex/experimental-regional-runtime`. Base: `a43f2131ccda8f41bbdebec5c14cb5d0b7451106`.
Implementação: `2ea41cf8b1c75a064b76e3dd65d70a53f1d1b0bf`.

[Programa](sector-runtime/README.md) · [Roadmap](sector-runtime/roadmap.md)
· [Progressão](sector-runtime/progress.md)
· [Contrato BaseTiles](sector-runtime-base-tiles-contract.md)
· [Memória](sector-runtime-memory-envelope.md)
· [Etapa anterior](sector-runtime-h0-base-tiles-identity-2026-11-09.md)

Continuação vigente: [observações versionadas e replay diagnóstico](sector-runtime-h0-base-tile-observations-2026-11-09.md).
Este relatório preserva evidências/manifesto de `2ea41cf8`; estado e próximo
trabalho atuais estão na continuação e no roadmap.

## Objetivo e entrega

Explicitar o input da aplicação de um tile comum e isolar a sequência de
consultas de biomas/materiais/mods, permitindo comparar o adaptador real com o
corpo anterior sem ligar Root/assets nos testes. É preparação para fechar
contexto e reexecutar cálculo privado, não entrega de um produtor puro completo.

WorldGenerationBaseTileInput, modelo **1**, contém block/environment biome,
transition, material/mod por camada, oceano/nível/enclosure e y absoluto.
Não possui Json, template, cache, seed, Root, ponteiros ou lifetime dinâmico.
O loop x→y obtém blockInfo uma vez por tile como antes e passa os escalares a
applyWorldGenerationBaseTile. Um resolver local emprestado vincula o template
existente no caminho (privado no worker, autoritativo no síncrono) e o banco de
materiais. Não há std::function/vtable, cópia de ServerTile, buffer/scan novo,
consulta antecipada ou heap próprio desse adaptador. Custo de overhead não medido.

O fluxo real síncrono/assíncrono usa o mesmo adaptador. A implementação preserva:

1. Escrita dos biomas antes de consultas; hues usam o novo **block biome**.
2. Material/cor default, hue, suporte do mod, mod e mod hue, somente nas camadas
   NullMaterialId. Foreground completo precede background; ar existente não
   significa camada ausente.
3. NoModId ainda consulta suporte e mod hue, inclusive em ar/meta; o banco real
   aceita NoMod antes de testar material. Mod não suportado conserva mod/hue
   anteriores. Não substituir consultas por atalhos aparentemente equivalentes.
4. Líquido somente depois das camadas e usando seus materiais finais; Keep
   conserva o estado anterior. Colisão/cache/dano/dungeon/root não são alterados.
5. Falhas propagadas na mesma posição, com as mesmas escritas parciais anteriores
   a cada consulta. O síncrono escreve no array vivo: congelar todas as consultas
   antes de escrever alteraria o estado após exceção. Não há retry interno.

**Resolver const/injetado não implica pureza.** O input escalar não inclui os
valores retornados ou a versão completa dos assets/contexto; duas implementações
do resolver podem produzir tiles diferentes com o mesmo input. O ticket existente
também não é chave de conteúdo. Repetir após falha sobre o tile parcial pode pular
uma camada já escrita e divergir do resultado obtido a partir do snapshot original.

Produto/ticket, comparador e kernel líquido permanecem **1**, pois esta extração
é comportamentalmente equivalente. Envelope/admissão continuam 1, retry 2,
sinal de capacidade 1 e estimativa geral 9. Sem alteração de classes/fórmulas
de memória, RNG/cópias, orçamento, workers, filas, TTL, níveis/pré-requisitos,
guards de commit, publicação serial, API Lua, codec ou config/default.

## Evidência e limites dos testes

24 testes novos em WorldGenerationBaseTileInputsTest usam ServerTile reais e
o comparador de campos existente. A referência mantém o corpo anterior do
próprio projeto independente dos dois helpers de aplicação/líquido novos.
RecordingResolver registra até seis consultas com tipo, argumentos e 17 campos
observados antes de cada chamada. Há controles separados para campos não tocados,
camadas existentes/ausentes, material Null, NoMod, mod rejeitado, block vs
environment biome, transition e líquido usando os materiais finais.

Uma matriz de **4.096 combinações de branches** compara campos completos e traces;
não são 4.096 mundos/assets diferentes. Falhas injetadas em cada uma das seis
consultas comparam estado parcial e interrupção antes do líquido. Resolver não
copiável comprova empréstimo; input permanece intacto. **1.024 tiles** particionados
em quatro workers, com resolver escalar imutável, coincidem com o corpo serial
anterior. Não são quatro WorldTemplates reais executados concorrentemente.

Controles negativos: omitir hue de NoMod é observável; reaplicar a tile parcial
não é reiniciar; input escalar igual não fecha o resolver. Nenhum desses testes
prova reexecução do blockInfo/WorldTemplate completo ou efeitos de mods instalados.

O achado herdado de WorldTile default/cópia não inicializar/copiar biomeTransition
continua **não corrigido**. Fixtures inicializam os campos e só comparam células
válidas; não há leitura de bool indefinido para fabricar equivalência. ASan/UBSan
não substituem MSan nem atribuem a esse achado a causa de crashes anteriores.

## Ambiente e validação executada

A skill cloud foi usada para conferir executor/rede e recuperar ferramentas de
build. O worktree/cache anterior deixou de existir durante a atividade; a retomada
recuperou a branch remota no SHA base acima em worktree novo, preservando o checkout
base `work`. Não houve reset destrutivo, instalação de pacotes ou edição do sistema.

GCC 14.2.0/C++17, CMake 3.29.2 e Ninja 1.13; Debug -Og, GUI/Tracy/allocators
alternativos OFF, BUILD_TESTING=ON, VCPKG_MANIFEST_INSTALL=OFF. Cache novo:
`/tmp/sector-input-h0.RX3XLi`; identifier `a43f2131-base-tile-inputs-local-dirty`.
C++: `-fsanitize=address,undefined -fno-omit-frame-pointer -DXXH_NO_INLINE_HINTS=1`.
C: `-DXXH_NO_INLINE_HINTS=1`; C/dependências pré-compiladas não têm a mesma
instrumentação. Core/base e objetos usados foram reconstruídos neste cache.

Foi reutilizado o vcpkg/sysroot já existente em `build/` do checkout base. O imgui
pré-compilado exige SDL mesmo no alvo NoAssets/GUI OFF; um hook CMake externo
ao Git importa SDL3 real antes dos targets, com PKG_CONFIG_PATH e
PKG_CONFIG_SYSROOT_DIR do sysroot existente. Não é target fictício, mudança de
dependência do projeto ou validação de GUI/CI. Reprodução desse hook local:

```cmake
# CMAKE_PROJECT_starbound_INCLUDE aponta para este arquivo fora do repositório.
# Depende do sysroot local; substituir pelos caminhos reais no novo executor.
set(ENV{PKG_CONFIG_PATH} "<sysroot>/usr/lib/x86_64-linux-gnu/pkgconfig:<sysroot>/usr/share/pkgconfig:<sysroot>/usr/lib/pkgconfig")
set(ENV{PKG_CONFIG_SYSROOT_DIR} "<sysroot>")
find_package(SDL3 CONFIG REQUIRED)
```

Headers oficiais Tracy 0.11.2 foram baixados para checagem de sintaxe em diretório
temporário, de `wolfpld/tracy`, commit exato
`b8c2e25c3d02009059e09f623072921677dd6bb4` (BSD-3-Clause), pin já adotado pelo
projeto. Origem/commit conferidos, sem copiar implementação externa para source,
trocar versão/pin ou ligar Tracy no build normal. Não houve nova análise de captura.

Com UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 e
ASAN_OPTIONS=halt_on_error=1, sem diagnóstico dos sanitizers:

1. **548/548 testes**, 27 suites; **24 novos** de inputs/consultas.
2. Os **64 casos afetados** (40 de ticket/comparador e 24 novos) repetidos vinte
   vezes com shuffle/seed GTest 1227: **1.280 execuções adicionais**, não únicos.
3. **27/27 grupos CTest** `^world_generation_contract_`, NoAssets; repetem os
   mesmos casos, não acrescentam outros 548 distintos.
4. Build dos contratos e **sete objetos reais** aprovado: geração, produto/
   comparador, TileDamage, TileDamageState, WorldTiles, WorldTemplate e WorldStorage.
   Só geração foi editada nesta etapa; os demais foram reconstruídos. Link do
   alvo NoAssets aprovado; não é linking completo do jogo.
5. Sintaxe/tipos de geração, produto/comparador e teste BaseTiles aprovados com
   STAR_ENABLE_TRACY/TRACY_ENABLE e headers oficiais acima; não é linking Tracy.
   Avisos herdados de fmt permanecem; a reconstrução fresca também encontrou
   avisos herdados em outras unidades, sem tratá-los como falhas corrigidas aqui.
6. Build final repetido: ninja sem trabalho pendente. `git diff --check` aprovado;
   **572 links locais em 43 documentos**, 14 módulos alcançáveis, 49 contratos
   únicos cobertos no roadmap e ordem 1–49 conferidos. Oito totais de fase e
   6/196 marcos = 3,1% recalculados, RAM-01/H0 mantidos em 25%. Cinco fontes do
   manifesto conferidas em bytes/SHA-256 antes do commit documental.

Reprodução após configurar um build equivalente na raiz, incluindo os sete
objetos acima (usar os binários CMake/CTest reais disponíveis no executor):

```bash
cmake --build <build-dir> --target world_generation_contract_tests -j3
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ASAN_OPTIONS=halt_on_error=1 \
  ./dist/world_generation_contract_tests --gtest_brief=1
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ASAN_OPTIONS=halt_on_error=1 \
  ./dist/world_generation_contract_tests \
  --gtest_filter='WorldGenerationBaseTilesTest.*:WorldGenerationBaseTileInputsTest.*' \
  --gtest_repeat=20 --gtest_shuffle --gtest_random_seed=1227 --gtest_brief=1
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ASAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir <build-dir> -R '^world_generation_contract_' --output-on-failure
```

## Continuação, compatibilidade e progresso

RAM-01/H0 permanecem **25%**, programa **3,1%**, 6/196 marcos dos 49 contratos.
O recorte apoia MOD-01/VAL-02 e o fechamento futuro de CPU-01; não conclui esses
contratos, o produtor completo ou PreparedSector, nem concede nota por testes.
Não há ganho de setores/s demonstrado ou alteração de configuração solicitada.

Próximo recorte executável: congelar/versionar observações do resolver para
cálculo independente, comparar replay a partir do input **original** e medir
custo de preparação antes de integrar cache/retry. Não antecipar consultas no
caminho síncrono nem reutilizar tile parcial. Auditar biomeTransition/default/
cópias antes de normalizar snapshots. Fechar assets/contexto profundo e demanda
real antes de ampliar pureza/consumidores; mailbox/DAG em sombra, provider único,
promoção/fairness e baseline integrado continuam gates. Não exigir contabilizar
cada heap do motor antes de H1 delimitado, nem ampliar filas com cobertura parcial.

Sem novo config/log/Tracy do usuário ou reanálise de captura antiga; dados brutos
fora do Git. Sem jogo/assets/mods, save/reload, multiplayer, TSan/MSan, OOM/pressão
externa, benchmark, Windows/macOS/ARM ou allocators alternativos executados.
Sem Actions, release, merge em main ou revisão independente. Fallback CPU completo
e compatibilidade com mods/saves/multiplayer permanecem gates essenciais; NPC
externo suspenso, Linux Clang adiado. Seed automática/Perlin continuam abertos.

## Manifesto das fontes

Fontes locais de 09/11/2026, nomes relativos ao checkout, bytes e SHA-256.
Cinco fontes alteradas; nenhum novo artefato de execução do usuário nesta etapa.

| Arquivo | Bytes | SHA-256 |
| --- | ---: | --- |
| `source/game/CMakeLists.txt` | 16815 | `683bf4513de9bcc516ce6374a609243fc90c332d83b5dd0186ff8a1a8fcca451` |
| `source/game/StarWorldGeneration.cpp` | 129103 | `89dad51b7080573f41e28606a7a49e1e4d01581e2a3a82f8ad9fa97964c2abe2` |
| `source/game/StarWorldGenerationBaseTileInput.hpp` | 2522 | `632312f5530b2ba8d2897e88ed60241e34c78572331b23a078db92239b39970a` |
| `source/test/CMakeLists.txt` | 6343 | `5d66ee7fab012db369a009cbb3f7692920538b37bdc10defc02fe9f64fbd833f` |
| `source/test/world_generation_base_tiles_test.cpp` | 38495 | `5c8b8182d72103a7f8691aae3415c43da3b8c870f2c9bdd7648be8a02036721a` |
