# H0 / RAM-01: ledger processual dos arrays privados de geração

Data: 09/11/2026. Branch de entrega: `Experimental`; branch local de revisão:
`codex/experimental-regional-runtime`. Base: `b05576c9`.
Implementação: `65bae55c960f22899b0ab4f5ae45495bad328240`.

[Programa principal](sector-runtime/README.md) · [Roadmap](sector-runtime/roadmap.md)
· [Memória e inventário](sector-runtime/memory-and-prefetch.md)
· [Recorte anterior](sector-runtime-h0-memory-admission-2026-11-09.md)

## Objetivo e resultado

Continuar RAM-01 com contabilidade compartilhada entre mundos, que acompanhe
a retenção dos snapshots mesmo após o fechamento de seu mundo. Preservar
cotas, modos, quantidade de workers e regras de publicação existentes.

O novo ledger acompanha reservas dos arrays privados de BaseTiles e dos
halos do planejador. Referências ao mesmo wrapper contam uma vez. A reserva
não desaparece com o registro lógico de tarefas enquanto o worker ou seu
resultado ainda mantiver esse recurso privado.

O runtime usa o ledger em modo **observacional**. Não impõe um teto global
arbitrário com uma contabilidade ainda incompleta. A API aceita teto finito
para testes isolados, mas nenhuma nova chave de configuração ou política de
admissão foi ativada. RAM-01 completo permanece pendente.

## Problema de lifecycle

`WorldGenerationTasks` pertence ao mundo e sua destruição invalida tickets.
Entretanto, jobs do pool e promises podem reter arrays depois disso. Somar
apenas `reservedBytes()` dos registros vivos esquece esses recursos.

O inverso também acontece: um halo pode ser destruído quando a closure do
planejador termina, enquanto os descritores prontos aguardam aplicação e o
registro lógico ainda mantém sua reserva rasa. Bytes lógicos e reservas de
recursos privados são medidas diferentes; não podem ser somados como se
representassem alocações físicas distintas.

## Implementação e ownership

`WorldGenerationMemoryLedger` mantém um estado compartilhado protegido por
mutex: bytes reservados, pico vitalício, número de grupos e divisões por duas
classes fixas (`BaseTilesArray`/`MicroDungeonHalo`). As operações de reserva,
liberação e snapshot são coerentes entre threads. O mutex não fica retido
durante alocação de payload, cálculo, callbacks ou publicação.

Cada reserva é um token compartilhado. Seu destrutor devolve a cobrança uma
única vez; o estado do ledger sobrevive aos seus facades enquanto houver
tokens. Copiar um facade compartilha o estado/cota; reatribuir o facade para
esquecer reservas antigas é impedido pelo tipo. Tokens não possuem mundo,
registro, entidades, callbacks ou acesso a Lua.

`WorldGenerationTrackedResource<T>` mantém o token antes do payload na ordem
dos membros: o payload é destruído primeiro. O factory reserva antes de
construir o payload. Falhas de construção, preparação parcial, dispatch ou
execução deixam o último dono liberar a reserva sem intervenção do mundo.
O wrapper não é copiável; compartilham-se referências ao mesmo wrapper.

Integração real:

- **BaseTiles:** reservar antes de `copySector`; o worker usa o wrapper e
  devolve a mesma referência na promise. Não há cópia adicional do array ou
  dupla cobrança por capturas/resultado. Ao publicar, o array é movido para
  `TileSectorArray`; o último wrapper encerra a reserva privada.
- **Planejamento:** reservar quantidade de arrays do halo × `sizeof(Array)`
  antes das cópias; o wrapper guarda o snapshot e é capturado pelo worker.
  A saída de planos não retém artificialmente o token do halo. A closure
  pode manter inputs um pouco além do retorno do produtor, até sua destruição;
  o ledger acompanha essa referência real, não apenas o estado `Ready`.
- **Mundos simultâneos/teardown:** o singleton processual observa os recursos
  de ambos, inclusive antigos sobreviventes. Fechar ou reabrir um mundo não
  zera a série nem exige chamar um mundo já destruído ao liberar o recurso.

Deduplicação significa uma cobrança por wrapper/token; duas cópias físicas
independentes do mesmo setor continuam consumindo duas reservas. O overhead
de tokens, control blocks e allocator não está incluído no número de bytes.
Há custo de sincronização/alocação por grupo, não por tile; seu impacto em
throughput ainda precisa de benchmark real.

## Escopo e distinções essenciais

