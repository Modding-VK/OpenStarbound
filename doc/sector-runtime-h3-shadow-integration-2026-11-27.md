# H3: integração cruzada sombra (WT-04 → IO-03 → IO-05)

Data: 27/11/2026 UTC. Branch `cursor/h3-kernels-publication-9f77`.
Antecedentes: [WT-04](sector-runtime-h3-publication-pass-2026-11-27.md),
[IO-03](sector-runtime-h3-shadow-checkpoint-2026-11-27.md),
[IO-05](sector-runtime-h3-shadow-packet-2026-11-27.md).

## Objetivo

Primeiro recorte de integração sombra após o marco 25% dos 10 contratos H3:
ligar fronteira confirmada de `WorldGenerationPublicationPass` a um unit
IO-03 (Tiles+Frontier) e entregar `TileArrayInitial` via IO-05 — sem
`.world`, rede ou publish vivo.

## Escopo

| Artefato | Papel |
| --- | --- |
| `WorldGenerationShadowIntegrationHarness` | ingest → commit → deliver |
| `ingestPublicationFrontier` | exige `frontierConfirmed` + staging committed |
| `commitCheckpointUnit` | Tiles + Frontier blob numa intenção |
| `deliverInitialPacket` | build/enqueue/send/apply; Sent ≠ Active |

## Validação

NoAssets `WorldGenerationShadowIntegrationTest.*`: **4/4**.

## Notas de maturidade

Não concede 50% a WT-04/IO-03/IO-05: lacunas vivas (WorldThread, disco,
rede/cliente) permanecem. É evidência de amarração sombra entre recortes
já em 25%.

## Limitações / o que falta no H3

- Equivalência viva e publish autoritativo (`apis_ready` live-publish)
- CPU-04 flora/spawn em sombra adicional
- MD decode TMX real / RNG vivo
- SQLite/`.world`, GPU física, donas regionais

## Próximo

Sob ordem do Project: aprofundar um contrato rumo a 50% em sombra, ou
aguardar live-publish para equivalência.
