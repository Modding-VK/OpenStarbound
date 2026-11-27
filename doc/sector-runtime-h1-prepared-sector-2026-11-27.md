# H1 / CPU-02 + MD-01 — produto PreparedSector, payload e path WorldStorage

Data: 27/11/2026 UTC. Branch `cursor/h1-graph-cpu-9f77`.

[Cancel/mailbox/scheduler](sector-runtime-h1-executive-cancel-mailbox-2026-11-27.md)
· [Progressão](sector-runtime/progress.md)

## Objetivo

Produto versionado (CPU-02) e candidatos sem halo (MD-01), mais APIs de
payload e caminho lógico WorldStorage para H2, sem install no mundo vivo.

## Estado

- **CPU-02** 25%: `Empty` / `OrderedCandidates` / `BaseTilesPrivate` /
  `TerrainFields` / `StepDelta` (opacos); `payloadView`,
  `worldStoragePath`, `probeHandoff` com outcomes Ready/Stale/NotAuthorized/
  Empty/Missing; `writesWorldStorage` sempre false neste recorte.
- **MD-01** 25%: candidatos só com IDs/nome/seed/posição.

7 testes `WorldGenerationPreparedSector*`. Sem Actions. Sem escrita em
WorldStorage. BaseTiles sombra permanece `installAuthorized=false`.

## APIs estáveis para dependentes (H2)

- `PreparedSector-payload` — `WorldGenerationPreparedPayloadView`
- `PreparedSector-WorldStorage-path` — `worldStoragePath` + `probeHandoff`

## Próximo passo

Install autorizado controlado (ainda sem merge em `main`) ou unificação na
integração. Não disparar Actions.
