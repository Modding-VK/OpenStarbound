# H0 / RAM-01: saída variável e retomada sem bloquear workers

Data: 09/11/2026. Entrega: `Experimental`; revisão local:
`codex/experimental-regional-runtime`. Base: `60d4fdc8b635dca962c6bd0a9474aa3a3bd0a37f`.
Implementação: `8b68ab6ae03984cf24e9a1c251023767472f109c`.

[Programa](sector-runtime/README.md) · [Roadmap](sector-runtime/roadmap.md)
· [Progressão](sector-runtime/progress.md) · [Contrato](sector-runtime-memory-envelope.md)
· [Inventário](sector-runtime/memory-and-prefetch.md)
· [Etapa anterior](sector-runtime-h0-composite-memory-2026-11-09.md)

## Objetivo e resultado

Tratar uma previsão insuficiente sem deixar workers segurando input enquanto
esperam por bytes para a saída. O recorte entrega resposta tipada de admissão,
protocolo owner-thread limitado de tentativas e produtores puros controlados
com dois workers/resultados retidos. Não transforma falha física em retry.

**O controle de retry não é chamado pela geração real.** O ledger real passa
a classificar a resposta do batch; a API anterior continua retornando o mesmo
optional, descartando o status adicional. BaseTiles continua com a reserva
composta anterior; não ganhou replay automático ou garantia de crescimento do
template. Singleton com pressão desligada, sem config, limites, workers, filas,
TTL, níveis, pré-requisitos, orçamento de update ou publicação alterados.

## Resposta atômica de admissão

`tryReserveBatch` devolve status, requestedBytes e optional de tokens da mesma
admissão. Não consulta lastAdvice depois de soltar o mutex para decidir o status.

| Status | Significado e reação |
| --- | --- |
| Admitted | Todas as parcelas reservadas; único caso que devolve tokens |
| Deferred | Pedido pode caber no escopo vazio, mas não nas condições atuais; drenar/liberar trabalho existente antes de nova admissão |
| Impossible | Soma excede o máximo fixo daquele escopo/demanda mesmo vazio; não repetir indefinidamente |
| Invalid | Classe/Json compartilhado/soma inválida ou envelope inválido; corrigir contrato, não tratar como pressão temporária |

Escopo ativado: visível/dependência usam hard; especulação usa soft. Desativado:
vale maximumBytes legado. Limites são imutáveis; não há promoção automática.
Overflow/classes inválidas retornam requestedBytes zero; pedido válido conserva
sua soma mesmo recusado. Tokens inativos não escapam em respostas recusadas.
Allocations dos tokens podem lançar antes de haver status; pressão não é OOM.
Batch vazio/zero, deduplicação Json separada, contadores de admissão e lifetime
anteriores preservados. Um status retornado não muda após outra thread operar.

## Protocolo limitado de retomada

`WorldGenerationMemoryRetry<N>`, modelo **1**, vincula um ledger/demanda e um
plano de parcelas ao owner. Não é thread-safe para acesso concorrente ao mesmo
descritor, scheduler global, sistema de cancelamento ou execução de callbacks.

1. admit concede um número de tentativa monotônico e um batch. A tentativa
   fica Active até o owner aceitar conclusão ou descriptor explícito de retry,
   mesmo se alguém soltar os tokens cedo. Não há dispatch duplo implícito.
2. O produtor puro descobre que precisa de saída maior **antes de alocá-la**,
   abandona a tentativa e devolve só bytes/descritor, não payload parcial.
3. retry só aceita a tentativa ativa, mesmas classes, nenhuma parcela menor
   e ao menos uma maior, sem overflow. Não busca footprint mínimo; é conservador.
4. Weak refs dos tokens declarados fecham a próxima admissão enquanto qualquer
   input, saída parcial, closure ou promise anterior ainda os retiver. A política
   não retém o payload nem libera memória física por cancelamento lógico.
5. Após Deferred, a mesma revisão de progresso retorna AwaitProgress sem outro
   check/alocação. O owner deve sinalizar evento útil de drain/capacidade, não
   incrementar a revisão a cada tick/poll. Sem esse evento a retomada permanece
   adiada, mesmo que bytes tenham sido liberados. O protocolo não cria o evento.
