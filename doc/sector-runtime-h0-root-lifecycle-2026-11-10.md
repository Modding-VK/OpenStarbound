# H0: Root real, fontes carregadas e recuperação

Data: 10/11/2026 UTC. Entrega: `Experimental`; revisão local:
`codex/experimental-regional-runtime`.
Base: `ed190c1a0197f0acc39db8d94ed95e8add505591`.
Implementação: `7001ae6d2f11c41c7252d3fa9077fa325579a595`.

[Programa](sector-runtime/README.md) · [Roadmap](sector-runtime/roadmap.md)
· [Progressão](sector-runtime/progress.md)
· [Contrato BaseTiles](sector-runtime-base-tiles-contract.md)
· [Envelope](sector-runtime-memory-envelope.md)
· [Etapa anterior](sector-runtime-h0-assets-lifecycle-2026-11-10.md)

## Entrega e fronteira

O novo alvo opt-in executa **Root normal**, não uma subclasse/host RootBase:
construtor, thread real de manutenção, factory worldGenerationLookupSources,
Assets e MaterialDatabase carregados, reload/hotReload/loadMods, listeners,
falhas/recuperação e destruição. As fontes de versões antes controladas agora
vêm da factory real e do parser das definições de material.

**Escopo mínimo, não jogo completo:** não chama Root::fullyLoad ou RootLoader,
não cria WorldServer/WorldTemplate completo, não executa blockInfo, setores,
scripts de mods, save de mundo ou multiplayer. Usa arquivos de primeira parte
gerados em diretórios exclusivos. Scope recebe worldEpoch=1/templateRevision=0
de teste, não identidade de mundo real. O alvo não usa tags de fixture para criar
Root ou MaterialDatabase. Catálogo continua fora da geração normal.

O gate de lifecycle de Root foi exercitado no escopo acima; a próxima entrega
deve fechar o contexto/inputs de BaseTiles, não repetir esses testes sem hipótese
nova. Não há ganho de setores/s medido, nova WorldThread ou autoridade regional.

## Cenário e configuração

Fixtures locais: preload.config vazio, _metadata com nome da fonte, probe.config,
metamaterials.config vazio, client.config com defaultFootstepSound e probe.material
ID 21, com supportsMods e tabela de dano inline. O parser real lê o material;
ParticleDatabase real encontra zero arquivos particle. O override contém metadata
com prioridade 10, probe.config e o mesmo material com supportsMods=false.
São arquivos sintéticos, não um pacote de mod ou scripts de terceiros.

Root::Settings é inicializado por valor: TTL 60 s, audioDecompressLimit=0,
workerPoolSize=0 na maioria dos casos e 2 no primeiro; logLevel=Error,
quiet=false, includeUGC=false, safe.allowRemoteAssets=false, configurationVersion=1.
Um caso habilita runtimeConfigFile somente no diretório de storage da fixture.
Os diretórios compartilhados pelos testes de Assets/Root não usam o RNG do jogo;
limpeza remove somente a raiz criada com exclusividade, depois dos owners.
Nenhum log/config/save do usuário foi alterado ou recebido nesta entrega.

## Comportamentos confirmados pelos 13 casos

| Fronteira | Resultado observado |
| --- | --- |
| Construção normal e singleton | Root e sua manutenção iniciam; Assets carrega e o singleton é limpo no encerramento |
| Factory e parser | Binding captura Assets/banco reais e gerações não zero; material é resolvido pelo nome/ID |
| Root::hotReload | Invalida binding, muda assetsGeneration e sourceEpoch; conserva o banco já construído |
| Assets::hotReload direto | Muda assetsGeneration, mas não sourceEpoch de Root nem definições materiais |
| Root::reload | Reconstrói Assets/material sob novos IDs; binding antigo conserva fisicamente definições antigas |
| Listener em hotReload | Factory não oferece vínculo válido durante a transição |
| Listener que falha em hotReload | Factory permanece indisponível; remover o listener e completar outra transição recupera |
| Listener que falha em reload | Fontes antigas seguem retidas, sem binding válido; recuperação cria fontes novas |
| loadMods(false) | Avança epoch e invalida binding sem aplicar a nova fonte; reload posterior aplica o override |
| loadMods(true) | Listener ocorre uma vez, ainda dentro da mudança aninhada; publicação de epoch só ao terminar |
| Destruição e retire | Binding não mantém Root vivo; fica inválido e libera Assets/banco ao soltar o último handle |
| Configuração | Alteração persiste através de reload usando apenas arquivo privado da fixture |
| Falha no carregamento lazy do material | Exceção propaga, não retorna banco parcial; reparar e hotReload permitem nova captura |

