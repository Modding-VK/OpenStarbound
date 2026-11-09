# H0 / RAM-01: reserva atômica opt-in do escopo contabilizado

Data: 09/11/2026. Entrega: `Experimental`; revisão local:
`codex/experimental-regional-runtime`. Base: `87f57217d9878154830f6d430d26131917a5dd31`.
Implementação: `4b5aed18a14e81a58f46e6b1d1028251a365b67b`.

[Programa](sector-runtime/README.md) · [Roadmap](sector-runtime/roadmap.md)
· [Progressão](sector-runtime/progress.md) · [Contrato](sector-runtime-memory-envelope.md)
· [Inventário](sector-runtime/memory-and-prefetch.md)
· [Etapa anterior](sector-runtime-h0-memory-envelope-2026-11-09.md)

Continuação: [reserva composta de BaseTiles](sector-runtime-h0-composite-memory-2026-11-09.md),
`dd59f3b4`. Os resultados/manifesto abaixo permanecem os desta etapa histórica.

## Objetivo e resultado

Remover a janela entre observar pressão e cobrar bytes, no escopo do ledger,
e preservar margem para dependências e overlap de realocações. O ledger opt-in
agora decide e cobra sob o mesmo mutex processual compartilhado pelos handles.

**O jogo mantém o singleton anterior, sem pressão ativada ou novo limite.**
Não há chave nova de config, provider processual configurável, classificação de
jobs reais por demanda, ajuste de workers/filas, cancelamento/evicção, TTL/níveis,
RNG/seleção, publicação, saves ou protocolo de mods alterados. Não é ganho de
setores/s demonstrado nem conclusão de RAM-01.

## Contrato e caminhos cobertos

A sobrecarga tipada do construtor recebe máximo legado e limites explícitos.
Ativada, valida resume < soft < hard <= maximumBytes antes de publicar o estado.
`withDemand` valida a demanda e retorna outra fachada do mesmo estado/limites/
histerese; não duplica a cota por mundo. Estados de ledgers independentes não
se fundem automaticamente. Tipos de pressão foram separados em header próprio
para evitar dependência circular, preservando seus nomes/API anteriores.

Sob o mutex existente:

1. reserve avalia a política sobre o uso atual e cobra antes de devolver o token.
2. resizeEstimate avalia somente o delta positivo e atualiza o mesmo token sob
   o mesmo lock. Recusa conserva o charge anterior. Shrink/igualdade não pedem
   nova admissão; destruição continua liberando normalmente.
3. reserveSharedJson avalia apenas novo backing. Owner já rastreado reutiliza
   o token mesmo sem headroom; conflito de modelo permanece recusado. A lógica
   de retirada de token expirado/último destrutor conserva identidade e mutex.
4. BufferAllocator reconstrói a fachada com a demanda original, reservando o
   request completo antes do raw allocator. O buffer velho continua cobrado
   enquanto o novo é admitido/alocado/copied; falha desfaz somente a parcela nova.

Visível e dependência podem usar headroom hard; especulação fica limitada ao
soft e à histerese já validada. Não se modifica o algoritmo da política. Os
tokens guardam a demanda de criação para seus crescimentos; copy/move/rebind e
propagação dos allocators não podem promover especulação inadvertidamente.
Demanda diferente entra na igualdade do allocator. Não há promoção de token
ou reconfiguração dinâmica do limite neste recorte.

O registro compartilhado Json só é cobrado após inserir sua entrada com sucesso;
requests de buffers são reservados antes do raw allocator. Tokens inativos
recusados não tentam obter o mutex em seu destrutor. Falha física de construção/
raw allocator/cópia mantém o unwind existente. Recusa de allocator continua
bad_alloc; não foi convertida em sucesso vazio ou retry automático de geração.

## Escopo, custos e observabilidade

Governa somente os bytes cobrados pelas 14 classes, incluindo estimativas e
suas exclusões. Não é teto de RSS, malloc overhead, Json/layout privado, decode,
estado autoritativo, GPU ou memória anterior à adoção. Coerência do envelope não
certifica precisão/completude do heap. Referências compartilhadas continuam
deduplicadas por owner; lifetimes físicos prevalecem sobre cancelamento lógico.

