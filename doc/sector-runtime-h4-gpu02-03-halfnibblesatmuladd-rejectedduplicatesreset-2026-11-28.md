# H4 — HalfSatMulAdd/MulSub/Equal3FillNibble + rejectedDuplicatesReset

Data: 28/11/2026 UTC. Branch `cursor/h4-gpu-shadow-9f77`. Após tip `21dee7f5`.

[GPU compute](sector-runtime/gpu-compute.md) · [Progressão](sector-runtime/progress.md)

## Recortes acumulados

1. **GPU-03:** `DenseUint32HalfSatMulAddNibble` / `DenseUint32HalfMulSubNibble` /
   `DenseUint32HalfEqual3FillNibble`
2. **GPU-02:** `rejectedDuplicatesReset`

## Validação

222/222 `WorldGenerationComputeTest.*` (flag OFF).

## Maturidade

H4 permanece em **25%**. Sem terreno/bioma, multi-driver ou 50%.
Compatibilidade: enum/catalog local ao serviço; fingerprints/ids de ticket
inalterados. Builds sem a flag GL continuam CPU-first.