Não ler Assets com callbacks dependentes de Root depois de encerrá-lo. A checagem
pós-destruição lê somente definições de material já construídas, com zero workers
de Assets naquele caso. Fontes retidas não dão permissão para prolongar trabalho
dependente de Root ou dispensar quiescência/guards. A exclusão já comprovada de
clearCache continua: versão de reload não congela conteúdo bruto mutável.

## Correção da seed automática — CPU-01, recorte específico

A primeira execução do construtor normal parou com UBSan em StarRandom.cpp:197:
overflow assinado de monotonicTicks × 1099511628211. A cadeia observada foi
Root::maintenanceMain → Random::addEntropy → checkInit → produceRandomSeed.
É a reprodução do achado do backlog, não causa atribuída aos crashes antigos.

O acumulador da seed passa de int64_t a uint64_t, com conversão explícita dos
ticks. A multiplicação agora tem wrap definido módulo 2^64. A constante, expressão
de XOR, chamadas a rand(), gerador multiply-with-carry, seeds explícitas e arquivos
persistidos não foram alterados. Não se promete equivalência com toda execução
anterior baseada em comportamento indefinido nem entre implementações de rand().

Três casos novos exercitam inicialização/entropia automática reais, os dez valores
de referência de seed/entropia explícitas e o consumo exato de uma seed pelo
construtor default. Passaram com ASan/UBSan; não é validação de toda reconstrução/
ordem de RNG do contexto WorldTemplate. Perlin, biomeTransition e horizonClouds
continuam pendentes; CPU-01 completo não está implementado.

## Ligação isolada e manutenção sem mudança de comportamento

STAR_BUILD_ROOT_LIFECYCLE_TESTS, **padrão OFF**, habilita o executável
world_generation_root_lifecycle_tests e um grupo CTest separado. Exige BUILD_TESTING.
O alvo compila fontes reais com function/data sections e descarta trechos não
usados na ligação. No GNU/Linux desativa apenas o blanket export herdado de
-rdynamic no executável de teste; não afrouxa ASan/UBSan ou stuba símbolos.
DWARF permanece e o stack de UBSan/erros esperados continuou simbolizado.
Flags correspondentes de MSVC/Apple estão descritas no CMake, mas não testadas.
O fluxo normal/Tracy do jogo não recebe essas opções de seção/exportação.

O primeiro link ainda retinha metadata UBSan dos grandes bancos de itens/entidades.
Cinco métodos reais de cleanup foram relocados, **sem alteração de assinatura/
corpo**, para StarDatabaseMaintenance.cpp: ObjectDatabase, ItemDatabase,
MonsterDatabase, TenantDatabase e ImageMetadataDatabase. Mesma unidade no jogo e
no alvo opt-in, sem implementações substitutas. Construtores/fábricas permanecem
nos arquivos antigos. Sem campos, ordem de locks ou política de cleanup novos.

Conferência byte a byte dos cinco corpos contra a base; SHA-256 agregado dos
hashes individuais, na ordem acima, separados por newline:
`29ec309cc70d68d8397d7fcd93bf595621313f8cf67a11c05504c41a2f3bc99c`.
Esses bancos não foram populados nos testes de Root; validar cleanup não vazio
continua sendo parte da matriz integrada. A extração mantém corpos visíveis ao
compilador e não muda a ordem de chamadas na manutenção de Root.

## Validações executadas

Skill de ambiente cloud usada para conferir executor/política e reutilizar
cache/sysroot/ferramentas existentes, sem instalação/download. GCC 14.2.0/C++17,
CMake 3.29.2/Ninja 1.13, Debug -Og; GUI/Tracy/allocators alternativos OFF,
BUILD_TESTING=ON, VCPKG_MANIFEST_INSTALL=OFF, opção Root ON neste cache.
Cache `/tmp/sector-input-h0.RX3XLi`, identifier `ed190c1a-root-lifecycle-local-dirty`.
Mesmo hook SDL3 externo ao Git; não é build limpo de todas as dependências.

