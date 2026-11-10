# H0: Assets real, invalidação e encerramento de workers

Continuidade: [Root real e fontes carregadas](sector-runtime-h0-root-lifecycle-2026-11-10.md).
Os resultados e limites abaixo permanecem históricos desta etapa.

Data: 10/11/2026 UTC. Branch de entrega: `Experimental`; revisão local:
`codex/experimental-regional-runtime`.
Base: `b362a9d78a26bf2e8024cb3005b0d2ee0ab4b8bf`.
Implementação: `571bb01e7f96b11e879310e05279be7f60185574`.

[Programa](sector-runtime/README.md) · [Roadmap](sector-runtime/roadmap.md)
· [Progressão](sector-runtime/progress.md)
· [Contrato BaseTiles](sector-runtime-base-tiles-contract.md)
· [Envelope](sector-runtime-memory-envelope.md)
· [Etapa anterior](sector-runtime-h0-real-lookup-readers-2026-11-10.md)

## Objetivo e resultado

Avançar a validação de lifecycle das fontes, antes de um contexto privado de
geração de setores comuns. A etapa anterior ligava leitores reais, mas usava
um substituto de Assets para suas versões. Agora os testes **constroem Assets
real, leem arquivos locais e executam hotReload/cache/workers/destruição**.
Encontraram dois problemas herdados, corrigidos no runtime: fill de ByteArray
vazio com memset de ponteiro nulo e janela de lost-wakeup na parada de Assets.

**Não é integração completa de Root, terreno ou jogo.** Um host derivado de
RootBase fornece Configuration real com safe.allowRemoteAssets=false, sem
Root::Root, manutenção, reload/hotReload/loadMods, listeners ou encerramento de
Root. A factory Root::worldGenerationLookupSources também não é chamada. O clock
parent e as definições materiais/biomas continuam controlados,
conforme a fixture da etapa anterior. Não se carregam os bancos completos ou
pacotes de assets/mods do usuário. Nenhum método de Assets foi substituído por
stub ou outro algoritmo para fazer o teste passar.

Esse recorte evita remodelar o bootstrap de Root só para conseguir um teste
isolado. O gate de Root real permanece aberto. Não há produção normal com
catálogo/fita, aceleração de setores/s demonstrada ou mudança de prioridade.

## Cenário reproduzível e fonte dos dados

Arquivos de primeira parte gerados pela fixture em diretório temporário exclusivo:
preload.config com array vazio, _metadata identificando a fixture, probe.config
com marker inteiro e probe.bin com texto fixo. Casos adicionais criam late.config
ou oito arquivos queuedN.config, alteram marker/bytes ou escrevem JSON quebrado
deliberadamente. Não são logs, saves ou conteúdo real do usuário.

Assets::Settings é inicializado por valor, com TTL=60 s, audioDecompressLimit=0
e zero workers na maioria dos casos. Dois casos usam dois workers reais:
fila/leitura/reload/join e construção/destruição imediata em 32 ciclos.
Sem sleeps, acesso remoto, scripts de mods ou RNG automático do jogo. O diretório
usa create_directory exclusivo e só sua raiz privada é removida após encerrar
os handles/workers. Um teste abortado antes da limpeza pode deixar sua fixture
temporária fora do Git; não foi feito cleanup amplo do executor.

O host guarda somente weak handle para Assets e limpa o singleton RootBase ao
encerrar. Isso permite verificar o último owner sem prolongar artificialmente
o lifetime. MaterialDatabase e WorldTemplate usam as seams privadas de metadata
anteriores; não suas fábricas completas. Catalog/input/liquid/scope/binding
mantêm as mesmas versões e não ganham comportamento de produção novo.

## Resultados de Assets e fronteira de validade

| Operação real | Resultado observado |
| --- | --- |
| Construção por DirectoryAssetSource | Índice, metadata, digest e JSON carregados; geração não zero |
| Editar arquivo já carregado | Não muda generationRevision; cache pode continuar devolvendo o conteúdo anterior |
| hotReload explícito, inclusive sem editar arquivos | Avança geração e invalida binding/catálogo anteriores |
| Aplicar catálogo antigo após hotReload | Refused sem writes, tanto por binding antigo como por scope antigo com binding novo |
| Reconstruir catálogo sob o scope novo | Applied reproduz o corpo legado sobre as mesmas definições de fixture |
| Handles de JSON/bytes anteriores | Continuam válidos com conteúdo anterior após limpar cache/hotReload |
| clearCache sem handles externos do JSON | Pode atualizar o conteúdo lido sem mudar a geração explícita; binding continua válido |
| JSON inválido, reparado no disco | Falha fica em cache até hotReload; recuperação então lê o reparo |
| Acrescentar arquivo depois da construção | hotReload não reescaneia; outro objeto Assets recém-construído vê o arquivo |
| Duas instâncias com conteúdo/digest iguais | Possuem gerações independentes; reload de uma não invalida a outra |
| Invalidar parent/drop do owner de Assets | Binding inválido ainda retém o objeto, liberado ao soltar o último handle |

