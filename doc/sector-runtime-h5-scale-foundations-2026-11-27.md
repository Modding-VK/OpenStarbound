# H5: fundações isoláveis de escala (VAL/MOD/CPU)

Data: 27/11/2026 UTC. Branch `cursor/h5-hardware-scale-9f77`, base `720f51e5`.

[GPU-05 seletor](sector-runtime-h5-hardware-selection-2026-11-27.md)
· [Progressão](sector-runtime/progress.md)

## Estado: marcos 25% nos contratos H5 restantes + CPU-05

Recortes puros (sem driver GPU, sem publicação, sem Actions):

| Contrato | Artefato | Testes |
| --- | --- | --- |
| VAL-03 | `StarWorldGenerationDeliveryMetrics.hpp` | 3 |
| VAL-04 | `StarWorldGenerationScenarioMatrix.hpp` | 2 |
| VAL-05 | `StarWorldGenerationAdoptionGates.hpp` | 4 |
| MOD-02 | `StarWorldGenerationEquivalenceVersion.hpp` | 3 |
| MOD-03 | `StarWorldGenerationExecutionProfile.hpp` | 3 |
| MOD-04 | `StarWorldGenerationPlatformMatrix.hpp` | 2 |
| CPU-05 | `StarWorldGenerationResourceGovernor.hpp` | 3 |
| GPU-05 (prévio) | `StarWorldGenerationHardwareSelection.hpp` | 7 |

Total H5 nesta branch: **27/27** contract tests (ASan/UBSan).

## Limites

Não há backend GPU, benchmark de streaming, mailbox H1 nem adoção
autoritativa. `evaluateWorldGenerationAdoption` permanece fechado sob evidência
parcial. Modos `legacy`/`adaptive`/`aggressive`/`dynamic` não foram renomeados.
Linux Clang continua adiado; GPU compute opcional com fallback CPU.

## Maturidade

- H5: **7/28 marcos → 25%**
- H1: CPU-05 **0→25%** (2/40 marcos de fase → 5%)
- Programa: **15/196 → 7,7%**

## Próximo passo

H4 GPU shadow real (probe RTX PASS @ `c7f6e605`). Probe Windows dos 27
contratos H5: [checklist RTX](sector-runtime-h5-windows-rtx-probe-checklist-2026-11-27.md)
(+ `scripts/windows/run-h5-contract-probe.ps1`). Após PASS do validador,
alimentar seletor/governador com capabilities reais. Sem merge INT; sem Actions.
