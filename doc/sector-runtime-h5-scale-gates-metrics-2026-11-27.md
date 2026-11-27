# H5 — Scale gates + calibrated transfer metrics

Data: 27/11/2026 UTC. Branch `cursor/h5-hardware-scale-9f77`.

[Scale bridge](sector-runtime-h5-scale-metrics-shadow-bridge-2026-11-27.md)
· [Progressao](sector-runtime/progress.md)

## Estado

Terceiro recorte isolavel VAL-03/VAL-05 (marco **permanece 25%**):

1. `WorldGenerationResourceMetric.transferTimingCalibrated` + `transferMs`
   alimentam `tryObservedDeliveryCost` sem inventar ms a partir de bytes.
2. `worldGenerationAdoptionFromScaleMetrics` + `summarizeWorldGenerationScaleGates`
   preenchem `outputComparison` / `measuredDeliveryGain` a partir de campanhas
   sombra Half*; **nunca** adotam GPU autoritativa sozinhos.

## Validacao

Filtro H5 scale: **36/36** `world_generation_contract_tests` (local).
Sem Actions. Sem merge INT. Sem 50%.

## Desktop

Probe tip apos push; checklist/script esperam 36 testes.
