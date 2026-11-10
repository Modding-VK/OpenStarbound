# H0: gerações reais e lifetime das fontes de consultas

Continuidade em 10/11/2026: [leitores reais e equivalência](sector-runtime-h0-real-lookup-readers-2026-11-10.md).
Este relatório preserva os resultados históricos da etapa de clocks/binding.

Data: 09/11/2026. Entrega: `Experimental`; revisão local:
`codex/experimental-regional-runtime`. Base: `4eedc81f41701e9456bac7c0b7fe1c8f4e59da21`.
Implementação: `5413e13a57b2f89860c0228c5aa563360144dac3`.
Encerramento documental: 10/11/2026; evidências de implementação/testes coletadas
em 09/11/2026, sem nova execução de jogo nesta retomada.

[Programa](sector-runtime/README.md) · [Roadmap](sector-runtime/roadmap.md)
· [Progressão](sector-runtime/progress.md)
· [Contrato BaseTiles](sector-runtime-base-tiles-contract.md)
· [Memória](sector-runtime-memory-envelope.md)
· [Etapa anterior](sector-runtime-h0-base-tile-lookup-context-2026-11-09.md)

## Entrega e alcance

O catálogo pode agora receber versões geradas pelas fontes reais, não só IDs
declarados em fixtures. Root::worldGenerationLookupSources captura handles fortes
de Assets/MaterialDatabase e a versão do lifecycle Root. Clock e binding são
modelo **1**; LookupScope passa a **2**, exigindo sourceEpoch de Root além das
gerações de assets/material. Não é schema de config/save/rede; scopes protótipos
modelo 1 deixam de ser válidos na API interna. Catálogo/entries continuam modelo 1.

**Os hooks de metadata estão no runtime real; uso do catálogo na geração normal
continua desligado/não integrado.** Nenhum worker, orçamento, fila, geração de
tile, cópia/RNG, nível, TTL ou publicação foi alterado. Não foi criada flag para
ativar o catálogo. A factory nova pode carregar fontes preguiçosamente quando
chamada; os getters antigos não passaram a chamá-la.

## Relógio e fronteiras auditadas

GenerationVersionClock usa identidades processuais monotônicas de um alocador
atômico comum, sem RNG, endereços ou reutilização. Zero é indisponível. Saturação
não dá wrap: novos IDs ficam zero e os vínculos falham fechados. Não é hash de
conteúdo, relógio de parede ou identificador persistido entre processos.

Change invalida antes das alterações e só publica ID novo ao fim de todos os
escopos sobrepostos/aninhados completados explicitamente. Falha/retorno sem
complete deixa zero; uma transação nova bem-sucedida pode recuperar. Retire é
terminal, inclusive com Change pendente. O mutex protege **somente metadata**,
é liberado antes dos locks/getters das fontes e não serializa kernels por tile.
Não substitui sincronização de dados ou torna Root globalmente livre de races.

| Fronteira real | Efeito no vínculo |
| --- | --- |
| Root::reload | Indisponível antes dos resets; nova versão só depois dos listeners |
| Root::hotReload | Indisponível durante Assets::hotReload e listeners |
| Root::loadMods, inclusive _reload=false | Invalida durante mudança de diretórios; nested reload não publica cedo |
| Root::~Root | Retire antes do encerramento; tickets não conservam Root vivo |
| Assets::hotReload, inclusive chamado diretamente | Geração própria sob o mutex existente, durante limpeza de caches/fila/frames |
| MaterialDatabase recém-construído | Nova versão das definições; setters são privados e usados na construção |

hotReload não reconstrói MaterialDatabase no legado: sua versão não muda por
isso, mas Assets e lifecycle Root mudam. Não forçar reconstrução de bancos ou
layouts para simular refresh. Cópia C++ das definições conserva a versão e a
semântica de cópia anterior; o getter público é somente leitura.

Fronteira da auditoria: as três consultas atuais usam layout/biomas já construídos
de WorldTemplate e definições de MaterialDatabase. Clock de Assets rastreia
**reload explícito**, não digest de todo conteúdo. clearCache/cleanup, alterações
externas de arquivo e qualquer futura leitura bruta de assets não são certificados
por esse ID. Root::settings e concorrência herdada de settings/mods também não
foram reestruturados. Respeitar lifecycle/locks legados e ampliar a auditoria antes
de kernels que leiam fontes mutáveis adicionais.

## Binding, invalidação e resultado privado

WorldGenerationLookupSources retém Assets/MaterialDatabase e o relógio de Root,
não Root ou mundo. Capture recusa nulos, zero, origem em mudança ou versões que
mudam entre leituras. valid confere as versões novamente. scope recebe epoch do
mundo/revisão de template do chamador e preenche gerações/sourceEpoch reais.
Esses dois dados de mundo ainda exigem o dono autoritativo e guards existentes.

LookupScope modelo 2 tem **oito campos**: worldEpoch, templateRevision,
assetsGeneration, materialsGeneration, sourceEpoch e versões scope/input/líquido.
Assim um evento Root que mantém os mesmos handles também invalida o catálogo.
Strong handles evitam reuso físico de fontes durante sua retenção; ID processual
não substitui verificar qual Root/contexto é esperado.

