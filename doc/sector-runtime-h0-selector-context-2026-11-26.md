# H0 / CPU-01: contexto de seletores com identidade e admissão

Data: 26/11/2026 UTC. Branch `Experimental`, base `d5788f61df95a1d4a126d95b8716637ba1bbbdee`.

[Referência terrestre](sector-runtime-h0-terrestrial-reference-2026-11-10.md) · [Roadmap](sector-runtime/roadmap.md) · [Progressão](sector-runtime/progress.md)

## Estado: em validação

`WorldGenerationRegionSelectorContext` compõe a captura de região já existente
com identidade por admissão e reserva de bytes no ledger antes de qualquer
chamada ao RNG global. Identidade inválida ou admissão recusada devolve
`nullopt` sem consumir seed. Após a reserva, a factory reutiliza
`WorldGenerationRegionSelectorSnapshots::capture`, preservando o construtor
default legado e as seeds derivadas. O contexto é move-only, materializa
parâmetros privados e libera a reserva ao destruir.

Não é layout/bioma/assets, lease de leitura, ticket BaseTiles, autorização de
commit, nem adoção nos jobs normais. A classe de memória usada é
`SnapshotMetadataEstimate` para o tamanho POD retido; polígonos, Json, Perlin,
materiais e caches mutáveis permanecem fora do escopo. Nenhuma cópia/RNG de
produção foi alterada.

Auditoria de defaults herdados (registrada, não corrigida):

1. `TerrainSelectorParameters` default inicializa seed/worldWidth/commonality e
   deixa `baseHeight` indefinido até atribuição do dono.
2. `WorldTile` default/cópia omitem `biomeTransition`; BaseTiles sobrescreve o
   campo em tiles válidos.
3. `SkyParameters()` em LookupDefaults omite `horizonClouds`; fixtures devem
   atribuir antes de ler/copiar.
4. Inspeção: `WorldRegion()` não inicializa `regionLiquids` (corpo em
   `StarWorldLayoutSampling.cpp`); testes de contrato não linkam esse objeto
   nesta etapa.

## Validação

Build local GCC 13/C++17, ASan/UBSan, GUI/Tracy OFF, cache
`build/selector-context-h0`, identifier `d5788f61-selector-context-local`.
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1` e `ASAN_OPTIONS=halt_on_error=1`.

1. **772/772 casos padrão**, 42 suites (8 novos nas suites de contexto e
   auditoria). Três microexperimentos DISABLED não executados.
2. Os 8 casos novos passaram em **10 repetições** com shuffle e seed inicial
   1250.
3. CTest dos grupos `WorldGenerationSelectorSnapshotTest`,
   `WorldGenerationSelectorContextTest` e
   `WorldGenerationDefaultInitializationAuditTest`: 100% aprovados.
4. Sem Root opt-in, jogo completo, Actions, benchmark de setores/s,
   mods/saves/multiplayer ou outras plataformas nesta entrega.

CPU-01/H0/RAM-01 permanecem em **25%**; programa **7/196 = 3,6%**. A admissão
cobre só o POD dos snapshots de parâmetros; ownership/contexto completo,
assets/mods representativos e sombra por setor seguem pendentes. Fallback CPU e
compatibilidade continuam gates; NPC externo suspenso; Linux Clang adiado.

## Próximo passo

Compor esta captura com inputs estáticos/custom terrain e identidade de
BaseTiles sob o mesmo ledger; comparar materialização privada contra o builder
real com fixtures representativas antes de jobs em sombra. Não normalizar
`biomeTransition`/`horizonClouds`/`regionLiquids` sem decisão de
compatibilidade. Não disparar Actions.

## Fontes locais de 26/11/2026 UTC

| Arquivo | Bytes | SHA-256 |
| --- | ---: | --- |
| `source/game/StarWorldGenerationSelectorContext.hpp` | 4640 | `e1240a38e7ccd6bc915d62c6c4d185d696abd83f644d822938c498aadee964de` |
| `source/test/world_generation_selector_context_test.cpp` | 7531 | `b2186a04fd49dfabf8f120099d8f205a623b9b0cb45dea7c613eaae7cb81c475` |
| `source/test/CMakeLists.txt` | (registro) | `864219190e8dd005b904eba59eb978952d72716997d59878acbe90037d2ec733` |
