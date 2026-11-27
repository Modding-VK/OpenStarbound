# H4 — Shl/Shr + catalog + compareShadowBatch (asset-free)

Data: 27/11/2026 UTC. Branch `cursor/h4-gpu-shadow-9f77`.

[GPU compute](sector-runtime/gpu-compute.md) · [Progressão](sector-runtime/progress.md)

## Recortes acumulados

1. **GPU-03:** `DenseUint32Shl` / `DenseUint32Shr` (shift & 31; CPU + GL opt-in)
2. **GPU-01:** `isKernelSupported` / `supportedKernels` catalog
3. **GPU-04:** `compareShadowBatch` over a shared corpus

## Validação

26/26 `WorldGenerationComputeTest.*` (flag OFF).

## Maturidade

H4 permanece em **25%**. Sem terreno/bioma, multi-driver ou 50%.
