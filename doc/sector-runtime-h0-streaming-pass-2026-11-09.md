# H0: passagens reproduzíveis e correção de continuação/avanço

Data: 09/11/2026. Branch de entrega: `Experimental`; branch local de revisão:
`codex/experimental-regional-runtime`. Base: `93dd37f1`, incluindo a correção
de identidade `d72b7239`. Implementação deste recorte:
`2131ead330d9e80957ea1211334716e703016d4a`.

[Programa principal](sector-runtime/README.md) · [Roadmap](sector-runtime/roadmap.md)
· [DAG-04](sector-runtime/dependency-scheduler.md#dag-04--conclusões-e-progresso-dentro-da-passagem)
· [Validação](sector-runtime/validation-and-telemetry.md)

## Objetivo e resultado

Continuar H0 priorizando entrega de setores comuns: reproduzir a interação
entre avanço visível, continuação de cadeia e encerramento da passagem antes
de aumentar admissão, workers ou memória. Iniciar VAL-01/02 com execução
controlada da política efetivamente usada pelo runtime.

O recorte corrige o encerramento indevido quando avanço e continuação estão
ativos juntos. A dona pode tentar outros candidatos distintos no orçamento
remanescente, preservando o primeiro follow-up para a próxima passagem.
Há 18 testes novos e os oito testes anteriores de identidade foram repetidos:
**26/26 aprovados com AddressSanitizer/UBSan**. Isso comprova invariantes da
política e produtos sintéticos do harness, não um ganho de throughput do jogo.

## Reprodução e causa

Na base examinada, `WorldStorage::generateQueue` decidia `continueVisible`,
movia a raiz bloqueada para o fim e contava o avanço autorizado. Entretanto,
o `if (scheduleVisibleChainContinuation) break` posterior encerrava o loop
incondicionalmente. Uma tentativa que fizesse progresso parcial podia impedir
qualquer raiz independente posterior de usar o tempo restante.

A decisão foi extraída para `WorldGenerationPass` e o teste
`ContinuationDoesNotCancelAnAllowedVisibleAdvance` foi executado primeiro
preservando a regra antiga `stop = continuation`. Resultado: um teste falhou
porque `stopForContinuation` era `true`; o teste de preservação do legado
passou. A correção muda essa regra para `continuation && !advance`; ambos
passaram na execução final, junto aos demais cenários.

O controle `originalContinuationBreak` do harness reinsere somente o guard
antigo para isolar seu efeito. É um contrafactual de teste, não uma execução
do binário anterior nem uma chave de configuração do jogo.

## Implementação e compatibilidade

`WorldGenerationPass` contém apenas política da passagem. `WorldStorage`
continua responsável por fila, pré-requisitos recursivos, carregamento,
revisões, autoridade, publicação e efeitos. O helper não recebe tiles,
promises, Lua ou acesso a workers.

- Avanço autorizado mais continuação: tentar outra raiz, sem o `break`
  incondicional. Ao atingir o limite, continua a parada por continuação;
  sem continuação, permanece o tail BaseTiles já existente.
- Com o preview de avanço ativo, cada chave é tentada no máximo uma vez no
  loop externo da passagem, inclusive no tail. Ao reencontrar todas as chaves
  já visitadas, parar em vez de girar ou repetir a mesma tentativa sem evento.
  Isso não proíbe visitas recursivas de pré-requisitos.
- A primeira raiz com progresso parcial mantém a reserva de continuação.
  Uma raiz posterior não sobrescreve o follow-up conquistado pela primeira.
  A validação existente de `(setor, sequência)` continua antes da retomada.
- Deadline e limite de etapas permanecem os mesmos. A reserva temporal de
  fairness foi apenas extraída para uma função pura com a mesma fórmula;
  o drain continua antes das tentativas e sem poder estender o deadline.
- Com `generationVisibleBlockedAdvancePreview=false`, não há rastreamento
  de visitas nem alocação por candidato nesse helper. A política legada de
  continuação e tail permanece preservada nos cenários comparados.

A ordem de seleção **entre raízes** pode mudar com o preview ligado, como já
previsto por ele; não declarar equivalência global de callbacks a partir do
harness. Etapas, suas dependências e seus efeitos continuam executados pela
WorldThread. Não mudam os valores/regras de TTL, o save, o protocolo, a seed,
os contratos dos workers ou os defaults de configuração. Não há publicação
paralela nem implementação de mailbox executiva nesta entrega.

Novos contadores de cardinalidade fixa, quando há profiling:

- `generation.visibleBlockedAdvance.continuedWithContinuation`;
- `generation.visibleBlockedAdvance.repeatedCandidateSkipped`;
- `generation.visibleBlockedAdvance.distinctCandidatesExhausted`.

Os contadores não incluem nomes ou payloads de mods. As métricas anteriores
de continuação, avanços, limite, deadline e progresso continuam disponíveis.

## Harness, comparação e resultados

`StarWorldGenerationPassHarness.hpp` alimenta a política de produção com fila
de interesse, ticks e relógio virtual, disponibilidade controlada de quatro
estágios sintéticos por raiz e produtos determinísticos. O fixture verifica
publicação em ordem de etapa por raiz e compara produtos finais por chave,
sem confundir a ordenação entre raízes com a equivalência de seu produto.

No cenário mínimo, A conclui uma etapa e aguarda seu segundo produtor até
100 ms virtuais; B/C já têm todos os produtores disponíveis. Com ambos os
previews ativos e orçamento de 16 ms virtuais:

| Controle da passagem | Tentativas | Etapas avançadas | Raízes completas |
| --- | ---: | ---: | ---: |
| Guard antigo reinserido | 1 | 1 | 0 |
| Regra corrigida | 3 | 9 | 2 |

A passagem corrigida usa 9 ms **virtuais**, conserva A como continuação e
entrega B/C. Ao liberar os produtores e executar os ticks seguintes, os dois
controles chegam aos mesmos 12 produtos sintéticos. Esses números são
resultados do modelo controlado, não milissegundos de CPU nem setores/s reais.

Os 18 testes novos cobrem: preservação do legado, limites 1–16, continuação
e tail, visita única, fila com uma raiz permanentemente bloqueada, primeira
reserva, prioridade promovida, deadline e quota incluindo zero, produtos e
ordem por raiz, e conclusão já salva sem contá-la como terreno inédito.
Uma matriz combina avanço ligado/desligado, continuação ligada/desligada,
16 limites e 1/3/17 raízes: **192 combinações determinísticas**, cada uma com
ticks de 0 a 512 ms virtuais e disponibilidade controlada. No modo legado,
também compara tentativa, conclusão e progresso de cada passagem.

## Verificações executadas e reprodução local

Ambiente cloud pronto, sem instalação de dependências. Compilador local:
GCC 14.2.0, C++17, Linux. GoogleTest já versionado no projeto.

```bash
task_test_dir=$(mktemp -d)
g++ -std=c++17 -O1 -g -pthread -Wall -Wextra -Werror \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  -Isource/game -Isource/test \
  source/test/gtest/gtest-all.cc source/test/core_tests_main.cpp \
  source/test/world_generation_pass_test.cpp \
  source/test/world_generation_placement_queue_test.cpp \
  -o "$task_test_dir/world_generation_h0_tests"
"$task_test_dir/world_generation_h0_tests" --gtest_brief=1
```

Esse binário isolado passou 26/26 testes sem warnings tratados como erro e
sem diagnóstico de sanitizers. Também passaram `git diff --check` e parsing/
checagem de tipos de `StarWorldStorage.cpp` via `g++ -std=c++17 -fsyntax-only`,
nos modos normal e `STAR_ENABLE_TRACY`/`TRACY_ENABLE`, usando os headers Tracy
oficiais 0.11.2 já disponíveis. Syntax-only não é linking nem build completa.
Na documentação, foram conferidos 194 links locais em 18 arquivos, sem
destinos ausentes; os 13 módulos continuam alcançáveis e os 49 IDs são únicos
e referenciados no roadmap. A âncora de DAG-04 também foi validada.

A suíte foi registrada no alvo CMake `world_generation_contract_tests` e em
CTest como `world_generation_contract_WorldGenerationPassTest`, com rótulo
`NoAssets` e timeout 120 s,
para ambos os tipos de build existentes. Não foram executados o alvo CMake
completo, CTest, Actions, build completa ou testes Windows/macOS/ARM.

## Configuração e manifesto

Não houve nova sessão de jogo ou arquivo de config coletado. Para comparar
esse recorte no jogo, usar um executável contendo `2131ead3` e os controles
existentes; não basta mudar a config em uma build anterior:

```json
"worldThreading": {
  "generationVisibleChainContinuationPreview": true,
  "generationVisibleBlockedAdvancePreview": true,
  "generationVisibleBlockedCandidateLimit": 4
}
```

O bloco é um recorte para integrar na config existente, não o arquivo inteiro
nem um novo preset padrão. O controle A/B desliga apenas o avanço, mantendo
save inicial, rota, duração, mods e demais configurações iguais.

Artefatos de código/teste conferidos em 09/11/2026, SHA-256:

| Arquivo no repositório | Bytes | SHA-256 |
| --- | ---: | --- |
| `source/game/StarWorldGenerationPass.hpp` | 1.799 | `2728f62852ca8981f244a2a9feb4ee6391993f4c82d97108131726fa9b68c172` |
| `source/game/StarWorldStorage.cpp` | 122.171 | `b99a27c93ca1438b20c859ca8604c6e4f7bf5fcd8125023ab2f6bb5a19e4900b` |
| `source/test/StarWorldGenerationPassHarness.hpp` | 5.965 | `69c335e70168e3d37c4de12cdb689f77f788a13fd203e08cad23d7bb9dbaeded` |
| `source/test/world_generation_pass_test.cpp` | 10.411 | `b23c4e6d2fe657b2f9d3296007064207e48838b6f6f89c634f93d078888569b3` |

Nenhuma captura/log foi reanalisada ou adicionada ao Git. A evidência anterior
e seu manifesto continuam na [auditoria](sector-generation-architecture-audit-2026-11-08.md)
e no [baseline](sector-runtime/baseline.md); não atribuir à run 29 a validação
deste commit. Binários temporários ficam fora do repositório.

## Limites, riscos e próximo trabalho

O harness não instancia `WorldStorage` completo: seu construtor e carregamento
dependem de `Root` e assets. O fixture não executa o gerador recursivo 3×3,
TTL, IO, Lua, mods, multiplayer ou apresentação no cliente. Não constitui o
baseline com rota/save/config de VAL-01, nem comparador real de VAL-02.
As comparações sintéticas não liberam adoção do preview por padrão.

A visita única é um bound conservador: se o worker de uma raiz ficar pronto
depois de sua tentativa, essa raiz espera a próxima passagem. Retentativa
baseada em **nova conclusão** depende do grafo/mailbox futuros; não introduzir
polling repetido como substituto. Uma operação serial indivisível ainda pode
ultrapassar o deadline; o helper não preempta factory ou scripts.

DAG-04 está implementado/testado apenas no recorte continuação/avanço.
Mailbox, drain orientado por eventos e resultados compartilhados continuam
em H1. H0 e VAL-01/02 seguem abertos; faltam revisão independente e A/B real
com assets/mods, incluindo ordem observável, justiça, espera e stuttering.

Próxima implementação: RAM-01, começar pela reserva antes de copiar inputs
de BaseTiles/halo em `prepareTilesAsync`/`generateMicroDungeons`, com testes
de retenção, liberação, erro e overflow; deixar
explícitos bytes rasos versus profundos e o escopo processual ainda pendente.
Conservar MOD-01 e evoluir o harness para dependências/output reais. Não
aumentar admissão antes desse contrato. NPC externo segue suspenso, Linux
Clang adiado e Actions manuais; nenhum merge em `main` ou release foi feito.
