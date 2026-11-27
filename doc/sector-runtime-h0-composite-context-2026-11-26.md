# H0 / CPU-01: composição seletores + static inputs + identidade BaseTiles

Data: 26/11/2026 UTC. Branch `Experimental`, base `48e39709`.

[Contexto de seletores](sector-runtime-h0-selector-context-2026-11-26.md) · [Roadmap](sector-runtime/roadmap.md) · [Progressão](sector-runtime/progress.md)

## Estado: em validação

`WorldGenerationCompositeContext` compõe, no mesmo ledger:

1. `WorldGenerationRegionSelectorContext` (admissão POD + RNG legado);
2. `WorldGenerationBlockInfoStaticInputs` (Json medido + custom terrain);
3. `WorldGenerationBaseTilesIdentity` (ticket de produto, sem alocar arrays).

Antes de qualquer seed global, valida identidades, alinhamento de
`templateRevision`/`worldEpoch`/`worldWidth` e a revisão viva de
`WorldTemplate`. Admissão de seletores recusada não captura static inputs.
Após capturar, exige que `staticInputs.templateRevision()` continue alinhada.
O contexto sobrevive à destruição da origem e permite leituras privadas
concorrentes. Não é layout/bioma/assets, lease, job, commit nem array BaseTiles.

## Validação

GCC 13/C++17, ASan/UBSan, GUI/Tracy OFF, `build/selector-context-h0`.
**777/777** casos padrão (5 novos); 10 repetições com shuffle/seed 1251.
Sem Root opt-in, Actions, streaming ou ganho de setores/s. CPU-01/H0/RAM-01
permanecem em 25%; programa 3,6%.

## Próximo passo

Feito na etapa seguinte: comparação Root com `buildRegion` em
[composite-root-compare](sector-runtime-h0-composite-root-compare-2026-11-26.md).
Continuar com builder terrestre / BaseTiles e sombra.

## Fontes locais de 26/11/2026 UTC

| Arquivo | Bytes | SHA-256 |
| --- | ---: | --- |
| `source/game/StarWorldGenerationCompositeContext.hpp` | 4272 | `82920ed95ead72bb6f2ebe92501930c021a28f4890f8c352f7b06b01b27e2b48` |
| `source/test/world_generation_composite_context_test.cpp` | 7601 | `81222ceb58733e6bb3c317e50a30569fe379b8bb03f4ad226436639be58d3d24` |
