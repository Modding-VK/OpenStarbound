# Validação do recorte TMX — Tracy 29

**Build:** `d8f26df1ce5eb5be6325c6f09b6afbb3809c1b90`  
**Objetivo:** coletar a decomposição interna de readers TMX em um miss frio de
MicroDungeon.

## Artefatos

| Artefato | Tamanho | SHA-256 |
| --- | ---: | --- |
| `29.tracy` | 3.834.633 bytes | `57dcf28b9017550920aef3b0217f33a40dae34cab19fd70bdaca57767a8015c7` |
| `exploration.zip` | 17.445.226 bytes | `6b3f650860283dc9f85869f13a6b869a6b0090d8b1486fdc2c77957ae588c544` |
| `starbound.log` | 61.104 bytes | `6f91ff5fd5c468d287c3ca07ae5f171fd4773849b9f2e044113ed86e1d3dc352` |
| `starbound.config` | 10.987 bytes | `2400efb70e994d287ac083c38b62ba15c76ba834a94ee69e9db7b3e6d90dfacf` |

A execução encerrou normalmente. O pré-aquecimento permaneceu desligado, como
necessário para observar um miss frio no caminho da factory.

## Resultado

Esta rota não acionou uma factory de MicroDungeon: Tracy registra zero eventos
de `MicroDungeonFactory generate`, `slowMicroDungeonFactories` está vazio e
não houve hit/miss da cache de definições. Logo, nenhum dos novos campos TMX
foi emitido. A instrumentação não pode ser avaliada nem a causa TMX da run 28
ser subdividida nesta amostra.

O planejamento assíncrono esteve ativo (748 planos agendados, 739 aplicados),
mas nenhum plano resultou em aplicação de factory nesta rota. Isso é compatível
com planos vazios, descarte/unload ou ausência de placements elegíveis; não
prova regressão de geração.

## Tracy e entrega geral

| Zona | Eventos | p50 | p95 | p99 | Máximo |
| --- | ---: | ---: | ---: | ---: | ---: |
| `WorldServerThread update` | 6.413 | 2,362 ms | 6,132 ms | 11,456 ms | 93,544 ms |
| `WorldStorage generate queue` | 1.585 | 1,084 ms | 4,531 ms | 7,000 ms | 28,995 ms |
| `Generate microdungeons` | 1.487 | 0,504 ms | 0,889 ms | 1,163 ms | 3,003 ms |
| `WorldServer entities` | 6.380 | 1,429 ms | 3,007 ms | 5,234 ms | 16,051 ms |

Foram concluídos 730 setores e expiraram 134; a espera máxima registrada foi
9,906 s. Sem factory na amostra, esses resultados não permitem avaliar a
otimização de TMX nem comparar diretamente com Tracy 28.

## Próximo passo exato

Repetir com o mesmo build e configuração, atravessando uma área/planeta que
produza uma MicroDungeon TMX nova — idealmente a rota que gerou o hash lento na
run 28. Manter `regionalDetails: true`, limiar de 25 ms e o pré-aquecimento
desligado. A coleta é válida somente se apresentar ao menos uma entrada em
`slowMicroDungeonFactories` com `definitionTmxMapMilliseconds`.

O fallback de `storage` no perfil transitório `Nowhere` continuou recuperável
e não causou crash; a causa raiz permanece pendente.