O wrapper apply verifica binding e scope antes de usar o catálogo e volta a
verificar depois. Sua distinção de resultados é parte do contrato:

| Resultado | Efeito e regra do dono |
| --- | --- |
| Refused | Nenhum write: fonte/scope/cobertura inválidos; caminho legado pode partir do estado intacto |
| Applied | Aplicação privada e checagem local aprovadas; revalidar fontes/task/template/tiles antes de publicar |
| Stale | Fonte mudou após a aplicação; descartar output privado, nunca fazer fallback/retry nesse buffer |

**Não é read lease ou autorização de commit.** Uma invalidação pode ocorrer logo
após a última checagem. O dono continua responsável pelo ponto de commit, read set,
epoch/revisões, array físico e template. Não usar essa API para writes vivos.
Binding inválido ainda retém fisicamente as fontes até o último handle: invalidação
não cancela workers, drena jobs ou recupera RAM. Root ainda exige encerrar seu
trabalho dependente antes da destruição, como no contrato anterior.

## Validação executada e limites

Skill cloud usada para conferir executor/política e reutilizar o cache existente,
sem instalação/download novo. GCC 14.2.0/C++17, CMake 3.29.2/Ninja 1.13, Debug -Og;
GUI/Tracy/allocators alternativos OFF, BUILD_TESTING=ON, VCPKG_MANIFEST_INSTALL=OFF.
Cache `/tmp/sector-input-h0.RX3XLi`, identifier
`4eedc81f-lookup-source-generations-local-dirty`. Mesmo hook SDL3/sysroot externo
ao Git descrito no relatório de inputs; nenhuma dependência nova do projeto.

C++ com -fsanitize=address,undefined -fno-omit-frame-pointer e XXH_NO_INLINE_HINTS=1;
C/dependências pré-compiladas não têm a mesma instrumentação. Com
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 e ASAN_OPTIONS=halt_on_error=1,
sem diagnóstico dos sanitizers:

1. **629/629 testes**, 31 suites; **28 novos** (11 de clock e 17 de binding).
   Dois microexperimentos DISABLED não são contados nem foram executados aqui.
2. **145 casos afetados × 20**, shuffle/seed GTest 1230: **2.900 execuções
   adicionais**, não casos únicos.
3. **31/31 grupos CTest** `^world_generation_contract_`, NoAssets, repetindo
   os mesmos casos normativos, sem experimentos de tempo.
4. Build/link NoAssets e verificação de **dez objetos reais**: Assets pelo alvo
   base; Root, MaterialDatabase, geração, produto/comparador, TileDamage,
   TileDamageState, WorldTiles, WorldTemplate e WorldStorage pelos targets de
   objetos. Root/Assets/MaterialDatabase foram recompilados, não todos os objetos
   inalterados forçados a rebuild. Build final sem trabalho pendente. Não é link do jogo.
5. Sintaxe/tipos Tracy de **seis fontes**: Root, Assets, MaterialDatabase,
   WorldGeneration e os dois testes BaseTiles/fontes. STAR_ENABLE_TRACY/TRACY_ENABLE
   com headers oficiais existentes de Tracy 0.11.2, commit
   b8c2e25c3d02009059e09f623072921677dd6bb4. Não linking Tracy. Avisos herdados de
   fmt e da expressão booleana de Assets::loadEmbeddedData permanecem.
6. `git diff --check` aprovado; **617 links locais em 46 documentos**, 14 módulos
   alcançáveis, 49 contratos únicos cobertos no roadmap e ordem 1–49 conferidos.
   Oito totais de fase e 6/196 marcos = 3,1% recalculados; RAM-01/H0 mantidos em
   25%. Treze fontes do manifesto conferidas em bytes/SHA-256.

Testes exercitam o **clock/binding reais com fontes controladas**, não os
construtores/callbacks de Root/Assets/MaterialDatabase. Cobrem nested/overlap,
falha/recuperação/retire, alocação concorrente de 512 IDs e saturação local sem
envenenar o alocador global, captura interrompida, troca de fontes/Root, retenção
física, geração zero, scope 2 e Refused/Applied/Stale. Invalidação após escrever
é reproduzida deterministicamente; não prova ausência de races em todas as fontes.
As regressões de catálogo/fita/corpo anterior continuam na bateria. **Reload com
assets reais, jogo, mods, save/reload e multiplayer não foram executados.**

Reprodução após configurar build equivalente conforme os relatórios anteriores:

```bash
cmake --build <build-dir> --target world_generation_contract_tests -j3
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ASAN_OPTIONS=halt_on_error=1 \
  ./dist/world_generation_contract_tests --gtest_brief=1
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ASAN_OPTIONS=halt_on_error=1 \
  ./dist/world_generation_contract_tests \
  --gtest_filter='WorldGenerationSourceVersionTest.*:WorldGenerationLookupSourcesTest.*:WorldGenerationBaseTilesTest.*:WorldGenerationBaseTileInputsTest.*:WorldGenerationBaseTileObservationTest.*:WorldGenerationBaseTileLookupContextTest.*' \
  --gtest_repeat=20 --gtest_shuffle --gtest_random_seed=1230 --gtest_brief=1
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ASAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir <build-dir> -R '^world_generation_contract_' --output-on-failure
```

