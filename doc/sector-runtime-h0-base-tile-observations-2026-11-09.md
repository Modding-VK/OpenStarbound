# H0: observações versionadas e replay protegido de BaseTiles

Data: 09/11/2026. Entrega: `Experimental`; revisão local:
`codex/experimental-regional-runtime`. Base: `966913d005a251831954405c5d95d7b123d30b2d`.
Implementação: `abfc383462ed07dfc62e6bdd2ec40b77dd0473a4`.

[Programa](sector-runtime/README.md) · [Roadmap](sector-runtime/roadmap.md)
· [Progressão](sector-runtime/progress.md)
· [Contrato BaseTiles](sector-runtime-base-tiles-contract.md)
· [Memória](sector-runtime-memory-envelope.md)
· [Etapa anterior](sector-runtime-h0-base-tile-inputs-2026-11-09.md)

Continuação vigente: [catálogo compartilhado de consultas](sector-runtime-h0-base-tile-lookup-context-2026-11-09.md).
Este relatório preserva evidências/manifesto de `abfc3834`; estado e próximo
trabalho atuais estão na continuação e no roadmap.

## Objetivo, entrega e decisão

Congelar resultados das consultas do adaptador BaseTiles para comparar aplicação
independente, sem resolver vivo, a partir dos dados originais. Foi entregue uma
**API diagnóstica explícita**, header-only: observeWorldGenerationBaseTile captura
e replayWorldGenerationBaseTile valida/aplica. Os caminhos normais do produtor
**não chamam essa API**. Não há nova flag/config, captura automática, fila, cache,
retentativa de tarefa ou mudança na geração/publicação real.

WorldGenerationBaseTileObservation, modelo **1**, guarda três versões
(observação/input/líquido), input escalar, 14 campos do estado inicial relevante,
até seis consultas com tipo/chaves/valor retornado, contagem e complete. Não possui
ponteiros, Root/template, RNG, Json, callbacks, heap próprio ou exceções retidas.
Não copia WorldTile/ServerTile nem normaliza colisão/cache.

**Decisão baseada no recorte:** manter a fita fora do hot path. Ela serve como
referência de equivalência/controle, não como implementação rápida de cache por
bloco. O microexperimento abaixo mostra custo adicional com resolver trivial e
176 bytes por captura nesta ABI. O próximo trabalho é um contexto compartilhado
e compacto de consultas, com vínculo explícito a revisão/assets; ainda não o
contexto puro completo de blockInfo/WorldTemplate ou PreparedSector.

## Captura, falhas e replay

1. A captura copia o input escalar por valor e invalida o registro anterior antes
   do trabalho. Passar observation.input como argumento não perde o input ao
   resetar o registro. Cada consulta chama o resolver original na posição atual
   do adaptador; somente depois de retornar registra chave/valor em storage fixo.
2. As escritas foreground→background→líquido e consultas mantêm ordem/argumentos,
   inclusive NoMod. Exceção propaga sem conversão: mantém o mesmo tile parcial e
   só as consultas já retornadas, complete=false. Nenhuma falha fica replayable.
   O limite fixo também impede aceitar overflow de observações.
3. Replay primeiro verifica **todo** o registro: complete, três versões, contagem,
   sequência condicional exata, tipo/argumentos de cada consulta e limites dos
   valores (hue 0–255, suporte 0–1). Não basta validar enquanto já escreve: uma
   divergência na última consulta também precisa recusar antes da primeira escrita.
4. Confere os 11 escalares do input e os 14 campos iniciais: cinco campos por
   camada e id/level/pressure/source do líquido. Floats comparam bits, preservando
   zero assinado e payload NaN. Camada parcialmente escrita é recusada, inclusive
   quando o material proposto continua Null mas a cor já mudou antes da exceção.
5. Só após validar cria um resolver de cursor local que devolve os valores
   congelados ao adaptador existente. Não consulta assets/template ou faz alocação.
   Cada chamada tem cursor próprio; observação imutável pode servir tiles privados
   em paralelo. O chamador deve manter observação/input estáveis e tile exclusivo.

Os campos do guard exigem inicialização. Biomas antigos **não são lidos** porque
o adaptador os sobrescreve antes de qualquer consulta; isso evita depender de
biomeTransition herdado não inicializado/copiado. Dano, dungeon, root, colisões e
cache passam intactos e não integram o guard. Mudanças nesses campos são permitidas
e preservadas; portanto o guard não comprova identidade de um tile inteiro.

