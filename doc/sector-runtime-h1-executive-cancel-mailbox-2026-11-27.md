# H1 / DAG-05 + mailbox DAG-04 + scheduler sombra

Data: 27/11/2026 UTC. Branch `cursor/h1-graph-cpu-9f77`.

[Interest/priority](sector-runtime-h1-executive-interest-priority-2026-11-27.md)
· [Progressão](sector-runtime/progress.md)

## Objetivo

Primeiro recorte funcional de cancelamento/falha/invalidação (DAG-05), mailbox
de conclusões em sombra (evidência adicional de DAG-04) e executor processual
mínimo com drenagem (CPU-03 / WT-01), sem escrever o mundo vivo.

## Estado

- **DAG-05** 0→25%: `invalidate` / `cancel` / `markExecuting` / `drainDiscarded` /
  `noteRetry` na sombra; invalidar não prova parada do produtor; drain só após
  `executing=false`.
- **DAG-04** permanece 25%: `WorldGenerationExecutiveMailbox` transporta token
  imutável e separa ticks de término/observação; `applyToShadow` não liga
  WorldStorage. Ainda não há reação na mesma passagem ao vivo.
- **CPU-03** 0→25%: `WorldGenerationExecutiveScheduler::startNext` escolhe
  frontier justa, marca executing e devolve passo com token; capacidade da
  mailbox limita backlog; sem pool física nem afinidade.
- **WT-01** 0→25%: `complete` + `drainMailbox` aplicam sucesso/falha à sombra
  sem esperar rodadas artificiais; falha invalida sem publish.

21 testes `WorldGenerationExecutive*` (ASan/UBSan, `STAR_BUILD_GUI=OFF`). Sem
Actions. Sem ganho de setores/s medido.

## Artefatos

| Artefato | Papel |
| --- | --- |
| `StarWorldGenerationExecutiveShadow.*` | Fases, cancel, retry, drain |
| `StarWorldGenerationExecutiveMailbox.*` | Post/consume/apply |
| `StarWorldGenerationExecutiveScheduler.*` | startNext / complete / drain |
| `world_generation_executive_*_test.cpp` | 21 casos sombra |

## Limitações

Sem WorldStorage, WorkerPool, orçamento de frame (WT-02), política TTL/load,
herança completa de cadeia auxiliar nem publicação autoritativa. Capacidade
global multi-mundo e classes IO/GPU ficam fora deste recorte.

## Próximo passo

WT-02 (motivos de saída/orçamento) em sombra, ou CPU-02/MD-01 leves. Não
disparar Actions nem merge em `main`.
