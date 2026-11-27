# H4 — Rotl/Rotr/Not + cancelBatch/pollBatch (asset-free)

Data: 27/11/2026 UTC. Branch `cursor/h4-gpu-shadow-9f77`.

[GPU compute](sector-runtime/gpu-compute.md) · [Progressão](sector-runtime/progress.md)

## Recortes acumulados (pós-INT `8ccfd9ba`)

1. **GPU-03:** `DenseUint32Rotl` / `DenseUint32Rotr` / `DenseUint32Not`
   (CPU + GL opt-in; rot & 31; Not ignora parameter)
2. **GPU-02:** `cancelBatch` / `pollBatch` (resultados independentes por ticket)

## Validação

28/28 `WorldGenerationComputeTest.*` (flag OFF).

## Maturidade

H4 permanece em **25%**. Sem terreno/bioma, multi-driver ou 50%.
