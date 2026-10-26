# Backlog de desempenho da exploração

As hipóteses de rework discutidas para setores de 64 × 64, dungeons compostas,
pré-geração e cálculo paralelo de microdungeons estão em
[world-generation-rework-hypotheses.md](world-generation-rework-hypotheses.md).
O plano de instrumentação por linha do tempo está em
[tracy-integration-plan.md](tracy-integration-plan.md).
O painel local do HUD e suas métricas estão em
[performance-overlay.md](performance-overlay.md).
O painel sob demanda de threads e pools está em
[thread-panel.md](thread-panel.md).
As pesquisas de operações estão separadas entre
[IA dos NPCs](npc-ai-parallelism-hypotheses.md) e
[objetos e roteadores](object-operations.md).
O [plano de ação de rework dos NPCs](npc-ai-rework-action-plan.md) organiza
as fases e seus critérios de aprovação. O primeiro marco é medir, isolar
leituras e comparar cálculos paralelos sem aplicar efeitos (fases 0–2);
a fase 0 recebeu [instrumentação detalhada de NPCs](npc-profiling.md), mas
baseline e aprovação ainda estão pendentes. As fases 1–7 não foram iniciadas.
O plano de workers e API para modders desses objetos está em
[object-worker-architecture-plan.md](object-worker-architecture-plan.md).
Novas dependências e ferramentas seguem a política de
[procedência de código externo](dependency-sources.md); suas pendências de CI
estão listadas nesse documento.

## Estado em 24/10/2026

Branch de trabalho: `perf/threading-audit`. A build Windows #21 compilou o
commit `70a4977f` e produziu o instalador. Ainda falta testar no jogo a fila de
prefetch de tiles básicos, introduzida em `3f848041`. `sectorWorkers` precisa
ser maior que zero para que o prefetch seja usado; começar com `2`.

O log anterior à fila de prefetch mostrou `generation.timeBudgetExhausted` em
algumas janelas, mas não mediu o tempo de espera de cada setor. Portanto, não
atribuir a demora do vilarejo Mantizi ao orçamento sem a próxima coleta.

## Atualização de evidência — 26/10/2026

O log da build `6bc48199` reproduziu a fila travada no mesmo planeta, agora
com as métricas detalhadas. Houve espera concluída de 40,6 s e pedido ainda
pendente com 60,0 s. Entre 1.295 resultados de tiles básicos coletados, 1.115
foram invalidados por mudança na revisão do setor; 2.272 resultados prontos
foram descartados para abrir vagas no limite de tarefas pendentes. O pool
tinha dois workers e aparecia sem trabalho nas amostras, enquanto a fila de
setores continuava longa. Ver [dados e limites](performance-baseline.md).

**Próxima prioridade:** localizar os caminhos que incrementam a revisão dos
setores pendentes e medir coordenadas de resultados invalidados/descartados.
Depois, avaliar uma política que preserve resultados prontos ainda solicitados
até sua aplicação ou invalidação, com limite de memória explícito. Repetir o
mesmo snapshot e rota para comparar espera, tentativas refeitas e integridade
dos tiles. Aumentar o número de workers ou o orçamento por tick não é a
primeira mudança indicada por este registro.
A [investigação de código](sector-generation-stall-investigation.md) encontrou
um caminho de consulta de colisão que eleva a revisão mesmo sem alteração
persistente; a participação desse caminho nos eventos do log ainda precisa
ser medida antes de modificar a regra de invalidação.

## Próxima coleta: vilarejo Mantizi

1. Guardar cópia do jogador e universo antes de entrar na região inédita.
2. Usar a build #21 com os mesmos mods, resolução, zoom e configuração de
   `sectorWorkers` da coleta anterior. Ativar `explorationProfiling` conforme
   [exploration-profiling.md](exploration-profiling.md).
3. Registrar vídeo e horário aproximado de entrada no vilarejo, primeira área
   incompleta e momento em que a estrutura fica inteira.
4. Enviar o `starbound.log` completo e informar se o setor visitado já tinha
   sido gerado antes. Para comparar duas builds, restaurar a mesma cópia de
   universo antes de cada execução.

Ler `generation.sectorWait` e `generation.oldestQueuedAge` junto com
`generation.queueDepthTotal / generation.queueDepthSamples`. Conferir
`generation.prefetchCompleted`, `generation.baseTiles.worker`,
`generation.microDungeons`, `generation.finalize` e
`generation.timeBudgetExhausted`. Um contador de prefetch alto sozinho não
prova que a estrutura ficou pronta antes: ele cobre só os tiles básicos.

## Decisões após a coleta

| Evidência | Próxima mudança | Critério de aceitação |
| --- | --- | --- |
| Espera alta com tiles básicos ainda pendentes | Limitar e ordenar o prefetch por distância, medir pedidos, descartes e tempo de conclusão | Reduzir `generation.sectorWait` na mesma rota sem elevar atrasos dos setores visíveis |
| Tiles básicos prontos, espera concentrada em `microDungeons` ou `finalize` | Priorizar a conclusão de setores visíveis já iniciados; preservar dependências entre vizinhos | Estruturas aparecem antes, sem tiles faltantes, alterações de dungeon ou divergência ao recarregar |
| Limite de tempo atingido com fila visível longa | Avaliar orçamento adaptativo com teto por fidelidade e folga do frame | Reduzir espera sem aumentar picos perceptíveis no cliente e na thread do mundo |
| `painter.environment` domina mesmo em área já gerada | Instrumentar suas subetapas antes de alterar iluminação | Identificar custo dominante e preservar aparência em mundos e mods |

## Trabalho posterior

- Investigar pathfinding de NPC com snapshot imutável, conforme
  [world-threading.md](world-threading.md). Lua, movimento, dano e escrita do
  mundo continuam na thread do mundo até existir uma fronteira segura.
- **Pesquisa futura de alto risco e alta complexidade:** avaliar a externalização
  completa do update dos NPCs para workers. O acesso direto e simultâneo ao
  estado vivo do mundo não garante leituras consistentes; um protótipo exigiria
  uma visão coerente por tick, comandos de efeito aplicados em ordem pela thread
  do mundo e regras para resultados obsoletos. Antes de considerar a migração,
  mapear todas as chamadas de IA e Lua que leem ou modificam terreno, objetos e
  outras entidades; medir contenção, custo de snapshots e descartes; demonstrar
  equivalência de comportamento para mods, multiplayer, persistência e ordem de
  eventos. Não tratar este item como extensão automática da fila de pathfinding.
  Histórico, evidências e desenho hipotético em
  [npc-ai-parallelism-hypotheses.md](npc-ai-parallelism-hypotheses.md).
- Medir separadamente inicialização e callbacks Lua de entidades; o contador
  atual `luaTotalMilliseconds` cobre apenas o `update` periódico.
- Considerar instalador comum sem PDBs e símbolos em artefato separado para
  reduzir tempo de empacotamento e tamanho do download. Medir o ganho real
  antes de alterar a distribuição.

Em qualquer mudança de geração, conferir recarregamento do mesmo mundo,
estruturas e iluminação, NPCs, Frackin' Universe e multiplayer antes de
considerar a otimização concluída.
