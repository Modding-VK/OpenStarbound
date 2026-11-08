# Validação da construção fria TMX — Tracy 28

**Build:** `ebd92509ddb2f3532e74252071ddcaecfb8be5be`  
**Objetivo:** decompor o primeiro miss frio de `DungeonDefinition` sem
pré-aquecimento e identificar a subfase responsável pelo stall serial.

## Artefatos

| Artefato | Tamanho | SHA-256 |
| --- | ---: | --- |
| `28.tracy` | 4.955.047 bytes | `00a464e1bf9582f7fc78c7ab9792a7ffa27dc7c073aee4c2b2eb20fde6d941f7` |
| `exploration.zip` | 16.905.847 bytes | `2c96e67e4293d441fc9d8f2fe4a143f7dde3bc5911a83b0890c54b12960e69d2` |
| `starbound.log` | 104.823 bytes | `38ddf1c855fcd3fa8e3532ea9ff9411aea90e6c0a61acbfa4f43076e41f3d749` |
| `starbound.config` | 10.987 bytes | `2400efb70e994d287ac083c38b62ba15c76ba834a94ee69e9db7b3e6d90dfacf` |

A execução encerrou normalmente. `generationMicroDungeonDefinitionPrewarmPreview`
estava explicitamente `false` para medir o caminho frio na factory.

## Causa estabelecida

Uma factory lenta de 192,313 ms teve miss de cache e 133,043 ms de definição.
O recorte interno identifica a causa:

| Fase | Duração |
| --- | ---: |
| JSON do `.dungeon` | 1,368 ms |
| Metadata/rules | 0,004 ms |
| Tileset | 0,389 ms |
| Partes, inclusivo | 131,275 ms |
| Partes `image` (2) | 1,436 ms |
| Partes `tmx` (2) | 129,831 ms |
| Aplicação/construção posterior | 56,898 ms |

O gargalo é inequívoco: a materialização de readers TMX, incluindo leitura dos
assets e construção de seus mapas, não o JSON da definição, metadata, tileset
ou partes image. Os tempos por tipo são subconjuntos inclusivos da fase de
partes. Nenhum nome ou path de asset foi gravado.

## Tracy e entrega

| Zona | Eventos | p50 | p95 | p99 | Máximo |
| --- | ---: | ---: | ---: | ---: | ---: |
| `WorldServerThread update` | 8.000 | 1,351 ms | 4,882 ms | 11,127 ms | 203,033 ms |
| `WorldStorage generate queue` | 1.969 | 1,259 ms | 5,526 ms | 10,231 ms | 201,554 ms |
| `Generate microdungeons` | 1.981 | 0,516 ms | 0,920 ms | 1,201 ms | 201,174 ms |
| `MicroDungeonFactory generate` | 5 | 12,202 ms | 192,311 ms | 192,311 ms | 192,311 ms |
| `WorldServer entities` | 7.926 | 0,459 ms | 1,419 ms | 2,631 ms | 30,297 ms |

O pico de factory de 192,313 ms coincide com o maior frame de 203,033 ms. A
cache teve 4 misses e 1 hit, ocupando no máximo 4/20 entradas, portanto não há
evicção ou contenção como causa deste evento.

Foram concluídos 1.049 setores e expiraram 159. A rota não é A/B equivalente
às anteriores; esses totais não são atribuídos à factory isolada. O bloqueio
por MicroDungeons continua relevante (995 tentativas visíveis).

## Decisão e próximo passo

Manter o pré-aquecimento desligado fora de testes. A próxima instrumentação
deve particionar `TMXPartReader::readAsset` em leitura do JSON do mapa,
construção de tilesets, camadas/decompressão e grupos de objetos. Só após saber
qual dessas etapas domina deve-se avaliar cache de mapas TMX ou pré-carregamento
de baixo risco. Leitura de assets em workers, publicação concorrente e mudança
da ordem de mods continuam fora de escopo.

O fallback de `storage` no perfil transitório `Nowhere` foi recuperado e não
causou crash; sua causa raiz permanece pendente.
