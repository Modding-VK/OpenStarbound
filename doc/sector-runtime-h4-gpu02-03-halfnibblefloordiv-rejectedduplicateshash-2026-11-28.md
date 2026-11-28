# H4 — HalfFloorDiv/RoundDiv/RemNibble + rejectedDuplicatesHash

Data: 28/11/2026 UTC. Branch `cursor/h4-gpu-shadow-9f77`. Após tip `280c4b11`.

[GPU compute](sector-runtime/gpu-compute.md) · [Progressão](sector-runtime/progress.md)

## Recortes acumulados

1. **GPU-03:** `DenseUint32HalfFloorDivNibble` / `DenseUint32HalfRoundDivNibble` /
   `DenseUint32HalfRemNibble`
2. **GPU-02:** `rejectedDuplicatesHash`

## Validação

234/234 `WorldGenerationComputeTest.*` (flag OFF).

## Maturidade

H4 permanece em **25%**. Sem terreno/bioma, multi-driver ou 50%.
Compatibilidade: enum/catalog local ao serviço; fingerprints/ids de ticket
inalterados. Builds sem a flag GL continuam CPU-first.
