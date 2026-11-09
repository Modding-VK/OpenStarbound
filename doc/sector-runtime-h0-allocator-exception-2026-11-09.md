# H0 / RAM-01: recuperação do pool após falha de inserção

Data: 09/11/2026. Entrega: `Experimental`; revisão local:
`codex/experimental-regional-runtime`. Base: `7f5b93b1222ea882ae8d105426fe9c86f93dd29a`.
Implementação: `1bd348e6aca350ff5b4ba302a2d7df5265c80bf2`.

[Programa principal](sector-runtime/README.md) · [Roadmap](sector-runtime/roadmap.md)
· [Memória](sector-runtime/memory-and-prefetch.md)
· [Relatório anterior](sector-runtime-h0-worker-cache-2026-11-09.md)

## Objetivo e resultado

Reproduzir e corrigir o risco de ponteiro pendente identificado por inspeção
no crescimento do vetor de blocos do `BlockAllocator`, antes de ensaios de
pressão de memória. A reprodução controlada confirmou a seleção de um bloco
fora do conjunto de blocos possuídos após exceção. O ponteiro agora só é
publicado quando a inserção do seu proprietário termina com sucesso.

A correção recupera o pool nesse caminho de falha, preservando as alocações
anteriores e permitindo novas tentativas. Não amplia admissão, orçamento,
workers ou cobertura de RAM. Não há ganho de throughput medido, nem evidência
de que esta falha tenha causado algum crash anterior enviado pelo usuário.

## Causa e correção

O caminho `allocate(1)` cria um `unique_ptr<Block>` quando todos os blocos
existentes estão cheios. Antes, atribuía `unfilledBlock` ao bloco novo **antes**
de inserir seu `unique_ptr` no vetor. Uma exceção durante crescimento do vetor
destruía o proprietário local, mas deixava a seleção não nula. O próximo
`allocate(1)` podia tentar usar esse bloco já destruído.

Agora o endereço permanece local até a inserção bem-sucedida. Falha na criação
ou inserção deixa a seleção anterior intacta; nesse caminho ela era nula. Os
blocos previamente inseridos continuam possuídos, e a próxima chamada pode
tentar alocar novamente. Não é necessário apagar estado ou capturar/suprimir
a exceção do caller. O sucesso conserva a ordem anterior de busca/inserção.

## Reprodução e controle negativo

Extraída a etapa privada `addBlock(Producer, Inserter)`, usada pelo próprio
`allocate(1)`. Em produção recebe lambdas de tipos estáticos para as operações
normais `make_unique` e `vector::insert`. O adapter amigo
`BlockAllocatorFailureTestAccess` só é definido no arquivo de testes e injeta
exceção nessas duas fronteiras. Não há override de `new`, hook configurável,
callback indireto novo ou alteração de layout do allocator.

Primeiro foi compilado esse recorte mantendo a publicação na posição antiga.
Dois testes selecionados falharam, processo com **exit 1**:

- `InsertionFailureInEmptyReboundPoolDoesNotPublishFreedBlock`;
- `FullVectorInsertionFailurePreservesLiveValuesAndRetry`.

Ambos observaram `selectionIsOwned(pool) == false` após `std::bad_alloc`.
O primeiro usa o pool de um rebind cujo vetor tem capacidade zero; o segundo
preenche todos os slots de todos os blocos até o limite da capacidade do vetor.
Assim representam fronteiras onde a inserção precisaria de storage adicional.
Os testes interrompem antes de dereferenciar uma seleção não possuída: foi
reproduzido o **estado inválido**, não provocado um crash/use-after-free.

A injeção lança antes da inserção real; não intercepta a alocação interna da
STL nem força OOM do sistema operacional. Usa o bloco real, seu proprietário
real, a ordenação real e o mesmo helper de produção. Isso testa a garantia de
exceção nessa fronteira, não todas as possíveis falhas de memória do processo.

Identifier do controle negativo: `7f5b93b1-allocator-red-local-dirty`.
O header local antes da correção tinha 9.625 bytes e SHA-256
`94077e94f9f75a5e17415f07748c8bcb4902c12967e9746ec3ceb4e3b0817df3`.
Essa versão transitória não foi commitada. Para reconstruir o controle em
checkout descartável, conservar os testes e mover a atribuição da seleção
para antes de `insert` no helper. Não usar essa ordem no runtime entregue.

Depois da reprodução, movida a publicação para depois de `insert` e executados
os mesmos casos, agora aprovados, além da suite completa. A ordem corrigida
e os três arquivos de fonte constam no commit/manifesto deste relatório.

## Validação acelerada executada

Ambiente cloud conferido pela skill operacional, sem instalar pacotes.
Reutilizada a build headless GCC 14.2.0/C++17 Debug (`-Og`), ASan/UBSan,
`STAR_BUILD_GUI=OFF`, `STAR_ENABLE_TRACY=OFF`, allocators alternativos OFF.
C++: `-fsanitize=address,undefined -fno-omit-frame-pointer` e
`-DXXH_NO_INLINE_HINTS=1`; C mantém somente o workaround de xxHash.
Dependências pré-compiladas/C não têm a mesma cobertura dos sanitizers C++.
Identifier da validação corrigida: `7f5b93b1-allocator-fixed-local-dirty`.

Com `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1` e
`ASAN_OPTIONS=halt_on_error=1`, sem diagnóstico dos sanitizers:

1. Build CMake dos contratos: **144/144 testes** em onze suites aprovados,
   incluindo **oito novos** em `BlockAllocatorExceptionTest`.
