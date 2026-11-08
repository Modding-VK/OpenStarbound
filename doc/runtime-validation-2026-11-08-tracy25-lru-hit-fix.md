# Validação da correção de hits LRU — Tracy 25

**Build:** `7b9e775ab82ed9d6331a26e4fbb0e09a811d505c`  
**Objetivo:** validar que hits de definição não evictam entradas não
relacionadas e medir o efeito sobre picos de MicroDungeon.

## Artefatos

| Artefato | Tamanho | SHA-256 |
| --- | ---: | --- |
| `25.tracy` | 31.883.693 bytes | `3d18902a927cce6f5900b9f9475add8ccb4a5dee34f10fa1868351fdf2d81aaf` |
| `exploration.zip` | 14.975.880 bytes | `236b904a0c35d64d9fb05535380e1ec20b0a2e35d4859e1ac6704e06b323a47b` |
| `starbound.log` | 68.799 bytes | `97b67b5a72edbc354b84a958e7605041d1704e7ba412f1a6349f331ad3d1f499` |
| `starbound.config` | 10.797 bytes | `8ad34b0130073510a32f7188b9c04d51f226da09b66a67582f232fb71aeffdd3` |

Execução encerrada normalmente, com a mesma configuração dinâmica das runs 24
e 25; o feeder especulativo de MicroDungeons continuou desligado.

## Resultado da LRU

No mundo principal (`...:9`, sessão `754257853.667`), houve 84 hits e 7
misses, sem evicções e com máximo de 7 entradas na capacidade de 20. Isso
confirma que o conserto preserva definições quentes e elimina o churn observado
na implementação anterior.

O hash `-5547472491861117508` ainda teve um único primeiro carregamento frio de
153,474 ms em leitura/construção e, na chamada seguinte, voltou a 0,003 ms de
definição. Portanto, esse pico restante não é evicção nem contenção: é custo
de primeira materialização do asset.

Em mundos posteriores, a cache chegou a 20/20 entradas e ocorreram 9 evicções
em 10 misses, comportamento esperado para inserções novas numa LRU cheia. Isso
não prova que aumentar a capacidade seja necessário para a rota principal;
apenas demonstra que a telemetria de ocupação/evicção funciona.

## Tracy

| Zona | Eventos | p50 | p95 | p99 | Máximo |
| --- | ---: | ---: | ---: | ---: | ---: |
| `WorldServerThread update` | 11.447 | 2,784 ms | 10,718 ms | 22,276 ms | 172,651 ms |
| `WorldStorage generate queue` | 2.821 | 0,059 ms | 7,135 ms | 21,046 ms | 168,852 ms |
| `MicroDungeonFactory generate` | 141 | 0,423 ms | 17,974 ms | 52,930 ms | 157,131 ms |
| `WorldServer entities` | 11.406 | 1,564 ms | 7,558 ms | 10,846 ms | 64,661 ms |

O máximo de factory (157,131 ms) e o maior frame correlacionável (172,651 ms)
ocorrem no mesmo instante aproximado e são compatíveis com o primeiro miss
lento. A melhoria reduziu a cauda geral do update frente à run 24, mas as rotas
e mundos não são A/B equivalentes; essa comparação é indicativa, não uma prova
de ganho percentual.

## Decisão e próximo passo

A correção da LRU deve permanecer. Não aumentar a capacidade global ainda: no
mundo principal ela ficou em 7/20 e não houve evicção. O próximo candidato é
um preview de pré-aquecimento **serial, limitado e orçado** de definições que
já foram selecionadas para planos visíveis, antes do commit de factory. Ele
não pode ler assets em workers, executar Lua ou pré-carregar por nome de mod.
Antes de implementar, definir limite de itens/tempo e validar memória, ordem de
RNG, save e equivalência com mods.

O fallback de `storage` para o perfil transitório `Nowhere` ainda apareceu,
mas recuperou a escrita de relatório e não causou crash.
