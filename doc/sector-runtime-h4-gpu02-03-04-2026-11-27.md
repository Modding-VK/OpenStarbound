# H4 / GPU-02 · GPU-03 · GPU-04: fila, lotes e sombra (CPU)

Data: 27/11/2026 UTC. Branch `cursor/h4-gpu-shadow-9f77`, base `49edd034`.

[GPU-01](sector-runtime-h4-gpu01-compute-interface-2026-11-27.md)
· [GPU compute](sector-runtime/gpu-compute.md)
· [Progressão](sector-runtime/progress.md)

## Objetivo

Completar os primeiros marcos funcionais dos três contratos restantes de H4
**sem** OpenGL compute real: fila/voo/recuperação, lotes/kernels exatos e
comparação em sombra estrutural.

## Implementação

`WorldGenerationComputeService` passa a oferecer:

- **GPU-02:** `maxInFlight`, staging bytes, pool de buffers, conclusão
  diferida + `pump`/`complete`, `failContext` com rejeição de duplicatas,
  cancel que revoga publicação
- **GPU-03:** `submitBatch`, kernels `DenseUint32Xor` e `DenseUint32Add`,
  equivalência bit-a-bit no CPU reference
- **GPU-04:** `compareShadow` com corpus de bordas; CPU↔CPU Equal;
  GPU shadow → `ShadowUnavailable` sem probe

## Validação

`WorldGenerationComputeTest.*` — **14/14** PASSED (ASan/UBSan). Sem Actions.

## Notas de maturidade

Cada contrato sobe **0→25%** (primeiro recorte). Não recebem 50%: falta
contexto GL real, fences de driver, kernels de terreno/bioma, corpus multi-driver
e ganho ponta a ponta.

## Limitações / bloqueios restantes nesta frente

- Probe OpenGL 4.3+/extensão (opt-in, fora da build normal)
- Thread dona de contexto GL + compartilhamento auditado
- Sombra CPU↔GPU em hardware real e campanhas de driver
- Integração com publicação/versionamento de H1–H3

Esses itens **não** são concluíveis neste ambiente sem GPU/driver e sem
avançar frentes dependentes. H4 nesta branch esgota o recorte CPU-first.

## Próximo passo

Integrar/revisar esta branch; frentes H1/H2 ou probe GL opt-in em trabalho
separado. Não disparar Actions. Não publicar divergências.
