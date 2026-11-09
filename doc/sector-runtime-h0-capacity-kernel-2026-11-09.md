# H0: capacidade do ledger e primeiro subkernel real de BaseTiles

Data: 09/11/2026. Entrega: `Experimental`; revisão local:
`codex/experimental-regional-runtime`. Base: `acbe96f3fc61d8bac53ba13bc4b50f7e37173050`.
Implementação: `8e190742712e932d895e673fc008ee229a608d81`.

[Programa](sector-runtime/README.md) · [Roadmap](sector-runtime/roadmap.md)
· [Progressão](sector-runtime/progress.md) · [Contrato](sector-runtime-memory-envelope.md)
· [CPU](sector-runtime/cpu-and-stages.md)
· [Etapa anterior](sector-runtime-h0-variable-memory-2026-11-09.md)

Continuação: [identidade/comparador BaseTiles](sector-runtime-h0-base-tiles-identity-2026-11-09.md),
`f7785963`. Os resultados e manifesto abaixo permanecem os desta etapa histórica.

## Objetivo e resultado

Substituir o evento manual de capacidade no protocolo de retry por uma revisão
do próprio ledger, sem perder uma liberação ocorrida após a recusa. Isolar uma
primeira fronteira pura, versionada e comparável **realmente usada** em BaseTiles.

Entregues: sinal de capacidade, captura atômica na recusa, consumo no próximo
poll do owner e kernel escalar de decisão inicial de líquido por tile. **Retry
continua sem consumidor na geração real; pressão do singleton segue desligada.**
O kernel é usado na geração, mas não ganhou retry, cache de produtos ou novo
executor. Sem config, limites, filas, workers, TTL, níveis, orçamento de update,
publicação ou simulação de líquidos alterados. Não é otimização do backlog de
oceanos/clima, prova de progresso de todo DAG ou ganho de setores/s demonstrado.

## Sinal de capacidade e ausência de sinal perdido

`WorldGenerationMemoryCapacityRevision`, modelo **1**, é uint64 saturante.
Snapshot e consulta pequena capacityRevision expõem o mesmo estado protegido
pelo mutex. A revisão avança uma vez em cada shrink positivo ou release de
recurso ativo, inclusive token zero que devolve um slot. Aliases/reuso, carga,
crescimento, refusas de tokens inativos e notas diagnósticas não avançam.
Json compartilhado conserva sua deduplicação/retirada por identidade.

`tryReserveBatch` só devolve deferredCapacity em Deferred, **sob o lock da própria
decisão**. Se houver release depois, o marker antigo permanece na resposta.
Ler um snapshot mais recente para registrar a recusa poderia engolir esse sinal;
o teste inclui esse controle negativo. O sinal permite reavaliar, não garante
que todo o pedido caiba, nem muda soft/hard/histerese ou a demanda.

Protocolo de retry passa ao modelo **2**, com admit sem argumento de revisão.
O owner compara a revisão do ledger vinculado ao marker da recusa. Sem mudança,
retorna AwaitProgress sem criar tokens/avaliar pressão novamente. Com mudança,
tenta a admissão normal. Fences dos tokens físicos continuam prevalecendo.
O cenário anterior de dois jobs/resultados retidos agora retoma sem o teste
fornecer artificialmente um número de progresso. Outros ledgers não o acordam.

Não há callback, alocação ou wakeup de thread no release. O owner ainda precisa
chamar admit; isto **não é mailbox, fila de prontos ou scheduler de eventos**.
Saturação não dá wrap: em uint64 máximo, a comparação deixa conservadoramente
de suprimir consultas, evitando perder futuros releases. O caso testa essa
aritmética, não executa 2^64 liberações. A revisão tem sentido só no mesmo ledger.

O incremento aproveita o mutex já existente; o poll suspenso faz uma consulta
curta sob lock, sem converter envelope. Há custo adicional não benchmarkado.
Liberação de **estimativa** também sinaliza capacidade declarada, sem comprovar
desalocação de heap/RSS. Não usar esse contador para inferir RAM livre do sistema.
O singleton não passou a impor limite, nem o jogo passou a executar o retry.

## Subkernel real: decisão inicial de líquido de BaseTiles

`StarWorldGenerationBaseTileLiquid.hpp` extrai a lógica de prepareBaseTileArray
do próprio projeto, mantendo a origem herdada. Decisão modelo **1**:

