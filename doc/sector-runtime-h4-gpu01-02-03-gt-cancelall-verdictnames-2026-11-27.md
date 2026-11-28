# H4 — Gt/Geu/Xnor + cancelAll + verdict names (asset-free)

Data: 27/11/2026 UTC. Branch `cursor/h4-gpu-shadow-9f77`.

[GPU compute](sector-runtime/gpu-compute.md) · [Progressão](sector-runtime/progress.md)

## Recortes acumulados (pós-INT `7c4b8fbf`)

INT tip já inclui o harden R1 ThreadStub sleep (fora desta branch H4).

1. **GPU-03:** `DenseUint32Gt` / `DenseUint32Geu` / `DenseUint32Xnor`
   (CPU + GL opt-in)
2. **GPU-02/01:** `cancelAll` + `pollStatusName` / `shadowVerdictName` /
   `commitVerdictName`

## Validação

36/36 `WorldGenerationComputeTest.*` (flag OFF).

## Maturidade

H4 permanece em **25%**. Sem terreno/bioma, multi-driver ou 50%.
