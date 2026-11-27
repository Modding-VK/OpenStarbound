# H4 / GPU-02: contabilização de staging / VRAM (asset-free)

Data: 27/11/2026 UTC. Branch `cursor/h4-gpu-shadow-9f77`.

[GPU compute](sector-runtime/gpu-compute.md)
· [RAM-01](sector-runtime/memory-and-prefetch.md)
· [Progressão](sector-runtime/progress.md)

## Objetivo

Próximo item **asset-free** após o gate `commit` (já na INT): cumprir
“Contabilizar staging RAM e VRAM em RAM-01” no serviço de compute, sem assets
base nem smoke Starbound.

## Implementação

- `WorldGenerationComputeStagingSnapshot` — staging atual/pico/limite, bytes e
  contagem do pool, open/in-flight, `gpuInFlight`, `estimatedVramBytes`
  (estimativa SSBO dos tickets GPU pendentes), `gpuCapable`
- `stagingSnapshot()` / `resetPeakStagingBytes()`
- Pico atualizado em cada `submit` admitido

## Fora de escopo

- Integração completa no envelope RAM-01 / WorldThread
- Medição de VRAM de driver real além da estimativa por staging
- 50% de marco; merge INT (Integrador)

## Validação

`WorldGenerationComputeTest.StagingSnapshotTracksPeakPoolAndGpuEstimate` e
suite na build normal (flag OFF).

## Maturidade

H4 / GPU-02 permanecem em **25%**: observação existe para consumidores RAM-01;
thread dona / fences assíncronos de driver ainda faltam.
