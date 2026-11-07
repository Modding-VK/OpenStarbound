# Validação do agendador de commits prontos — Tracy 12

**Branch/build observada:** `Experimental`, `cd0f0eec5cd04d49ffb64984b26029de925f7fd3`  
**Objetivo:** validar o preview `generationReadyResultSchedulerPreview`, que
entrega BaseTiles prontos pela ordem autoritativa da fila visível.

## Artefatos

| Artefato | Tamanho | SHA-256 |
| --- | ---: | --- |
| `12.tracy` | 26.934.320 bytes | `503f1e74e2d41753b01ebb5a851a0f77f4d674ff96815a165e23eaddeb5f528d` |
| `exploration.zip` | 18.523.152 bytes | `2229fa7cdd296e61d3141caf12789e6fc87c574ba57a4c1777f45c4f77ed9e7e` |
| `starbound.log` | 75.623 bytes | `c1bd0126ced713238c23890a001a7722c54f4068e642d9a12e7e8a788cf43456` |

O ZIP passou no teste de integridade. O log identifica a build acima e termina
com parada normal da aplicação. Não contém `Access violation`, `MapException`
ou `WorldStorageException` para a sessão observada. A configuração completa não
foi anexada; os contadores de seleção confirmam que o preview estava ativo.

## Cenário e resultado funcional

Os dados do mundo `CelestialWorld:383738451:-48324174:-267567852:4`, instância
23, cobrem 64 janelas / 129,058 s.

| Medida | Resultado |
| --- | ---: |
| Setores completos | 465 (aprox. 3,60/s) |
| Profundidade média da fila | 63,16 setores |
| Maior idade observada da fila | 50,96 s |
| Commits prontos visíveis selecionados | 108 |
| Selecionados pela prioridade da fila | 108 |
| Commits de prefetch selecionados | 100 |
| Resultados prontos commitados | 208 |
| `generation.baseTiles.tilesUnavailable` | 0 |
| Decode de tiles agendado/aplicado/cancelado | 438 / 386 / 37 |

O preview fez exatamente a separação pretendida: resultados visíveis foram
selecionados pela posição da fila e resultados de prefetch ficaram depois. Não
houve admissão de BaseTiles sem tiles instalados nem crash nesta rota.

## Tracy oficial

O trace foi exportado pelo `tracy-csvexport` oficial 0.11.2 de
`wolfpld/tracy@b8c2e25c3d02009059e09f623072921677dd6bb4`, com eventos
individuais e correspondência exata de zona.

| Zona | Eventos | p50 | p95 | p99 | Máximo |
| --- | ---: | ---: | ---: | ---: | ---: |
| `WorldServerThread update` | 3.699 | 27,868 ms | 75,425 ms | 139,474 ms | 534,289 ms |
| `WorldServer entities` | 3.646 | 23,624 ms | 66,671 ms | 134,406 ms | 532,163 ms |
| `WorldStorage generate queue` | 730 | 1,789 ms | 18,484 ms | 65,970 ms | 149,899 ms |
| `Base tiles worker` | 209 | 1,686 ms | 5,351 ms | 6,863 ms | 103,143 ms |
| `Generate microdungeons` | 381 | 0,571 ms | 0,982 ms | 1,402 ms | 149,604 ms |
| `Generate cave liquid` | 159 | 3,332 ms | 4,124 ms | 4,256 ms | 4,529 ms |

## Limites da comparação com Tracy 11

Esta não é uma comparação controlada: Tracy 11 cobriu 267,102 s, completou
1.662 setores e teve update médio antes de geração de 12,00 ms; nesta sessão o
mesmo sinal médio foi 26,70 ms. `WorldServer entities` também tem cauda muito
maior nesta rota. Portanto, não é permitido atribuir a menor taxa observada de
setores/s nem o atraso máximo pronto→commit de 1.583,07 ms ao novo agendador.

Há indício favorável, mas não conclusivo, de menor idade/profundidade de fila:
50,96 s / 63,16 contra 59,38 s / 70,94 na sessão 11. Somente uma rota A/B com
mesma seed, save, mods, percurso e duração pode estabelecer efeito causal.

## Próximos passos

1. Repetir a mesma rota duas vezes, com e sem
   `generationReadyResultSchedulerPreview`, preservando configuração, save,
   mods e duração; anexar a configuração integral.
2. Comparar setores/s, p50/p95/p99 e máximo de idade/profundidade da fila,
   atraso pronto→commit e métricas `selectedVisible/selectedPrefetch`.
3. Manter o preview desligado por padrão até a equivalência de tiles, dungeons,
   saves, unload/reload e multiplayer.
4. Depois do A/B, avançar prioridade da cadeia mínima de microdungeons e
   `CaveLiquid`; não externalizar entidades/NPCs.

Complementa [runtime-validation-2026-11-07-tracy11.md](runtime-validation-2026-11-07-tracy11.md),
[sector-scheduler-progress-plan.md](sector-scheduler-progress-plan.md) e o
[checklist operacional](regional-runtime-execution-checklist.md).