Registro válido **não é hash de conteúdo, selo de integridade, geração de assets,
autoridade de commit ou permissão de replay do produtor completo**. Ele comprova
estrutura/compatibilidade local das observações, não que valores públicos não
tenham sido alterados ou que assets atuais produziriam os mesmos valores. Não é
formato serializado ou parser de dados não confiáveis. Revisões profundas, demanda,
lifetime/admissão e guards vivos continuam obrigatórios antes de integrar tarefas.
O domínio aritmético do líquido continua o herdado: diferença de níveis cabe em int.

## Validação executada

Ambiente cloud conferido pela skill: executor pronto, política aplicada; cache
e dependências existentes reutilizados, sem instalação/download novo. Mesma
configuração do relatório anterior: GCC 14.2.0/C++17, CMake 3.29.2/Ninja 1.13,
Debug -Og, GUI/Tracy/allocators alternativos OFF, BUILD_TESTING=ON e
VCPKG_MANIFEST_INSTALL=OFF. Cache `/tmp/sector-input-h0.RX3XLi`, identifier
`966913d0-base-tile-observations-local-dirty`. Hook SDL3/sysroot externo ao Git
permanece conforme a etapa anterior; nenhuma dependência de projeto alterada.

C++ com -fsanitize=address,undefined -fno-omit-frame-pointer e XXH_NO_INLINE_HINTS=1;
C/dependências pré-compiladas não têm a mesma instrumentação. Todas as execuções
abaixo usam UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 e
ASAN_OPTIONS=halt_on_error=1, sem diagnóstico dos sanitizers:

1. **575/575 testes**, 28 suites: **27 novos** normativos. O microexperimento
   separado fica DISABLED por padrão e não é contado nesses 575.
2. **91 casos afetados × 20**, shuffle/seed GTest 1228: **1.820 execuções
   adicionais**, não casos únicos.
3. **28/28 grupos CTest** `^world_generation_contract_`, NoAssets, repetindo os
   casos normativos do mesmo binário. O experimento não é executado pelo CTest.
4. Build/link NoAssets aprovados; atualização de StarVersion e do teste BaseTiles.
   Build dos sete objetos reais (geração, produto/comparador, TileDamage,
   TileDamageState, WorldTiles, WorldTemplate e WorldStorage) conferido sem trabalho
   pendente; não foram todos forçados a recompilar nesta entrega. Não é link do jogo.
5. Sintaxe/tipos de geração, produto/comparador e teste BaseTiles aprovados com
   STAR_ENABLE_TRACY/TRACY_ENABLE. Headers oficiais já existentes de Tracy 0.11.2,
   commit b8c2e25c3d02009059e09f623072921677dd6bb4, não linking Tracy. Aviso herdado
   de fmt permanece; nenhuma implementação externa incorporada.
6. `git diff --check` aprovado; **587 links locais em 44 documentos**, 14 módulos
   alcançáveis, 49 contratos únicos cobertos no roadmap e ordem 1–49 conferidos.
   Oito totais de fase e 6/196 marcos = 3,1% recalculados; RAM-01/H0 mantidos em
   25%. Quatro fontes do manifesto conferidas em bytes/SHA-256.

Cobertura: matriz de 4.096 branches compara captura/replay/corpo anterior
independente do projeto com campos completos e trace observado. Seis pontos de
exceção preservam estado parcial e invalidam captura. Cada input/campo relevante,
versão, tipo/argumento e faixa de valor tem controle de recusa sem escrita; inclui
truncamento/excesso/reordenação da fita. NoMod, mods rejeitados, camadas existentes,
líquido/float bits, lifetime do resolver e chaves iguais com respostas diferentes
por posição são exercitados. Uma observação serve 1.024 tiles em quatro workers
com cursores independentes; não são quatro templates reais calculados em paralelo.

Reprodução, após configurar build equivalente conforme o relatório anterior:

```bash
cmake --build <build-dir> --target world_generation_contract_tests -j3
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ASAN_OPTIONS=halt_on_error=1 \
  ./dist/world_generation_contract_tests --gtest_brief=1
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ASAN_OPTIONS=halt_on_error=1 \
  ./dist/world_generation_contract_tests \
  --gtest_filter='WorldGenerationBaseTilesTest.*:WorldGenerationBaseTileInputsTest.*:WorldGenerationBaseTileObservationTest.*' \
  --gtest_repeat=20 --gtest_shuffle --gtest_random_seed=1228 --gtest_brief=1
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ASAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir <build-dir> -R '^world_generation_contract_' --output-on-failure
```

