# H0 / VAL-02: IslandSurface sob sombra e pressão de admissão

Data: 26/11/2026 UTC. Branch `Experimental`, base `52684594`.

[Sombras concorrentes](sector-runtime-h0-sector-shadow-concurrent-2026-11-26.md)
· [Progressão](sector-runtime/progress.md)

## Estado: em validação

`TightLedgerAdmitsOneShadowAndRefusesTheSecond` — ledger com capacidade de um
array: primeira sombra admite, segunda recusa, descarte reabre a capacidade.

`SectorShadowIslandSurfaceColumnsStayPrivateThroughComposite` — layout com
IslandSurface, composto + sombra 32×32 equivalentes ao vivo; columnCache do
layout privado aquece sem contaminar um irmão frio; descarte sem publicação.

Sem Actions, workers ou ganho de setores/s. CPU-01/H0/RAM-01 em 25%.

## Validação

ASan/UBSan: 7 sombras × 10 → **784/784** contrato; Island × 5; **68/68** RealBlockInfo.

## Próximo passo

Revisão de template stale com sombra ativa, ou fechar lacunas restantes de
CPU-01 antes de H1. Não disparar Actions.
