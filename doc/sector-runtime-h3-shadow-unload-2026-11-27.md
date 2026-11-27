# H3 / IO-04: unload/drain em sombra (harness)

Data: 27/11/2026 UTC. Branch `cursor/h3-kernels-publication-9f77`.
Antecedente: [IO-03 shadow checkpoint](sector-runtime-h3-shadow-checkpoint-2026-11-27.md).

## Objetivo

Primeiro recorte de IO-04: harness que revoga autorização de publish no
unload, preserva reservas, drena (wait) ou descarta conclusões do produtor,
protege `keepAlive`, bloqueia handoff parcial à dona central sem checkpoint
válido, exige checkpoint para modo legado e só libera buffers privados após
drain — sem `.world` / SQLite / WorldThread vivo.

## Escopo

| Artefato | Papel |
| --- | --- |
| `WorldGenerationShadowUnloadHarness` | produce/complete/unload/drain/handoff |
| `requestUnload` | revoga publish por setor |
| `drainWait` / `drainDiscard` | conclusão sem instalar; keepAlive protegido |
| `tryHandoffPartialToCentral` | PartialBlocked se unloading/ready |
| `switchToLegacyMode` | CheckpointRequired sem passo confirmado |
| `releasePrivateBuffers` | só após Unloaded/Discarded/Published |

## Validação

NoAssets `WorldGenerationShadowUnloadTest.*`: **6/6**.

## Limitações

Sem disco, sem GPU física, sem donas regionais reais. IO-05 pacotes/cliente
continua aberto. Publish vivo permanece fora.

## Próximo

IO-05 pacotes confirmados em sombra, ou amarração unload↔frontier WT-04 —
ainda sem `.world`.
