# H0 / RAM-01: reservar antes de copiar snapshots

Data: 09/11/2026. Branch de entrega: `Experimental`; branch local de revisão:
`codex/experimental-regional-runtime`. Base: `6ee9ae91`.
RAM-01: `5c3c4c8f7ef56668707799c10970234e4811e24e`.
Correção de hash/testes: `f9330a2841561cb35649958bec8dea006b217fcf`.

[Programa principal](sector-runtime/README.md) · [Roadmap](sector-runtime/roadmap.md)
· [RAM-01](sector-runtime/memory-and-prefetch.md#ram-01--reserva-e-contabilização-de-bytes)
· [Relatório anterior](sector-runtime-h0-streaming-pass-2026-11-09.md)

## Objetivo e problema estabelecido

Iniciar RAM-01 sem ampliar admissão: eliminar cópias privadas feitas antes de
saber se o registro compartilhado do mundo pode reservar chave e bytes.
Preparar a expansão futura de throughput sem multiplicar alocações rejeitadas.

Na base examinada, `prepareTilesAsync` copiava um array BaseTiles, preparava
holder/template/material database e só então chamava `reserve`. No caminho
assíncrono de `generateMicroDungeons`, o halo inteiro era copiado antes de
reservar. Os limites individuais das filas não impediam recusa pelo registro
compartilhado entre etapas; uma admissão negada já havia pago essas cópias.

A preparação do snapshot também ficava fora do `try` que retirava a reserva
em falha de dispatch. Antecipar apenas a chamada de `reserve`, sem proteger
essas saídas, introduziria chaves/bytes presos em retorno antecipado ou erro.
Além disso, a inserção da pendência era posterior ao dispatch: uma falha de
alocação do mapa nesse ponto poderia deixar produtor sem registro de sua promise.

## Implementação

`WorldGenerationTaskPreparation` adquire a reserva do registro real antes de
copiar. Enquanto não houver handoff, o destrutor retira uma tarefa ainda
`Queued` no retorno/erro. Inputs privados são declarados depois do guard:
seus destrutores rodam antes da liberação da reserva durante unwinding.

O rollback confere identidade e ponteiro do ticket, faz a transição exclusiva
`Queued → Failed` e usa a retirada existente. Não retira uma tarefa Running
ou terminal por supor que uma preparação acabou. `handoff()` desarma o guard;
depois dele, valem a promise e o drain normal, inclusive em unload/cancelamento.

Nos dois caminhos de geração:

1. Verificar disponibilidade/metadados e os limites existentes.
2. Calcular bytes com multiplicação verificada contra overflow.
3. Adquirir a reserva; não copiar se ela for recusada.
4. Preparar arrays, template e metadados privados sob o guard.
5. Alocar/preencher o registro da pendência antes de entregar o job ao pool.
6. Após aceitação, fazer handoff e guardar a promise sem alocação: emplacement
   em `Maybe` vazio de um handle movível sem exceção, verificado por `static_assert`.

O halo tem sua lista de setores válidos enumerada e verificada sem copiar
tiles; sua reserva usa exatamente quantidade de arrays × `sizeof(Array)`.
Uma falha parcial destrói os arrays já copiados antes de desfazer a reserva.
O mesmo helper verifica o produto do teto de bytes; as fórmulas e limites
anteriores são preservados. Não se aumenta quantidade de jobs, workers ou RAM.

## Defeito adicional encontrado pelos sanitizers

A primeira execução completa obteve 75/75 verificações funcionais e 7/7 grupos
CTest, mas **não era uma validação limpa**: UBSan apontou aritmética de ponteiro
nulo em `StarFlatHashTable.hpp`, dentro de `end()` de um mapa vazio.

Com `UBSAN_OPTIONS=halt_on_error=1`, até o teste antigo
`WorldGenerationTasksTest.LifecycleAndIdentity` terminou com erro na primeira
reserva. A causa precede RAM-01: `data() + size() - 1` calculava uma sentinela
antes de `nullptr` para o recipiente ainda não alocado. O mesmo caminho é
usado por consultas e iteração do registro de tarefas.

A correção retorna iterador nulo quando não há buckets, sem alterar o ramo
dos mapas preenchidos, hashes, layout ou ordenação. Acrescentaram-se dois
testes em `flat_hash_test.cpp` para recipientes vazios/movidos/limpos. Essa
correção é separada da política de admissão e não é atribuída a um crash das
runs antigas. Nenhum sanitizer foi desabilitado para ocultar o diagnóstico.

### Achado independente: seed automática do RNG

Após corrigir a sentinela, os 75 contratos passaram em modo estrito. A
execução ampliada de `FlatHashSet.Random` parou em outro problema anterior:
`produceRandomSeed`, em `StarRandom.cpp:197`, multiplica um `int64_t` derivado
do relógio por `1099511628211`, com overflow assinado observado pelo UBSan.
Esse caminho não foi corrigido nesta entrega: alterar inicialização/RNG exige
auditoria própria de compatibilidade em CPU-01.

Os dois testes randomizados de hash passaram a usar fontes privadas com seed
fixa (`0x48617368536574` para set e `0x486173684d6170` para map). Assim, os
mesmos 100.000 elementos e operações de cada teste são reproduzíveis, sem
consultar a seed automática/global. Isso não resolve o defeito de produção
nem desabilita instrumentação: a pendência está registrada no backlog e no
módulo CPU. Não declarar o RNG automático validado pelos testes de hash.

## Escopo de memória e compatibilidade

Este é um recorte **parcial** de RAM-01:

| Contabilizado/preservado | Ainda fora do ledger |
| --- | --- |
| Objeto `Array` completo, incluindo campos inline | Allocator/control blocks, containers e metadados |
| Chave única e soma rasa de bytes por mundo | Templates privados, caches e Json compartilhado |
| Rollback de preparação não submetida | Distribuições e saída variável dos planos |
| Reserva normal de jobs submetidos até consumo/descarte | Cota global por processo e bytes únicos entre mundos |
| Invalidação mantém o ciclo normal do produtor | Ledger processual sobrevivente ao fechamento do mundo |

`sizeof(Array)` não mede RSS nem toda a memória do pipeline. Não presumir
buffers dinâmicos inexistentes dentro de campos inline; inventariar os
objetos reais e seu ownership antes da contabilidade profunda. A destruição
do mundo ainda invalida handles enquanto workers podem reter inputs: um
orçamento global terá de acompanhar a vida física desses recursos.

Geração, seed, distribuição, ordem de publicação, validações de revisão,
save, protocolo e autoridade permanecem nos caminhos existentes. Mudar o
momento de preparação não prova equivalência de mods sem comparação em jogo.
As rotas síncronas não ganham novo pool nem ativação de preview. Não há nova
chave de configuração e nenhum default mudou.

O rollback pré-submissão não espera uma promise inexistente e não emite o
mesmo evento terminal de profiling de um produtor drenado. Uma falha fatal
nessa fase pode deixar evento `reserved` sem terminal no relatório; o ledger
é desfeito pelo guard. Não interpretar esse traço incompleto como prova de
bytes retidos. `admissionDeferred` também cobre recusa do planejador no
registro compartilhado; `snapshotBytes` continua cumulativo, não RAM residente.

## Validação local

Execução final com `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1` e
`ASAN_OPTIONS=halt_on_error=1`, sem alertas nos cenários selecionados:

- build CMake dos alvos `world_generation_contract_tests` e `core_tests`:
  concluída com fontes C++ atuais, sem reutilizar objetos antigos do core;
- binário de contratos: **75/75** testes, em sete suítes;
- seleção `FlatHashMap.*:FlatHashSet.*`: **7/7**, incluindo as duas regressões
  novas e os dois testes randomizados com seeds privadas fixas;
- CTest filtrado por `^world_generation_contract_`: **7/7** grupos NoAssets;
- whitespace e documentação: 194 links locais em 18 arquivos sem destino
  ausente, 13 módulos alcançáveis, 49 IDs únicos/referenciados e âncora RAM-01
  válida.

Os resultados totalizam 82 casos GTest selecionados; CTest repete os 75 do
binário de contratos, não soma outros 75 casos distintos. A primeira rodada
com UBSan recuperável foi descartada como validação limpa, apesar dos asserts
aprovados; a falha do RNG automático permanece explicitamente fora do escopo
validado. Não foram rodados todos os testes de `core_tests`.

Foram adicionados dez testes ao `WorldGenerationTasksTest`: reserva anterior
à cópia, recusa compartilhada entre etapas sem cópia, rollback em retorno,
falha parcial com ordem de destruição, recusa do pool, retenção após handoff
em cancelamento/Ready/falha, proteção de produtor Running, identidade antiga
versus substituição e overflow dos produtos de bytes. A suíte passa de dez
para vinte testes; os dois testes de hash são regressões adicionais do core.

As falhas de cópia/dispatch são injetadas de forma controlada sobre o registro
e o guard reais. Não são fault injection do allocator do jogo, nem execução
de `WorldStorage` com assets. Os testes não demonstram ganho de setores/s.

Checagens de sintaxe/tipos de `StarWorldGeneration.cpp` e
`StarWorldGenerationTasks.cpp` passaram com GCC 14.2.0/C++17, nos modos normal
e `STAR_ENABLE_TRACY`/`TRACY_ENABLE`, usando os headers Tracy 0.11.2 já
disponíveis. Syntax-only não é build/linking do cliente ou do servidor.

### Ambiente e reprodução

A skill de ambiente cloud foi usada para conferir readiness e aproveitar
dependências já instaladas. Nenhuma dependência foi instalada/baixada.
Foi criada uma build CMake separada, headless, com:

- `CMAKE_BUILD_TYPE=Debug`, `STAR_BUILD_GUI=OFF`, `BUILD_TESTING=ON`;
- `STAR_ENABLE_TRACY=OFF`, sem allocators alternativos;
- GCC 14.2.0, C++17, ASan/UBSan nos objetos C++;
- `VCPKG_MANIFEST_INSTALL=OFF`, usando bibliotecas existentes;
- source identifier local `6ee9ae91-ram01-local-dirty`, sem confundir com
  um executável distribuído ou uma build limpa do commit de base.

O Ninja existente foi selecionado por caminho absoluto por não estar no
PATH. A compilação Debug encontrou `always_inline` inválido no xxHash com
`-Og`; a opção suportada `XXH_NO_INLINE_HINTS=1` foi aplicada **somente à
build local**, em C e C++. Não se modificaram código externo ou presets.
Houve warnings herdados de fmt; não alegar build sem warnings. Bibliotecas
externas pré-compiladas e código C não receberam a mesma instrumentação C++.

Após configurar uma build equivalente, os comandos de verificação são:

```bash
cmake --build <build-dir> --target world_generation_contract_tests core_tests
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ASAN_OPTIONS=halt_on_error=1 \
  ./dist/world_generation_contract_tests --gtest_brief=1
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ASAN_OPTIONS=halt_on_error=1 \
  ./dist/core_tests --gtest_filter='FlatHashMap.*:FlatHashSet.*' --gtest_brief=1
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ASAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir <build-dir> -R '^world_generation_contract_' --output-on-failure
```

Essa seleção não executa todos os testes do core nem todos os testes com
rótulo `NoAssets` do projeto. Build completa de jogo, execução com assets,
Windows/macOS/ARM, multiplayer, comparação A/B e Actions continuam fora da
validação desta atividade. Não houve merge em `main` nem release.

## Manifesto e continuidade

Nenhum arquivo novo de config/log/Tracy foi recebido ou reanalisado. Os
artefatos de código conferidos em 09/11/2026 são:

| Arquivo no repositório | Bytes | SHA-256 |
| --- | ---: | --- |
| `source/game/StarWorldGeneration.cpp` | 113.296 | `fef0696d93dea32fd10016cf113a7ae45c99b053419a50c8a40462115bfdb8e8` |
| `source/game/StarWorldGenerationTasks.cpp` | 6.292 | `cc158c615d4314a6d01d98de63d533ff7831559dbd96edd46f5b46ac51f54a37` |
| `source/game/StarWorldGenerationTasks.hpp` | 4.794 | `0e140805767cb362ef5c05bf6f6e24a2e2f0d4bd5a0cdc8d46dbe0e28bbeccb5` |
| `source/test/world_generation_tasks_test.cpp` | 15.869 | `2fafb810ed0177cac78b13a9d764e8f88e2154be2a8b5cf4eeee4704375cfa3f` |
| `source/core/StarFlatHashTable.hpp` | 20.337 | `fe1b10c903594c3276aa1daa77c968531afaccd605446dcf8aa5b40137f55bf4` |
| `source/test/flat_hash_test.cpp` | 6.783 | `e4fe2c846885051f6c8f571951b030fca9462cd5fb9499f130298cef4c4a8812` |
| `source/core/StarRandom.cpp` (não alterado, achado pendente) | 7.690 | `762620f9ce3c15bd19f49a915f1749534dc4dca5c5356c41aeb6432f41f40454` |

Binários e artefatos de build permanecem fora do Git. O identifier local
`-dirty` identifica a execução anterior aos commits, com os mesmos hashes
de fonte acima; não representa uma build distribuída. A evidência de desempenho
anterior e os manifestos de runs permanecem na
[auditoria](sector-generation-architecture-audit-2026-11-08.md) e no
[baseline](sector-runtime/baseline.md).

Próximo recorte: inventário/ledger processual dos recursos realmente retidos,
separando arrays, metadados, templates compartilhados e produtos de tamanho
variável, com tokens que sobrevivam ao fechamento do mundo. Preservar cotas
por mundo e manter MOD-01 explícito antes de aumentar admissão/ownership.
RAM-01 completo, VAL-01/02 com assets e o executor de H1 continuam abertos.
NPC externo permanece suspenso, Linux Clang adiado e Actions manuais.