## Memória, continuidade e progresso

Layout medido nesta ABI x86_64: clock **64 bytes**, binding **72**, scope 2 **56**,
Context<32> **328** (antes 320 no scope 1). Entries continuam 256 bytes reservados,
48 ativos no exemplo de seis chaves. Root tem um shared clock alocado, incluindo
control block não medido aqui; Assets tem clock embutido, MaterialDatabase uma
versão escalar. Binding não aloca internamente, mas pode prolongar retenção de
fontes inteiras, caches e threads associadas. Não atribuir só 72 bytes a esse
lifetime nem recobrar o mesmo backing por leitor. Classes/fórmulas do ledger
privado não foram ampliadas para declarar contabilização de bancos globais.

Próximo recorte executável: comparar as três consultas **dos leitores reais**,
vinculadas às fontes/ao template esperado, com catálogo/fita e cobertura/misses
explícitos. Exercitar hotReload/reload, listener que falha, fontes antigas retidas
e templates existentes sem reconstrução implícita. Revalidar depois de preparar
e no dono antes de qualquer publicação. Só então sombra por setor, com admissão/
bytes, outputs equivalentes e descarte stale sem fallback em output alterado.
Não exigir cada heap do motor antes de H1 delimitado; manter exclusões explícitas.

RAM-01/H0 seguem **25%**, programa **3,1%**, 6/196 marcos dos 49 contratos.
Scope 2, clock/binding 1 são infraestrutura, não contexto completo de CPU-01 ou
consumidor do catálogo no jogo. Retry 2, estimativa 9, envelope/admissão 1 e 14
classes de memória inalterados, pressão padrão desligada. Sem ganho de setores/s
ou benchmark de overhead desta integração demonstrado.

Sem novos config/log/Tracy do usuário ou reanálise. Sem TSan/MSan, OOM/pressão
externa, Windows/macOS/ARM ou allocators alternativos. Achados biomeTransition,
seed automática e Perlin continuam abertos, sem causa de crash atribuída.
Sem Actions, release, main ou revisão independente. Compatibilidade com mods/
saves/multiplayer e fallback CPU são gates essenciais; NPC externo suspenso e
Linux Clang adiado. Dados brutos continuam fora do Git.

## Manifesto das fontes

Fontes locais de 09/11/2026, nomes relativos ao checkout, bytes/SHA-256. Treze
fontes alteradas; nenhum artefato novo do usuário recebido nesta entrega.

| Arquivo | Bytes | SHA-256 |
| --- | ---: | --- |
| `source/core/CMakeLists.txt` | 6376 | `34141f628f7733bee30d9c0988c674510b0e84947701d969aba0ee63312e8f09` |
| `source/core/StarGenerationVersion.hpp` | 2812 | `96a991f1b8d7e475d7515e95cd9899b11f80f8094b076d8384da1a656c67faa2` |
| `source/base/StarAssets.cpp` | 60106 | `31294a357e7efdf1bcba264de5eca18fbaf62635df47bd623d1161f6af89b752` |
| `source/base/StarAssets.hpp` | 14150 | `07fcf251cebb9372414db005120545753d1f2647cd7bc749172fde7c39ad48f1` |
| `source/game/CMakeLists.txt` | 16952 | `3dd5e33a1a86fb12a4a6a82547621e9dd27a2ec1bd5b0adf55a5c472ae34071f` |
| `source/game/StarMaterialDatabase.hpp` | 9284 | `ec4a5b906605fdcc9f9f0a1be98e0e573d04217b17fe2ddfccd751ec33183ace` |
| `source/game/StarRoot.cpp` | 30396 | `d29b57f63b91359cd01c8b2c4c5baedf8e9ea48da1cdca8bf95b01627b733b97` |
| `source/game/StarRoot.hpp` | 10633 | `438bc2f29f9ccc4e3483c91e7f8d6c33b97f8d3a13811b3cba098d192765eb75` |
| `source/game/StarWorldGenerationBaseTileLookupContext.hpp` | 7490 | `47c5208662384f6531eed231bb2df00af3ee4c6e4c7aa7c80203f34be1b45f5a` |
| `source/game/StarWorldGenerationLookupSources.hpp` | 3251 | `1f5d5a206859b385c6e71cc8ef179fdc52b47052c3fb19973df1062a7677fede` |
| `source/test/CMakeLists.txt` | 6548 | `90529551c98c0e8949f20decdc464e51b6db45be6b6e3eda031aed42dd8afc98` |
| `source/test/world_generation_base_tiles_test.cpp` | 79412 | `f367cdda1422e59a3197daaa9a7256a2397905c9407b97c5ee211827be8a2b9b` |
| `source/test/world_generation_lookup_sources_test.cpp` | 15070 | `86f2aee5117c8d5ea10176cd252b80a9fbbfec1a08ac9b54fed8fb722ac8c4ec` |
