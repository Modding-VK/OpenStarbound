# H4 — Lt/Leu/AbsDiff + backendName + summarizeShadowReports (asset-free)

Data: 27/11/2026 UTC. Branch `cursor/h4-gpu-shadow-9f77`.

[GPU compute](sector-runtime/gpu-compute.md) · [Progressão](sector-runtime/progress.md)

## Recortes acumulados (pós-INT `6cf7cd4b`)

1. **GPU-03:** `DenseUint32Lt` / `DenseUint32Leu` / `DenseUint32AbsDiff`
   (CPU + GL opt-in; unsigned compares / absdiff)
2. **GPU-01/04:** `backendName` / `parseBackendName` + `summarizeShadowReports`

## Validação

34/34 `WorldGenerationComputeTest.*` (flag OFF).

## Maturidade

H4 permanece em **25%**. Sem terreno/bioma, multi-driver ou 50%.