Os bytes são **reservas declaradas dos objetos Array privados**, incluindo
seus campos inline. Uma reserva pode anteceder a alocação e elevar o pico
mesmo se a construção falhar. Não se trata de medição de RSS ou de todo o
heap. No modo observacional, apenas overflow aritmético recusa uma reserva;
os limites efetivos de admissão por mundo continuam sendo os anteriores.

Publicar não libera o array autoritativo: ele apenas sai do escopo privado.
Também pode haver um breve prolongamento conservador da reserva até o último
wrapper vazio ser destruído. Não interpretar o contador como bytes exatos de
malloc/free de toda a aplicação.

Ficam fora deste recorte: arrays autoritativos, templates/Json compartilhado,
cache thread-local de `workerWorldTemplate`, mapas/listas/distribuições,
descritores e strings dos planos, decode salvo, allocator e VRAM. O
[inventário](sector-runtime/memory-and-prefetch.md#inventário-de-ownership-e-cobertura-atual)
identifica dono e término de retenção antes de ampliar classes e orçamento.
Não somar o ledger privado às cotas lógicas por tarefa: há sobreposição.

## Telemetria de cardinalidade fixa

Com profiling habilitado, `readyBaseTileSectors` amostra o ledger no máximo
uma vez por segundo por coletor de mundo, sem varrer payloads. Campos:

| Campo sob `generation.memory.process.` | Semântica |
| --- | --- |
| `samples` | número de observações na janela do coletor |
| `sampledReservedArrayBytesTotal` | soma dos bytes reservados nas observações, não volume alocado |
| `sampledResourcesTotal` | soma dos grupos observados; um halo pode conter vários arrays |
| `sampledBaseTilesArrayBytesTotal` | parcela de BaseTiles na soma amostrada |
| `sampledMicroDungeonHaloArrayBytesTotal` | parcela de halos na soma amostrada |
| `sampleMaximumReservedArrayBytes` | máximo entre as amostras da janela |
| `lifetimePeakReservedArrayBytes` | pico de reservas desde a criação do ledger processual |
| `admissionDeferred` | recusa pelo ledger; não um novo limite configurado no runtime |

As somas divididas por `samples` produzem médias amostradas, não RSS médio.
Coletores de mundos diferentes observam o **mesmo** ledger: não somar suas
séries como memória independente. O pico vitalício não reseta por janela,
troca de mundo ou rotação do relatório; reseta com novo processo/ledger.
Se não houver coletor ativo, falta de amostra não prova contagem zero.
Nenhuma métrica nova contém nomes de mods, jogadores ou payloads.

## Validação executada

Ambiente cloud conferido pela skill operacional, sem instalação de pacotes.
Reutilizada a build headless GCC 14.2.0/C++17 Debug com ASan/UBSan e dependências
já instaladas. O source identifier local foi atualizado para
`b05576c9-ram-process-local-dirty`; os hashes abaixo identificam o código
efetivamente validado, depois commitado em `65bae55c`.

Resultados com `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1` e
`ASAN_OPTIONS=halt_on_error=1`, sem diagnóstico dos sanitizers:

1. Build CMake de `world_generation_contract_tests`: concluída.
2. **95/95 testes** do binário de contratos, incluindo 20 casos novos em
   `WorldGenerationMemoryTest`.
3. Os 20 novos repetidos **20 vezes**, com ordem embaralhada e seed GTest
   `1209`: 400 execuções adicionais aprovadas, não 400 casos únicos.
4. CTest filtrado por `^world_generation_contract_`: **8/8 grupos NoAssets**,
   repetindo os casos do binário, não acrescentando 95 casos distintos.
5. Compilação CMake do objeto real `StarWorldGeneration.cpp.o`: concluída
   com as mesmas opções de diagnóstico; não é linking de cliente/servidor.
6. Checagem de sintaxe/tipos de `StarWorldGeneration.cpp` e do novo módulo nos
   modos normal e `STAR_ENABLE_TRACY`/`TRACY_ENABLE`: aprovada. O segundo modo
   usa headers Tracy 0.11.2 existentes, sem uma build Tracy completa.
7. `git diff --check`: aprovado.
8. Documentação: 201 links locais em 18 arquivos sem destino ausente; 13
   módulos alcançáveis, 49 IDs únicos/referenciados e âncora do inventário
   válida. Os cinco manifestos de fonte abaixo coincidem em tamanho e hash.

Os casos incluem: soma por classe/pico, referência compartilhada contada uma
vez, destruição do payload antes do token, falha de construtor, recusa sem
construção, cópia parcial, teto finito isolado, overflow, classe inválida,
grupo zero-byte, retirada lógica, fechamento/reabertura de mundo, facade
destruído, falha de produtor, fronteira de publicação e singleton processual.

Um teste usa **WorkerPool real e gate controlado**: o produtor mantém o input
enquanto o registro do mundo é destruído; depois seu resultado mantém uma
única cobrança até a última referência sair. Outro libera o halo depois da
saída da closure sem cobrar o resultado de plano. O teste concorrente executa
8.000 ciclos de reserva/liberação em oito threads por repetição e verifica
coerência das somas e saldo final zero. ASan/UBSan não substituem ThreadSanitizer;
TSan não foi executado.

Reprodução após configurar uma build equivalente:

```bash
cmake --build <build-dir> --target world_generation_contract_tests
UBSAN_OPTIONS=halt_on_error=1 ASAN_OPTIONS=halt_on_error=1 \
  ./dist/world_generation_contract_tests --gtest_brief=1
UBSAN_OPTIONS=halt_on_error=1 ASAN_OPTIONS=halt_on_error=1 \
  ./dist/world_generation_contract_tests --gtest_filter='WorldGenerationMemoryTest.*' \
  --gtest_repeat=20 --gtest_shuffle --gtest_random_seed=1209 --gtest_brief=1
UBSAN_OPTIONS=halt_on_error=1 ASAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir <build-dir> -R '^world_generation_contract_' --output-on-failure
```

O cache local mantém `XXH_NO_INLINE_HINTS=1` em C/C++ para a incompatibilidade
Debug do xxHash já descrita no relatório anterior; não houve mudança de
presets/código externo. Dependências pré-compiladas/código C não têm a mesma
instrumentação C++. Warnings herdados de fmt não foram declarados resolvidos.

## Configuração, compatibilidade e limites de evidência

Não houve nova config, log ou captura fornecida/coletada; nenhum arquivo de
configuração foi alterado. Profiling existente habilita as amostras; o ledger
acompanha os recursos também com profiling desligado. Sem preview novo,
mudança de seeds/TTL/pré-requisitos ou escrita paralela. A referência agregada
não dá ao worker acesso ao mundo; promises/commit continuam na dona.

Não foram executados jogo com assets/mods, rota de streaming real, multiplayer,
Windows/macOS/ARM, A/B de desempenho, build completa do jogo ou Actions. Não há
ganho de setores/s medido. O overflow da seed automática de `StarRandom.cpp`
permanece pendente em CPU-01; esses testes não validam essa inicialização.
NPC externo continua suspenso e Linux Clang adiado. Não houve merge em `main`
ou publicação de release.

## Manifesto e continuação

Fontes conferidas em 09/11/2026 (SHA-256); binários/capturas permanecem fora do Git:

| Arquivo | Bytes | SHA-256 |
| --- | ---: | --- |
| `source/game/StarWorldGenerationMemory.hpp` | 3.311 | `2d09d381948b2d3ee29850cb384a7428ae48c33abc0f0cdda4e474a43043806a` |
| `source/game/StarWorldGenerationMemory.cpp` | 2.418 | `c79162397d1c091372f3318e5136d3ba2ed230da729e4e686b044bd94f3743d4` |
| `source/game/StarWorldGeneration.hpp` | 12.172 | `e49a28e00767bdbb18a4e744cba1e2fba4041c41caf6d6feba43c735390fed01` |
| `source/game/StarWorldGeneration.cpp` | 114.999 | `74af2bcd56c50e5d6791cdfcfc4dbcae12b70585eb30a717510ad9b2b3f45240` |
| `source/test/world_generation_memory_test.cpp` | 13.297 | `ef3c337fe0e61446a6c5ef1dcddc99db4580148e18e0124059fa189d1d4f59a7` |

O inventário e o [roadmap](sector-runtime/roadmap.md) são a sequência vigente.
Próximo: tamanhos/versionamento e backing storage único de templates/Json,
metadados e produtos de tamanho variável. Depois, cotas processuais orientadas
a hardware/configuração e reação à pressão, sem duplicar contabilização.
Manter MOD-01 e comparador real de outputs antes de ampliar admissão. RAM-01
completo e os gates de H0 continuam abertos; este recorte não implementa o
executor de H1, a nova WorldThread autoritativa ou compute GPU.
