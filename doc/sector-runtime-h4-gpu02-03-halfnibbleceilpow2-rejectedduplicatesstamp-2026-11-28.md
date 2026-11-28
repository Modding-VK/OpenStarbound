# H4 — HalfCeilPow2/FloorPow2/ToggleLow + rejectedDuplicatesStamp

Data: 28/11/2026 UTC. Branch `cursor/h4-gpu-shadow-9f77`. Após INT `e4ff2cff` / base `e4f89cf7`.

[GPU compute](sector-runtime/gpu-compute.md) · [Progressão](sector-runtime/progress.md)

## Recortes acumulados

1. **GPU-03:** `DenseUint32HalfCeilPow2Nibble` /
   `DenseUint32HalfFloorPow2Nibble` /
   `DenseUint32HalfToggleLowNibble`
2. **GPU-02:** `rejectedDuplicatesStamp`

## Validação

288/288 `WorldGenerationComputeTest.*` (flag OFF).

## Maturidade

H4 permanece em **25%**. Sem terreno/bioma, multi-driver ou 50%.
Compatibilidade: enum/catalog local ao serviço; fingerprints/ids de ticket
inalterados. Builds sem a flag GL continuam CPU-first.
