# H4 — Clz/Ctz/Popcount + completeBatch + kernelName (asset-free)

Data: 27/11/2026 UTC. Branch `cursor/h4-gpu-shadow-9f77`.

[GPU compute](sector-runtime/gpu-compute.md) · [Progressão](sector-runtime/progress.md)

## Recortes acumulados (pós-INT `d2371654`)

1. **GPU-03:** `DenseUint32Clz` / `DenseUint32Ctz` / `DenseUint32Popcount`
   (CPU + GL opt-in; zero → 32 for clz/ctz; parameter ignored)
2. **GPU-02/01:** `completeBatch` + `kernelName` / `parseKernelName`

## Validação

30/30 `WorldGenerationComputeTest.*` (flag OFF).

## Maturidade

H4 permanece em **25%**. Sem terreno/bioma, multi-driver ou 50%.