| Entrada/fronteira | Saída/efeito | Exclusões |
| --- | --- | --- |
| Foreground/background já resolvidos, oceanLiquid, nível, enclosure e y | Keep, Clear, Endless ou Filled; líquido/pressão escalares | Nenhum WorldTemplate, Root, RNG, asset, entidade ou mundo vivo |
| Adapter recebe LiquidStore privado e a entrada escalar | Keep não atribui; demais ações usam os mesmos construtores anteriores | Nenhum outro campo de tile, cópia de ServerTile ou buffer novo |

Chamada real fica na mesma posição do loop, depois de resolver os materiais.
Ordem/número de blockInfo, consultas de hue e supportsMod permanecem iguais.
Inteiros, comparação y < nível, subtração e conversão de pressão são preservados.
O contrato exige diferença representável em int, como o caminho anterior; não
foi introduzida correção de overflow para inputs fora desse domínio.

Versiona **só a decisão escalar**, não identidade de mundo/template/setor ou um
produtor completo. Finalizar Keep depende do LiquidStore anterior, conservado
no adapter: cache/identidade de output futuro deve incluir esse estado, não só
a entrada escalar. Não há replay de BaseTiles inteiro, simulação paralela de
líquidos, novo algoritmo de oceanos ou offload GPU nesta extração.

## Validação executada

Skill cloud usada para conferir executor/build/rede, sem instalar/baixar
dependências. GCC 14.2.0/C++17, Debug `-Og`, GUI/Tracy/allocators alternativos
OFF, `VCPKG_MANIFEST_INSTALL=OFF`. C++ com
`-fsanitize=address,undefined -fno-omit-frame-pointer -DXXH_NO_INLINE_HINTS=1`;
C/dependências pré-compiladas não têm a mesma instrumentação. Identifier:
`acbe96f3-capacity-kernel-local-dirty`; fontes do commit/manifesto abaixo.

Com `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1` e
`ASAN_OPTIONS=halt_on_error=1`, sem diagnóstico dos sanitizers:

1. **484/484 testes**, 25 suites. **32 novos**: vinte de capacidade e doze do
   kernel; 32 casos de retry atualizados para a API sem revisão manual.
2. As três suites afetadas (64 casos) repetidas vinte vezes, shuffle e seed
   GTest 1225: **1.280 execuções adicionais**, não casos únicos.
3. **25/25 grupos CTest** `^world_generation_contract_`, NoAssets; repetem
   casos do mesmo binário, não outros 484 casos distintos.
4. Contratos e cinco objetos reais aprovados: geração, ledger, envelope,
   template e storage. Últimos três recompilados por headers, não editados.
   O alvo NoAssets também compila StarLiquidTypes.cpp existente, sem alterações.
   Sem linking completo do jogo.
5. Sintaxe/tipos de geração, ledger, envelope e três testes afetados aprovados
   com STAR_ENABLE_TRACY/TRACY_ENABLE e headers Tracy 0.11.2 existentes. Retry
   conferido novamente após adaptar dois chamadores remanescentes da API antiga.
   Não é linking Tracy completo; aviso herdado de fmt permanece.
6. `git diff --check` aprovado; **525 links locais em 40 documentos**, incluindo
   todos os relatórios H0 (cobertura documental ampliada em relação à etapa
   anterior), 14 módulos alcançáveis e 49 contratos únicos cobertos pelo roadmap.
   Ordem 1–49, oito totais de fase e 6/196 marcos = 3,1% conferidos; RAM-01/H0
   mantidos em 25%. Dez fontes conferidas em bytes/SHA-256.

Capacidade: liberação por worker, facade/root fechado, aliases Json, concorrência
de oito threads, shrink/histerese, rollback de construção, marker da decisão,
scope estrangeiro, fences, sinais sem bytes suficientes e saturação.

Kernel: **7.560 combinações** comparadas com a expressão anterior independente,
conferindo ID, source e bits de level/pressure, não padding do struct. Inclui
materiais normais/nulos/meta, oceanos vazios/IDs extremos, fronteira y, enclosure
e três estados anteriores. Casos adicionais cobrem zero assinado e diferenças
válidas junto a limites int. Um conjunto de **1.024 decisões** em quatro workers
coincide com a referência serial: não são 1.024 setores completos gerados.

Reprodução após build equivalente, usando os mesmos cinco alvos de objetos do
relatório anterior e world_generation_contract_tests:

