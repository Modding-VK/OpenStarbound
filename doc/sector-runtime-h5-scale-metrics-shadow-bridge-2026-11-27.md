# H5 — Scale metrics bridge for H4 shadow observations

Data: 27/11/2026 UTC. Branch `cursor/h5-hardware-scale-9f77`.

[Fundacoes H5](sector-runtime-h5-scale-foundations-2026-11-27.md)
· [GPU compute](sector-runtime/gpu-compute.md)
· [Progressao](sector-runtime/progress.md)

## Estado

Segundo recorte isolavel VAL-03/GPU-05 (permanece marco **25%**):
`WorldGenerationScaleMetricsRecorder` agrega observacoes no formato H4
(shadow Equal/Diverged/Unavailable, familia DenseUint32* ate
HalfSignMask/HalfIsZero/HalfIsNonzero, `anyPendingOpenKernels`) e expoe
amostra de roteamento para `WorldGenerationHardwareSelector`.

Nao e driver GL, nao publica GPU, nao sobe maturidade para 50%. Ganho medido
so quando lotes Equal existem e o custo GPU agregado e menor que o CPU.

## Validacao

`WorldGenerationScaleMetricsTest` — **6/6** no alvo
`world_generation_contract_tests` (junto com as 8 suites H5 anteriores =
**33/33** no filtro scale). Sem Actions. Sem merge INT.

## Desktop

Probe local: tip desta branch + checklist Windows atualizado; coordenar via
`front-status/desktop-v-pessoal.md` (Instalaveis smoke ja OK boot+1166).

## Proximo passo

Desktop puxa tip e roda filtro H5 incl. ScaleMetrics. Depois: alimentar
amostras reais do probe H4 `1a7570cd` sem merge INT.
