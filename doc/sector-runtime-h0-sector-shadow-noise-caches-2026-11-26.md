# H0 / VAL-02: sombra com noises/caches privados

Data: 26/11/2026 UTC. Branch `Experimental`, base `836ad2d8`.

[Sombra oceano/adjacente](sector-runtime-h0-sector-shadow-ocean-adjacent-2026-11-26.md)
· [Progressão](sector-runtime/progress.md)

## Estado: em validação

`SectorShadowPrivateNoiseCachesMatchCompositeBaseTiles` captura o composto sobre
um WorldTemplate com árvore CacheSelector/Mix/FlatSurface/Perlin e block/blend
noise, admite a sombra, preenche BaseTiles 32×32 via static inputs + layout
privado e compara com o caminho vivo. Confirma que o cache do layout privado
aqueceu de forma exclusiva (limpo não afeta um layout irmão frio) e descarta
sem publicação.

Não muda RNG/cópias de produção, não adota jobs e não dispara Actions.
CPU-01/H0/RAM-01 seguem em 25%.

## Validação

ASan/UBSan: 1 caso × 5; **66/66** RealBlockInfo.

## Próximo passo

Sombra com IslandSurface/coluna privada ou duas sombras concorrentes no mesmo
ledger; depois demanda/mailbox em H1 quando os gates de H0 aplicáveis
permitirem. Não disparar Actions.