```bash
UBSAN_OPTIONS=halt_on_error=1 ASAN_OPTIONS=halt_on_error=1 \
  ./dist/world_generation_contract_tests --gtest_brief=1
UBSAN_OPTIONS=halt_on_error=1 ASAN_OPTIONS=halt_on_error=1 \
  ./dist/world_generation_contract_tests \
  --gtest_filter='WorldGenerationMemoryCapacityTest.*:WorldGenerationBaseTileLiquidTest.*:WorldGenerationMemoryRetryTest.*' \
  --gtest_repeat=20 --gtest_shuffle --gtest_random_seed=1225 --gtest_brief=1
UBSAN_OPTIONS=halt_on_error=1 ASAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir <build-dir> -R '^world_generation_contract_' --output-on-failure
```

## Limites e próxima entrega

Sem novo config/log/Tracy do usuário ou captura anterior reanalisada; dados
brutos fora do Git. Sem jogo/assets/mods, save/reload, multiplayer, OOM/pressão
externa, TSan, benchmark de overhead/throughput, Windows/macOS/ARM ou allocators
alternativos executados. Sem Actions, release, merge em main ou revisão
independente. Não atribui causa de crash. Gates essenciais de compatibilidade
mods/saves/multiplayer e fallback CPU mantidos; NPC externo suspenso, Linux
Clang adiado. Seed automática e Perlin Uninitialized de CPU-01 não corrigidos.

RAM-01/H0 permanecem **25%**, programa **3,1%**, 6/196 marcos. O subkernel apoia
MOD-01/VAL-02, mas não completa esses contratos nem concede marco a CPU-01.
As 14 classes, envelope/admissão 1 e fórmulas profundas anteriores não mudaram.

Próximo recorte executável: identidade versionada do produto/contexto de
BaseTiles e comparador dos campos completos, incluindo líquido anterior em Keep,
epoch do mundo e revisões de tiles/template. Estender a fronteira pura para
materiais/mods/biomas sem antecipar consultas ou alterar cópias/RNG. Vincular
demanda ao interesse real antes de retry do produtor completo. O sinal de
capacidade já permite polling correto, mas integração de mailbox/DAG em sombra,
provider único, quotas/fairness, promoção, pressão externa e baseline integrado
continuam gates para limites/defaults e maior admissão. Não exigir medir cada
heap do motor antes de um protótipo H1 delimitado.

## Manifesto das fontes

Fontes locais de 09/11/2026, nomes relativos ao checkout, bytes e SHA-256.
Nenhum novo artefato de execução do usuário nesta etapa.

| Arquivo | Bytes | SHA-256 |
| --- | ---: | --- |
| `source/game/CMakeLists.txt` | 16672 | `8d4351487d5d566696965aa9b7270a1ce1a876be98914e115ef95cfd632206ae` |
| `source/game/StarWorldGeneration.cpp` | 129085 | `cb77e6f4288dbbe9c998145c197ca5329704552baeb73c6acec1e8ae5a34c1cd` |
| `source/game/StarWorldGenerationBaseTileLiquid.hpp` | 2304 | `b19b094043a124bbcb1b60c6e789aa6bba9541e09bf9216710bc7dfafbc813e1` |
| `source/game/StarWorldGenerationMemory.cpp` | 13158 | `c39bb4ae6cd8c8efa0dfd7818ca91c642ada76c3463385ff1445b610981b7094` |
| `source/game/StarWorldGenerationMemory.hpp` | 9155 | `35aab8bf51165a5ef3a78f7aec2f5b368983c777a98b64dc53aa5b0bfb336b28` |
| `source/game/StarWorldGenerationMemoryRetry.hpp` | 5328 | `887ddee63fb202164689eb0acf93c016445c02edf1d8fb6573e80ada438514d2` |
| `source/test/CMakeLists.txt` | 6127 | `ac8c19ac48d183009d44de798d393514540fc5507be0428246e69e9f8a03b364` |
| `source/test/world_generation_base_tile_liquid_test.cpp` | 6891 | `45349c2c7eef056584f3b8ac349fa4adb30ce11307d463389d1c8be98e0ecc1e` |
| `source/test/world_generation_memory_capacity_test.cpp` | 11135 | `ac5e591a005d815f15f2034f777297afbf12f86aea31bef83e9a312c2f6bf388` |
| `source/test/world_generation_memory_retry_test.cpp` | 24108 | `04868cc1146677595387d4c20621085e4d0d9321059c122775104bb3aa1c9a03` |
