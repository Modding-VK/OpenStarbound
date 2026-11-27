# H1 / CPU-05 — governador de recursos em sombra

Data: 27/11/2026 UTC. Branch `cursor/h1-graph-cpu-9f77`.

[Prepared sector](sector-runtime-h1-prepared-sector-2026-11-27.md)
· [Cancel/mailbox/scheduler](sector-runtime-h1-executive-cancel-mailbox-2026-11-27.md)
· [Progressão](sector-runtime/progress.md)

## Objetivo

Primeira política de capacidade por fila executável e custo observado, sem
redimensionar WorkerPools nem alterar `dt` de simulação.

## Estado

- **CPU-05** 0→25%: `WorldGenerationResourceGovernor` com perfis Balanced /
  HighThroughput; `suggest` devolve `WorldGenerationExecutivePassBudget` a
  partir de frontier, mailbox, produtos retidos e overrun vs target.

4 testes `WorldGenerationResourceGovernor*`. Sem Actions. Sem medição de
render/GPU integrada nem estacionamento físico de workers.

## Próximo passo

Integração controlada dos recortes H1 no caminho vivo, ou gates H0 restantes
em `Experimental`. Não disparar Actions.