Ativada, a avaliação faz a conversão fixa das 14 classes sob o lock já existente.
Desativada, esse scan/check é evitado; o máximo legado ainda funciona. Houve
acréscimo de campos internos de demanda/diagnóstico; sizeof dos wrappers é o
compilado para esta build, não uma constante de capturas anteriores. Não altera
layout de dados persistidos nem acrescenta cópias de elementos para diagnóstico.

`snapshot().admission`, modelo **1**, expõe enabled, pressureChecks,
pressureRefusals e lastAdvice. Os contadores saturam e medem avaliações/refusas
da política, não falhas de malloc nem todos os motivos de recusa da API. LastAdvice
é pré-cobrança; não é um gauge recalculado ao liberar cada token. Não foram
adicionadas séries ao profiling do jogo para esses campos nesta entrega.

Modelos de envelope **1**, geral **9**, Json **2**, Json enfileirado **1**,
distribuições **3**, candidatos/temporários **2** e nomes/buffers/cache **1**
mantidos. Nenhuma classe, fórmula profunda ou margem de hardware foi criada.

## Validação executada

Ambiente cloud conferido pela skill operacional; sem instalar/baixar dependências.
GCC 14.2.0/C++17, Debug `-Og`, GUI/Tracy/allocators alternativos OFF,
`VCPKG_MANIFEST_INSTALL=OFF`. C++ com
`-fsanitize=address,undefined -fno-omit-frame-pointer -DXXH_NO_INLINE_HINTS=1`;
C/dependências pré-compiladas não têm a mesma instrumentação. Identifier:
`87f57217-atomic-memory-local-dirty`; fontes do commit/manifesto abaixo.

Com `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1` e
`ASAN_OPTIONS=halt_on_error=1`, sem diagnóstico dos sanitizers:

1. **388/388 testes**, 21 suites; **34 novos**, 26 de ledger/concorrência e oito
   de allocator/overlap/rollback.
2. Novos casos repetidos vinte vezes com shuffle e seed GTest 1222:
   **680 execuções adicionais**, não casos únicos.
3. **21/21 grupos CTest** `^world_generation_contract_`, NoAssets; repetem
   casos do binário, não outros 388 testes distintos.
4. Contratos e cinco objetos reais aprovados: geração, ledger, envelope,
   template e storage. Os dois últimos recompilados por headers, não editados.
   Sem linking completo do jogo.
5. Sintaxe/tipos de geração, ledger, envelope e dois testes aprovados com
   STAR_ENABLE_TRACY/TRACY_ENABLE e headers Tracy 0.11.2 existentes. Não é
   linking Tracy completo. Aviso herdado de fmt permanece.
6. `git diff --check` aprovado; 467 links locais em 35 documentos sem destino
   ausente, 14 módulos alcançáveis, 49 contratos únicos cobertos pelo roadmap,
   ordem 1–49 e oito totais de fase conferidos. Recalculados 6/196 marcos = 3,1%;
   RAM-01/H0 mantidos em 25%. Nove fontes conferidas em bytes/SHA-256.

Casos novos: defaults e teto legado, limites/demanda inválidos, fachadas/classes
compartilhadas, histerese, delta/shrink, resize concorrente, size_t máximo,
demanda do token, reuso/conflito/novos owners Json, capacidade zero, fechamento
de facade, construção lançante, oito disputantes pelo soft/hard, demandas mistas,
Json concorrente e envelope coerente. O caso de conselho obsoleto permite oito
conselhos positivos, mas somente uma reserva real de 150 num limite hard 200.

Allocators exercitam recusa antes do raw allocator, falha física injetada,
preservação do buffer antigo, overlap observado durante a cópia, cópia lançante
e retry, demanda em copy/move/rebind/propagação e cobrança antes da alocação.
Esses testes mostram margem para uma dependência e realocações declaradas;
não provam progresso de todo DAG, fairness ou ausência de OOM real. Suites
anteriores de bytes/seleção/colisões também passaram; ASan/UBSan não é TSan.

Reprodução, após configurar build equivalente, a partir da raiz:

