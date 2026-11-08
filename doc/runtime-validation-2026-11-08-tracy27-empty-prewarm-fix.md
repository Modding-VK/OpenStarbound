# Validação do bypass de planos vazios — Tracy 27

**Build:** `79698af44399749fcf6dc6982862a28dfdafa6f5`  
**Objetivo:** validar que planos privados prontos sem MicroDungeon não recebem
uma passagem extra de pré-aquecimento e não atrasam a entrega de setores.

## Artefatos

| Artefato | Tamanho | SHA-256 |
| --- | ---: | --- |
| `27.tracy` | 3.006.668 bytes | `d8075f597cddcd32e8950e63a097fe64a689cf247374131af83a40ffafdea4c3` |
| `exploration.zip` | 16.218.568 bytes | `ee36c489e4b4633ecc624101f94f1e58fa67adb51ac540fc57625918044b4fe6` |
| `starbound.log` | 72.902 bytes | `71ada1ec75cb63771ff39f36844289b7fbdfdb4d8bb8bae3dcc205e1e77b335e` |
| `starbound.config` | 10.986 bytes | `24bf9bb33bb1a4c0c238a0af1a9076d1c3c52f07af3ac8593e313a4dc24b2e1c` |

A execução encerrou normalmente, com pré-aquecimento habilitado a 1 ms e 1
candidato. Os demais previews e modo dinâmico mantiveram a configuração da run
26.

## Correção validada

`generation.microDungeonDefinitionPrewarm.skippedEmptyPlans` registrou 814.
As passagens reais de pré-aquecimento caíram de 760 na run 26 para 40, para 40
definições tocadas. Isso confirma que resultados vazios agora são aplicados na
mesma passagem em vez de ganhar um round trip serial inútil.

| Métrica | Tracy 26 | Tracy 27 |
| --- | ---: | ---: |
| Passagens de prewarm | 760 | 40 |
| Definições tocadas | 65 | 40 |
| Planos vazios ignorados | não medido | 814 |
| Setores completos | 522 | 609 |
| Setores expirados | 330 | 194 |

As rotas não são A/B equivalentes, portanto os totais de setores não provam
ganho percentual. A queda de passagens extras, porém, é consequência direta do
novo contador e da alteração de controle de fluxo.

## Cache, factory e Tracy

Houve 20 hits e 20 misses no pré-aquecimento; as factories posteriores não
tiveram entradas acima do limiar de 25 ms. A maior factory foi 24,700 ms. Isso
confirma que os misses foram retirados da aplicação da factory.

| Zona | Eventos | p50 | p95 | p99 | Máximo |
| --- | ---: | ---: | ---: | ---: | ---: |
| `WorldServerThread update` | 5.522 | 1,208 ms | 5,741 ms | 10,509 ms | 140,102 ms |
| `WorldStorage generate queue` | 1.352 | 1,383 ms | 6,122 ms | 8,468 ms | 139,279 ms |
| `Generate microdungeons` | 1.794 | 0,541 ms | 0,966 ms | 2,008 ms | 138,795 ms |
| `MicroDungeonFactory generate` | 37 | 0,344 ms | 24,199 ms | 24,700 ms |
| `WorldServer entities` | 5.428 | 0,312 ms | 1,056 ms | 5,083 ms | 37,800 ms |

O p95/p99 permanecem baixos, mas o máximo de 140,102 ms mostra que o primeiro
miss continua uma leitura serial não preemptível, agora no pré-aquecimento. O
preview melhorou a fronteira de commit, mas não é ainda uma solução para o
stutter máximo e não deve ser promovido como padrão.

## Decisão e próximo passo

O bypass de planos vazios permanece habilitado no preview. Para uso geral,
manter `generationMicroDungeonDefinitionPrewarmPreview` desligado até existir
uma estratégia que trate o carregamento frio sem apenas deslocar seu frame
longo. A próxima investigação deve isolar o custo de `DungeonDefinition`
(JSON, partes/tilesets e readers) e avaliar cache/prefetch de baixo risco no
ciclo de carregamento, sem acesso a assets por workers e sem mudar ordem de
mods, seed, entidades ou saves.

O fallback de `storage` no perfil transitório `Nowhere` foi recuperado e não
causou crash; a causa raiz continua pendente.
