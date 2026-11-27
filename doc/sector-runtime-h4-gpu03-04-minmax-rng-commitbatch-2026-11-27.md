# H4 — DenseUint32 Min/Max/Sub + RNG + commitBatch (asset-free)

Data: 27/11/2026 UTC. Branch `cursor/h4-gpu-shadow-9f77`.

[GPU compute](sector-runtime/gpu-compute.md) · [Progressão](sector-runtime/progress.md)

## Recortes acumulados nesta frente

1. **GPU-03:** `DenseUint32Min` / `Max` / `Sub` (CPU + shaders opt-in)
2. **GPU-04:** `WorldGenerationComputeRngStream` + `drawRng` / `fillRng`
   (fluxos explícitos, ordem preservada)
3. **GPU-04:** `commitBatch` (commits independentes, sem rollback parcial)

## Fora de escopo

- Terreno/bioma, assets base, smoke Starbound
- 50% de marco; merge INT sob demanda do Integrador

## Validação

23/23 `WorldGenerationComputeTest.*` (flag OFF) neste agente.

## Maturidade

H4 permanece em **25%**.