```bash
cmake --build <build-dir> --target world_generation_contract_tests \
  game/CMakeFiles/star_game.dir/StarWorldGeneration.cpp.o \
  game/CMakeFiles/star_game.dir/StarWorldGenerationMemory.cpp.o \
  game/CMakeFiles/star_game.dir/StarWorldGenerationMemoryEnvelope.cpp.o \
  game/CMakeFiles/star_game.dir/StarWorldTemplate.cpp.o \
  game/CMakeFiles/star_game.dir/StarWorldStorage.cpp.o -j3
UBSAN_OPTIONS=halt_on_error=1 ASAN_OPTIONS=halt_on_error=1 \
  ./dist/world_generation_contract_tests --gtest_brief=1
UBSAN_OPTIONS=halt_on_error=1 ASAN_OPTIONS=halt_on_error=1 \
  ./dist/world_generation_contract_tests --gtest_filter='WorldGenerationMemoryAdmissionTest.*' \
  --gtest_repeat=20 --gtest_shuffle --gtest_random_seed=1222 --gtest_brief=1
UBSAN_OPTIONS=halt_on_error=1 ASAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir <build-dir> -R '^world_generation_contract_' --output-on-failure
```

## Limites, progresso e continuação

Nenhum novo config/log/Tracy do usuário ou captura anterior reanalisada; nenhum
arquivo do usuário alterado ou dado bruto no Git. Não executados jogo/assets/
mods, save/reload, multiplayer, TSan, OOM/pressão externa reais, benchmark de
overhead/throughput, Windows/macOS/ARM ou allocators alternativos. Sem Actions,
release, merge em main ou revisão independente.

RAM-01/H0 permanecem em **25%**, programa **3,1%**. O backend atômico é funcional
no escopo declarado e testado, mas não completa orçamento/integração processual,
qualidade do heap, limites de hardware ou outro marco cumulativo integral.

Próximo: contrato de demanda por produtor e reserva composta antes do dispatch,
incluindo inputs/saída/overlap e rollback parcial. Provar progresso quando
resultados dependem de mais memória, não apenas disponibilidade soft→hard.
Depois, provider processual único configurável, quotas/fairness entre mundos,
promoção de demanda e pressão externa/limites do usuário. Não ativar orçamento
do jogo sem esses gates e MOD-01/VAL-02/baseline integrado. H1 pode começar em
sombra com escopo delimitado, sem exigir contabilizar todo heap do motor.

Prioridade: setores comuns e uso útil de CPU/RAM/GPU sem modelo de CPU fixo,
preservando mods/saves/multiplayer e fallback CPU. NPC externo suspenso e Linux
Clang adiado; seed automática/Perlin Uninitialized continuam pendências CPU-01.

## Manifesto das fontes

Fontes locais de 09/11/2026, nomes relativos ao checkout, bytes e SHA-256.
Nenhum novo artefato de execução do usuário nesta etapa.

| Arquivo | Bytes | SHA-256 |
| --- | ---: | --- |
| `source/game/CMakeLists.txt` | 16591 | `ca0929466eb74545b4c7fbf04a58cb3ae38dc47b66f1b0cba32e1b21a88a84e5` |
| `source/game/StarWorldGenerationBufferAllocator.hpp` | 5048 | `f0e492d102899bccab92d766df104f068550eed452e5e8e2386a10b9577ea343` |
| `source/game/StarWorldGenerationMemory.cpp` | 9668 | `4b62e7bf38570d0a9cabcdfd570396857a72d2053114e27864f3062cef7fe7c2` |
| `source/game/StarWorldGenerationMemory.hpp` | 5639 | `cd2c3157a818cd3d6e22ca50df90cad626c6193c057afe9cc8f23dba55a1ea47` |
| `source/game/StarWorldGenerationMemoryEnvelope.hpp` | 1474 | `5342852c3f881dc60f0c3eedc388b077f45839ca36098fec52fb17ac36c75961` |
| `source/game/StarWorldGenerationMemoryPressure.hpp` | 1884 | `482526bfd7ebfddd945efa91bee63f0774a910b5562f9d013f9b6726f293f739` |
| `source/test/CMakeLists.txt` | 5780 | `d4a68758784e3c9e0e5c9c81f0cad70f6863ed9ec385f8070b196fa6921a628b` |
| `source/test/world_generation_buffer_test.cpp` | 23116 | `f9d1ddf398af9ebeb3c5079a94cd61ce2556779ba610ca610fe091352767aa31` |
| `source/test/world_generation_memory_admission_test.cpp` | 15496 | `7ebb2c663c34bda3c82523f8a85dc040dfa25d909cb3c9e3e5a93dd9f3386c73` |
