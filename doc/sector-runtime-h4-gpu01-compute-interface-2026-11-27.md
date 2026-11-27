# H4 / GPU-01: interface e capacidades CPU/GPU

Data: 27/11/2026 UTC. Branch `cursor/h4-gpu-shadow-9f77`, base `e55c008f`.

[GPU compute](sector-runtime/gpu-compute.md) · [Progressão](sector-runtime/progress.md)

## Objetivo

Primeiro recorte funcional de GPU-01: serviço independente de
`WorldServer`/renderer/Root com `submit` / `poll` / `cancel`, probe de
capacidades e backend CPU de referência. Build normal permanece sem OpenGL
compute.

## Implementação

- `StarWorldGenerationCompute.hpp` / `.cpp` — `WorldGenerationComputeService`
- Kernel exato `DenseUint32Xor` (candidato GPU-03) no CPU reference
- Probe nunca marca `gpuOpenGlCompute` sem detecção real 4.3+/extensão
- Inputs imutáveis copiados; cancel revoga publicação; sem escrita no mundo vivo

## Validação

`world_generation_contract_tests --gtest_filter=WorldGenerationComputeTest.*`
— **6/6** PASSED (ASan/UBSan). Sem Actions.

## Compatibilidade

Não eleva requisito gráfico. Servidor dedicado / macOS sem compute seguem no
CPU. Mods/saves/multiplayer inalterados.

## Limitações

Sem thread de contexto GL, fences, buffers em voo (GPU-02), sem sombra
CPU↔GPU (GPU-04), sem claim de ganho. GPU submit recusado até probe real.

## Próximo passo

Entregue em
[GPU-02/03/04](sector-runtime-h4-gpu02-03-04-2026-11-27.md).
Probe GL opt-in permanece fora da build normal. Não disparar Actions.
