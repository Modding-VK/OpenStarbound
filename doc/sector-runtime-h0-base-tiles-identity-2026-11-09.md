# H0: identidade de BaseTiles e comparador dos campos completos

Data: 09/11/2026. Entrega: `Experimental`; revisão local:
`codex/experimental-regional-runtime`. Base: `d599754dba73838cf0f59573bf41ee054e1cca6e`.
Implementação: `f7785963cf70efa6d8703807e44ceaba2da3fc7c`.

[Programa](sector-runtime/README.md) · [Roadmap](sector-runtime/roadmap.md)
· [Progressão](sector-runtime/progress.md)
· [Contrato BaseTiles](sector-runtime-base-tiles-contract.md)
· [Memória](sector-runtime-memory-envelope.md)
· [Etapa anterior](sector-runtime-h0-capacity-kernel-2026-11-09.md)

Continuação vigente: [inputs escalares e consultas BaseTiles](sector-runtime-h0-base-tile-inputs-2026-11-09.md).
Este relatório preserva evidências/manifesto da entrega `f7785963`; o próximo
trabalho e a validação atuais estão na continuação e no roadmap.

## Objetivo e resultado

Vincular cada resultado BaseTiles à admissão/contexto esperado e permitir comparar
os campos dos ServerTile reais sem confundir estado com padding, serialização ou
normalização de cache. Não transformar identidade de task em autorização de commit.

O fluxo real agora captura/devolve WorldGenerationBaseTilesIdentity, modelo e
produto **1**: task, revisões de tiles/template, tamanho do mundo, bounds exatos
do setor e versões de produto/kernel escalar. Commit confere essa identidade e
conserva os guards anteriores de task/epoch, array físico, template e revisões.
Unload continua anulando o vínculo ao array e invalidando a task. Não houve
mudança de ordem, TTL, níveis, pré-requisitos, workers, filas ou publicação serial.

Mismatch de resultado/contexto soma o diagnóstico
`generation.baseTiles.staleProductIdentity` quando profiling está ativo; segue
o descarte/reagendamento stale existente. Não há nova chave de config ou execução
do comparador completo no hot path. O contador é contrato de código, não evento
observado em nova run do usuário. Pending/resultado/capture carregam mais metadata;
sizeof(PendingBaseTiles) continua alimentando a estimativa existente por build.

**Ticket não é hash de conteúdo, contexto puro completo ou cache key reutilizável.**
Não contém os bytes iniciais, seed/Json profundo, todas as gerações de assets ou
materiais. Mesmo ticket não comprova payload igual. Não se removeu ponteiro/revisão
porque a igualdade do ticket passou. Retry completo continua sem consumidor real,
pressão padrão desligada e nenhuma ampliação de orçamento/defaults.

## Comparador e rotinas puras de teste

Comparador modelo **1**, descrito no contrato: 25 bits cobrem materiais/hues/mods/
cores, biomas, colisões/cache ativo, dano completo, dungeon/root e líquido. Floats
comparam bits, incluindo zero assinado/NaN; sem memcmp de classes/structs, codec,
alocação ou cópia de ServerTile. Damage inclui tempo privado de efeito e campo
derivado que o save de tile não conserva. Capacidades, padding e slots inativos
ficam fora. Modos Exact e IgnoreCollisionCache são explícitos; o segundo ignora
só dirty/geometria, não collision kinds ou dados. Modo desconhecido falha fechado.

Array comparison exige identidade válida e só visita bounds válidos, recortados
na borda do mundo. Devolve contagens, máscara e primeiro local diferente em ordem
x→y. Não mede setores entregues/s e não é comparação de terreno integrado.

Para construir tiles reais sem ligar Root/assets no alvo NoAssets, constructor e
reset de TileDamageStatus foram movidos **sem alterar os corpos** de StarTileDamage.cpp
para StarTileDamageState.cpp, registrado nos dois alvos. Dano/recovery/codec não
mudaram. O acesso privado do comparador é somente de leitura; o friend de teste
exercita cada parcela. A unidade StarWorldTiles.cpp existente é ligada aos testes.

## Achado herdado: biomeTransition e cache de cópia

