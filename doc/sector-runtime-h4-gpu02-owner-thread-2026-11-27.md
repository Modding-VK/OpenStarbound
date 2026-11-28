# H4 / GPU-02: afinidade de thread dona (asset-free)

Data: 27/11/2026 UTC. Branch `cursor/h4-gpu-shadow-9f77`.

[GPU compute](sector-runtime/gpu-compute.md)
· [staging accounting](sector-runtime-h4-gpu02-staging-accounting-2026-11-27.md)
· [Progressão](sector-runtime/progress.md)

## Objetivo

Próximo item **asset-free** após `stagingSnapshot` (já na INT): cumprir
“Submissão ocorre na dona de um contexto válido” sem integrar WorldThread ainda.

## Implementação

- `adoptOwnerThread()` / `clearOwnerThread()` / `hasOwnerThread()` /
  `isOwnerThread()`
- Com dona adotada, threads estrangeiras recebem recusa em
  submit/batch/pump/complete/poll/cancel/failContext/commit/compareShadow /
  setDeferred/resetPeak
- Getters de observação (`openTickets`, `stagingSnapshot`, …) continuam
  seguros via mutex

## Fora de escopo

- Integração WorldThread / renderer GL compartilhado
- Assets base / smoke Starbound
- 50% de marco; merge INT (Integrador)

## Validação

`WorldGenerationComputeTest.OwnerThreadRejectsForeignMutations` e suite na
build normal (flag OFF).

## Maturidade

H4 / GPU-02 permanecem em **25%**: afinidade local existe; a coordenadora
WorldThread ainda não é a dona autoritativa.
