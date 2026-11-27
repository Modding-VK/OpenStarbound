# H0 / VAL-02: capstone 2×2 + ledger apertado oceano

Data: 27/11/2026 UTC. Branch `cursor/h0-foundations-9f77`, base `4773186b`.

[Diagonal adjacent](sector-runtime-h0-sector-shadow-diagonal-adjacent-2026-11-27.md)
· [Progressão](sector-runtime/progress.md)

## Estado: validado localmente

Quatro sombras terrestres nos setores `{0,0}{1,0}{0,1}{1,1}` admitidas no
mesmo ledger Root após captura composta; BaseTiles 32×32 privados
comparados a `blockInfo`/layout privados; descarte escalonado sem
publicação. Oceano: captura no ledger ilimitado da fixture, admissão no
ledger apertado (`retainedBytes()` de uma sombra) — primeira admite,
segunda recusa; após `discard` a segunda admite.

CPU-01/H0/RAM-01/VAL-02 permanecem em 25%. Sem Actions. Sem escrita no
mundo vivo.

## Validação

ASan/UBSan (`detect_leaks=0:halt_on_error=1`):
**134/134** `WorldGenerationRealBlockInfoTest`.

Artefato local (fora do Git):
`/opt/cursor/artifacts/rbi-four-sector-capstone-2026-11-27.log`
(28279 bytes,
SHA-256 `f4bc4ead90830ead6e1b82ac810a602b2af020249a297b91e2c8a5f3ccd3e477`).

## Limitações

Não amplia o comparador além de BaseTiles; não é MOD-01 completo nem
VAL-01 de streaming; não muda RNG/cópias/defaults auditados; não
publica no mundo vivo.

## Próximo passo

Matriz geográfica de sombra BaseTiles 2×2 + pressão de admissão
fechada neste recorte. Segue VAL-02 além de BaseTiles (candidatos/IDs/
prioridades/ordem) ou MOD-01/VAL-01 conforme fila H0 — só com autorização
do Project se depender de outra frente. Não disparar Actions.
