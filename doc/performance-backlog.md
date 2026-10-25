# Backlog de desempenho da exploração

As hipóteses de rework discutidas para setores de 64 × 64, dungeons compostas,
pré-geração e cálculo paralelo de microdungeons estão em
[world-generation-rework-hypotheses.md](world-generation-rework-hypotheses.md).

## Estado em 24/10/2026

Branch de trabalho: `perf/threading-audit`. A build Windows #21 compilou o
commit `70a4977f` e produziu o instalador. Ainda falta testar no jogo a fila de
prefetch de tiles básicos, introduzida em `3f848041`. `sectorWorkers` precisa
ser maior que zero para que o prefetch seja usado; começar com `2`.

O log anterior à fila de prefetch mostrou `generation.timeBudgetExhausted` em
algumas janelas, mas não mediu o tempo de espera de cada setor. Portanto, não
atribuir a demora do vilarejo Mantizi ao orçamento sem a próxima coleta.

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
- Medir separadamente inicialização e callbacks Lua de entidades; o contador
  atual `luaTotalMilliseconds` cobre apenas o `update` periódico.
- Considerar instalador comum sem PDBs e símbolos em artefato separado para
  reduzir tempo de empacotamento e tamanho do download. Medir o ganho real
  antes de alterar a distribuição.

Em qualquer mudança de geração, conferir recarregamento do mesmo mundo,
estruturas e iluminação, NPCs, Frackin' Universe e multiplayer antes de
considerar a otimização concluída.
