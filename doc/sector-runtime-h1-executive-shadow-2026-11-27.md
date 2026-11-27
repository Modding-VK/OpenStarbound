# H1 / DAG-02: grafo executivo persistente em sombra

Data: 27/11/2026 UTC. Branch `cursor/h1-graph-cpu-9f77`, base `e55c008f`.

[Contrato DAG-02](sector-runtime/dependency-scheduler.md#dag-02--grafo-persistente-e-demanda-compartilhada)
· [Progressão](sector-runtime/progress.md)

## Estado: primeiro marco (25%)

`WorldGenerationExecutiveShadowGraph` retém nós compartilhados de preparação,
arestas de preparação vs publicação em conjuntos distintos, demanda por
consumidor e comparação ao plano recursivo de
`WorldGenerationDependencyGraph::build`, registrando divergências sem alterar
resultados de produção nem publicar no mundo vivo.

## Escopo deste recorte

- Ingestão deduplicada de planos recursivos na sombra.
- `addDemand` / `removeDemand`: remover um consumidor não cancela o nó enquanto
  outro permanece.
- Comparação preparation↔plano e publication↔plano.
- Sem mailbox, sem prioridade (DAG-03), sem cancelamento físico (DAG-05), sem
  ligação a `WorldStorage`.

## Validação

`world_generation_contract_tests`: 5 casos novos
`WorldGenerationExecutiveShadowTest` + 7 `WorldGenerationDependencyTest`
aprovados (ASan/UBSan halt_on_error). Sem Actions.

## Próximo passo

Estender a sombra com interesse/unload e antecipação pura; depois DAG-03
(prioridade/justiça). Não disparar Actions.