C++ usa -fsanitize=address,undefined -fno-omit-frame-pointer e XXH_NO_INLINE_HINTS=1;
C/dependências pré-compiladas não têm a mesma cobertura. Depois da correção,
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 e ASAN_OPTIONS=halt_on_error=1
permaneceram estritos, sem diagnóstico dos sanitizers nas aprovações:

1. **669/669 casos padrão**, 35 suites, incluindo os três testes de seed novos.
2. **13/13 casos de Root**, uma suite, todos novos e opt-in. São **682 casos
   únicos com a opção ON**, não 682 no alvo padrão/Actions padrão.
3. **185×20 =3.700 execuções adicionais** em dez suites padrão e
   **13×20 =260** no executável Root, ambos shuffle/seed inicial 1233:
   **3.960 adicionais**, não casos únicos. Três microexperimentos DISABLED não
   foram executados; nenhum benchmark de velocidade nesta entrega.
4. **35+1 grupos CTest aprovados**, filtro de contratos + Root, label NoAssets;
   Root tem também RootLifecycle e TIMEOUT=120 s. Não é toda a bateria core_tests/
   game_tests nem prova de que a opção foi ligada em CI. Workflows não mudaram.
   Opção OFF também conferida: 35 grupos de contratos, zero de Root e build do
   alvo padrão sem trabalho pendente; depois o cache foi restaurado para ON.
5. Build/link dos dois executáveis com core/base e serviços reais. Random foi
   recompilado na unidade normal. Oito objetos normais do jogo conferidos:
   cinco bancos de origem, nova manutenção, Root e WorldGeneration; os seis
   primeiros foram recompilados, demais reaproveitados. Não link do jogo completo.
6. Sintaxe/tipos Tracy de seis fontes: Random, DatabaseMaintenance, Root e os
   testes de seed/Root/leitores. STAR_ENABLE_TRACY/TRACY_ENABLE e headers locais
   oficiais 0.11.2, commit verificado b8c2e25c3d02009059e09f623072921677dd6bb4.
   Não link Tracy. Avisos herdados de fmt/Assets não corrigidos aqui.

JSON inválido/material e listeners em falha são cenários intencionais. Seus
logs/exceções esperados não são novos diagnósticos de sanitizer. A execução
original que abortou antes da correção da seed não é contada como aprovada.

Conferência documental: **669 links locais em 49 documentos**, 14 módulos
alcançáveis, 49 contratos únicos/cobertos no roadmap e ordem 1–49. Oito totais
de fase e 6/196 marcos =3,1% recalculados; manifesto de treze fontes verificado
por bytes/SHA-256. `git diff --check` aprovado; build final sem trabalho pendente.

Reprodução com o mesmo setup de dependências dos relatórios anteriores:

```bash
cmake -S source -B <build-dir> -DBUILD_TESTING=ON -DSTAR_BUILD_ROOT_LIFECYCLE_TESTS=ON
cmake --build <build-dir> --target world_generation_contract_tests world_generation_root_lifecycle_tests -j3
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ASAN_OPTIONS=halt_on_error=1 \
  ./dist/world_generation_contract_tests --gtest_brief=1
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ASAN_OPTIONS=halt_on_error=1 \
  ./dist/world_generation_root_lifecycle_tests --gtest_brief=1
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ASAN_OPTIONS=halt_on_error=1 \
  ./dist/world_generation_contract_tests \
  --gtest_filter='AutomaticRandomSeedTest.*:ByteArrayTest.*:WorldGenerationAssetsLifecycleTest.*:WorldGenerationLookupReadersTest.*:WorldGenerationSourceVersionTest.*:WorldGenerationLookupSourcesTest.*:WorldGenerationBaseTilesTest.*:WorldGenerationBaseTileInputsTest.*:WorldGenerationBaseTileObservationTest.*:WorldGenerationBaseTileLookupContextTest.*' \
  --gtest_repeat=20 --gtest_shuffle --gtest_random_seed=1233 --gtest_brief=1
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ASAN_OPTIONS=halt_on_error=1 \
  ./dist/world_generation_root_lifecycle_tests --gtest_repeat=20 --gtest_shuffle --gtest_random_seed=1233 --gtest_brief=1
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ASAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir <build-dir> -R '^world_generation_(contract_|root_lifecycle$)' --output-on-failure
```

## Continuidade e progresso

