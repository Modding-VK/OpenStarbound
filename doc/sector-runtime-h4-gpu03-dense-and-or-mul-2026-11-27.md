# H4 / GPU-03: DenseUint32 And / Or / Mul (asset-free)

Data: 27/11/2026 UTC. Branch `cursor/h4-gpu-shadow-9f77`.

[GPU compute](sector-runtime/gpu-compute.md)
· [Probe harness](sector-runtime-h4-gpu-probe-harness-2026-11-27.md)
· [Progressão](sector-runtime/progress.md)

## Objetivo

Após o probe GL (já na INT / PASS RTX), avançar o próximo item **asset-free**
da família GPU-03 “máscaras/transformações inteiras densas”: `DenseUint32And`,
`DenseUint32Or`, `DenseUint32Mul` (multiplicação wrapping uint32), com CPU
reference, shaders opt-in e sombra CPU↔CPU (e CPU↔GPU quando a flag GL está
ligada).

## Fora de escopo neste recorte

- Campos de terreno/bioma (precisam de operadores/assets de geração)
- Ganho ponta a ponta / 50% de marco
- Smoke ou assets base Starbound
- Merge INT (Integrador)

## Validação

`WorldGenerationComputeTest.*` na build normal (flag OFF). Harness opt-in
estendido para os três kernels novos.

## Maturidade

H4 / GPU-03 permanecem em **25%**: a família densa cresceu, mas terreno/bioma,
classificação e ganho medido ainda faltam para 50%.