## Microcusto sintético: resultado e fronteiras

Experimento opt-in executado separadamente e aprovado com checksum: 8.192 tiles,
nove rodadas por modo, ordem rotativa. Alocação/inicialização ficam fora do trecho
medido. Resolver de escalares constantes; modo replay inclui acumular seu booleano
de sucesso. Mesmo binário Debug/ASan/UBSan do executor, sem gerar blockInfo, assets,
IO, fila, simulação ou publicar setores. Não há afirmação de custo Release, no
hardware do usuário, de ganho com resolver caro ou de throughput do jogo.

| Modo | Mediana ns/tile | Mínimo | Máximo |
| --- | ---: | ---: | ---: |
| Aplicação direta do adaptador | 152,37 | 117,54 | 623,28 |
| Aplicação com captura | 565,16 | 490,99 | 692,71 |
| Replay com preflight completo | 703,88 | 532,91 | 1.125,82 |

sizeof(observação) = **176 bytes** nesta ABI; 1.024 registros seriam **180.224
bytes (176 KiB)** por setor, sem contar origem/output/containers. Não foram
retidos no runtime nem incluídos implicitamente no ledger. Uma futura integração
deve reservar/contabilizar seu escopo antes de alocar; não ampliar filas com esse
overhead excluído. Não promover esta fita a produto residente padrão.

```bash
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ASAN_OPTIONS=halt_on_error=1 \
  ./dist/world_generation_contract_tests --gtest_also_run_disabled_tests \
  --gtest_filter='WorldGenerationBaseTileObservationTest.DISABLED_SyntheticObservationCosts' \
  --gtest_brief=1
```

## Continuação, compatibilidade e progresso

Próximo recorte executável: contexto imutável **compartilhado e compacto** de
consultas por chaves/versão explícita, usando a fita como referência, não como
cache por tile. Definir vínculo a revisão/assets e recusa antes da aplicação
quando uma chave/contexto não estiver coberto; comparar outputs/ordem/exceções
e bytes/custo contra a referência antes de um protótipo em sombra por setor.
Não antecipar consultas no síncrono, assumir resolver arbitrário puro ou converter
falhas em fallback depois de escrever. Fechamento profundo de blockInfo/template,
demanda, mailbox/DAG, provider/fairness e streaming integrado continuam gates.

RAM-01/H0 seguem **25%**, programa **3,1%**, 6/196 marcos dos 49 contratos.
Sem ganho de setores/s demonstrado ou novo marco por quantidade de testes.
Modelos input/líquido/ticket/produto/comparador/envelope/admissão continuam 1;
retry 2, sinal 1 e estimativa geral 9 inalterados. Nenhum limite/default, RNG,
cópia de tile, API Lua, save ou regra de publicação/commit foi modificado.

Sem novos config/log/Tracy do usuário, reanálise de capturas, jogo/assets/mods,
save/reload, multiplayer, TSan/MSan, OOM/pressão externa ou build Windows/macOS/
ARM/allocators alternativos. Achados biomeTransition, seed automática e Perlin
seguem abertos, sem causa de crash atribuída. Sem Actions, release, main ou
revisão independente. Compatibilidade com mods/saves/multiplayer e fallback CPU
completo permanecem gates; NPC externo suspenso e Linux Clang adiado. Dados brutos fora do Git.

## Manifesto das fontes

Fontes locais de 09/11/2026, nomes relativos ao checkout, bytes/SHA-256. Quatro
fontes alteradas; nenhum artefato novo do usuário recebido nesta entrega.

| Arquivo | Bytes | SHA-256 |
| --- | ---: | --- |
| `source/game/CMakeLists.txt` | 16862 | `51ab99bfa039df594c652f18995076c54a50da87547d0d0b546311f61aa49eb8` |
| `source/game/StarWorldGenerationBaseTileObservation.hpp` | 7262 | `0ab7ac774bc18b17ef0a6a9f17abcd8ea3096299161c85db386dbf9ca745474d` |
| `source/test/CMakeLists.txt` | 6386 | `94e1ac9fd919867e90732e777446062ce4ffd4638f07e82f50ce558e8efef199` |
| `source/test/world_generation_base_tiles_test.cpp` | 57675 | `fd1cb8042701aaaf249225c207d3d4e2eada8063fb46a23362926b85664e52c7` |
