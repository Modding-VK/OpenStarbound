# H3 / IO-03: checkpoint coerente em sombra (harness)

Data: 27/11/2026 UTC. Branch `cursor/h3-kernels-publication-9f77`.
Antecedente: [IO-02 shadow DB](sector-runtime-h3-shadow-db-2026-11-27.md).

## Objetivo

Primeiro recorte de IO-03: journal de intenção sobre o harness IO-02, com
unidade lógica completa, seal obrigatório, recovery idempotente após crash
simulado e rejeição de intenção parcial — sem `.world` / SQLite / mundo vivo.

## Escopo

| Artefato | Papel |
| --- | --- |
| `WorldGenerationShadowCheckpointHarness` | begin/put/seal/commit/recover/reject |
| Unidade lógica | todas as chaves required antes do seal |
| `stepEpoch` | só o próximo passo após `confirmedStepEpoch` |
| `simulateCrashAfterSeal` + `recover` | replay idempotente; parcial → reject |
| Durabilidade vs visibilidade | `sync` após `commit`; leitor só no checkpoint |

## Validação

NoAssets `WorldGenerationShadowCheckpointTest.*`: **6/6**.

## Limitações

Sem disco, sem donas regionais reais, sem B-tree vivo. IO-04 unload e IO-05
pacotes/cliente continuam abertos. Publish vivo permanece fora.

## Próximo

IO-04 unload/drain em sombra, ou amarração do checkpoint ao frontier WT-04
confirmado — ainda sem `.world`.