**Conclusão:** generationRevision de Assets cobre reload explícito, não é hash
de todo conteúdo, monitor de disco ou lease de leitura. O teste de clearCache
torna essa exclusão concreta. As três consultas atuais de BaseTiles leem
definições já construídas, mas isso não autoriza futuros kernels a ler JSON cru
mutável confiando só no scope. Capturar/reter/versionar os inputs efetivamente
consumidos, revalidar fontes esperadas e preservar o ponto de commit do dono.
Não alterar o reload legado para simular uma garantia que essa API não oferece.

## Correção 1 — ByteArray vazio no caminho do digest

A primeira execução estrita parou em StarByteArray.cpp:116:
"null pointer passed as argument 1". Cadeia observada:
Assets::Assets → DataStreamBuffer::serialize → takeData → Buffer::takeData →
Buffer::reset(0) → ByteArray::fill. Depois do move, o array tem storage nulo e
size zero; a chamada memset(nullptr, c, 0) viola a pré-condição nonnull.

fill continua fazendo o mesmo resize, mas só chama memset quando m_size>0.
Não aloca um byte fictício, muda capacidade/ownership ou serializa dados novos.
Para arrays não vazios, o corpo de preenchimento permanece igual. Quatro casos
novos verificam fill vazio/default/construção, capacidade e bytes inativos
previamente inicializados, fill sem resize e takeData/reuso do buffer vazio.
O caso ByteArrayTest.All existente também passa a integrar o alvo acelerado.
Não é auditoria de todas as operações zero-length de ByteArray nem correção dos
achados de RNG, Perlin, biomeTransition ou SkyParameters.

## Correção 2 — parada dos workers sem perder wakeup

Antes da correção, a bateria completa passou uma vez, mas a repetição ficou
bloqueada. Inspeção do processo mostrou a thread principal e um Assets::workerM
ambos em futex_wait; o processo de teste foi finalizado explicitamente com
SIGTERM para continuar o diagnóstico. Não contar essa tentativa como aprovação.
Não havia gdb instalado; não foi capturado backtrace nativo dessa espera.

A inspeção do código identifica esta janela suficiente para lost-wakeup:

1. Worker lê stop=false e entra na coleta Lua, fora do mutex de assets.
2. Destrutor publica stop e faz broadcast de queued.
3. Worker adquire o mutex depois do broadcast, não revalida stop e espera queued.
4. Destrutor espera o join; sem outro evento, a espera não termina.

O hang observado é consistente com essa janela; não se atribui a ele um endereço
exato de condição sem backtrace nem se associa aos crashes das runs antigas.
A correção fecha a janela no código: stop é publicado sob m_assetsMutex, com
broadcast de queued **e done**; worker revalida stop sob o mesmo mutex antes de
inspecionar/esperar a fila. A espera por dependência também confere stop, pois
doLoad/doPost podem liberar o mutex durante IO. O check rápido anterior à coleta
Lua permanece. Prioridades, quantidades de workers, conteúdo, locks do reload e
efeitos dos mods não são reordenados.

O teste novo repete criação/destruição imediata 32 vezes com dois workers. A
bateria adicional completou 640 desses ciclos (32×20), incluídos nas 3.640
execuções abaixo, não uma contagem extra de testes. É estresse com término
observado, não prova TSan ou reprodução determinística de toda interleaving.
Não altera o contrato de terminar trabalho dependente antes de destruir fontes,
nem torna seguro destruir Assets enquanto qualquer chamador externo ainda o usa.

## Validações executadas

Ambiente cloud inspecionado conforme a skill operacional; reutilizados cache e
sysroot anteriores, sem instalação/download novo. GCC 14.2.0/C++17, CMake 3.29.2/
Ninja 1.13, Debug -Og; GUI/Tracy/allocators alternativos OFF, BUILD_TESTING=ON,
VCPKG_MANIFEST_INSTALL=OFF. Cache `/tmp/sector-input-h0.RX3XLi`, identifier
`b362a9d7-assets-lifecycle-local-dirty`. Mesmo hook SDL3 externo ao Git.

C++ com -fsanitize=address,undefined -fno-omit-frame-pointer e XXH_NO_INLINE_HINTS=1;
C/dependências pré-compiladas não têm instrumentação equivalente. Depois das
correções, UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 e
ASAN_OPTIONS=halt_on_error=1 permaneceram estritos, sem diagnósticos novos:

- **666/666 testes**, 34 suites. São 16 casos novos (12 Assets + quatro ByteArray)
  e um caso ByteArray anterior incorporado ao alvo, além dos 649 anteriores.
  Os três microexperimentos DISABLED não foram executados nesta etapa.
- **182 casos de nove suites ×20**, shuffle/seed inicial 1232:
  **3.640 execuções adicionais**. A tentativa anterior com 181 casos, antes da
  correção do stop, foi interrompida; não faz parte desse resultado aprovado.
- **34/34 grupos CTest** `^world_generation_contract_`, label NoAssets. Aqui
  NoAssets significa sem pacote externo do jogo; a suite nova usa arquivos
  temporários e Assets real, não apenas funções puras. TIMEOUT=120 s por grupo.
