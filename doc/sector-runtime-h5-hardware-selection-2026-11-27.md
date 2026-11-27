# H5 / GPU-05: seleção por custo de entrega (primeiro recorte)

Data: 27/11/2026 UTC. Branch `cursor/h5-hardware-scale-9f77`, base `e55c008f`.

[Progressão](sector-runtime/progress.md) · [GPU compute](sector-runtime/gpu-compute.md)

## Estado: em validação (marco 25%)

`WorldGenerationHardwareSelector` — modelo puro e local:

- custo completo de entrega = execução + transferência + fence + materialização;
- GPU ausente ⇒ sempre CPU;
- `performanceMode` `legacy` (e desconhecido) força CPU sem renomear os modos
  existentes;
- `adaptive` / `aggressive` / `dynamic` podem escolher GPU só com custo menor;
- histerese por margem; prazo pode preferir CPU;
- custo inválido não inverte o backend sticky.

Não é driver, fila, fence, publicação nem autoridade de mundo. Adoção
autoritativa continua dependente de H4 equivalente e gate de ganho medido.

## Validação

`WorldGenerationHardwareSelectionTest` — **7/7** no alvo
`world_generation_contract_tests` (ASan/UBSan). Sem Actions.

## Compatibilidade

Fallback CPU completo preservado. Mods/saves/multiplayer não alterados.
Modos `legacy`/`adaptive`/`aggressive`/`dynamic` não são substituídos pelos
perfis conceituais H5.

## Próximo passo

Fundações VAL/MOD/CPU-05 em
[scale foundations](sector-runtime-h5-scale-foundations-2026-11-27.md).
Recortes H5 isoláveis esgotados; falta H1/H4 reais. Não disparar Actions.
