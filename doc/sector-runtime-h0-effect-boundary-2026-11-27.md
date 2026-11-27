# H0 / MOD-01: catálogo de fronteiras de efeito

Data: 27/11/2026 UTC. Branch `cursor/h0-foundations-9f77`, base `54f69cd2`.

[Compatibility](sector-runtime/compatibility.md) · [Progressão](sector-runtime/progress.md)

## Estado: validado localmente

`WorldGenerationEffectBoundary` classifica operações H0 como
`PureKernel` / `PreparedDependency` / `AuthoritativeCallback` /
`MultiRegionEffect` (este último proibido no catálogo H0 atual).

Inventário cobre BaseTiles, CompositeContext, SectorShadow, VAL-02
candidatos, Perlin, WorldTile/Sky/TerrainSelector defaults auditados,
Assets/Root e PlacementQueue/WorkerPool. Flags:
`mayInvokeScript`, `mutatesLiveWorld`, `requiresOwnerThread`,
`readsUninitializedDefault` (não normaliza defaults herdados).

H0/MOD-01 em 25%. Sem Actions. Sem VAL-01. Sem H1–H5/R*.

## Validação

ASan/UBSan: **6/6** `WorldGenerationEffectBoundaryTest`.

Artefato:
`/opt/cursor/artifacts/mod01-effect-boundary-2026-11-27.log`
(SHA-256 `600d3909fd2dd7af5febb8d4f3f206f17b3771fe39dbd8336765d69fd1931025`).

## Limitações

Não é auditoria de todo o motor; não autoriza workers a chamar Lua;
não corrige biomeTransition/horizonClouds/baseHeight/Perlin Uninitialized.

## Próximo passo

Fechado parcialmente pela
[expansão do catálogo](sector-runtime-h0-effect-boundary-expand-2026-11-27.md).
Não disparar Actions.
