# H4 — Nand/Nor/MulHi + ticketIsOpen + rejected-duplicate reset (asset-free)

Data: 27/11/2026 UTC. Branch `cursor/h4-gpu-shadow-9f77`.

[GPU compute](sector-runtime/gpu-compute.md) · [Progressão](sector-runtime/progress.md)

## Recortes acumulados (em paralelo ao merge INT de `2bbbda52`)

1. **GPU-03:** `DenseUint32Nand` / `DenseUint32Nor` / `DenseUint32MulHi`
   (CPU + GL opt-in; MulHi = high 32 de uint64 product)
2. **GPU-02:** `ticketIsOpen` / `rejectedDuplicateCount` /
   `clearRejectedDuplicates`

## Validação

38/38 `WorldGenerationComputeTest.*` (flag OFF).

## Maturidade

H4 permanece em **25%**. Sem terreno/bioma, multi-driver ou 50%.
