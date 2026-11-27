# H4 — Harness opt-in de probe OpenGL compute

Data: 27/11/2026 UTC. Branch `cursor/h4-gpu-shadow-9f77`.

[Checklist Windows/RTX](sector-runtime-h4-gpu-probe-windows-rtx-checklist-2026-11-27.md)
· [GPU compute](sector-runtime/gpu-compute.md)
· [Progressão](sector-runtime/progress.md)

## Objetivo

Entregar o mínimo para o validador Desktop-Vinicius (RTX) rodar probe real:
`probeCapabilities` sem hardcode falso no build GL, harness Windows, dispatch
DenseUint32* com fences/shaders, `compareShadow` GPU além de
`ShadowUnavailable`, e checklist runnable.

## Implementação

- `StarWorldGenerationComputeGl.{hpp,cpp}`: stub na build normal; com
  `-DSTAR_ENABLE_WORLDGEN_GL_COMPUTE=ON` cria contexto SDL3 oculto (4.3+),
  GLEW, detecta compute, compila shaders XOR/ADD, SSBO + `glDispatchCompute` +
  `glFenceSync` / `glClientWaitSync` + readback.
- `WorldGenerationComputeService::probeCapabilities()` delega ao device GL
  (claim só se compute detectado).
- `compareShadow` / submit GPU usam `dispatch` real quando a capacidade é
  verdadeira; caso contrário permanecem `ShadowUnavailable` / refuse.
- Harness: `world_generation_gl_compute_probe` (+ script
  `scripts/windows/run-h4-gl-compute-probe.ps1`).

## Maturidade

H4 permanece em **25%** até o validador local arquivar PASS em hardware.
Não inventar 50%. Sem merge INT. Sem Actions.

## Validação neste ambiente

Build normal (flag OFF) recompilada e `WorldGenerationComputeTest` executado
localmente. PASS do harness GL exige o worker Windows/RTX.
