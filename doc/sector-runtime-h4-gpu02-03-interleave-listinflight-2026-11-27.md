# H4 — ExpandLow16ToEven/Interleave/DeinterleaveHalves + listInFlightByKernel (asset-free)

Data: 27/11/2026 UTC. Branch `cursor/h4-gpu-shadow-9f77`. Após INT tip `f00a158a`.

[GPU compute](sector-runtime/gpu-compute.md) · [Progressão](sector-runtime/progress.md)

## Recortes acumulados

1. **GPU-03:** `DenseUint32ExpandLow16ToEven` / `DenseUint32InterleaveHalves` /
   `DenseUint32DeinterleaveHalves` (CPU + GL opt-in)
2. **GPU-02:** `listInFlightByKernel`

## Validação

78/78 `WorldGenerationComputeTest.*` (flag OFF).

## Maturidade

H4 permanece em **25%**. Sem terreno/bioma, multi-driver ou 50%.
