# H5 — Scenario campaign recorder (VAL-04) + governor scale bridge

Data: 27/11/2026 UTC. Branch `cursor/h5-hardware-scale-9f77`.

[Gates/metrics](sector-runtime-h5-scale-gates-metrics-2026-11-27.md)
· [Progressao](sector-runtime/progress.md)

## Estado

Segundo recorte VAL-04 / CPU-05 (marco **permanece 25%**):

- `WorldGenerationScenarioCampaignRecorder` registra Passed/Failed/Skipped*
  sem streaming; skip exige capability ausente.
- `recordGpuScenariosFromScale` preenche cenarios GPU a partir de ScaleMetrics.
- `adviseWorldGenerationResourcesFromScale` prefere GPU so com routing ready +
  ganho medido.

## Validacao

Filtro H5: **40/40** local. Sem Actions. Sem merge INT.

## Integracao

phase `ready-for-integration` no tip anterior `ec3c7129`; este tip continua
acumulando contratos H5 para o Integrador puxar quando autorizado.
