# H0 / RAM-01: reserva composta antes do dispatch de BaseTiles

Data: 09/11/2026. Entrega: `Experimental`; revisão local:
`codex/experimental-regional-runtime`. Base: `633c64e1ed55b936d0392697fc605381850faf0d`.
Implementação: `dd59f3b42aec2536b035f7cf39c9275b614a784e`.

[Programa](sector-runtime/README.md) · [Roadmap](sector-runtime/roadmap.md)
· [Progressão](sector-runtime/progress.md) · [Contrato](sector-runtime-memory-envelope.md)
· [Inventário](sector-runtime/memory-and-prefetch.md)
· [Etapa anterior](sector-runtime-h0-atomic-memory-2026-11-09.md)

## Objetivo e resultado

Antes, BaseTiles reservava seu array antes da cópia, mas só tentava reservar
o template privado no worker. Agora admite as duas parcelas declaradas juntas
antes de copiar/despachar e transfere os tokens aos owners, sem recobrança.
Isso elimina admissão parcial dessas parcelas; não prevê todo heap do job.

**Pressão do singleton continua desligada, sem limite finito ou config novos.**
Não foram ampliados workers, filas, orçamento de update, TTL, níveis ou
prerrequisitos. Não há ganho de setores/s demonstrado, DAG novo ou WorldThread
regional implementada. A antecipação da cobrança do template é uma mudança
observacional: bytes/recursos enfileirados podem aumentar sem corresponder a
RSS já alocado. Não comparar esses saldos como consumo físico.

## Contrato implementado

`reserveBatch(array<WorldGenerationMemoryRequest, N>)` retorna optional de array
fixo de tokens. Valida classes e soma sem overflow, cria tokens inativos antes
do lock e faz uma avaliação/cobrança conjunta sob o mutex existente. Recusa
retorna vazio sem cobrar prefixo/picos; alocações dos tokens podem lançar e
desfazem tokens ainda inativos. Não há vetor intermediário ou callback sob lock.
O total de recursos também é verificado antes de ativar qualquer parcela.

Batch vazio é no-op bem-sucedido, sem check; parcelas zero continuam recursos
distintos. Classes repetidas representam recursos físicos distintos. Json
compartilhado é excluído: continua exigindo deduplicação por owner. Após admitir,
as parcelas transferem/liberam independentemente. Não há transação única entre
registro owner-thread de tasks e ledger processual; a guarda existente desfaz
a task se preparação/segunda admissão falhar antes do handoff.

Primeiro contrato de produtor real, BaseTiles assíncrono:

| Parcela | Reserva e transferência | Limite da garantia |
| --- | --- | --- |
| Input/output privado | Um sizeof(Array), wrapper capturado e devolvido na promise | Mesmo array modificado in place; não cobrar duas vezes. Estado autoritativo anterior está fora do ledger privado |
| Possível novo template | sizeof(WorldTemplate) antes do dispatch, token consumido pelo cache | Estimativa inicial; não todo crescimento de LRU/layout/biomas |
| Overlap do cache | Entrada antiga cobrada até último owner; nova parcela já admitida | Hit devolve previsão não usada; substituir não libera uso antigo ainda ativo |

BaseTiles declara Visible explicitamente, igual à demanda padrão anterior.
Não deriva ainda demanda do interesse/dependência/especulação do scheduler.
Planejador, metadados e Json não foram migrados para reserva composta.

`getReserved` valida ledger, demanda, classe e tamanho do token. Miss consome
esse token; hit libera a previsão sem reconstruir payload. A identidade do
cache passa a incluir ledger/demanda, além de fonte/revisão: não reutilizar um
payload cobrado em outro orçamento/classificação. Corrige uma lacuna de
isolamento da API, não uma falha observada nos logs. O caminho get anterior
continua disponível, sem cópia adicional do producer.

Owners físicos prevalecem sobre invalidação/retirada lógica. Payload é destruído
antes do token. Retorno precoce, cópia parcial ou falha pré-aceitação do produtor
desfazem parcelas locais e task. Após handoff, resultado/cache vivo continua
cobrado. Não há escrita paralela no mundo ou alteração de publicação, RNG,
seleção, formatos de saves, Lua, protocolo ou contrato de mods.

## Prova de progresso e sua fronteira

Um produtor controlado com array de 136 bytes reutilizado como saída e template
de 64 bytes conclui num hard de 200, sem nova admissão desses bytes. A promise
retém o array até drain; saída da thread libera o cache. Outro caso transfere
parcelas distintas de input/output/overlap também sem recobrança.

O controle negativo cresce o template previsto de 64 para 65 em hard: refresh
marca estimativa incompleta, conservando a cobrança anterior. O produtor não
ganhou garantia de heap de 65. **O teste positivo não prova progresso do BaseTiles
real, ausência de OOM ou de deadlock em todo DAG.** Crescimentos internos seguem
observacionais; não ativar limite finito do jogo com base nestes resultados.
Modelos de envelope/admissão 1 e fórmulas anteriores mantidos.

## Validação executada

Skill de ambiente cloud usada para conferir ambiente/build, sem instalar ou
baixar dependências. GCC 14.2.0/C++17, Debug `-Og`, GUI/Tracy/allocators
alternativos OFF, `VCPKG_MANIFEST_INSTALL=OFF`. C++ com
`-fsanitize=address,undefined -fno-omit-frame-pointer -DXXH_NO_INLINE_HINTS=1`;
C/dependências pré-compiladas não têm a mesma instrumentação. Identifier:
`633c64e1-composite-memory-local-dirty`; fontes do commit/manifesto abaixo.

