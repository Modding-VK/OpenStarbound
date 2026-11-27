# H0 / VAL-02: sombra por setor (BaseTiles privados do composto)

Data: 26/11/2026 UTC. Branch `Experimental`, base `5e2e2297`.

[Composto vs BaseTiles terrestre](sector-runtime-h0-composite-terrestrial-basetiles-2026-11-26.md)
· [Progressão](sector-runtime/progress.md)

## Estado: em validação

`WorldGenerationSectorShadow` admite um array BaseTiles no ledger contra a
identidade do composto (ou ticket equivalente), expõe o buffer privado para
preenchimento no dono, revalida com `matches` e libera via `discard` /
destruição. Não há caminho de publicação no mundo vivo, job, mailbox ou
workers.

Contrato padrão: admissão, recusa por identidade inválida/orçamento, descarte,
expectativa stale e admissão a partir do composto. Root opt-in:
`SectorShadowProducesPrivateTerrestrialBaseTilesThenDiscards` preenche 32×32 a
partir dos static inputs do composto, compara com o builder terrestre e descarta
após falha de `matches` com revisão stale.

CPU-01/H0/RAM-01 seguem em 25%. Sem Actions, sem ganho de setores/s.

## Validação

ASan/UBSan: **5** casos novos × 10 repetições; **782/782** contrato padrão;
Root opt-in 1 caso × 5; **63/63** RealBlockInfo.

## Próximo passo

Estendido em
[oceano/setor adjacente](sector-runtime-h0-sector-shadow-ocean-adjacent-2026-11-26.md).
Segue: noises/caches privados alinhados ao composto. Não disparar Actions.
