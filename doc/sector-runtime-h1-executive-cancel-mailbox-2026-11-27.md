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
- **WT-02** 0→25%: `runPass` drena e inicia sob orçamento (`maxStarts` /
  `maxDrainMessages` / `deadlineTick`) e devolve
  `WorldGenerationExecutivePassExitReason` comparável; sem dt de simulação
  acelerado.

24 testes `WorldGenerationExecutive*` (ASan/UBSan, `STAR_BUILD_GUI=OFF`). Sem
Actions. Sem ganho de setores/s medido.

## Artefatos

| Artefato | Papel |
| --- | --- |
| `StarWorldGenerationExecutiveShadow.*` | Fases, cancel, retry, drain |
| `StarWorldGenerationExecutiveMailbox.*` | Post/consume/apply |
| `StarWorldGenerationExecutiveScheduler.*` | startNext / complete / drain |
| `world_generation_executive_*_test.cpp` | 21 casos sombra |

## Limitações

Sem WorldStorage, WorkerPool, política TTL/load, herança completa de cadeia
auxiliar nem publicação autoritativa. Capacidade global multi-mundo, classes
IO/GPU e separação completa sim/prep/publish no WorldThread vivo ficam fora.

## Próximo passo

CPU-02/MD-01 leves ou integração controlada. Não disparar Actions nem merge
em `main`.
