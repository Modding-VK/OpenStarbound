# H4 — Eq/Ne/BitReverse + fingerprintIdentity + listOpenTickets (asset-free)

Data: 27/11/2026 UTC. Branch `cursor/h4-gpu-shadow-9f77`.

[GPU compute](sector-runtime/gpu-compute.md) · [Progressão](sector-runtime/progress.md)

## Recortes acumulados (pós-INT `38ef1b2f`)

1. **GPU-03:** `DenseUint32Eq` / `DenseUint32Ne` / `DenseUint32BitReverse`
   (CPU + GL opt-in; masks 0/`0xffffffff`; reverse is involution)
2. **GPU-04/02:** `fingerprintIdentity` + `listOpenTickets`

## Validação

32/32 `WorldGenerationComputeTest.*` (flag OFF).

## Maturidade

H4 permanece em **25%**. Sem terreno/bioma, multi-driver ou 50%.