O construtor WorldTile e seu operador de cópia não inicializam/copiam
biomeTransition; ServerTile copia por esse operador. BaseTiles sobrescreve o
campo nos tiles válidos, mas isso não prova inicialização de todo snapshot do
motor. Cópia também invalida/não conserva o cache de colisão. Portanto fixtures
preenchem os campos explicitamente e um teste da normalização de cache inicializa
o flag antes de comparar, sem ler bool indefinido. Células físicas fora da borda
parcial não são lidas.

Não corrigido neste recorte: alterar cópia pode afetar consumidores/compatibilidade
e precisa de auditoria própria. Não é causa estabelecida dos crashes anteriores.
ASan/UBSan não substituem MSan. O comparador exige campos/entries ativas inicializados;
não deve ser chamado indiscriminadamente sobre tiles default ou snapshots parciais.

## Validação executada

Skill cloud usada para conferir executor/build/rede; sem instalar/baixar dependências.
GCC 14.2.0/C++17, Debug `-Og`, GUI/Tracy/allocators alternativos OFF,
`VCPKG_MANIFEST_INSTALL=OFF`. C++ com
`-fsanitize=address,undefined -fno-omit-frame-pointer -DXXH_NO_INLINE_HINTS=1`;
C/dependências pré-compiladas não têm a mesma instrumentação. Identifier:
`d599754d-base-tiles-identity-local-dirty`; fontes no manifesto abaixo.

Com `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1` e
`ASAN_OPTIONS=halt_on_error=1`, sem diagnóstico dos sanitizers:

1. **524/524 testes**, 26 suites; **40 novos** de identidade/comparação.
2. Os 40 repetidos vinte vezes, shuffle e seed GTest 1226: **800 execuções
   adicionais**, não casos únicos.
3. **26/26 grupos CTest** `^world_generation_contract_`, NoAssets; repetem
   os casos do binário, não outros 524 casos distintos.
4. Contratos e **sete objetos reais** aprovados: geração, produto/comparador,
   TileDamage, TileDamageState, WorldTiles, WorldTemplate e WorldStorage.
   Últimos três não editados; recompilados por dependências. Link do alvo
   NoAssets aprovado, não linking completo do jogo.
5. Sintaxe/tipos de geração, produto/comparador, TileDamage, TileDamageState,
   WorldTiles e teste novo aprovados com STAR_ENABLE_TRACY/TRACY_ENABLE e headers
   Tracy 0.11.2 existentes. Não é linking Tracy; aviso herdado de fmt permanece.
6. `git diff --check` aprovado; **557 links locais em 42 documentos**, 14 módulos
   alcançáveis, 49 contratos únicos cobertos pelo roadmap e ordem 1–49 conferidos.
   Oito totais de fase e 6/196 marcos = 3,1% recalculados, RAM-01/H0 mantidos em
   25%. Doze fontes do manifesto conferidas em bytes/SHA-256.

Casos: defaults inválidos, bounds/edge/overflow de coordenada, epoch/sequence/
stage/region/versões, readmissão de task, echo do worker, 21 campos públicos com
bits separados, dano privado/derivado, todos os campos de geometria ativa,
NaN/zero assinado, root opcional, modes, arrays 32×32 e borda 3×1, primeira
diferença x→y, Keep/líquido anterior e adapter escalar vs lógica anterior com
restante do tile preservado. Resultado tagged stale continua cobrando o array
enquanto promise/owner o retêm. Não executa WorldTemplate/terreno com assets.

Reprodução após build equivalente na raiz, incluindo os sete objetos acima:

```bash
cmake --build <build-dir> --target world_generation_contract_tests -j3
UBSAN_OPTIONS=halt_on_error=1 ASAN_OPTIONS=halt_on_error=1 \
  ./dist/world_generation_contract_tests --gtest_brief=1
UBSAN_OPTIONS=halt_on_error=1 ASAN_OPTIONS=halt_on_error=1 \
  ./dist/world_generation_contract_tests --gtest_filter='WorldGenerationBaseTilesTest.*' \
  --gtest_repeat=20 --gtest_shuffle --gtest_random_seed=1226 --gtest_brief=1
UBSAN_OPTIONS=halt_on_error=1 ASAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir <build-dir> -R '^world_generation_contract_' --output-on-failure
```

