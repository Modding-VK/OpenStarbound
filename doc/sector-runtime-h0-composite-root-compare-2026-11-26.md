# H0 / VAL-02: composto vs região real (Root opt-in)

Data: 26/11/2026 UTC. Branch `Experimental`, base `325831fb`.

[Contexto composto](sector-runtime-h0-composite-context-2026-11-26.md) · [Progressão](sector-runtime/progress.md)

## Estado: em validação

O teste opt-in `CompositeContextMatchesRealRegionParametersAndPrivateSelectors`
captura `WorldGenerationCompositeContext` a partir de um `WorldTemplate` real
(com config de fixture e custom terrain), depois constrói a região via
`buildRegion` sob Root com manutenção bloqueada. Confere:

- Json dos parâmetros terrain/fg/bg do composto contra os seletores da região;
- amostras `get()` de seletores privados criados a partir do composto;
- config/custom terrain do composto após destruir o template.

O alvo Root passou a linkar `StarWorldGenerationTasks.cpp` e
`StarWorldRegionPartition.cpp` para a identidade BaseTiles. Sem adoção em jobs,
sem arrays BaseTiles alocados, sem mudar RNG/cópias de produção.

## Validação

GCC 13/C++17, ASan/UBSan, `STAR_BUILD_ROOT_LIFECYCLE_TESTS=ON`.
1 novo caso opt-in × 5 repetições; **61/61** RealBlockInfo; **777/777**
contratos padrão. Sem Actions, terrestrial completo nesta etapa, streaming ou
ganho de setores/s. CPU-01/H0/RAM-01 em 25%; programa 3,6%.

## Próximo passo

Estender a comparação do composto ao builder terrestre / BaseTiles 32×32 com
fixtures Root; depois sombra por setor sem publicação. Não disparar Actions.
