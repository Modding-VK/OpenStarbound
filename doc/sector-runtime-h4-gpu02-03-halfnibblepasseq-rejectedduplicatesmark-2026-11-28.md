# H4 — HalfPassEq/PassNe/StepNe + rejectedDuplicatesMark

Data: 28/11/2026 UTC. Branch `cursor/h4-gpu-shadow-9f77`. Após INT `7d6e745a` / base `07d36f45`.

[GPU compute](sector-runtime/gpu-compute.md) · [Progressão](sector-runtime/progress.md)

## Recortes acumulados

1. **GPU-03:** `DenseUint32HalfPassEqNibble` /
   `DenseUint32HalfPassNeNibble` /
   `DenseUint32HalfStepNeNibble`
2. **GPU-02:** `rejectedDuplicatesMark`

## Validação

276/276 `WorldGenerationComputeTest.*` (flag OFF).

## Maturidade

H4 permanece em **25%**. Sem terreno/bioma, multi-driver ou 50%.
Compatibilidade: enum/catalog local ao serviço; fingerprints/ids de ticket
inalterados. Builds sem a flag GL continuam CPU-first.
