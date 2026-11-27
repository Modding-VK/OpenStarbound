# H5 — Scale profile advice (MOD-03)

Data: 27/11/2026 UTC. Branch `cursor/h5-hardware-scale-9f77`.

[Evidence bundle](sector-runtime-h5-scale-evidence-bundle-2026-11-27.md)
· [Progressao](sector-runtime/progress.md)

## Estado

`recommendWorldGenerationProfileFromScaleEvidence` sugere DynamicBias /
HighThroughput / CPU fallback a partir do evidence pack. **Sempre** exige
opt-in explicito; nunca renomeia `performanceMode` no lugar. Tambem:
`materializeTimingCalibrated` no VAL-03.

## Validacao

Filtro H5: **46/46** local. Marco **25%**. Sem merge INT.
