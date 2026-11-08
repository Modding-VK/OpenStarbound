# Validação do pré-aquecimento de definições — Tracy 26

**Build:** `c7672a2c3180d42955370a92219e39c76245d393`  
**Objetivo:** verificar se o preview de pré-aquecimento desloca misses de
definição para antes do commit de MicroDungeon sem degradar a entrega de setores.

## Artefatos

| Artefato | Tamanho | SHA-256 |
| --- | ---: | --- |
| `26.tracy` | 3.394.049 bytes | `d88bc0db882a77f8fc84e3c3842a21fde5c55c4804b433807a78be148b7bb098` |
| `exploration.zip` | 15.620.054 bytes | `b4eb3c6ed8ff7dd047d7aded0b3c8c1d75f89ca7123ae1f68515b01297aaea11` |
| `starbound.log` | 88.232 bytes | `eaa8050f7d9fdc591829f0e557e4679b005c616d40ee34a670861487cb186f3b` |
| `starbound.config` | 10.986 bytes | `24bf9bb33bb1a4c0c238a0af1a9076d1c3c52f07af3ac8593e313a4dc24b2e1c` |

A execução encerrou normalmente. O preview foi ativado com limite de 1 ms e 1
candidato, mantendo os demais parâmetros dinâmicos anteriores.

## Resultado funcional

O pré-aquecimento foi executado: 65 tentativas, 37 hits, 28 misses, 8 evicções,
26 deadlines e 30 adiamentos. A factory posterior não teve miss de definição;
a única factory lenta registrada gastou 45,475 ms no `build`, com somente
0,002 ms de definição. A separação entre materialização da definição e commit,
portanto, funcionou como projetada.

Contudo, o maior pré-aquecimento individual levou 134,765 ms. A leitura de
asset é não preemptível; logo, o limite de 1 ms limita novas admissões, mas não
impede um frame longo já iniciado. O maior update da WorldThread foi 135,811
ms e coincide com esse trabalho de pré-aquecimento, não com uma factory.

## Regressão de entrega

O preview adiciona hoje uma passagem extra mesmo quando um plano pronto não
contém MicroDungeons. Foram 760 passagens de pré-aquecimento para apenas 65
definições tocadas; havia 739 planos agendados, 720 aplicados e somente 13
MicroDungeons aceitas. A evidência é compatível com a maioria dos planos vazios
sendo adiada desnecessariamente antes do commit.

A run apresentou 522 setores completos e 330 expirados, com bloqueio visível
de MicroDungeons em 1.056 tentativas e espera máxima de setor de 14,463 s. A
rota não é A/B equivalente à run 25, portanto não atribui todo o aumento de
expirações ao preview, mas a passagem extra para planos vazios é uma regressão
de lógica estabelecida e deve ser corrigida antes de outra validação.

## Tracy

| Zona | Eventos | p50 | p95 | p99 | Máximo |
| --- | ---: | ---: | ---: | ---: | ---: |
| `WorldServerThread update` | 6.146 | 1,100 ms | 5,194 ms | 10,523 ms | 135,811 ms |
| `WorldStorage generate queue` | 1.518 | 1,137 ms | 6,262 ms | 8,682 ms | 135,145 ms |
| `Generate microdungeons` | 2.220 | 0,012 ms | 0,930 ms | 1,922 ms | 134,776 ms |
| `MicroDungeonFactory generate` | 65 | 0,328 ms | 2,405 ms | 45,773 ms |
| `WorldServer entities` | 6.094 | 0,316 ms | 0,945 ms | 2,733 ms | 40,285 ms |

O p95/p99 é baixo nesta rota, mas o máximo permanece incompatível com a meta de
eliminar stutter de carregamento frio; o custo foi apenas deslocado para outro
ponto da mesma thread.

## Decisão e próximo passo

Manter `generationMicroDungeonDefinitionPrewarmPreview: false` até corrigir o
caso de plano vazio: ele deve aplicar na mesma passagem, sem pré-aquecimento ou
defer adicional. Depois, repetir a mesma rota. Mesmo após essa correção, o
preview não pode ser considerado solução para um miss individual de 135 ms;
qualquer próximo desenho precisa tratar a materialização fria como operação
não-preemptível e preservar compatibilidade de assets/mods.

O fallback de `storage` no perfil transitório `Nowhere` foi recuperado e não
causou crash; sua causa raiz segue pendente.
