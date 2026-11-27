# H1 / DAG-02+DAG-03: interesse e prioridade em sombra

Data: 27/11/2026 UTC. Branch `cursor/h1-graph-cpu-9f77`.

[Executive shadow](sector-runtime-h1-executive-shadow-2026-11-27.md)
· [Progressão](sector-runtime/progress.md)

## Estado

- **DAG-02** permanece em 25%: `applyInterest` / `clearInterest` /
  `anticipatePreparation` / `unloadSector` / frontier retida. Interesse vivo /
  WorldStorage pendentes para 50%.
- **DAG-03** 25→50%: além de wrap/`selectFair`, há TTL explícito (sem renovação
  por prioridade), teto de carga Background, herança de urgência Visible em
  auxiliar, benefício estimado (`etaTick`), benefício de desbloqueio e
  `selectFairTraced` com motivo/bloqueadores limitados.

Sem publicação no mundo vivo. Sem Actions.

## Validação

8 testes `WorldGenerationExecutivePriority*` (+ suite Executive* restante)
ASan/UBSan.

## Limitações (por que não 75%)

Sem integração no caminho de geração vivo, multiplayer real ou logs de
produção. O marco 75% exige cenários com assets/mods/saves aplicáveis.

## Próximo passo

Outros contratos H1 ainda em 25% (produto BaseTiles no store, etc.) ou
integração na branch `cursor/integration-sector-runtime-7342`. Não publicar
no mundo vivo nesta frente.
