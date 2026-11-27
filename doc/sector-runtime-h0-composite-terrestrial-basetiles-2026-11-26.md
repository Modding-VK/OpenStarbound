# H0 / VAL-02: composto vs BaseTiles terrestre (Root opt-in)

Data: 26/11/2026 UTC. Branch `Experimental`, base `b24bea19`.

[Composto vs região](sector-runtime-h0-composite-root-compare-2026-11-26.md) · [Progressão](sector-runtime/progress.md)

## Estado: em validação

`CompositeContextStaticInputsMatchTerrestrialBaseTiles` captura o composto após
`buildTerrestrialLayout` com assets mínimos e um custom terrain, depois:

- amostra `blockInfo` via `RealSource` com `staticInputs` do composto e layout
  privado equivalente;
- aplica BaseTiles 32×32 e compara com `compareWorldGenerationBaseTiles` usando
  a identidade BaseTiles do composto;
- confirma config/custom terrain após destruir o template.

Não aloca arrays admitidos no ledger além do composto, não publica no mundo
vivo e não muda RNG/cópias de produção. CPU-01/H0/RAM-01 seguem em 25%.

## Validação

ASan/UBSan, Root opt-in: 1 caso novo × 5 repetições; **62/62** RealBlockInfo.
Sem Actions, sombra por setor ou ganho de setores/s.

## Próximo passo

Harness de sombra por setor: produzir BaseTiles privados a partir do composto,
revalidar identidade no dono e descartar sem publicação. Não disparar Actions.