- Build/link do alvo acelerado com core/base reais. ByteArray e Assets foram
  recompilados nas unidades normais usadas pelo jogo; Root/WorldGeneration
  conferidos como objetos existentes, sem recompilação forçada. Build final
  sem trabalho pendente. Não é link do jogo completo ou execução de core_tests.
- Sintaxe/tipos Tracy de quatro fontes: ByteArray, Assets, byte_array_test e
  world_generation_lookup_readers_test. STAR_ENABLE_TRACY/TRACY_ENABLE e headers
  oficiais locais 0.11.2, commit verificado
  b8c2e25c3d02009059e09f623072921677dd6bb4. Não link Tracy. Avisos herdados de fmt
  e Assets::loadEmbeddedData não corrigidos; warning novo de signedness no teste
  foi eliminado antes da validação final.

O caso de JSON inválido produz AssetException/log de erro **esperado**, distinto
dos diagnósticos dos sanitizers ou de uma falha da bateria.

Conferência documental: **651 links locais em 48 documentos**, 14 módulos
alcançáveis, 49 contratos únicos/cobertos no roadmap e ordem 1–49. Oito totais
de fase e 6/196 marcos = 3,1% recalculados. Manifesto de cinco fontes conferido
por bytes/SHA-256; `git diff --check` aprovado.

Reprodução após configurar build equivalente:

```bash
cmake --build <build-dir> --target world_generation_contract_tests -j3
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ASAN_OPTIONS=halt_on_error=1 \
  ./dist/world_generation_contract_tests --gtest_brief=1
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ASAN_OPTIONS=halt_on_error=1 \
  ./dist/world_generation_contract_tests \
  --gtest_filter='ByteArrayTest.*:WorldGenerationAssetsLifecycleTest.*:WorldGenerationLookupReadersTest.*:WorldGenerationSourceVersionTest.*:WorldGenerationLookupSourcesTest.*:WorldGenerationBaseTilesTest.*:WorldGenerationBaseTileInputsTest.*:WorldGenerationBaseTileObservationTest.*:WorldGenerationBaseTileLookupContextTest.*' \
  --gtest_repeat=20 --gtest_shuffle --gtest_random_seed=1232 --gtest_brief=1
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ASAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir <build-dir> -R '^world_generation_contract_' --output-on-failure
```

## Próximas ações, limitações e progresso

1. Validar Root real: construção/encerramento, reload/hotReload/loadMods,
   listeners que falham, fontes antigas retidas e templates existentes sem
   reconstrução implícita de bancos/layouts. O host RootBase não fecha esse gate.
2. Fechar o próximo contexto de setores comuns: blockInfo, inicialização/RNG,
   fontes esperadas e captura dos inputs realmente lidos, não apenas metadata.
3. Revalidar após preparar e no dono antes de sombra por setor com bytes/
   admissão/comparador. Refused não escreve; Stale posterior exige descarte,
   nunca fallback/retry sobre tile parcial.
4. Streaming reproduzível com save/config/assets/mods/rota fixos, setores inéditos
   entregues/s, interesse→entrega e resposta de update/frame; depois mailbox/DAG,
   demanda/provider/fairness e escala de hardware conforme o roadmap.

**H0/RAM-01 continuam 25%; programa 3,1%, 6/196 marcos dos 49 contratos.**
Correções reais e testes parciais não fecham outro marco dos contratos completos.
Sem nova classe/fórmula/cota no ledger, versão de modelo, configuração, preview
ativado, GPU, formato de save/rede ou escrita paralela no mundo. Compatibilidade
com mods/saves/multiplayer e fallback CPU continuam gates essenciais; NPC externo
suspenso, Linux Clang adiado. Não há benchmark de aceleração nesta entrega.

Sem novos arquivos do usuário ou reanálise de logs/config/Tracy antigos; dados
brutos fora do Git. Sem assets completos do jogo, mods/scripts de mods, save/
multiplayer, TSan/MSan, pressão/OOM, outros allocators/plataformas/arquiteturas,
revisão independente, Actions, release ou merge em main.

## Manifesto das fontes

Cinco fontes locais de 10/11/2026, nomes relativos ao checkout, bytes/SHA-256.
Nenhum novo artefato do usuário; os inputs temporários são gerados pelo teste.

| Arquivo | Bytes | SHA-256 |
| --- | ---: | --- |
| `source/base/StarAssets.cpp` | 60466 | `e719acb31f38ca94e1e3e8f73733831d3b71c8f4369db90312776dc94c6a51f2` |
| `source/core/StarByteArray.cpp` | 5216 | `6ae013b71e18a1689d1e08ea8eee4fe9f27ef5cacef72532cc83b4282e36e9d0` |
| `source/test/CMakeLists.txt` | 6884 | `360d653a27f63bfd6ee704c13baa70442ae04062d92f7d4a6f5717c172b0c326` |
| `source/test/byte_array_test.cpp` | 2548 | `b2399dc59e6b64cf0831c05762378ee94932c062cad9f8962a3d63ddb12a689e` |
| `source/test/world_generation_lookup_readers_test.cpp` | 35340 | `a4a149df372cfb303e26939ab4eac8c65af938343e9f0757890c80e048c32614` |