6. Impossible/Invalid, limite explícito de tentativas admitidas e conclusão são
   terminais. Deferred não consome tentativa de produção. stop não sobrescreve
   terminal anterior, nem cancela produtor, libera payload ou publica resultado vazio.
7. `bad_alloc` e exceções de produtor continuam exceções; nenhum catch genérico
   foi convertido em indicação de previsão insuficiente. O owner decide parar.

Não há wait por orçamento dentro do protocolo. Esperas na fixture são barreiras
de ordenação do teste e espera pela conclusão de promises. O pool é drenado/
parado para confirmar a saída física das closures antes de medir; não é proposta
de parar o pool do jogo por tentativa. As retomadas também executam no pool.
Owners físicos precisam conservar seus tokens. A fence não prova ausência de
backing omitido, payload que escapou sem token ou dependências externas.

## Fixture de dois jobs e resultados retidos

Hard 200 bytes declarados, resume 80, soft 100; um resultado antigo retém 40.
Cada job prevê input 60 + saída 20, mas descobre saída de 60. Um é Visible, outro
Dependency. A saída é determinística e não usa RNG global do motor.

| Etapa | Bytes contabilizados | Evidência |
| --- | ---: | --- |
| Duas tentativas nos dois workers + resultado antigo | 200 | Ambas entram; nenhuma ultrapassa o teto declarado |
| Descoberta de saída maior e encerramento das duas tentativas | 40 | Zero alocações de saída; inputs/previsões liberados |
| A repete com 120 reservados e conclui, deixando output 60 | 100 | 40 antigos + 60 de A; input de A já liberado |
| B pede 120 enquanto os dois resultados estão retidos | 100 | Deferred sem capturar novo input; repetição do mesmo evento não reconsulta |
| Consumo dos 40 antigos + novo evento permite B concluir | 120 | Outputs A/B de 60; pico nunca supera 200; liberar ambos zera recursos |

Um caso adicional confere os bytes de saída após retry contra produção com
previsão suficiente desde o início. Worker único executa outro trabalho depois
da tentativa abortada; oito descritores disputam o mesmo soft sem multiplicar
a cota. Esses são contratos de produtores controlados, **não prova de progresso
do BaseTiles/planejador real, fairness geral, ausência de OOM ou ganho de setores/s**.

## Validação executada

Skill de ambiente cloud usada para conferir executor, build e rede disponíveis;
sem instalar/baixar dependências. GCC 14.2.0/C++17, Debug `-Og`, GUI/Tracy/
allocators alternativos OFF, `VCPKG_MANIFEST_INSTALL=OFF`. C++ com
`-fsanitize=address,undefined -fno-omit-frame-pointer -DXXH_NO_INLINE_HINTS=1`;
C/dependências pré-compiladas não têm a mesma instrumentação. Identifier:
`60d4fdc8-memory-retry-local-dirty`; fontes do commit/manifesto abaixo.

Com `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1` e
`ASAN_OPTIONS=halt_on_error=1`, sem diagnóstico dos sanitizers:

1. **452/452 testes**, 23 suites; **32 novos** de admissão detalhada/retry.
2. Novos casos repetidos vinte vezes, shuffle e seed GTest 1224: **640 execuções
   adicionais**, não casos únicos.
3. **23/23 grupos CTest** `^world_generation_contract_`, NoAssets; repetem os
   casos do mesmo binário, não outros 452 casos distintos.
4. Contratos e cinco objetos reais aprovados: geração, ledger, envelope,
   template e storage. Somente o ledger foi editado entre essas cinco fontes;
   demais recompilados por headers. Não é linking completo do jogo.
5. Sintaxe/tipos de geração, ledger, envelope, teste novo e testes de batch
   aprovados com STAR_ENABLE_TRACY/TRACY_ENABLE e headers Tracy 0.11.2 existentes.
   Teste novo conferido novamente após ajustes finais. Não é linking Tracy.
   Aviso herdado de fmt permanece. A fixture de histerese foi corrigida para
   reter o resultado admitido durante a medição, antes da validação final.