## Continuação e limites

Sem novo config/log/Tracy do usuário ou captura anterior reanalisada; dados brutos
fora do Git. Sem jogo/assets/mods, save/reload, multiplayer, TSan/MSan, OOM/pressão
externa, benchmark de overhead/throughput, Windows/macOS/ARM ou allocators alternativos
executados. Sem Actions, release, merge em main ou revisão independente. Sem causa
de crash atribuída, RNG/cópias de elementos alterados ou escrita paralela no mundo.
NPC externo suspenso e Linux Clang adiado; seed automática/Perlin de CPU-01 abertos.

RAM-01/H0 permanecem **25%**, programa **3,1%**, 6/196 marcos. Identidade e comparador
apoiam DAG-01/MOD-01/VAL-02, sem completar contratos ou conceder marco a PreparedSector.
Memória mantém as 14 classes/envelope/admissão 1, retry 2, sinal 1 e fórmulas anteriores.
Não há ganho de setores/s demonstrado.

Próximo recorte executável: adaptar materiais/mods/biomas para inputs explícitos e
comparar os outputs do produtor sem antecipar consultas ou mudar cópias/RNG. Auditar
biomeTransition/default/cópias antes de normalizar snapshots. Contexto profundo e
versões de assets/materiais, demanda ligada ao interesse, retry/cache de produtos,
mailbox/DAG em sombra, provider único/fairness e baseline integrado continuam gates.
Não exigir medir cada heap do motor antes de um protótipo H1 delimitado; preservar
mods/saves/multiplayer, fallback CPU e publicação serial.

## Manifesto das fontes

Fontes locais de 09/11/2026, nomes relativos ao checkout, bytes e SHA-256.
Dez fontes alteradas e duas referências herdadas de tiles; nenhum novo artefato
de execução do usuário nesta etapa.

| Arquivo | Bytes | SHA-256 |
| --- | ---: | --- |
| `source/game/CMakeLists.txt` | 16774 | `acc803d1f76c3cdf06fc3136b07da9457a9264543aad4b88b09412222ce18705` |
| `source/game/StarTileDamage.cpp` | 10421 | `0833eba9e9fd7f400cd90cdfa15caf36606f0b34156493851b53b56c151d88c0` |
| `source/game/StarTileDamage.hpp` | 4656 | `8e507d580e621908a44255590146a2ca48fb2c8ab22fffe3a85047e8000cd4ab` |
| `source/game/StarTileDamageState.cpp` | 500 | `8e53bba99dd8656de535914f79d039437e24a4758019de29dbce4e423dc4f06b` |
| `source/game/StarWorldGeneration.cpp` | 129614 | `c3ed3d8bb7470897cd3b35d28d56e5ababc87fd1200915e06982f626195cd67c` |
| `source/game/StarWorldGeneration.hpp` | 14207 | `3b8a4247475c2fdf4ecefc2b903fc43cc9ca4f603007497558e07b0be4121ebf` |
| `source/game/StarWorldGenerationBaseTiles.cpp` | 6876 | `f2c8ea7e61a402798a74a0d5cb86928b6eb0e2d183bed5306eb25ac11db76c32` |
| `source/game/StarWorldGenerationBaseTiles.hpp` | 3160 | `c2da627974af26ef58b391f028ed83c473de33f0ad493aae1388e3a844683f6d` |
| `source/test/CMakeLists.txt` | 6305 | `4365d07e0ba0a83955d065a16e0834e53643270e9b83ebeff932832712e9dd2f` |
| `source/test/world_generation_base_tiles_test.cpp` | 20367 | `1b3827a7bcbc5e48dd4a7e9b63ff9122ecfe28aca5f10742df9016e6a591e2d0` |
| `source/game/StarWorldTiles.hpp` | 12046 | `a66b0032895d3ca3fa16ea2ab908c32920b70b8b24fa9a7d4b09fbbc34620044` |
| `source/game/StarWorldTiles.cpp` | 7758 | `4f46c02b7284770481ccc6f9ab6af3b207bc265006d2e8ba00d54fd1d97c8ba2` |
