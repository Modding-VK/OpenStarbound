# H4 / GPU-02 · GPU-04: failContext CPU recovery + reprodutor mínimo

Data: 27/11/2026 UTC. Branch `cursor/h4-gpu-shadow-9f77`.

[GPU compute](sector-runtime/gpu-compute.md)
· [And/Or/Mul](sector-runtime-h4-gpu03-dense-and-or-mul-2026-11-27.md)
· [Progressão](sector-runtime/progress.md)

## Objetivo

Próximo item **asset-free** após DenseUint32 And/Or/Mul (já na INT):

1. **GPU-02:** `failContext` recalcula tickets abertos no CPU reference e rejeita
   polls duplicados posteriores (resultado GPU tardio não publica).
2. **GPU-04:** corpus de bordas com coords negativas-as-uint32 / wrap /
   near-limit (`shadowEdgeCorpus`); relatório de sombra com
   `WorldGenerationComputeShadowReproducer` mínimo (sem payload de mod);
   `compareShadowBuffers` para harness/unitário.

## Fora de escopo

- Terreno/bioma, assets base, smoke Starbound
- Campanha multi-driver / 50% de marco
- Merge INT (Integrador)

## Validação

`WorldGenerationComputeTest.*` na build normal (flag OFF).

## Maturidade

H4 permanece em **25%**. Recovery/reprodutor avançam os contratos sem fechar
ganho ponta a ponta nem publicação versionada.