2. Os oito novos, embaralhados com seed GTest `1212`, repetidos 100 vezes:
   **800 execuções adicionais** aprovadas, não 800 casos únicos.
3. CTest `^world_generation_contract_`: **11/11 grupos NoAssets**; repete os
   casos do binário, não acrescenta outros 144 casos distintos.
4. Objetos reais de `StarWorldGeneration.cpp` e `StarWorldTemplate.cpp`,
   mais dependências afetadas do core: compilados, sem linking do jogo completo.
5. Sintaxe/tipos de geração, template e teste novo com
   `STAR_ENABLE_TRACY`/`TRACY_ENABLE`, usando headers Tracy 0.11.2 existentes:
   aprovada; não é build/linking completa da variante Tracy.
6. `git diff --check` aprovado; 234 links locais em 21 documentos sem destino
   ausente, 13 módulos alcançáveis, 49 IDs únicos referenciados no roadmap e
   três fontes do manifesto conferidas em tamanho e SHA-256.

Os oito casos cobrem criação/inserção em pool vazio e cheio, valores vivos
preservados, retry pelo `allocate(1)` público, 50 inserções falhas consecutivas
sem acumular blocos/bytes, aliases compartilhados e outro tipo do rebind
intacto, exceção de outro tipo e cache retido entre jobs após unwinding.
O caso integrado mantém o ledger coerente após falha, reutiliza o pool no job
seguinte e termina com saldo zero depois da destruição do cache.

Reprodução positiva após configurar build equivalente:

```bash
cmake --build <build-dir> --target world_generation_contract_tests \
  game/CMakeFiles/star_game.dir/StarWorldGeneration.cpp.o \
  game/CMakeFiles/star_game.dir/StarWorldTemplate.cpp.o -j3
UBSAN_OPTIONS=halt_on_error=1 ASAN_OPTIONS=halt_on_error=1 \
  ./dist/world_generation_contract_tests --gtest_brief=1
UBSAN_OPTIONS=halt_on_error=1 ASAN_OPTIONS=halt_on_error=1 \
  ./dist/world_generation_contract_tests --gtest_filter='BlockAllocatorExceptionTest.*' \
  --gtest_repeat=100 --gtest_shuffle --gtest_random_seed=1212 --gtest_brief=1
UBSAN_OPTIONS=halt_on_error=1 ASAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir <build-dir> -R '^world_generation_contract_' --output-on-failure
```

## Compatibilidade e limites

Nenhum config, log ou Tracy novo foi fornecido/coletado ou reanalisado; nenhum
arquivo do usuário foi alterado. O cenário foi NoAssets com falhas controladas,
sem planeta real. Dados brutos e binários continuam fora do Git.

Sem nova chave, métrica ou teto. Mantidos API pública/layout/traits do
allocator, block size, política de cache, campos de contabilidade e caminhos
de fallback multi-elemento. Os comentários de rebind/igualdade foram alinhados
ao comportamento compartilhado já existente, sem trocar a implementação.
Não foram alterados seeds/RNG, TTL, níveis, Lua, save, rede ou commit serial.
Compatibilidade com mods/saves/multiplayer segue obrigatória e requer teste
integrado; testes puros não a demonstram sozinhos.

Ficam fora: OOM real/pressão sustentada, falhas em todas as outras operações
do allocator/containers, construção arbitrária de payloads, benchmark de
overhead/throughput, jogo/assets/mods, save/reload, multiplayer, TSan,
Windows/macOS/ARM, build completa ou Actions. A correção não transforma o
allocator em thread-safe; uso continua restrito à dona. O overflow da seed
automática em `StarRandom.cpp:197` permanece aberto em CPU-01.
NPC externo suspenso, Linux Clang adiado; sem release, merge em `main` ou
revisão independente nesta atividade.

## Manifesto e retomada

Fontes conferidas em 09/11/2026, bytes e SHA-256, correspondentes a `1bd348e6`:

| Arquivo | Bytes | SHA-256 |
| --- | ---: | --- |
| `source/core/StarBlockAllocator.hpp` | 9803 | `f6816a7e8a53d69cbfe5aa628f0a668337acfb2db1e634a8f29f41261e86fc34` |
| `source/test/block_allocator_exception_test.cpp` | 7769 | `b05898b8f21feb0c27c639be470b9656da22e8459b0426d7514d14037f147fb0` |
| `source/test/CMakeLists.txt` | 4923 | `bdc4481d4c937a970279a81300ec04e57201fc87ae2cc708de37a13f66e34c6e` |

Próximo trabalho executável, ainda em RAM-01/H0:

1. Cobrir reserva/crescimento de outputs variáveis e caminho síncrono, incluindo
   falha/cancelamento e retenção até consumo físico. Não confundir estimate
   após construir com reserva anterior à alocação.
2. Ampliar cobertura de layout/biomas/seletores e metadados excluídos, com
   storage compartilhado único e medição obrigatória independente de profiling
   antes de qualquer teto processual de admissão.
3. Manter MOD-01 explícito e ampliar VAL-01/02 para streaming real e outputs
   com assets/mods; medir overhead e ganho ponta a ponta. Ensaios de pressão
   continuam distintos da injeção de exceção validada aqui.

O achado específico do pool está corrigido/validado nesse escopo. RAM-01
completo, gates de H0, executor H1, nova WorldThread autoritativa e GPU
continuam pendentes. Não ampliar filas com a contabilidade parcial atual.
