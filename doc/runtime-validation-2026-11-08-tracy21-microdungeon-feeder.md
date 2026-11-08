# Validação do feeder limitado de MicroDungeons — Tracy 21

**Build:** `1e95b6a79dcc3e25ccb99dd5aaade11d034f1112`  
**Objetivo:** medir se até quatro candidatos de dependência de MicroDungeons
ocupam a pool ociosa sem restaurar o desperdício de resultados stale.

## Artefatos

| Artefato | Tamanho | SHA-256 |
| --- | ---: | --- |
| `21.tracy` | 30.146.387 bytes | `ef99224a9b5f03d8010934e8aba396cd472fbe8c786d607032e2a94f21491b8c` |
| `exploration.zip` | 11.439.513 bytes | `1c88188d52082410575bf846a22edb826373a03730be4f5d9df5fabb733250f5` |
| `starbound.log` | 92.668 bytes | `04ee16066adc2ad2fe862d17c97cdbadada170ee1fe0664f746cc2f97e30b4bd` |
| `starbound.config` | 10.796 bytes | `84a3e31d7d9c9b488d7dac48c27796ad04fcbd12a22d841a172cf67a8be40338` |

O Tracy foi exportado pelo `tracy-csvexport` oficial 0.11.2. A execução
encerrou normalmente, sem crash de mundo.

## Configuração e resultado

Além dos previews de Tracy 20, esta execução ativou:

```json
"generationDependencyMicroDungeonPreparationPreview": true,
"generationDependencyMicroDungeonCandidateLimit": 4
```

Sessão finalizada: 133,371 s.

| Medida | Resultado |
| --- | ---: |
| Setores completos / expirados | 596 / 291 |
| Conclusões por segundo | 4,47 |
| Pool: workers médios / ativos médios | 11,0 / 0,12 |
| Planos pendentes médios | 1,14 |
| Feeder: tentativa / admissão | 1.656 / 646 |
| Feeder: commitado / stale | 410 / 224 |
| Stale do feeder | 34,7% |
| Stale global MicroDungeons | 278 / 1.157 = 24,0% |
| Entrega explícita pronta commitada | 464 |

O feeder elevou pouco a ocupação da pool (0,07 para 0,12 workers ativos na
comparação apenas indicativa com Tracy 20), mas reintroduziu muito trabalho
descartado. Não há justificativa para aumentar candidatos, workers ou o
orçamento serial a partir deste resultado.

## Tracy e aplicação serial

| Zona | Eventos | p50 | p95 | p99 | Máximo |
| --- | ---: | ---: | ---: | ---: | ---: |
| `WorldServerThread update` | 8.053 | 7,790 ms | 21,118 ms | 71,492 ms | 462,390 ms |
| `WorldStorage generate queue` | 1.843 | 1,661 ms | 17,121 ms | 44,164 ms | 130,500 ms |
| `WorldServer entities` | 8.009 | 5,356 ms | 11,782 ms | 64,324 ms | 134,021 ms |
| `Generate microdungeons` | 39.558 | 0,001 ms | 0,001 ms | 0,672 ms | 129,521 ms |
| `Microdungeon plan candidates` | 1.157 | 6,396 ms | 11,117 ms | 13,109 ms | 21,527 ms |
| `Generate cave liquid` | 715 | 0,094 ms | 6,948 ms | 7,585 ms | 8,899 ms |

A aplicação de MicroDungeons continua concentrando os outliers: `factory`
totalizou 1.899,238 ms em 649 amostras, com máximo de 129,493 ms. O feeder não
resolve esse custo e piora o staleness; a próxima otimização deve identificar
as dungeons/factories lentas, não produzir mais snapshots especulativos.

## Decisão

1. Voltar `generationDependencyMicroDungeonPreparationPreview` para `false`.
2. Manter `generationMicroDungeonReadyResultSchedulerPreview: true`: a fila
   pronta ainda evita espera desnecessária sem criar snapshots extras.
3. Instrumentar, no próximo recorte, a factory lenta por identificador estável
   de template/footprint e limiar, sem registrar conteúdo de mods. Isso permite
   isolar os outliers >25 ms e otimizar a causa sem alterar determinismo.
4. O `BadVariantType` reapareceu em `Nowhere` antes da criação do mundo. A
   correção posterior endurece todas as conversões opcionais de objeto/array
   na extração de categorias; validar na próxima build.