1. Próximo recorte: contexto privado de BaseTiles/blockInfo. Auditar leituras de
   Root/assets/seletores, inicialização/cópias, seed e ordem de RNG; capturar os
   inputs consumidos, não confiar em ponteiros const/epoch como snapshot completo.
2. Comparar saída privada, revalidar no dono e só então sombra delimitada por
   setor, com reserva de bytes/read set/descarte stale. Refused não escreve;
   Stale posterior descarta, sem fallback/retry sobre tile parcial.
3. Streaming com rota/save/config/assets/mods fixos para medir setores inéditos
   entregues/s, interesse→entrega e update/frame, depois mailbox/DAG e executor
   produtivo conforme o roadmap. Não pedir outra run sem hipótese/comparador.

**H0/RAM-01 permanecem 25%, programa 3,1%, 6/196 marcos dos 49 contratos.**
Root com fontes mínimas e correção de seed não completam contexto de CPU-01,
retenção profunda/consumidor de RAM-01 ou benchmark de VAL-01. Não há marco novo
concedido só pelo número de testes. Modelos, 14 classes de memória, cotas,
configuração de geração, defaults, TTL e publicação autoritativa mantidos. Sem
mudança intencional do algoritmo de conteúdo; equivalência integrada ainda pendente.
A nova opção é exclusivamente de build de teste e fica desligada por padrão.

Sem dados novos do usuário/reanálise de capturas antigas, jogo/assets completos,
scripts de mods, world save/reload, multiplayer, TSan/MSan, pressão/OOM, GPU,
outros allocators/plataformas/arquiteturas, revisão independente, Actions, release
ou main. Compatibilidade com mods/saves/multiplayer e fallback CPU continuam
críticos; NPC externo suspenso e Linux Clang adiado. Dados brutos fora do Git.

## Manifesto das fontes

Treze fontes locais de 10/11/2026, caminhos relativos ao checkout, bytes/SHA-256.
Inputs temporários são gerados pelo teste; não há artefato novo do usuário.

| Arquivo | Bytes | SHA-256 |
| --- | ---: | --- |
| `source/core/StarRandom.cpp` | 7872 | `9ca04268a2b9a17f5afd59b8aadc0f6bbf299f58d47037873bd5bedd32cb40c6` |
| `source/game/CMakeLists.txt` | 17163 | `17295868d1dcbcc8cebed3c024a89f7fb5cb70508a498d11963ed9eade7bbea0` |
| `source/game/StarDatabaseMaintenance.cpp` | 1091 | `b391ca4360ec521e6f5b98a3a65a55a1021938ae481f87b7f82193d0be1af389` |
| `source/game/StarImageMetadataDatabase.cpp` | 7597 | `113b5763d240d060a2a19464ff5ea666749c19f4353052551d4b881fc61f34c7` |
| `source/game/StarItemDatabase.cpp` | 29674 | `4318566a745281c1aabbd17b447bce6f2ce8e3406a83b46386fb72b4c2eb59d7` |
| `source/game/StarMonsterDatabase.cpp` | 22349 | `7bf6bb5050125a7b553a92143c52953f0cc3dd6b26c60778ccaac3a00be301bd` |
| `source/game/StarObjectDatabase.cpp` | 27105 | `530ef31d6a2c6a58bca70434cc73d59e97937d070e914ebeb3f068e11f6b2919` |
| `source/game/StarTenantDatabase.cpp` | 3540 | `cbd5921c65f16b8490b05517c825f8eeeb5731cec4e9771865361b24bc770b98` |
| `source/test/CMakeLists.txt` | 9460 | `365f6ad189d075f47d987170e410ea075b35fc3eb187d1342b3374838d16c79d` |
| `source/test/StarWorldGenerationTestDirectory.hpp` | 1800 | `0030d844af5bdfa75a2cafb8bafd53aa0258795e881a34b5e332eb93451165b2` |
| `source/test/random_seed_test.cpp` | 1199 | `af9018e83bfbaacb7b1ba8b0558bdbe787fb8da436b973e85a6ca235c8e155e7` |
| `source/test/world_generation_lookup_readers_test.cpp` | 33789 | `7b512f9c9ca02cb2f624cafc2ed74fb9a1ada2bf3aac2b132cbd1efaf83a8e7b` |
| `source/test/world_generation_root_lifecycle_test.cpp` | 11595 | `2a99578e7a760edec0c86af416cc77681073b2b64c43ce2b0b85bc6e7d53e34f` |
