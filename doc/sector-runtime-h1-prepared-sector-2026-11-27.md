# H1 / CPU-02 + MD-01 — produto PreparedSector e candidatos sem halo

Data: 27/11/2026 UTC. Branch `cursor/h1-graph-cpu-9f77`.

[Cancel/mailbox/scheduler](sector-runtime-h1-executive-cancel-mailbox-2026-11-27.md)
· [Progressão](sector-runtime/progress.md)

## Objetivo

Primeiro recorte de produto versionado (CPU-02) e seleção leve de
microdungeons por IDs de colocação (MD-01), sem halo de tiles e sem instalar
no mundo vivo.

## Estado

- **CPU-02** 0→25%: `WorldGenerationPreparedSectorProduct` com identidade
  (setor, template/distribution/placement revisions), kinds `Empty` e
  `OrderedCandidates`, store com share e invalidação stale; preparar não
  muta nível persistido.
- **MD-01** 0→25%: candidatos carregam `placementId` / nome / posição / seed
  apenas; produto vazio prova ausência sob a mesma identidade.

3 testes `WorldGenerationPreparedSector*`. Sem Actions. Sem BaseTiles privados
no produto nem ligação ao path MicroDungeons vivo.

## Relacionado

CPU-05 sombra: `WorldGenerationResourceGovernor` sugere orçamentos a partir
da frontier/mailbox/retidos/overrun (mesmo commit de continuidade H1).

## Próximo passo

Amarrar produto/governor ao caminho vivo ou elevar contratos a 50% com
integração. Não disparar Actions.
