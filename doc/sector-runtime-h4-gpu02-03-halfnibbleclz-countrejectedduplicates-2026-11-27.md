# H4 — HalfClz/Ctz/CloNibble + countRejectedDuplicates

Data: 27/11/2026 UTC. Branch `cursor/h4-gpu-shadow-9f77`. Após tip `f048e8b4`.

[GPU compute](sector-runtime/gpu-compute.md) · [Progressão](sector-runtime/progress.md)

## Recortes acumulados

1. **GPU-03:** `DenseUint32HalfClzNibble` / `DenseUint32HalfCtzNibble` /
   `DenseUint32HalfCloNibble` (contagens por nibble do low16; 4 se vazio/cheio)
2. **GPU-02:** `countRejectedDuplicates` (alias de `rejectedDuplicateCount`)

## Validação

192/192 `WorldGenerationComputeTest.*` (flag OFF).

## Maturidade

H4 permanece em **25%**. Sem terreno/bioma, multi-driver ou 50%.
Compatibilidade: enum/catalog local ao serviço; fingerprints/ids de ticket
inalterados. Builds sem a flag GL continuam CPU-first.
