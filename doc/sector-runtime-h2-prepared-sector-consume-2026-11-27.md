# H2 / CPU-02: consumo PreparedSector (payload + WorldStorage path)

Data: 27/11/2026 UTC. Branch `cursor/h2-products-prefetch-9f77`, base `b9572592`.
H1 tip consumido: `4d014af2` (`PreparedSector-payload`, `PreparedSector-WorldStorage-path`).

[Identidade H2](sector-runtime-h2-prepared-sector-2026-11-27.md)
· [Progressão](sector-runtime/progress.md)
· [CPU-02](sector-runtime/cpu-and-stages.md)

## Estado: unificação/consumo validado (sem install)

O handle residente H2 passa a expor as formas H1 de consumo:

| API | Comportamento H2 |
| --- | --- |
| `payloadView()` | kind/bytes/contentToken; `installAuthorized=false` |
| `worldStoragePath(epoch)` | chave lógica setor+id+kind; sem path FS |
| `probeHandoff(epoch, live)` | StaleIdentity / NotAuthorized (BaseTiles sombra) / Ready; `writesWorldStorage=false` |

Mapeamento `WorldGenerationPreparedProduct` → `WorldGenerationPreparedProductKind`
(Empty / OrderedCandidates / BaseTilesPrivate / TerrainFields / StepDelta).
Terrain/Delta usam `opaqueContentToken` no identity. BaseTiles sombra →
`NotAuthorized` (espelha H1 sem `installAuthorized`).

Sem VAL-01. Sem escrita WorldStorage. Sem publicação live.

CPU-02 permanece **25%** (consumo de formas H1; produtos reais/compartilhamento
e install autorizado ainda abertos).

## Validação

ASan/UBSan NoAssets: **30/30** (PreparedSector **7** + cache/prefetch/batch/decode **23**).

Artefato:
`/opt/cursor/artifacts/h2-prepared-sector-consume-2026-11-27.log`
(SHA-256 `b4aab88a4caddbc1ec9710276d3c83ed6cc7b544be1a777d707f22ebb289e90b`).

## Limitações / próximo

Não importa o `WorldGenerationPreparedSectorStore` H1 (conflito de modelo);
o Integrador unifica store H1 + handle H2 depois. Sem Actions.