6. `git diff --check` aprovado; **495 links locais em 37 documentos**, 14 módulos
   alcançáveis, 49 contratos únicos cobertos pelo roadmap, ordem 1–49 e oito
   totais de fase conferidos. Recalculados 6/196 marcos = 3,1%, RAM-01/H0
   mantidos em 25%. Seis fontes conferidas em bytes/SHA-256.

Casos incluem statuses atômicos, teto legado/soft/hard/overflow/Json, construtor
inválido, tentativa ativa/stale, crescimento estrito, fences de input/output,
fechamento de fachada, eventos sem capacidade, limite de tentativas, terminais,
stop sem liberar owners, isolamento de demanda, concorrência, duas produções
com resultados retidos, equivalência de bytes e exceções. Sem TSan ou OOM real.

Reprodução, após configurar build equivalente na raiz:

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
  ./dist/world_generation_contract_tests --gtest_filter='WorldGenerationMemoryRetryTest.*' \
  --gtest_repeat=20 --gtest_shuffle --gtest_random_seed=1224 --gtest_brief=1
UBSAN_OPTIONS=halt_on_error=1 ASAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir <build-dir> -R '^world_generation_contract_' --output-on-failure
```

## Compatibilidade, limites e continuação

Sem novo config/log/Tracy do usuário ou captura anterior reanalisada; dados
brutos fora do Git. Sem jogo/assets/mods, save/reload, multiplayer, pressão
externa/OOM, benchmark de overhead/throughput, Windows/macOS/ARM ou allocators
alternativos executados. Sem Actions, release, merge em main ou revisão
independente. Nenhuma causa de crash atribuída a este recorte.

Não houve replay de Lua, callbacks ou escrita no mundo. RNG, ordem/cópias de
elementos, saves e protocolo preservados. O helper não fornece allocator que
consuma crédito de saída variável dos containers existentes: não conectá-los
ao batch para depois cobrar a mesma parcela novamente. As 14 classes e modelos
de envelope/admissão 1/estimativas anteriores mantêm suas exclusões; não é teto
RSS ou de todo heap. Nem memória estimada vira capacidade física garantida.

RAM-01/H0 permanecem **25%**, programa **3,1%**, 6/196 marcos. Próximo recorte:
vincular eventos úteis de drain/capacidade ao owner e definir o adaptador do
primeiro produtor **puro e versionado**, com demanda ligada ao interesse real
e comparação de outputs (MOD-01/VAL-02), antes de permitir retry real. Não
reexecutar BaseTiles/planejador inteiro só por capturar uma exceção. H1/DAG-02
em sombra pode usar o escopo delimitado, sem exigir medir cada heap do motor.
Provider processual único configurável, quotas/fairness, promoção, pressão
externa e baseline integrado continuam gates para limites/defaults e filas.
Setores comuns e hardware útil permanecem prioridade, com mods/saves/multiplayer
e fallback CPU. NPC externo suspenso, Linux Clang adiado; CPU-01 seed automática
e Perlin Uninitialized não corrigidos.

## Manifesto das fontes

Fontes locais de 09/11/2026, nomes relativos ao checkout, bytes e SHA-256.
Nenhum novo artefato de execução do usuário nesta etapa.

| Arquivo | Bytes | SHA-256 |
| --- | ---: | --- |
| `source/game/CMakeLists.txt` | 16630 | `224082116f81ceab20f06302d4be45d8ed8d813fa5543c891e1d1e387a28795e` |
| `source/game/StarWorldGenerationMemory.cpp` | 12722 | `86d59c85f6d4f41192d5543b4eb40d250f6dcd5253439810bc170402d20a3a04` |
| `source/game/StarWorldGenerationMemory.hpp` | 8048 | `6f7bf7b5143b1e3ac16fe5d3b8fe6a5907f614a14a6ff05d0a4d0fd3a0191836` |
| `source/game/StarWorldGenerationMemoryRetry.hpp` | 5232 | `c4226e54b4b747e32232e3e1884833b657d71495766b2bd6d5d0e87f2f2fe166` |
| `source/test/CMakeLists.txt` | 5932 | `4bad22b8a91d44af70c1ac7cbf6a235e1bec06690ffaf39929c3400eacd387cd` |
| `source/test/world_generation_memory_retry_test.cpp` | 24024 | `e22252aa3a6a2370fcacb76c66284861bdb65a42571483d47bd9b9eae018152f` |
