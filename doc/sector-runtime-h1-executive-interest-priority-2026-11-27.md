# H1 / DAG-02+DAG-03: interesse/unload/antecipação e prioridade em sombra

Data: 27/11/2026 UTC. Branch `cursor/h1-graph-cpu-9f77`, base `857cf5fd`.

[Executive shadow](sector-runtime-h1-executive-shadow-2026-11-27.md)
· [Progressão](sector-runtime/progress.md)

## Estado

- **DAG-02** permanece em 25%: `applyInterest` / `clearInterest` / `anticipatePreparation` /
  `unloadSector` / frontier retida. Ainda faltam interesse vivo e confronto no
  caminho de geração para 50%.
- **DAG-03** 0→25%: `WorldGenerationExecutivePriority` com distância wrap,
  classe de interesse, idade e `selectFair` sem starvation óbvia por demanda
  Background densa.

Sem publicação no mundo vivo. Sem Actions.

## Validação

11 testes `WorldGenerationExecutive*` ASan/UBSan.

## Próximo passo

DAG-05 sombra (cancel/invalidate/drain) ou mailbox DAG-04. Não disparar Actions.
