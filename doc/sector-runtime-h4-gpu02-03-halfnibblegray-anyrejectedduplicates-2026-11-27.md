# H4 — Kernel uint16_t + HalfSwap/GrayEncode/GrayDecodeNibble + anyRejectedDuplicates

Data: 27/11/2026 UTC. Branch `cursor/h4-gpu-shadow-9f77`. Após lote `a132e680`.

[GPU compute](sector-runtime/gpu-compute.md) · [Progressão](sector-runtime/progress.md)

## Recortes acumulados

1. **GPU-01/03:** `WorldGenerationComputeKernel` promovido a `uint16_t` (ids >255)
2. **GPU-03:** `DenseUint32HalfSwapAdjacentBitsNibble` /
   `DenseUint32HalfGrayEncodeNibble` / `DenseUint32HalfGrayDecodeNibble`
3. **GPU-02:** `anyRejectedDuplicates`

## Validação

190/190 `WorldGenerationComputeTest.*` (flag OFF).

## Maturidade

H4 permanece em **25%**. Sem terreno/bioma, multi-driver ou 50%.
Compatibilidade: enum/catalog local ao serviço; fingerprints/ids de ticket
inalterados. Builds sem a flag GL continuam CPU-first.
