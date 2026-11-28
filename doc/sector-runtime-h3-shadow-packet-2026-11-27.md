# H3 / IO-05: pacotes confirmados em sombra (harness)

Data: 27/11/2026 UTC. Branch `cursor/h3-kernels-publication-9f77`.
Antecedente: [IO-04 shadow unload](sector-runtime-h3-shadow-unload-2026-11-27.md).

## Objetivo

Primeiro recorte de IO-05: harness que monta pacotes só a partir de bytes
confirmados no checkpoint, preserva ordem `TileArrayInitial` antes de deltas,
marca interesse→prepare→publish→enqueue→send→receive→apply, mantém
`sectorActive` só após apply do inicial, separa relógios servidor/cliente e
rejeita subtração entre processos — sem `.world` / rede viva / cliente real.

## Escopo

| Artefato | Papel |
| --- | --- |
| `WorldGenerationShadowPacketHarness` | build/enqueue/send/receive/apply |
| `tryBuildFromProvisional` | sempre `ProvisionalBlocked` |
| `tryBuildInitial` / `tryBuildDelta` | só `readCheckpoint`; delta exige initial applied |
| Fases | ordem estrita; Sent ≠ sectorActive |
| `compareClocks` | `ClockMismatch` (sem latência por subtração) |
| Formato | `sectorFormat32` / extent 32 |

## Validação

NoAssets `WorldGenerationShadowPacketTest.*`: **6/6**.

## Limitações

Sem compressão em workers, sem `TileArrayUpdate` vivo, sem multiplayer real.
Publish vivo e pacote de rede continuam fora. H3 primeiro recorte dos 10
contratos de entrada está completo em sombra.

## Próximo

Integração sombra cruzada (frontier WT-04 ↔ checkpoint ↔ pacote) ou
equivalência viva — só com `apis_ready` live-publish / autorização explícita.
