# H4 / GPU-04: revalidação de input no commit

Data: 27/11/2026 UTC. Branch `cursor/h4-gpu-shadow-9f77`.

[GPU compute](sector-runtime/gpu-compute.md)
· [failContext/reprodutor](sector-runtime-h4-gpu02-04-failcontext-reproducer-2026-11-27.md)
· [Progressão](sector-runtime/progress.md)

## Objetivo

Próximo item **asset-free** após failContext/reprodutor (já na INT): implementar
o gate de publicação do contrato GPU-04 — *“Um resultado GPU aprovado ainda
precisa de revalidação do input no momento do commit”*.

## Implementação

- `fingerprintInput(List<uint32_t>)` — fingerprint estável do buffer imutável
- `commit(ticket, liveIdentity, liveInputFingerprint, result*)` — consome o
  ticket; `Accepted` só se identidade + fingerprint ainda batem com o submit;
  `StaleIdentity` / `StaleInput` revogam publicação e rejeitam polls/commits
  duplicados posteriores; `NotReady` se ainda pendente

## Fora de escopo

- Terreno/bioma, assets base, smoke Starbound
- 50% de marco / multi-driver
- Merge INT (Integrador)

## Validação

`WorldGenerationComputeTest.CommitRevalidatesIdentityAndInputFingerprint` e
suite completa na build normal (flag OFF).

## Maturidade

H4 / GPU-04 permanecem em **25%**: o gate de commit existe, mas campanha
multi-driver e integração de publicação versionada (H1–H3) ainda faltam.
