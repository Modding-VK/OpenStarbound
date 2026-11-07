# A/B do agendador de resultados prontos — lua

**Build observada:** `cd0f0eec5cd04d49ffb64984b26029de925f7fd3`  
**Objetivo:** comparar `generationReadyResultSchedulerPreview: false` e
`true` em novas sessões no mesmo mundo lunar, separadas das sessões anteriores.

## Artefatos e integridade

| Variante | Artefatos recebidos | Integridade / SHA-256 |
| --- | --- | --- |
| `false` | `.z01` (33.554.432 bytes) e `.zip` (2.865.964 bytes) | ZIP multipartes reconstruído/testado; partes `66851d47185fcec2ceb75e283bc2881ff90fb099e254428eb921101ca7961129` / `62d9401465a075b99dd579c75dfd35b2fb8a85fd9aa261a530b6f04d898f10fa`; trace `2cb2f4cf80fba6ff242028fcde17611d5170a6e5792f48041af996ec344638e2` |
| `true` | `.zip` (26.623.389 bytes) | ZIP testado; arquivo `6a91ea05ca7ad6ab86abdd76662ad2e3c676ccbba9731aed0ce767485f380cae`; trace `e57af410acabf41ba1f7b5ca21b357f6b7fdb27e239e09d0a832c23ac226d673` |

Os dois logs identificam a mesma build e conjunto de assets/mods. Não contêm
`Access violation`, `MapException` ou `WorldStorageException`. Os dados são do
mundo `CelestialWorld:383738451:-48324174:-267567852:4:3`.

## Confirmação das variantes

Os contadores distinguem as execuções sem depender do nome dos arquivos: a
variante `false` tem zero `selectedByQueuePriority`; a `true` tem 333.

| Medida regional | `false` | `true` |
| --- | ---: | ---: |
| Duração coberta | 175,319 s | 91,002 s |
| Resultados BaseTiles commitados | 1.180 (6,73/s) | 729 (8,01/s) |
| Seleções visíveis / prefetch | 455 / 725 | 333 / 396 |
| Seleções por prioridade da fila | 0 | 333 |
| Profundidade média da fila | 63,12 | 67,74 |
| Idade média / máxima da fila | 7,974 / 16,861 s | 8,154 / 16,196 s |
| Espera média / máxima de setor | 2,398 / 16,150 s | 2,431 / 15,945 s |
| Atraso médio / máximo pronto→commit | 6,414 / 242,363 ms | 9,546 / 194,191 ms |
| BaseTiles ativos médios / workers | 0,115 / 11 | 0,100 / 11 |

## Tracy oficial

Exportação feita com `tracy-csvexport` oficial 0.11.2,
`wolfpld/tracy@b8c2e25c3d02009059e09f623072921677dd6bb4`.

| Zona | `false` p50 / p95 / p99 / máx (ms) | `true` p50 / p95 / p99 / máx (ms) |
| --- | --- | --- |
| `WorldServerThread update` | 2,369 / 8,760 / 14,989 / 169,658 | 1,987 / 10,638 / 17,726 / 138,886 |
| `WorldStorage generate queue` | 0,936 / 7,015 / 14,498 / 165,921 | 1,052 / 8,659 / 15,074 / 137,275 |
| `WorldServer entities` | 0,946 / 5,680 / 7,804 / 42,503 | 0,939 / 7,642 / 9,333 / 12,895 |
| `Base tiles worker` | 1,548 / 3,363 / 5,094 / 37,113 | 1,544 / 3,639 / 5,506 / 22,966 |
| `Generate microdungeons` | 0,552 / 0,920 / 1,191 / 165,627 | 0,556 / 0,885 / 1,135 / 130,342 |
| `Generate cave liquid` | 0,016 / 3,542 / 4,459 / 5,156 | 0,016 / 3,344 / 4,086 / 4,551 |

## Decisão e próximos passos

O preview está funcional: seleciona resultados visíveis pela prioridade
autoritativa e não introduziu crash. A taxa observada de commits BaseTiles foi
19% maior com `true`, mas as durações e o percurso não são equivalentes. É um
sinal, não uma atribuição causal de desempenho.

O carregamento lento de setor novo no céu é mensurável em ambas: o pior
`generation.sectorWait` continua perto de 16 s. O commit priorizado reordena
somente trabalho pronto; não reduz a espera por Tiles/BaseTiles, dependências,
microdungeons ou líquidos. A ocupação BaseTiles fica em apenas ~0,1 de 11
workers, confirmando preparação/admissão como gargalo seguinte.

1. Manter o preview desligado por padrão até A/B com rota e duração idênticas.
2. Instrumentar cada pedido visível: admissão, Tiles, BaseTiles, vizinhos,
   microdungeons, líquidos, pronto e commit.
3. Propagar prioridade visível pela cadeia mínima de pré-requisitos, mantendo
   prefetch limitado e a publicação autoritativa na `WorldThread`.

Complementa [runtime-validation-2026-11-07-tracy12.md](runtime-validation-2026-11-07-tracy12.md)
e o [checklist operacional](regional-runtime-execution-checklist.md).
