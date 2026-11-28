# H4 — HalfModAdd/ModSub/CeilDivNibble + rejectedDuplicatesPurge

Data: 28/11/2026 UTC. Branch `cursor/h4-gpu-shadow-9f77`. Após tip `31cf85b5`.

[GPU compute](sector-runtime/gpu-compute.md) · [Progressão](sector-runtime/progress.md)

## Recortes acumulados

1. **GPU-03:** `DenseUint32HalfModAddNibble` / `DenseUint32HalfModSubNibble` /
   `DenseUint32HalfCeilDivNibble`
2. **GPU-02:** `rejectedDuplicatesPurge`

## Validação

232/232 `WorldGenerationComputeTest.*` (flag OFF).

## Maturidade

H4 permanece em **25%**. Sem terreno/bioma, multi-driver ou 50%.
Compatibilidade: enum/catalog local ao serviço; fingerprints/ids de ticket
inalterados. Builds sem a flag GL continuam CPU-first.
