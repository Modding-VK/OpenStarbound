# Programa principal de setores: adoção e primeiro recorte de H0

Data: 08/11/2026. Branch de entrega: `Experimental`; branch local de revisão:
`codex/experimental-regional-runtime`. Base documental: `aa743068`; runtime de
partida: `7ca7ed2a`. Implementação deste recorte:
`d72b72394807a29060ece58ba89e11d966903cf2`.

[Programa principal](sector-runtime/README.md) · [Roadmap](sector-runtime/roadmap.md)
· [Contrato DAG-01](sector-runtime/dependency-scheduler.md#dag-01--identidades-estáveis)

## Objetivo e decisão operacional

O usuário adotou o novo programa como plano principal e autorizou iniciar seu
trabalho. O índice, roadmap, backlog, checklist, README e `AGENTS.md` passam a
apontar para uma única sequência: H0–H5 e evolução regional R1–R2. Planos
anteriores conservam evidência e decisões compatíveis, não uma fila concorrente.

O objetivo continua sendo aumentar substancialmente a entrega de setores
comuns com uso útil de CPU, RAM e GPU disponível, sem vincular o desenho ao
5600G ou a um número fixo de núcleos. Primeiro, estabelecer invariantes e
validação acelerada. Compatibilidade com mods/saves/multiplayer é bloqueante;
NPC externo segue suspenso, Linux Clang adiado e Actions manuais.

O recorte de DAG-01 não desloca o foco para otimização de TMX. Ele remove um
bloqueador de correção antes de permitir mais preparação simultânea no
pipeline comum. Não há nesta entrega novo executor, GPU compute, expansão de
orçamento ou threads regionais autoritativas.

## Evidência de código e causa

Na base `7ca7ed2a`, snapshots e descritores de `MicroDungeonPlan` retinham
índices de `m_queuedPlacements`. O sucesso de outro plano filtrava essa lista
e compactava seus elementos enquanto produtores assíncronos ainda podiam
conservar os índices anteriores.

Exemplo: A/B/C ocupam índices 0/1/2; um plano cumpre A e a limpeza move B/C
para 0/1. Um resultado antigo de B pode então acessar C; o de C pode acessar
fora da lista. É uma falha estabelecida por inspeção e reproduzida como
sequência de eventos no teste de regressão, não a atribuição de um crash ou
de uma latência específica das capturas antigas a essa causa.

O caminho anterior também mantinha um ponteiro para a entrada através da
chamada à factory viva, cuja fachada pode enfileirar colocações. A correção
não conserva ponteiro/referência à entrada através dessa chamada.

## Implementação e fronteiras

- `WorldGenerationPlacementQueue` é um registro exclusivamente da dona;
  atribui IDs de 64 bits monotônicos, não reaproveita IDs removidos e rejeita
  overflow em vez de dar wrap. Não é uma fila concorrente de workers.
- O registro ordenado preserva a ordem relativa das inserções sobreviventes,
  inclusive no desempate dos candidatos. Busca/remoção são logarítmicas;
  desaparece a reconstrução da lista inteira a cada limpeza. Há alocação de
  nó por colocação pendente; não foi medido ganho de custo deste recipiente.
- Snapshots e planos, síncronos e assíncronos, carregam o ID e cópias das
  distribuições, nunca a promise nem acesso ao registro vivo.
- ID ausente é ignorado antes da factory. Falha de colocação mantém a entrada
  pendente. Após sucesso, a dona retira a entrada e então cumpre sua promise;
  resultados posteriores com aquele ID não geram nem cumprem novamente.
- A factory permanece serial e não reentrante; a dona recupera a entrada
  depois de seu retorno. Nenhuma thread nova escreve tiles ou chama Lua/RPC.

A identidade é local à instância de `WorldGenerator`. O caminho assíncrono
mantém as validações existentes de tarefa/epoch, template e revisões; este
registro não substitui esses contratos. Não há API pública nova de
cancelamento de colocações nem migração do estado entre mundos. Versionar o
conjunto completo de inputs e resultados vazios continua em DAG-02/CPU-02.

Seed, escolha de dungeon, ordem relativa de candidatos, pré-requisitos de
etapa, TTL, save e protocolo permanecem inalterados. A promise continua no
mesmo ponto serial de sucesso da factory, antes do pós-processamento dos
tiles, como no legado. Não são adicionadas chaves de configuração.
As fases de profiling `generation.microDungeonPlan.apply.cleanup` e
`generation.microDungeonPlan.cleanup` deixam de ser emitidas porque a limpeza
por reconstrução foi eliminada; não interpretar sua ausência como perda de
uma fase ainda existente.

## Validações executadas

Compilador local: GCC 14.2.0, C++17, Linux. Foram executados:

1. Oito testes de `WorldGenerationPlacementQueueTest`, compilados diretamente
   com o GoogleTest já versionado no projeto, `-O1 -g -pthread -Wall -Wextra`
   e `-fsanitize=address,undefined -fno-omit-frame-pointer`: **8/8 passaram**,
   sem diagnóstico dos sanitizers. Exercitam o registro usado pelo runtime,
   não uma cópia do algoritmo de produção.
2. `g++ -std=c++17 -fsyntax-only` em `StarWorldGeneration.cpp`, com os
   diretórios de includes do projeto: aprovado.
3. A mesma checagem com `STAR_ENABLE_TRACY`, `TRACY_ENABLE` e headers do Tracy
   oficial 0.11.2 disponíveis no ambiente: aprovada. Isso verifica parsing e
   tipos, não linking de uma build Tracy.
4. `git diff --check`: aprovado.
5. Documentação: 229 links locais conferidos em 22 arquivos, sem destino
   ausente; os 13 módulos são alcançáveis pelo índice e os 49 IDs de contrato
   são únicos/referenciados no roadmap, incluindo sua notação abreviada.
   A âncora de DAG-01 usada neste relatório também foi conferida.

Os testes cobrem remoção frontal com planos antigos de B/C, ID retirado
seguido por nova inserção, retorno invertido/duplicado, conclusão única,
retry após falha, 1.024 inserções durante a geração simulada, ordem de
inserção, ID desconhecido e payload exclusivamente movível. O cenário de
remoção usa a operação real do registro; não oferece uma API de cancelamento
ao jogo. O ramo de exaustão de 64 bits não foi alcançado nesses testes.

A nova suíte também está registrada no alvo CMake
`world_generation_contract_tests` e em CTest com rótulo `NoAssets`/timeout de
120 s. Não foi executado esse alvo CMake completo nem CTest nesta atividade;
a execução acima foi o binário isolado da nova suíte.

Não foram feitos build completo do jogo, teste Windows/macOS/ARM, execução
com assets/mods, multiplayer, A/B de streaming ou Actions. O teste de
conclusão única simula o efeito de cumprimento com um contador; não executa
Lua nem uma promise RPC real. Portanto DAG-01 está validado apenas nesse
escopo; não é adoção universal do novo pipeline nem prova de aceleração.

## Configuração e manifesto

Não houve nova sessão de jogo nem config de usuário coletada. A correção não
exige ajuste em `worldThreading`; ambos os caminhos de planejamento usam o
registro. Nenhum preview foi habilitado por padrão.

Artefatos de implementação/teste conferidos em 08/11/2026 (SHA-256):

| Arquivo no repositório | Bytes | SHA-256 |
| --- | ---: | --- |
| `source/game/StarWorldGenerationPlacementQueue.hpp` | 1.735 | `6c1a2051860b27bf2d10724a9772410514679d7094e03bf010d9485e9ed1012f` |
| `source/game/StarWorldGeneration.cpp` | 111.667 | `832e2680804199748484ea79687412ff1a310274bd1fae1ffb6b0819f3143fd5` |
| `source/test/world_generation_placement_queue_test.cpp` | 4.026 | `50cc037bbbad00aa3095284af7ec23d5f1ab7857fb8791cca8bc85dcb551c6a3` |

Nenhum log/captura foi reanalisado ou incluído no Git. O diagnóstico de
desempenho de partida e o manifesto das runs anteriores permanecem na
[auditoria de arquitetura](sector-generation-architecture-audit-2026-11-08.md)
e no [baseline](sector-runtime/baseline.md). Binários temporários dos testes
permanecem fora do repositório.

## Estado e próxima implementação

H0 está ativo. O recorte de identidade de DAG-01 foi implementado; o item
maior continua aberto para validação integrada. Os demais itens não são
considerados concluídos por essa correção.

Próximo passo: VAL-01/02, iniciar harness de streaming com interesse/ticks
determinísticos e conclusão controlada de produtores. Primeiro reproduzir a
interação de DAG-04: continuação de cadeia, avanço do preview e encerramento
antecipado da passagem. Comparar ordem/outputs com a referência antes de
alterar o drain; depois medir setores inéditos publicáveis/s e pronto→commit.
O harness sem assets complementa, mas não substitui, rota/save/mods fixos.

Só ampliar admissão após o contrato de memória RAM-01 e os testes de H0.
Continuam pendentes revisão independente e validação com assets antes de
adotar uma nova política ou integrar mudanças de alto impacto em `main`.