Com `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1` e
`ASAN_OPTIONS=halt_on_error=1`, sem diagnóstico dos sanitizers:

1. **420/420 testes**, 22 suites; **32 novos** de reserva composta.
2. Os 32 repetidos vinte vezes, shuffle e seed GTest 1223: **640 execuções
   adicionais**, não casos únicos.
3. **22/22 grupos CTest** `^world_generation_contract_`, NoAssets; repetem
   os casos do mesmo binário, não outros 420 testes distintos.
4. Contratos e cinco objetos reais aprovados: geração, ledger, envelope,
   template e storage. Últimos três recompilados por dependências; não é
   linking completo do jogo.
5. Sintaxe/tipos aprovados em geração, ledger, envelope, teste novo e testes
   do cache com STAR_ENABLE_TRACY/TRACY_ENABLE e headers Tracy 0.11.2 existentes.
   Não é linking Tracy completo. Aviso herdado de fmt permanece; aviso novo
   de macro do teste foi corrigido antes da validação final.
6. `git diff --check` aprovado; **481 links locais em 36 documentos**, 14 módulos
   alcançáveis, 49 contratos únicos cobertos pelo roadmap, ordem 1–49 e oito
   totais de fase conferidos. Recalculados 6/196 marcos = 3,1%; RAM-01/H0
   mantidos em 25%. Seis fontes conferidas em bytes/SHA-256.

Casos: soma/size_t máximo, classes inválidas/Json, parcelas repetidas/zero,
batch vazio, máximo legado, soft/hard/demanda, release individual/facade,
disputa de oito threads e contra reserve simples, snapshots coerentes,
cache hit/miss/substituição lançante, tokens incorretos, isolamento ledger/
demanda, rollback pré-dispatch, cancelamento lógico/lifetime físico, progresso
limitado e controle negativo de crescimento. Falha de submissão injetada antes
da aceitação, não OOM real do pool. Não foi injetada falha física na alocação
de cada token do batch. ASan/UBSan não é TSan.

Reprodução na raiz, após configurar build equivalente:

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
  ./dist/world_generation_contract_tests --gtest_filter='WorldGenerationMemoryBatchTest.*' \
  --gtest_repeat=20 --gtest_shuffle --gtest_random_seed=1223 --gtest_brief=1
UBSAN_OPTIONS=halt_on_error=1 ASAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir <build-dir> -R '^world_generation_contract_' --output-on-failure
```

## Continuação e limites

Nenhum novo config/log/Tracy do usuário; nenhuma captura anterior reanalisada.
Dados brutos fora do Git. Não executados jogo/assets/mods, save/reload,
multiplayer, TSan, OOM/pressão externa, benchmark de overhead/throughput,
Windows/macOS/ARM ou allocators alternativos. Sem Actions, release, merge em
main ou revisão independente. Nenhuma causa de crash atribuída a este recorte.
Controles anteriores de cópias/ordem/seleção continuam aprovados.

RAM-01/H0 permanecem **25%**; programa **3,1%**, 6/196 marcos. Mais casos/commits
não completam orçamento processual. Reserva composta inicial entregue;
crescimento variável e classificação de todos os produtores continuam abertos.

Próximo recorte executável: modelar produtor de saída variável e decisão de
adiar/repetir/drenar quando sua previsão não cobre crescimento, com fixture
de dois jobs concorrentes/resultados retidos. Nenhum pode segurar input enquanto
espera indefinidamente por bytes para produzir saída. Estender para planejador
só com parcelas transferíveis explícitas, sem recobrança e sem mudar a história
de cópias. Vincular demanda ao interesse real. Depois: provider processual único
configurável, quotas/fairness, promoção e pressão externa/limites do usuário.
H1 em sombra pode avançar com escopo delimitado, sem contabilizar todo heap;
depende dos gates aplicáveis MOD-01/VAL-02 e não autoriza novo limite/default
ou escrita paralela no mundo.

Prioridade: setores comuns e uso útil de CPU/RAM/GPU, sem CPU fixa, com
mods/saves/multiplayer e fallback CPU. NPC externo suspenso, Linux Clang adiado.
Pendências CPU-01 de seed automática e Perlin Uninitialized não corrigidas.

## Manifesto das fontes

Fontes locais de 09/11/2026, nomes relativos ao checkout, bytes e SHA-256.
Nenhum novo artefato de execução do usuário nesta etapa.

| Arquivo | Bytes | SHA-256 |
| --- | ---: | --- |
| `source/game/StarWorldGeneration.cpp` | 129352 | `63b50428d222ea4a007c5813a427b437f0d81cba20403c97eeff195cd3e76a92` |
| `source/game/StarWorldGenerationMemory.cpp` | 12039 | `ba6ef7726c1349a36ada018f85d70bdcbc22786e25e46cfb827e201e50f2cf8f` |
| `source/game/StarWorldGenerationMemory.hpp` | 7062 | `5fe54891ab824aa44e2f945b154bb2c96e350f088ba6ce0257afa53757cee6dd` |
| `source/game/StarWorldGenerationWorkerCache.hpp` | 4090 | `2d677ffb14c1041d3d24d7b3b2a4b78a54d81b074edc62ebe0ec9f4212b94691` |
| `source/test/CMakeLists.txt` | 5856 | `bbf52a48353bfaee9bd466b1c09a51f5c30c704a0b604294bb1de35e2ad09d75` |
| `source/test/world_generation_memory_batch_test.cpp` | 20608 | `97ddf599011d106456ef75aaf04e441e38a9c4ddf8c10b14368444ca58856c39` |
